#include "grid.h"
#include "grid_w_tracer.h"
#include "emulator.h"
#include "emulator_w_tracer.h"
#include "utils.h"
#include "config.h"
#include <iostream>
#include <iomanip>

void run_simulation_pair(
    const std::vector<uint8_t>& programA,
    const std::vector<uint8_t>& programB,
    int program_size,
    EmulatorResult& result
) {
    std::vector<uint8_t> tape = programA;
    tape.insert(tape.end(), programB.begin(), programB.end());
    result = emulate(tape, 0, program_size);
}

void run_simulation_pair_with_tracer(
    const std::vector<Token>& programA,
    const std::vector<Token>& programB,
    int program_size,
    EmulatorResultWithTracer& result
) {
    std::vector<Token> tape = programA;
    tape.insert(tape.end(), programB.begin(), programB.end());
    result = emulate_w_tracer(tape, 0, program_size);
}

int main() {
    Config config;
    config.random_seed = 42;
    config.grid_width = 10;
    config.grid_height = 10;
    config.program_size = 32;
    config.mutation_rate = 0.001;
    config.epochs = 5;

    std::cout << "Testing main_grid logic..." << std::endl;
    seed_random(config.random_seed);

    Grid grid(config.grid_width, config.grid_height, config.program_size);
    grid.initialize_random();

    for (int epoch = 0; epoch < config.epochs; epoch++) {
        std::vector<std::vector<uint8_t>> soup = grid.get_all_programs();
        std::vector<std::pair<int, int>> program_pairs = grid.create_spatial_pairs(2);
        std::vector<EmulatorResult> results(program_pairs.size());

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            if (idx_a == -1) {
                soup[idx_b] = mutate(soup[idx_b], config.mutation_rate);
                continue;
            }

            run_simulation_pair(soup[idx_a], soup[idx_b], config.program_size, results[i]);

            std::vector<uint8_t> programA_new(results[i].tape.begin(),
                                              results[i].tape.begin() + config.program_size);
            std::vector<uint8_t> programB_new(results[i].tape.begin() + config.program_size,
                                              results[i].tape.end());

            soup[idx_a] = mutate(programA_new, config.mutation_rate);
            soup[idx_b] = mutate(programB_new, config.mutation_rate);
        }

        grid.set_all_programs(soup);
    }

    std::cout << "\nFirst 3 programs from main_grid:" << std::endl;
    for (int i = 0; i < 3; i++) {
        int y = i / config.grid_width;
        int x = i % config.grid_width;
        const auto& prog = grid.get_program(x, y);
        std::cout << "Program " << i << ": ";
        for (size_t j = 0; j < std::min(size_t(8), prog.size()); j++) {
            std::cout << std::setw(3) << static_cast<int>(prog[j]) << " ";
        }
        std::cout << "..." << std::endl;
    }

    // Now test main_grid_w_tracer logic
    std::cout << "\n\nTesting main_grid_w_tracer logic..." << std::endl;
    seed_random(config.random_seed);

    GridWithTracer grid2(config.grid_width, config.grid_height, config.program_size);
    grid2.initialize_random(get_rng());

    for (int epoch = 0; epoch < config.epochs; epoch++) {
        std::vector<std::vector<Token>> soup = grid2.get_all_programs();

        std::vector<std::pair<int, int>> program_pairs = grid2.create_spatial_pairs(2, get_rng());
        std::vector<EmulatorResultWithTracer> results(program_pairs.size());

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            if (idx_a == -1) {
                soup[idx_b] = grid2.mutate(soup[idx_b], config.mutation_rate, epoch + 1, get_rng());
                continue;
            }

            run_simulation_pair_with_tracer(soup[idx_a], soup[idx_b], config.program_size, results[i]);

            std::vector<Token> result_a(results[i].tape.begin(),
                                        results[i].tape.begin() + config.program_size);
            std::vector<Token> result_b(results[i].tape.begin() + config.program_size,
                                        results[i].tape.end());

            soup[idx_a] = grid2.mutate(result_a, config.mutation_rate, epoch + 1, get_rng());
            soup[idx_b] = grid2.mutate(result_b, config.mutation_rate, epoch + 1, get_rng());
        }

        // Update grid with all new programs at once
        grid2.set_all_programs(soup);
    }

    std::cout << "\nFirst 3 programs from main_grid_w_tracer:" << std::endl;
    for (int i = 0; i < 3; i++) {
        int y = i / config.grid_width;
        int x = i % config.grid_width;
        const auto& prog = grid2.get_program(x, y);
        std::cout << "Program " << i << ": ";
        for (size_t j = 0; j < std::min(size_t(8), prog.size()); j++) {
            std::cout << std::setw(3) << static_cast<int>(prog[j].get_char()) << " ";
        }
        std::cout << "..." << std::endl;
    }

    return 0;
}
