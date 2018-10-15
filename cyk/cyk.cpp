#include <iostream>
#include <regex>
#include <set>
#include <unordered_map>
#include <unordered_set>

using char_pair = std::pair<char, char>;
using char_set = std::set<char>;

// Provides a way to use a pair of variables as a key in unordered containers.
struct pair_hash {
  std::size_t operator()(const char_pair &p) const {
    // pretend XOR is not commutative for pairs such as 'AB' and 'BA'
    return std::hash<char>{}(p.first) ^ (std::hash<char>{}(p.second) << 1);
  }
};

// A context-free grammar is a 4-uple consisting of variables, terminals,
// productions and the start variable. Only two of these are needed here.
struct grammar {
  char start_prod;
  std::unordered_map<char, std::unordered_set<char_pair, pair_hash>> rules;
  std::vector<std::string> possible_inputs;
};

// Provides a way to use a grammar as a key in unordered containers.
struct gram_hash {
  std::size_t operator()(const grammar &g) const {
    uint32_t hash = g.start_prod;
    for (const auto &rule : g.rules) {
      hash ^= std::hash<char>{}(rule.first);
      for (const auto &pair : rule.second) {
        hash ^= rule.second.hash_function()(pair);
      }
    }
    return hash;
  }
};

// Two grammars are equal if they have the same rules and same possible inputs.
// This prevents unnecessary instantation of CYK with two equal inputs.
struct gram_eq {
  bool operator()(const grammar &g1, const grammar &g2) const {
    return g1.rules == g2.rules && g1.possible_inputs == g2.possible_inputs;
  }
};

using input_map = std::unordered_map<std::string, bool>;
using grammar_map = std::unordered_map<grammar, input_map, gram_hash, gram_eq>;

// Creates a structure containing the description of a context-free grammar in
// Chomsky normal form, and does so through matching possible rules with
// regular expressions.
grammar load_grammar(std::istream &in) {
  grammar g{};

  try {
    std::string prod, term, line;
    in >> g.start_prod >> prod >> term;

    // end of input with multiple grammars
    if (g.start_prod == 0) {
      return g;
    }

    // various input file limitations according to original problem
    bool valid_start_prod = std::isupper(g.start_prod);
    bool valid_prod = std::all_of(prod.begin(), prod.end(), isupper);
    bool start_in_prod = prod.find(g.start_prod) != std::string::npos;
    bool no_space_term = term.find(' ') == std::string::npos;
    bool no_hashtag_term = term.find('#') == std::string::npos;
    if (!(valid_start_prod && valid_prod && start_in_prod && no_space_term &&
          no_hashtag_term)) {
      throw std::invalid_argument("input is not valid");
    }

    // ignore rest of line
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    const std::regex valid_rules("[" + prod + "] -> ([" + term + "]|[" + prod +
                                 "]{2})");

    while (getline(in, line) && line != "# -> #" &&
           std::regex_match(line, valid_rules)) {
      // if rule produces only a terminal, fill the remaining pair element with
      // an useless symbol
      g.rules[line[0]].insert(
          char_pair(line[5], (line.size() == 6) ? '#' : line[6]));
    }

    constexpr uint8_t max_word_len = 50;
    while (getline(in, line) && line != "#") {
      // input must not feature letters that symbolize productions
      bool letter_is_prod = std::any_of(line.begin(), line.end(), [=](char c) {
        return prod.find(c) != std::string::npos;
      });
      if (line.size() > max_word_len || letter_is_prod) {
        throw std::invalid_argument("word is not valid");
      }
      g.possible_inputs.emplace_back(line);
    }
  } catch (const std::exception &e) {
    exit(EXIT_FAILURE);
  }

  return g;
}

// Identify which rules produce a given _ordered_ pair of symbols.
char_set get_rules_for_symbols(const grammar &g, const char_pair &p) {
  char_set result;
  for (const auto &rule : g.rules) {
    for (const char_pair &poss : rule.second) {
      if (poss == p) {
        result.insert(rule.first);
      }
    }
  }
  return result;
}

// Creates the cartesian product of two sets of characters.
std::set<char_pair> cartesian_prod(const char_set &a, const char_set &b) {
  std::set<char_pair> result;
  for (const char &a1 : a) {
    for (const char &b1 : b) {
      result.insert(std::make_pair(a1, b1));
    }
  }
  return result;
}

// CYK employs dynamic programming to test membership of a string in a
// context-free language, here represented by a grammar. It builds a triangular
// matrix of rules that produce substrings of the given input.
bool cyk(const grammar &g, const std::string &w) {
  const std::string::size_type n = w.size();
  std::vector<std::vector<char_set>> table(n, std::vector<char_set>(n));
  std::unordered_map<char_pair, char_set, pair_hash> memo;

  // fill table's diagonal with the rules that produce characters in the input
  for (uint32_t i = 0; i < n; ++i) {
    char_set rules = get_rules_for_symbols(g, char_pair(w[i], '#'));
    // if no rules produce any of the letters in the word, then it cannot be a
    // member of the language
    if (rules.empty()) {
      return false;
    }
    table[i][i].insert(rules.begin(), rules.end());
  }

  // non-trivial substring lengths not featured above
  for (uint32_t l = 1; l < n; l++) {
    // starting index for a substring of length l
    for (uint32_t r = 0; r < n - l; r++) {
      // every character of the substring w[r:r+l]
      for (uint32_t t = 0; t < l; t++) {
        // the substring can be split in two parts, each produced by their
        // own rules; all rules from both parts must be considered to figure
        // out if any produces the substring, hence the cartesian product
        std::set<char_pair> prod =
            cartesian_prod(table[r][r + t], table[r + t + 1][r + l]);
        for (const char_pair &pair : prod) {
          // memoize rules producing a given pair of characters
          if (memo.find(pair) == memo.end()) {
            memo[pair] = get_rules_for_symbols(g, pair);
          }
          table[r][r + l].insert(memo[pair].begin(), memo[pair].end());
        }
      }
    }
  }

  return table[0][n - 1].find(g.start_prod) != table[0][n - 1].end();
}

// Executes CYK for all words in the list of possible inputs. Features an
// example of memoization since repeated words can appear in the input file.
input_map cyk_wrapper(const grammar &g) {
  input_map memo;
  for (const std::string &word : g.possible_inputs) {
    if (memo.find(word) == memo.end()) {
      memo[word] = cyk(g, word);
    }
  }
  return memo;
}

// Prints output according to SPOJ guidelines.
void cyk_printer(const grammar &g, input_map results, const uint32_t index) {
  std::cout << "Instancia " << index << std::endl;
  for (const std::string &word : g.possible_inputs) {
    std::cout << word;
    if (!results[word]) {
      std::cout << " nao";
    }
    std::cout << " e uma palavra valida" << std::endl;
  }
  std::cout << std::endl;
}

int32_t main() {
  grammar g{};
  grammar_map memo;
  uint32_t i = 0;

  do {
    g = load_grammar(std::cin);
    if (g.start_prod != 0) {
      if (memo.find(g) == memo.end()) {
        memo[g] = cyk_wrapper(g);
      }
      cyk_printer(g, memo[g], ++i);
    }
  } while (g.start_prod != 0);

  exit(EXIT_SUCCESS);
}
