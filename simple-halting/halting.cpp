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

using state = std::pair<uint32_t, std::unordered_map<std::string, uint16_t>>;

struct pair_hash {
  std::size_t operator()(const state &p) const {
    uint32_t summation = 0;
    for (const auto &reg : p.second) {
      summation += reg.second;
    }

    return std::hash<uint32_t>{}(p.first) ^ std::hash<uint32_t>{}(summation);
  }
};

struct program {
  std::vector<std::string> lines;
  std::unordered_map<uint32_t, uint32_t> cond_markers;
  std::unordered_map<state, bool, pair_hash> recursion_states;

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

uint16_t parse_arg(const program &p, const std::string &s) {
  if (s.empty()) {
    return -1;
  }
  try {
    return p.reg_bank.at(s);
  } catch (const std::out_of_range &e) {
    return std::stoi(s);
  }
}

uint16_t evaluate(program &p) {
  uint32_t current = 0;
  std::stack<uint32_t> calls, inputs;
  while (current != p.lines.size()) {
    std::stringstream ss(p.lines[current]);
    std::string instruction, args, arg1, arg2;
    std::getline(ss, instruction, ' ');
    std::getline(ss, args, ' ');
    std::stringstream sss(args);
    std::getline(sss, arg1, ',');
    std::getline(sss, arg2, ',');
    auto inst =
        std::make_tuple(instruction, parse_arg(p, arg1), parse_arg(p, arg2));

    if (instruction == "IFEQ") {
      current = (std::get<1>(inst) == std::get<2>(inst))
                    ? current
                    : p.cond_markers[current];
    } else if (instruction == "IFNEQ") {
      current = (std::get<1>(inst) != std::get<2>(inst))
                    ? current
                    : p.cond_markers[current];
    } else if (instruction == "IFG") {
      current = (std::get<1>(inst) > std::get<2>(inst))
                    ? current
                    : p.cond_markers[current];
    } else if (instruction == "IFGE") {
      current = (std::get<1>(inst) >= std::get<2>(inst))
                    ? current
                    : p.cond_markers[current];
    } else if (instruction == "IFL") {
      current = (std::get<1>(inst) < std::get<2>(inst))
                    ? current
                    : p.cond_markers[current];
    } else if (instruction == "IFLE") {
      current = (std::get<1>(inst) <= std::get<2>(inst))
                    ? current
                    : p.cond_markers[current];
    } else if (instruction == "MOV") {
      p.reg_bank[arg1] = std::get<2>(inst);
    } else if (instruction == "ADD") {
      p.reg_bank[arg1] = (p.reg_bank[arg1] + std::get<2>(inst)) % MAX_INT;
    } else if (instruction == "SUB") {
      p.reg_bank[arg1] = (p.reg_bank[arg1] - std::get<2>(inst)) % MAX_INT;
      if (p.reg_bank[arg1] == 65535) {
        p.reg_bank[arg1] = MAX_INT - 1;
      }
    } else if (instruction == "MUL") {
      p.reg_bank[arg1] = (p.reg_bank[arg1] * std::get<2>(inst)) % MAX_INT;
    } else if (instruction == "DIV") {
      if (std::get<2>(inst) == 0) {
        p.reg_bank[arg1] = 0;
      } else {
        p.reg_bank[arg1] = (p.reg_bank[arg1] / std::get<2>(inst)) % MAX_INT;
      }
    } else if (instruction == "MOD") {
      if (std::get<2>(inst) == 0) {
        p.reg_bank[arg1] = 0;
      } else {
        p.reg_bank[arg1] = (p.reg_bank[arg1] % std::get<2>(inst)) % MAX_INT;
      }
    } else if (instruction == "CALL") {
      calls.push(current);
      inputs.push(p.reg_bank["R0"]);

      current = -1;
      p.reg_bank["R0"] = std::get<1>(inst);
      auto s = std::make_pair(current, p.reg_bank);
      if (p.recursion_states.find(s) == p.recursion_states.end()) {
        p.recursion_states.insert(std::make_pair(s, true));
      } else {
        return MAX_INT;
      }
    } else if (instruction == "RET") {
      p.reg_bank["R9"] = std::get<1>(inst);
      if (static_cast<unsigned int>(!calls.empty()) != 0u) {
        current = calls.top();
        calls.pop();
        p.reg_bank["R0"] = inputs.top();
        inputs.pop();
      } else {
        return p.reg_bank["R9"];
      }
    }

    current++;
  }

  return p.reg_bank["R9"];
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
    uint16_t result = evaluate(p);
    if (result == MAX_INT) {
      std::cout << "*" << std::endl;
    } else if (static_cast<uint8_t>(!p.lines.empty()) != 0u) {
      std::cout << result << std::endl;
    }
  } while (static_cast<uint8_t>(!p.lines.empty()) != 0u);

  exit(EXIT_SUCCESS);
}
