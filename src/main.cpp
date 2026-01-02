#include "emulator.h"
#include "utils.h"
#include "metrics.h"
#include "config.h"

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <thread>
#include <iomanip>

void run_simulation_pair(
    const std::vector<uint8_t>& programA,
    const std::vector<uint8_t>& programB,
    int program_size,
    EmulatorResult& result
) {
    // Concatenate programs
    std::vector<uint8_t> tape = programA;
    tape.insert(tape.end(), programB.begin(), programB.end());

    // Run emulation
    result = emulate(tape, 0, program_size);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_file = "configs/small_config.yaml";
    if (argc > 2 && std::string(argv[1]) == "--config") {
        config_file = argv[2];
    }

    // Load configuration
    Config config;
    try {
        config = load_config(config_file);
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return 1;
    }

    // Set random seed for reproducibility
    seed_random(config.random_seed);
    std::mt19937 rng(config.random_seed);

    // Initialize soup with random programs
    std::vector<std::vector<uint8_t>> soup;
    for (int i = 0; i < config.soup_size; i++) {
        soup.push_back(generate_random_program(config.program_size));
    }

    std::cout << "Starting simulation with:" << std::endl;
    std::cout << "  Soup size: " << config.soup_size << std::endl;
    std::cout << "  Program size: " << config.program_size << std::endl;
    std::cout << "  Mutation rate: " << config.mutation_rate << std::endl;
    std::cout << "  Epochs: " << config.epochs << std::endl;
    std::cout << std::endl;

    // Main simulation loop
    for (int epoch = 0; epoch < config.epochs; epoch++) {
        // Create random permutation
        std::vector<int> perm(config.soup_size);
        for (int i = 0; i < config.soup_size; i++) {
            perm[i] = i;
        }
        std::shuffle(perm.begin(), perm.end(), rng);

        // Create program pairs
        std::vector<std::pair<int, int>> program_pairs;
        for (int i = 0; i < config.soup_size; i += 2) {
            program_pairs.push_back({perm[i], perm[i + 1]});
        }

        // Run simulations with multithreading
        std::vector<EmulatorResult> results(program_pairs.size());
        std::vector<std::thread> threads;

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            // Wait if too many threads are running
            if (threads.size() >= num_threads) {
                threads[0].join();
                threads.erase(threads.begin());
            }

            threads.emplace_back(
                run_simulation_pair,
                std::ref(soup[idx_a]),
                std::ref(soup[idx_b]),
                config.program_size,
                std::ref(results[i])
            );
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }

        // Process results and update soup
        double total_iterations = 0;
        double total_skipped = 0;
        double finished_runs = 0;
        double terminated_runs = 0;

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;
            const EmulatorResult& result = results[i];

            // Extract and mutate programs
            std::vector<uint8_t> programA_new(
                result.tape.begin(),
                result.tape.begin() + config.program_size
            );
            std::vector<uint8_t> programB_new(
                result.tape.begin() + config.program_size,
                result.tape.end()
            );

            soup[idx_a] = mutate(programA_new, config.mutation_rate);
            soup[idx_b] = mutate(programB_new, config.mutation_rate);

            total_iterations += result.iteration;
            total_skipped += result.skipped;
            finished_runs += (result.state == "Finished") ? 1.0 : 0.0;
            terminated_runs += (result.state == "Terminated") ? 1.0 : 0.0;
        }

        total_skipped /= program_pairs.size();
        total_iterations /= program_pairs.size();
        terminated_runs /= program_pairs.size();
        finished_runs /= program_pairs.size();

        // Evaluate and print statistics
        if (epoch % config.eval_interval == 0) {
            // Flatten soup
            std::vector<uint8_t> flat_soup;
            for (const auto& program : soup) {
                flat_soup.insert(flat_soup.end(), program.begin(), program.end());
            }

            double hoe = higher_order_entropy(flat_soup);

            std::cout << "Epoch: " << epoch << std::endl;
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "\tHigher Order Entropy=" << hoe
                      << ",\tAvg Iters=" << total_iterations
                      << ",\tAvg Skips=" << total_skipped
                      << ",\tFinished Ratio=" << finished_runs
                      << ",\tTerminated Ratio=" << terminated_runs
                      << std::endl;

            if (hoe > 1.0) {
                std::cout << "The first " << config.num_print_programs << " programs:" << std::endl;
                for (int program_idx = 0; program_idx < config.num_print_programs; program_idx++) {
                    print_tape(soup[program_idx], -1, -1, -1, false);
                }
            }
        }
    }

    return 0;
}
