#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <regex>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

static const uint16_t constexpr MAX_INT = 1000;

struct program {
  std::vector<std::string> lines;
  std::unordered_map<uint32_t, uint32_t> cond_markers;

  std::unordered_map<std::string, uint16_t> reg_bank = {
      {"R0", -1}, {"R1", 0}, {"R2", 0}, {"R3", 0}, {"R4", 0},
      {"R5", 0},  {"R6", 0}, {"R7", 0}, {"R8", 0}, {"R9", 0},
  };
};

program load_program(std::istream &file, const std::regex &match) {
  program p{};

  try {
    uint32_t number_lines = 0, current = 0;
    std::stack<uint32_t> cond_matches;

    file >> number_lines >> p.reg_bank["R0"];

    // end of input with multiple programs
    if (number_lines == 0 && p.reg_bank["R0"] == 0) {
      return p;
    }

    // wrong second arg or file not found
    if (p.reg_bank["R0"] >= MAX_INT) {
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

  program p{};
  do {
    p = load_program(std::cin, valid_lines);
  } while (static_cast<uint8_t>(!p.lines.empty()) != 0u);

  exit(EXIT_SUCCESS);
}
