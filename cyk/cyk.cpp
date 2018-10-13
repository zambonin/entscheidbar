#include <iostream>
#include <regex>
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

int32_t main() {
  grammar g{};

  do {
    g = load_grammar(std::cin);
  } while (g.start_prod != 0);

  exit(EXIT_SUCCESS);
}
