#include <iostream>
#include <regex>
#include <unordered_map>
#include <unordered_set>

static const uint16_t constexpr MAX_INT = 1000;
static const uint16_t constexpr pow10[3] = {1, 10, 100};

using bank = std::array<uint16_t, 10>;
using instruction = std::tuple<std::string_view, uint8_t, uint16_t, uint16_t>;
template <typename T>
using func_map_type =
    std::unordered_map<std::string_view, std::function<T(uint16_t, uint16_t)>>;
using cond_func_map_type = func_map_type<bool>;
using arith_func_map_type = func_map_type<uint16_t>;

// Provides a way to use an array of integers as a key in unordered containers.
struct bank_hash {
  std::size_t operator()(const bank &b) const {
    uint32_t summation = 0;
    for (const uint16_t reg : b) {
      summation = summation * 31 + std::hash<uint16_t>{}(reg);
    }
    return std::hash<uint32_t>{}(summation);
  }
};

// In this language, a program consists of a sequence of lines, a memory
// consisting of ten registers, markers that represent how a conditional should
// modify the program counter, and a set of old memory states at each CALL, to
// identify infinite loops.
struct program {
  std::vector<std::string> lines;
  std::unordered_map<uint32_t, uint32_t> cond_markers;
  bank reg_bank = {0};
  std::unordered_set<bank, bank_hash> recursion_states;
};

// Creates a structure resembling a program, according to the definition below,
// doing so through regular expressions to match lines and check for syntax
// errors.
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

    // problem restrictions
    if (number_lines > 100 || p.reg_bank[0] >= 1000) {
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

// Gives the value for a register or numerical string.
uint16_t parse_arg(const program &p, const std::string_view &s) {
  if (s.empty()) {
    return -1;
  }
  if (s[0] == 'R') {
    return p.reg_bank[s[1] - '0'];
  }
  std::string::size_type len = s.size();
  uint16_t n = 0, i = len;
  // pretend we're atoi
  while ((i--) != 0u) {
    n += pow10[i] * (s[len - i - 1] - '0');
  }
  return n;
}

// Converts a program line into a tuple of strings and integers, representing
// the opcode, first, second and third arguments, respectively, if applicable.
instruction parse_inst(const program &p, const uint32_t n) {
  const std::string_view s = p.lines[n];
  const std::string::size_type space = s.find(' '), comma = s.find(',');

  // binary instruction if a comma is present
  if (comma != std::string::npos) {
    return std::make_tuple(
        s.substr(0, space), s.substr(space + 1, comma - space - 1)[1] - '0',
        parse_arg(p, s.substr(space + 1, comma - space - 1)),
        parse_arg(p, s.substr(comma + 1, std::string::npos)));
  }

  // unary instruction if a space is present
  if (space != std::string::npos) {
    return std::make_tuple(s.substr(0, space), s.substr(space + 1)[1] - '0',
                           parse_arg(p, s.substr(space + 1)), -1);
  }

  // endif keyword, degenerate case
  return std::make_tuple(s, -1, -1, -1);
}

// Processes the program according to the language description. Arithmetic and
// conditional instructions are calculated through function map lookups. CALL
// and RET are unique in their behavior and cannot be easily isolated.
uint16_t evaluate(program &p, cond_func_map_type &cond_functions,
                  arith_func_map_type &arith_functions) {
  uint32_t current = 0;
  uint16_t arg1, arg2;
  uint8_t output;
  std::stack<std::pair<uint32_t, bank>> stack;
  std::unordered_map<uint16_t, uint16_t> memoization;
  std::string_view command;

  while (current != p.lines.size()) {
    std::tie(command, output, arg1, arg2) = parse_inst(p, current);

    if (command == "CALL") {
      if (memoization.find(arg1) != memoization.end()) {
        // memoize CALL evaluations and return early if argument is known
        p.reg_bank[9] = memoization[arg1];
      } else {
        // push recursion to stack and keep track of program counter and memory
        stack.push(std::make_pair(current, p.reg_bank));
        current = -1;
        p.reg_bank[0] = arg1;
        if (p.recursion_states.find(p.reg_bank) == p.recursion_states.end()) {
          // if memory state has already happened, an infinite loop will occur
          p.recursion_states.insert(p.reg_bank);
        } else {
          return MAX_INT;
        }
      }
    } else if (command == "RET") {
      // save to memoization if return is still not there
      memoization[p.reg_bank[0]] = p.reg_bank[9] = arg1;
      if (static_cast<uint8_t>(!stack.empty()) != 0u) {
        std::tie(current, p.reg_bank) = stack.top();
        p.reg_bank[9] = arg1;
        stack.pop();
      } else {
        return p.reg_bank[9];
      }
    } else if (command[0] == 'I') {
      current = cond_functions[command](arg1, arg2) ? current
                                                    : p.cond_markers[current];
    } else if (command[0] != 'E') {
      p.reg_bank[output] = arith_functions[command](arg1, arg2) % MAX_INT;
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
