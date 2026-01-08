#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <cstdint>
#include <string>
#include <random>

namespace bcolors {
    const std::string RED = "\033[0;30;41m";
    const std::string GREEN = "\033[0;30;42m";
    const std::string YELLOW = "\033[0;30;43m";
    const std::string BLUE = "\033[0;30;44m";
    const std::string ENDC = "\033[0m";
}

void print_tape(
    const std::vector<uint8_t>& tape,
    int head0_pos = 0,
    int head1_pos = 0,
    int pc_pos = 0,
    bool skip_non_instruction = true
);

void seed_random(unsigned int seed);

// Get reference to shared RNG for reproducibility
std::mt19937& get_rng();

std::vector<uint8_t> mutate(
    std::vector<uint8_t> tape,
    double mutation_rate = 0.0
);

std::vector<uint8_t> mutate(
    std::vector<uint8_t> tape,
    double mutation_rate,
    std::mt19937& rng
);

std::vector<uint8_t> generate_random_program(int length);

std::vector<uint8_t> generate_random_program(int length, std::mt19937& rng);

#endif // UTILS_H
