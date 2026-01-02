#include "utils.h"
#include <iostream>
#include <random>
#include <cctype>

void print_tape(
    const std::vector<uint8_t>& tape,
    int head0_pos,
    int head1_pos,
    int pc_pos,
    bool skip_non_instruction
) {
    const std::string instructions = "<>{}-+.,[]";
    const uint8_t zero = '0';

    for (size_t i = 0; i < tape.size(); i++) {
        uint8_t byte = tape[i];
        char ch;

        if (skip_non_instruction &&
            instructions.find(byte) == std::string::npos &&
            byte != zero) {
            ch = ' ';
        } else {
            if (std::isprint(byte)) {
                ch = static_cast<char>(byte);
            } else {
                ch = ' ';
            }
        }

        if (static_cast<int>(i) == head0_pos) {
            std::cout << bcolors::BLUE << ch << bcolors::ENDC;
        } else if (static_cast<int>(i) == head1_pos) {
            std::cout << bcolors::RED << ch << bcolors::ENDC;
        } else if (static_cast<int>(i) == pc_pos) {
            std::cout << bcolors::GREEN << ch << bcolors::ENDC;
        } else {
            std::cout << ch;
        }
    }
    std::cout << std::endl;
}

std::vector<uint8_t> mutate(
    std::vector<uint8_t> tape,
    double mutation_rate
) {
    if (mutation_rate == 0.0) {
        return tape;
    }

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    static std::uniform_int_distribution<> byte_dis(0, 255);

    for (size_t i = 0; i < tape.size(); i++) {
        if (dis(gen) < mutation_rate) {
            tape[i] = static_cast<uint8_t>(byte_dis(gen));
        }
    }

    return tape;
}

std::vector<uint8_t> generate_random_program(int length) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);

    std::vector<uint8_t> program(length);
    for (int i = 0; i < length; i++) {
        program[i] = static_cast<uint8_t>(dis(gen));
    }
    return program;
}
