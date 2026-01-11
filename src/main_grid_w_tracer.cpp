#include "emulator_w_tracer.h"
#include "utils.h"
#include "metrics.h"
#include "config.h"
#include "grid_w_tracer.h"

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <thread>
#include <iomanip>
#include <sstream>
#include <chrono>

void run_simulation_pair_with_tracer(
    const std::vector<Token>& programA,
    const std::vector<Token>& programB,
    int program_size,
    EmulatorResultWithTracer& result
) {
    // Concatenate programs
    std::vector<Token> tape = programA;
    tape.insert(tape.end(), programB.begin(), programB.end());

    // Run emulation with tracer
    result = emulate_w_tracer(tape, 0, program_size);
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_file = "configs/grid_config.yaml";
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
    std::mt19937 rng(config.random_seed);

    // Create and initialize grid with tokens
    GridWithTracer grid(config.grid_width, config.grid_height, config.program_size);
    grid.initialize_random(rng);

    std::cout << "Starting grid simulation with token tracking:" << std::endl;
    std::cout << "  Grid size: " << config.grid_width << "x" << config.grid_height
              << " (" << grid.get_total_programs() << " programs)" << std::endl;
    std::cout << "  Program size: " << config.program_size << std::endl;
    std::cout << "  Mutation rate: " << config.mutation_rate << std::endl;
    std::cout << "  Epochs: " << config.epochs << std::endl;
    std::cout << "  Token snapshots will be saved to data/tokens/" << std::endl;
    std::cout << std::endl;

    // Create output directories
    system("mkdir -p data/tokens");

    // Save initial token snapshot
    std::cout << "Saving initial token snapshot (epoch 0)..." << std::endl;
    grid.save_tokens_to_csv("data/tokens/tokens_epoch_0000.csv", 0);

    // Main simulation loop
    auto start_time = std::chrono::steady_clock::now();

    for (int epoch = 0; epoch < config.epochs; epoch++) {
        // Get all programs (tokens)
        std::vector<std::vector<Token>> soup;
        for (int y = 0; y < grid.get_height(); y++) {
            for (int x = 0; x < grid.get_width(); x++) {
                soup.push_back(grid.get_program(x, y));
            }
        }

        // Create spatial pairs using Von Neumann neighborhoods (r=2)
        // For simplicity, create random pairs for now
        std::vector<std::pair<int, int>> program_pairs;
        int total_programs = grid.get_total_programs();
        std::vector<int> indices(total_programs);
        for (int i = 0; i < total_programs; i++) indices[i] = i;
        std::shuffle(indices.begin(), indices.end(), rng);

        for (size_t i = 0; i + 1 < indices.size(); i += 2) {
            program_pairs.push_back({indices[i], indices[i + 1]});
        }

        // Run simulations with multithreading
        std::vector<EmulatorResultWithTracer> results(program_pairs.size());
        std::vector<std::thread> threads;

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            threads.emplace_back(
                run_simulation_pair_with_tracer,
                std::cref(soup[idx_a]),
                std::cref(soup[idx_b]),
                config.program_size,
                std::ref(results[i])
            );

            // Limit concurrent threads
            if (threads.size() >= num_threads) {
                for (auto& thread : threads) {
                    thread.join();
                }
                threads.clear();
            }
        }

        // Join remaining threads
        for (auto& thread : threads) {
            thread.join();
        }

        // Selection: choose best from each pair
        std::uniform_real_distribution<> mutation_dist(0.0, 1.0);
        std::uniform_int_distribution<> byte_dist(0, 255);
        std::uniform_int_distribution<> pos_dist(0, config.program_size - 1);

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            // Extract two halves from result
            std::vector<Token> result_a(results[i].tape.begin(),
                                        results[i].tape.begin() + config.program_size);
            std::vector<Token> result_b(results[i].tape.begin() + config.program_size,
                                        results[i].tape.end());

            // Apply mutation with new tokens
            if (mutation_dist(rng) < config.mutation_rate) {
                int mut_pos = pos_dist(rng);
                uint8_t new_char = static_cast<uint8_t>(byte_dist(rng));
                // Create new token with current epoch and mutation position
                result_a[mut_pos] = Token(epoch + 1, mut_pos, new_char);
            }

            if (mutation_dist(rng) < config.mutation_rate) {
                int mut_pos = pos_dist(rng);
                uint8_t new_char = static_cast<uint8_t>(byte_dist(rng));
                result_b[mut_pos] = Token(epoch + 1, mut_pos, new_char);
            }

            // Update grid with new token programs
            int x_a = idx_a % grid.get_width();
            int y_a = idx_a / grid.get_width();
            int x_b = idx_b % grid.get_width();
            int y_b = idx_b / grid.get_width();

            grid.set_program(x_a, y_a, result_a);
            grid.set_program(x_b, y_b, result_b);
        }

        // Progress reporting
        if ((epoch + 1) % 10 == 0 || epoch + 1 == config.epochs) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                current_time - start_time).count();

            std::cout << "Epoch " << std::setw(4) << (epoch + 1) << "/" << config.epochs
                      << " - Elapsed: " << elapsed << "s" << std::endl;
        }

        // Save token snapshots at visualization intervals
        if ((epoch + 1) % config.visualization_interval == 0) {
            std::stringstream filename;
            filename << "data/tokens/tokens_epoch_"
                     << std::setw(4) << std::setfill('0') << (epoch + 1) << ".csv";

            std::cout << "  Saving token snapshot: " << filename.str() << std::endl;
            grid.save_tokens_to_csv(filename.str(), epoch + 1);
        }
    }

    // Save final token snapshot
    std::cout << "\nSaving final token snapshot..." << std::endl;
    std::stringstream final_filename;
    final_filename << "data/tokens/tokens_epoch_"
                   << std::setw(4) << std::setfill('0') << config.epochs << ".csv";
    grid.save_tokens_to_csv(final_filename.str(), config.epochs);

    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::seconds>(
        end_time - start_time).count();

    std::cout << "\nSimulation complete!" << std::endl;
    std::cout << "Total time: " << total_time << "s" << std::endl;
    std::cout << "Token data saved to data/tokens/" << std::endl;

    return 0;
}
