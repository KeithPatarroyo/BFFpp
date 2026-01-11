#include "grid.h"
#include "grid_w_tracer.h"
#include "emulator.h"
#include "emulator_w_tracer.h"
#include "utils.h"
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
    int grid_size = 5;
    int program_size = 16;
    int num_epochs = 3;
    double mutation_rate = 0.001;

    // Initialize both grids with same seed
    seed_random(42);
    Grid grid1(grid_size, grid_size, program_size);
    grid1.initialize_random();

    seed_random(42);
    GridWithTracer grid2(grid_size, grid_size, program_size);
    grid2.initialize_random(get_rng());

    // Run epochs side by side
    for (int epoch = 0; epoch < num_epochs; epoch++) {
        std::cout << "\n=== Epoch " << epoch << " ===" << std::endl;

        // Grid 1 (no tracer)
        seed_random(42 + epoch * 1000);  // Use different seed per epoch for debugging
        std::vector<std::vector<uint8_t>> soup1 = grid1.get_all_programs();
        std::vector<std::pair<int, int>> pairs1 = grid1.create_spatial_pairs(2);
        std::vector<EmulatorResult> results1(pairs1.size());

        for (size_t i = 0; i < pairs1.size(); i++) {
            int idx_a = pairs1[i].first;
            int idx_b = pairs1[i].second;

            if (idx_a == -1) {
                soup1[idx_b] = mutate(soup1[idx_b], mutation_rate);
                continue;
            }

            run_simulation_pair(soup1[idx_a], soup1[idx_b], program_size, results1[i]);

            std::vector<uint8_t> programA_new(results1[i].tape.begin(),
                                              results1[i].tape.begin() + program_size);
            std::vector<uint8_t> programB_new(results1[i].tape.begin() + program_size,
                                              results1[i].tape.end());

            soup1[idx_a] = mutate(programA_new, mutation_rate);
            soup1[idx_b] = mutate(programB_new, mutation_rate);
        }

        grid1.set_all_programs(soup1);

        // Grid 2 (with tracer)
        seed_random(42 + epoch * 1000);
        std::vector<std::vector<Token>> soup2 = grid2.get_all_programs();
        std::vector<std::pair<int, int>> pairs2 = grid2.create_spatial_pairs(2, get_rng());
        std::vector<EmulatorResultWithTracer> results2(pairs2.size());

        for (size_t i = 0; i < pairs2.size(); i++) {
            int idx_a = pairs2[i].first;
            int idx_b = pairs2[i].second;

            if (idx_a == -1) {
                soup2[idx_b] = grid2.mutate(soup2[idx_b], mutation_rate, epoch + 1, get_rng());
                continue;
            }

            run_simulation_pair_with_tracer(soup2[idx_a], soup2[idx_b], program_size, results2[i]);

            std::vector<Token> result_a(results2[i].tape.begin(),
                                        results2[i].tape.begin() + program_size);
            std::vector<Token> result_b(results2[i].tape.begin() + program_size,
                                        results2[i].tape.end());

            soup2[idx_a] = grid2.mutate(result_a, mutation_rate, epoch + 1, get_rng());
            soup2[idx_b] = grid2.mutate(result_b, mutation_rate, epoch + 1, get_rng());
        }

        grid2.set_all_programs(soup2);

        // Compare results
        std::cout << "Pairs match: " << (pairs1 == pairs2 ? "YES" : "NO") << std::endl;

        bool programs_match = true;
        for (int i = 0; i < grid_size * grid_size; i++) {
            int y = i / grid_size;
            int x = i % grid_size;
            const auto& prog1 = grid1.get_program(x, y);
            const auto& prog2 = grid2.get_program(x, y);

            bool match = true;
            for (size_t j = 0; j < prog1.size(); j++) {
                if (prog1[j] != prog2[j].get_char()) {
                    match = false;
                    break;
                }
            }

            if (!match) {
                programs_match = false;
                std::cout << "Program " << i << " differs!" << std::endl;
                std::cout << "  Grid1: ";
                for (size_t j = 0; j < std::min(size_t(8), prog1.size()); j++) {
                    std::cout << std::setw(3) << static_cast<int>(prog1[j]) << " ";
                }
                std::cout << std::endl;
                std::cout << "  Grid2: ";
                for (size_t j = 0; j < std::min(size_t(8), prog2.size()); j++) {
                    std::cout << std::setw(3) << static_cast<int>(prog2[j].get_char()) << " ";
                }
                std::cout << std::endl;
            }
        }

        if (programs_match) {
            std::cout << "All programs match!" << std::endl;
        }
    }

    return 0;
}
