#include "utils.h"
#include <iostream>
#include <random>
#include <cctype>

// Static random number generator for utilities
static std::mt19937 rng;
static bool rng_seeded = false;

void seed_random(unsigned int seed) {
    rng.seed(seed);
    rng_seeded = true;
}

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

    // Initialize with random_device if not seeded
    if (!rng_seeded) {
        std::random_device rd;
        rng.seed(rd());
        rng_seeded = true;
    }

    std::uniform_real_distribution<> dis(0.0, 1.0);
    std::uniform_int_distribution<> byte_dis(0, 255);

    for (size_t i = 0; i < tape.size(); i++) {
        if (dis(rng) < mutation_rate) {
            tape[i] = static_cast<uint8_t>(byte_dis(rng));
        }
    }

    return tape;
}

std::vector<uint8_t> generate_random_program(int length) {
    // Initialize with random_device if not seeded
    if (!rng_seeded) {
        std::random_device rd;
        rng.seed(rd());
        rng_seeded = true;
    }

    std::uniform_int_distribution<> dis(0, 255);

    std::vector<uint8_t> program(length);
    for (int i = 0; i < length; i++) {
        program[i] = static_cast<uint8_t>(dis(rng));
    }
    return program;
}
