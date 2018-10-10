#include <iostream>
#include <regex>
#include <unordered_map>

static const uint16_t constexpr MAX_INT = 1000;

using state = std::pair<uint32_t, std::array<uint16_t, 10>>;
using instruction = std::tuple<std::string, std::string, uint16_t, uint16_t>;
using cond_func_map_type =
    std::unordered_map<std::string, std::function<bool(uint16_t, uint16_t)>>;
using arith_func_map_type =
    std::unordered_map<std::string,
                       std::function<uint16_t(uint16_t, uint16_t)>>;

struct pair_hash {
  std::size_t operator()(const state &p) const {
    uint32_t summation = 0;
    for (const auto &reg : p.second) {
      summation += reg;
    }

    return std::hash<uint32_t>{}(p.first) ^ std::hash<uint32_t>{}(summation);
  }
};

struct program {
  std::vector<std::string> lines;
  std::unordered_map<uint32_t, uint32_t> cond_markers;
  std::unordered_map<state, bool, pair_hash> recursion_states;
  std::array<uint16_t, 10> reg_bank = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0};
};

program load_program(std::istream &file, const std::regex &match) {
  program p{};

  try {
    uint32_t number_lines = 0, current = 0;
    std::stack<uint32_t> cond_matches;

    file >> number_lines >> p.reg_bank[0];

    // end of input with multiple programs
    if (number_lines == 0 && p.reg_bank[0] == 0) {
      return p;
    }

    // wrong second arg or file not found
    if (p.reg_bank[0] >= MAX_INT) {
      exit(EXIT_FAILURE);
    }

    // ignore rest of line
    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::string line;
    while (current != number_lines && getline(file, line) &&
           std::regex_match(line, match)) {
      p.lines.push_back(line);
      current = p.lines.size();
      if (line == "ENDIF") {
        p.cond_markers[cond_matches.top() - 1] = current - 1;
        cond_matches.pop();
      } else if (line.find("IF") != std::string::npos) {
        p.cond_markers.emplace(std::make_pair(current - 1, 0));
        cond_matches.push(current);
      }
    }

    bool unbalanced_ifs = !cond_matches.empty();
    bool last_line_not_ret =
        (current != 0) && p.lines.back().find("RET") == std::string::npos;

    if (unbalanced_ifs || last_line_not_ret) {
      exit(EXIT_FAILURE);
    }
  } catch (const std::exception &e) {
    exit(EXIT_FAILURE);
  }

  return p;
}

uint16_t parse_arg(const program &p, const std::string &s) {
  if (s.empty()) {
    return -1;
  }
  if (s[0] == 'R') {
    return p.reg_bank[s[1] - '0'];
  }
  return std::stoi(s);
}

instruction parse_inst(const program &p, const uint32_t n) {
  std::string s = p.lines[n];
  std::string::size_type space = s.find(' '), comma = s.find(',');

  if (comma != std::string::npos) {
    return std::make_tuple(
        s.substr(0, space), s.substr(space + 1, comma - space - 1),
        parse_arg(p, s.substr(space + 1, comma - space - 1)),
        parse_arg(p, s.substr(comma + 1, std::string::npos)));
  }
  if (space != std::string::npos) {
    return std::make_tuple(s.substr(0, space), s.substr(space + 1),
                           parse_arg(p, s.substr(space + 1)), -1);
  }

  return std::make_tuple(s, s, -1, -1);
}

uint16_t evaluate(program &p, cond_func_map_type &cond_functions,
                  arith_func_map_type &arith_functions) {
  uint32_t current = 0;
  std::stack<state> stack;
  std::unordered_map<uint16_t, uint16_t> memoization;
  std::string command, output;
  uint16_t arg1, arg2;

  while (current != p.lines.size()) {
    std::tie(command, output, arg1, arg2) = parse_inst(p, current);

    if (command[0] == 'I') {
      current = cond_functions[command](arg1, arg2) ? current
                                                    : p.cond_markers[current];
    } else if (command == "CALL") {
      if (memoization.find(arg1) != memoization.end()) {
        p.reg_bank[9] = memoization[arg1];
      } else {
        auto copy_bank(p.reg_bank);
        stack.push(std::make_pair(current, copy_bank));

        current = -1;
        p.reg_bank[0] = arg1;

        auto s = std::make_pair(current, p.reg_bank);
        if (p.recursion_states.find(s) == p.recursion_states.end()) {
          p.recursion_states.insert(std::make_pair(s, true));
        } else {
          return MAX_INT;
        }
      }
    } else if (command == "RET") {
      memoization[p.reg_bank[0]] = p.reg_bank[9] = arg1;
      if (static_cast<unsigned int>(!stack.empty()) != 0u) {
        std::tie(current, p.reg_bank) = stack.top();
        p.reg_bank[9] = arg1;
        stack.pop();
      } else {
        return p.reg_bank[9];
      }
    } else if (command != "ENDIF") {
      p.reg_bank[output[1] - '0'] =
          arith_functions[command](arg1, arg2) % MAX_INT;
    }

    current++;
  }

  return p.reg_bank[9];
}

int32_t main() {
  const std::string integers("([1-9]?[0-9]{1,2})|1000"), registers("R[0-9]"),
      operand("(" + integers + "|" + registers + ")"), no_arg_key("ENDIF"),
      call_key("(CALL|RET)"), valid_arith_key("(MO[DV]|ADD|SUB|MUL|DIV)"),
      valid_cond_key("(IF(N?EQ|GE?|LE?))"),
      valid_call_line(call_key + " " + operand),
      valid_arith_line(valid_arith_key + " " + registers + "," + operand),
      valid_cond_line(valid_cond_key + " " + operand + "," + operand);

  const std::regex valid_lines(no_arg_key + "|" + valid_call_line + "|" +
                                   valid_arith_line + "|" + valid_cond_line,
                               std::regex_constants::optimize);

  cond_func_map_type cond_functions = {
      {"IFEQ", std::equal_to<>()}, {"IFNEQ", std::not_equal_to<>()},
      {"IFG", std::greater<>()},   {"IFGE", std::greater_equal<>()},
      {"IFL", std::less<>()},      {"IFLE", std::less_equal<>()},
  };

  arith_func_map_type arith_functions = {
      {"ADD", std::plus<>()},
      {"SUB",
       [](auto a, auto b) { return (a - b == -1) ? MAX_INT - 1 : a - b; }},
      {"MUL", std::multiplies<>()},
      {"DIV", [](auto a, auto b) { return (b == 0) ? 0 : a / b; }},
      {"MOD", [](auto a, auto b) { return (b == 0) ? 0 : a % b; }},
      {"MOV", [](auto /*a*/, auto b) { return b; }},
  };

  program p{};
  do {
    p = load_program(std::cin, valid_lines);
    uint16_t result = evaluate(p, cond_functions, arith_functions);
    if (result == MAX_INT) {
      std::cout << "*" << std::endl;
    } else if (static_cast<uint8_t>(!p.lines.empty()) != 0u) {
      std::cout << result << std::endl;
    }
  } while (static_cast<uint8_t>(!p.lines.empty()) != 0u);

  exit(EXIT_SUCCESS);
}
