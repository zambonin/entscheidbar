#include <iostream>
#include <regex>
#include <set>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

struct grammar {
  char start_prod;
  std::unordered_set<char> productions, terminals;
  std::unordered_map<char, std::unordered_set<std::string>> rules;
  std::vector<std::string> possible_inputs;
};

grammar load_grammar(std::istream &in) {
  grammar g{};

  try {
    std::string prod, term, line;
    in >> g.start_prod >> prod >> term;

    // end of input with multiple grammars
    if (g.start_prod == 0) {
      return g;
    }

    g.productions.insert(prod.begin(), prod.end());
    g.terminals.insert(term.begin(), term.end());

    if (g.productions.find(g.start_prod) == g.productions.end()) {
      exit(EXIT_FAILURE);
    }

    // ignore rest of line
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    const std::regex valid_rules("[" + prod + "] -> ([" + term + "]|[" + prod +
                                 "]{2})");

    while (getline(in, line) && line != "# -> #" &&
           std::regex_match(line, valid_rules)) {
      g.rules[line[0]].insert(
          line.substr(line.find('>') + 2, std::string::npos));
    }

    while (getline(in, line) && line != "#") {
      g.possible_inputs.emplace_back(line);
    }
  } catch (const std::exception &e) {
    exit(EXIT_FAILURE);
  }

  return g;
}

std::set<char> get_rules_for_symbols(const grammar &g,
                                     const std::string &symb) {
  std::set<char> result;
  for (const auto &rule : g.rules) {
    for (const auto &poss : rule.second) {
      bool term = symb.size() == 1 && poss[0] == symb[0];
      bool non_term = symb.size() == 2 && poss == symb;
      if (term || non_term) {
        result.insert(rule.first);
      }
    }
  }
  return result;
}

std::set<char> get_rules_for_symbol(const grammar &g, const char &c) {
  return get_rules_for_symbols(g, std::string({c}));
}

std::set<std::string> cartesian_prod(const std::set<char> &a,
                                     const std::set<char> &b) {
  std::set<std::string> result;
  for (const char &a1 : a) {
    for (const char &b1 : b) {
      result.insert(std::string({a1, b1}));
    }
  }
  return result;
}

bool cyk(const grammar &g, const std::string &w) {
  const std::string::size_type n = w.size();
  std::vector<std::vector<std::set<char>>> table(
      n, std::vector<std::set<char>>(n));

  for (uint32_t i = 0; i < n; ++i) {
    std::set<char> rules = get_rules_for_symbol(g, w[i]);
    table[i][i].insert(rules.begin(), rules.end());
  }

  for (uint32_t l = 1; l < n; l++) {
    for (uint32_t r = 0; r < n - l; r++) {
      for (uint32_t t = 0; t < l; t++) {
        std::set<std::string> prod =
            cartesian_prod(table[r][r + t], table[r + t + 1][r + l]);
        for (const auto &pair : prod) {
          std::set<char> rules = get_rules_for_symbols(g, pair);
          table[r][r + l].insert(rules.begin(), rules.end());
        }
      }
    }
  }

  return table[0][n - 1].find(g.start_prod) != table[0][n - 1].end();
}

void cyk_wrapper(const grammar &g, const uint32_t index) {
  std::cout << "Instancia " << index << std::endl;
  for (const std::string &word : g.possible_inputs) {
    std::string result = cyk(g, word) ? "" : " nao";
    std::cout << word << result << " e uma palavra valida" << std::endl;
  }
  std::cout << std::endl;
}

int32_t main() {
  grammar g{};
  uint32_t i = 0;

  do {
    g = load_grammar(std::cin);
    if (g.start_prod != 0) {
      cyk_wrapper(g, ++i);
    }
  } while (g.start_prod != 0);

  exit(EXIT_SUCCESS);
}
