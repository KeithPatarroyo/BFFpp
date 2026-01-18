#include "emulator.h"
#include "utils.h"
#include "metrics.h"
#include "config.h"
#include "grid.h"
#include "websocket_server.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include <thread>
#include <iomanip>
#include <sstream>

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
    seed_random(config.random_seed);

    // Create and initialize grid
    Grid grid(config.grid_width, config.grid_height, config.program_size);
    grid.initialize_random();

    std::cout << "Starting grid simulation with:" << std::endl;
    std::cout << "  Grid size: " << config.grid_width << "x" << config.grid_height
              << " (" << grid.get_total_programs() << " programs)" << std::endl;
    std::cout << "  Program size: " << config.program_size << std::endl;
    std::cout << "  Mutation rate: " << config.mutation_rate << std::endl;
    std::cout << "  Epochs: " << config.epochs << std::endl;
    std::cout << "  Visualization interval: " << config.visualization_interval << std::endl;
    std::cout << std::endl;

    // Start WebSocket server for live visualization
    WebSocketServer ws_server(8080);
    ws_server.start();
    std::cout << "WebSocket server started on port 8080" << std::endl;
    std::cout << "Open data/live_visualization.html in your browser for real-time updates" << std::endl;
    std::cout << std::endl;

    // Create output directories
    system("mkdir -p data/visualizations");

    // Save initial visualization
    std::stringstream filename;
    filename << "data/visualizations/grid_epoch_0000.html";
    grid.save_html(filename.str());
    std::cout << "Saved initial visualization: " << filename.str() << std::endl;

    // Send initial state via WebSocket
    std::string json_data = grid.to_json(0, 0.0, 0.0, 0.0);
    ws_server.broadcast(json_data);

    // Main simulation loop
    for (int epoch = 0; epoch < config.epochs; epoch++) {
        // Check if paused
        while (ws_server.is_paused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Get all programs as flat vector
        std::vector<std::vector<uint8_t>> soup = grid.get_all_programs();

        // Create spatial pairs using Von Neumann neighborhoods (r=2)
        std::vector<std::pair<int, int>> program_pairs = grid.create_spatial_pairs(2);

        // Run simulations with multithreading
        std::vector<EmulatorResult> results(program_pairs.size());
        std::vector<std::thread> threads;

        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            // Skip mutation-only cases for now (handle in result processing)
            if (idx_a == -1) {
                continue;
            }

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
        int executed_pairs = 0;

        for (size_t i = 0; i < program_pairs.size(); i++) {
            int idx_a = program_pairs[i].first;
            int idx_b = program_pairs[i].second;

            // Handle mutation-only cases (no neighbor available)
            if (idx_a == -1) {
                // Just mutate the program without execution
                soup[idx_b] = mutate(soup[idx_b], config.mutation_rate);
                continue;
            }

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
            executed_pairs++;
        }

        // Calculate averages (only for executed pairs)
        if (executed_pairs > 0) {
            total_skipped /= executed_pairs;
            total_iterations /= executed_pairs;
            terminated_runs /= executed_pairs;
            finished_runs /= executed_pairs;
        }

        // Update grid with new programs
        grid.set_all_programs(soup);

        // Save pairing information starting at epoch 16324
        const int PAIRING_START_EPOCH = 16324;
        if (epoch + 1 >= PAIRING_START_EPOCH) {
            // Build map: position index -> combined position index
            std::map<int, int> pairing_map;
            for (const auto& pair : program_pairs) {
                int idx_a = pair.first;
                int idx_b = pair.second;

                if (idx_a == -1) {
                    // Mutation-only: no pairing
                    pairing_map[idx_b] = -1;
                } else {
                    // Paired: each points to the other
                    pairing_map[idx_a] = idx_b;
                    pairing_map[idx_b] = idx_a;
                }
            }

            // Save pairing CSV
            std::stringstream pairing_filename;
            pairing_filename << "data/pairings/pairings_epoch_"
                           << std::setfill('0') << std::setw(4) << (epoch + 1) << ".csv";

            // Create directory if needed
            system("mkdir -p data/pairings");

            std::ofstream pairing_file(pairing_filename.str());
            if (pairing_file.is_open()) {
                // Write header
                pairing_file << "epoch,position_x,position_y,program,combined_x,combined_y\n";

                // Write data for each position
                for (int y = 0; y < grid.get_height(); y++) {
                    for (int x = 0; x < grid.get_width(); x++) {
                        int idx = y * grid.get_width() + x;
                        const auto& program = soup[idx];

                        // Get combined position
                        int combined_idx = pairing_map[idx];
                        int combined_x = -1;
                        int combined_y = -1;

                        if (combined_idx >= 0) {
                            combined_y = combined_idx / grid.get_width();
                            combined_x = combined_idx % grid.get_width();
                        }

                        // Write row
                        pairing_file << (epoch + 1) << ","
                                   << x << ","
                                   << y << ",\"";

                        // Write program as string
                        for (uint8_t ch : program) {
                            pairing_file << static_cast<char>(ch);
                        }

                        pairing_file << "\"," << combined_x << "," << combined_y << "\n";
                    }
                }

                pairing_file.close();
                std::cout << "\tSaved pairing data: " << pairing_filename.str() << std::endl;
            }
        }

        // Calculate stats
        std::vector<uint8_t> flat_soup;
        for (const auto& program : soup) {
            flat_soup.insert(flat_soup.end(), program.begin(), program.end());
        }
        double hoe = higher_order_entropy(flat_soup);

        // Broadcast live update via WebSocket
        if (ws_server.has_clients()) {
            std::string json_data = grid.to_json(epoch + 1, hoe, total_iterations, finished_runs);
            ws_server.broadcast(json_data);
        }

        // Evaluate and print statistics
        if (epoch % config.eval_interval == 0) {
            std::cout << "Epoch: " << epoch << std::endl;
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "\tHigher Order Entropy=" << hoe
                      << ",\tAvg Iters=" << total_iterations
                      << ",\tAvg Skips=" << total_skipped
                      << ",\tFinished Ratio=" << finished_runs
                      << ",\tTerminated Ratio=" << terminated_runs;
            if (ws_server.has_clients()) {
                std::cout << ",\tWebSocket Clients=" << ws_server.get_client_count();
            }
            std::cout << std::endl;
        }

        // Save visualization periodically
        if (epoch > 0 && epoch % config.visualization_interval == 0) {
            std::stringstream vis_filename;
            vis_filename << "data/visualizations/grid_epoch_"
                        << std::setfill('0') << std::setw(4) << epoch << ".html";
            grid.save_html(vis_filename.str());
            std::cout << "\tSaved visualization: " << vis_filename.str() << std::endl;
        }
    }

    // Save final visualization
    std::stringstream final_filename;
    final_filename << "data/visualizations/grid_epoch_"
                   << std::setfill('0') << std::setw(4) << config.epochs << ".html";
    grid.save_html(final_filename.str());
    std::cout << "\nSaved final visualization: " << final_filename.str() << std::endl;
    std::cout << "\nSimulation complete!" << std::endl;

    return 0;
}
