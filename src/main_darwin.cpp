#include "emulator.h"
#include "utils.h"
#include "metrics.h"
#include "config.h"
#include "grid.h"
#include "websocket_server.h"

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>

struct DarwinConfig {
    int grid_width;        // W (width of each half)
    int grid_height;       // H (height)
    int program_size;

    // Phase 1: Two independent grids (0 to t1)
    std::string left_config;
    std::string right_config;
    int barrier_removal_epoch;  // t1

    // Phase 2: Merged grid (t1 to t2)
    std::string merged_config;
    int final_epoch;            // t2

    int eval_interval;
    int visualization_interval;
    unsigned int random_seed;
};

// Simple YAML parser for Darwin config
DarwinConfig load_darwin_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + filename);
    }

    DarwinConfig config;
    std::string line;

    while (std::getline(file, line)) {
        // Skip comments and empty lines
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "grid_width") config.grid_width = std::stoi(value);
        else if (key == "grid_height") config.grid_height = std::stoi(value);
        else if (key == "program_size") config.program_size = std::stoi(value);
        else if (key == "left_config") config.left_config = value;
        else if (key == "right_config") config.right_config = value;
        else if (key == "barrier_removal_epoch") config.barrier_removal_epoch = std::stoi(value);
        else if (key == "merged_config") config.merged_config = value;
        else if (key == "final_epoch") config.final_epoch = std::stoi(value);
        else if (key == "eval_interval") config.eval_interval = std::stoi(value);
        else if (key == "visualization_interval") config.visualization_interval = std::stoi(value);
        else if (key == "random_seed") config.random_seed = std::stoul(value);
    }

    file.close();
    return config;
}

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

void evolve_grid_epoch(
    Grid& grid,
    const Config& config,
    std::vector<EmulatorResult>& results,
    double& total_iterations,
    double& total_skipped,
    double& finished_runs,
    double& terminated_runs,
    std::mt19937& rng
) {
    std::vector<std::vector<uint8_t>> soup = grid.get_all_programs();
    std::vector<std::pair<int, int>> program_pairs = grid.create_spatial_pairs(2, rng);

    results.resize(program_pairs.size());
    std::vector<std::thread> threads;

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    for (size_t i = 0; i < program_pairs.size(); i++) {
        int idx_a = program_pairs[i].first;
        int idx_b = program_pairs[i].second;

        if (idx_a == -1) {
            continue;
        }

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

    for (auto& thread : threads) {
        thread.join();
    }

    total_iterations = 0;
    total_skipped = 0;
    finished_runs = 0;
    terminated_runs = 0;
    int executed_pairs = 0;

    for (size_t i = 0; i < program_pairs.size(); i++) {
        int idx_a = program_pairs[i].first;
        int idx_b = program_pairs[i].second;

        if (idx_a == -1) {
            soup[idx_b] = mutate(soup[idx_b], config.mutation_rate, rng);
            continue;
        }

        const EmulatorResult& result = results[i];

        std::vector<uint8_t> programA_new(
            result.tape.begin(),
            result.tape.begin() + config.program_size
        );
        std::vector<uint8_t> programB_new(
            result.tape.begin() + config.program_size,
            result.tape.end()
        );

        soup[idx_a] = mutate(programA_new, config.mutation_rate, rng);
        soup[idx_b] = mutate(programB_new, config.mutation_rate, rng);

        total_iterations += result.iteration;
        total_skipped += result.skipped;
        finished_runs += (result.state == "Finished") ? 1.0 : 0.0;
        terminated_runs += (result.state == "Terminated") ? 1.0 : 0.0;
        executed_pairs++;
    }

    if (executed_pairs > 0) {
        total_skipped /= executed_pairs;
        total_iterations /= executed_pairs;
        terminated_runs /= executed_pairs;
        finished_runs /= executed_pairs;
    }

    grid.set_all_programs(soup);
}

int main(int argc, char* argv[]) {
    std::string darwin_config_file = "configs/darwin_config.yaml";
    if (argc > 2 && std::string(argv[1]) == "--config") {
        darwin_config_file = argv[2];
    }

    // Load Darwin experiment configuration
    DarwinConfig darwin_config;
    try {
        darwin_config = load_darwin_config(darwin_config_file);
    } catch (const std::exception& e) {
        std::cerr << "Error loading Darwin config: " << e.what() << std::endl;
        return 1;
    }

    // Load individual configs for each phase
    Config left_config = load_config(darwin_config.left_config);
    Config right_config = load_config(darwin_config.right_config);
    Config merged_config = load_config(darwin_config.merged_config);

    // Create separate RNGs using seeds from individual config files
    std::mt19937 left_rng(left_config.random_seed);
    std::mt19937 right_rng(right_config.random_seed);
    std::mt19937 merged_rng(merged_config.random_seed);

    // Create two separate grids for Phase 1
    Grid left_grid(darwin_config.grid_width, darwin_config.grid_height, darwin_config.program_size);
    Grid right_grid(darwin_config.grid_width, darwin_config.grid_height, darwin_config.program_size);

    left_grid.initialize_random(left_rng);
    right_grid.initialize_random(right_rng);

    std::cout << "=== DARWIN EXPERIMENT ===" << std::endl;
    std::cout << "Phase 1: Independent evolution (epochs 0-" << darwin_config.barrier_removal_epoch << ")" << std::endl;
    std::cout << "  Left grid: " << darwin_config.grid_width << "x" << darwin_config.grid_height
              << " (" << left_grid.get_total_programs() << " programs)" << std::endl;
    std::cout << "  Right grid: " << darwin_config.grid_width << "x" << darwin_config.grid_height
              << " (" << right_grid.get_total_programs() << " programs)" << std::endl;
    std::cout << "\nPhase 2: Merged evolution (epochs " << darwin_config.barrier_removal_epoch
              << "-" << darwin_config.final_epoch << ")" << std::endl;
    std::cout << "  Merged grid: " << (2 * darwin_config.grid_width) << "x" << darwin_config.grid_height
              << " (" << (2 * left_grid.get_total_programs()) << " programs)" << std::endl;
    std::cout << std::endl;

    // Start WebSocket server
    WebSocketServer ws_server(8080);
    ws_server.start();
    std::cout << "WebSocket server started on port 8080" << std::endl;
    std::cout << "Open data/live_darwin.html in your browser for real-time updates" << std::endl;
    std::cout << std::endl;

    // Create output directories
    system("mkdir -p data/visualizations/darwin");

    // PHASE 1: Independent evolution (0 to t1)
    std::cout << "--- PHASE 1: BARRIER IN PLACE ---" << std::endl;

    // Track entropy history for all grids in both phases
    std::vector<int> epochs_phase1, epochs_phase2;
    std::vector<double> left_entropy_phase1, right_entropy_phase1, merged_entropy_phase1;
    std::vector<double> left_entropy_phase2, right_entropy_phase2, merged_entropy_phase2;

    for (int epoch = 0; epoch < darwin_config.barrier_removal_epoch; epoch++) {
        // Check if paused
        while (ws_server.is_paused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::vector<EmulatorResult> left_results, right_results;
        double left_iters, left_skips, left_finished, left_terminated;
        double right_iters, right_skips, right_finished, right_terminated;

        // Evolve left grid
        evolve_grid_epoch(left_grid, left_config, left_results,
                         left_iters, left_skips, left_finished, left_terminated, left_rng);

        // Evolve right grid
        evolve_grid_epoch(right_grid, right_config, right_results,
                         right_iters, right_skips, right_finished, right_terminated, right_rng);

        // Calculate combined stats
        std::vector<uint8_t> left_flat, right_flat;
        for (const auto& prog : left_grid.get_all_programs()) {
            left_flat.insert(left_flat.end(), prog.begin(), prog.end());
        }
        for (const auto& prog : right_grid.get_all_programs()) {
            right_flat.insert(right_flat.end(), prog.begin(), prog.end());
        }

        double left_hoe = higher_order_entropy(left_flat);
        double right_hoe = higher_order_entropy(right_flat);

        // Calculate conceptual merged grid entropy (combining both populations)
        std::vector<uint8_t> conceptual_merged = left_flat;
        conceptual_merged.insert(conceptual_merged.end(), right_flat.begin(), right_flat.end());
        double merged_hoe = higher_order_entropy(conceptual_merged);

        // Track entropy history for Phase 1 (all three grids)
        epochs_phase1.push_back(epoch);
        left_entropy_phase1.push_back(left_hoe);
        right_entropy_phase1.push_back(right_hoe);
        merged_entropy_phase1.push_back(merged_hoe);

        // Broadcast to WebSocket (send both grids with barrier flag)
        if (ws_server.has_clients()) {
            std::ostringstream json;
            json << "{";
            json << "\"epoch\":" << epoch << ",";
            json << "\"phase\":1,";
            json << "\"barrier_active\":true,";
            json << "\"barrier_removal_epoch\":" << darwin_config.barrier_removal_epoch << ",";
            json << "\"grid_width\":" << darwin_config.grid_width << ",";
            json << "\"grid_height\":" << darwin_config.grid_height << ",";

            // Left grid data
            json << "\"left\":{";
            json << "\"entropy\":" << std::fixed << std::setprecision(6) << left_hoe << ",";
            json << "\"avg_iters\":" << std::fixed << std::setprecision(3) << left_iters << ",";
            json << "\"finished_ratio\":" << std::fixed << std::setprecision(6) << left_finished << ",";
            json << "\"grid\":" << left_grid.to_json(epoch, left_hoe, left_iters, left_finished).substr(
                left_grid.to_json(epoch, left_hoe, left_iters, left_finished).find("\"grid\":") + 7,
                left_grid.to_json(epoch, left_hoe, left_iters, left_finished).length() -
                left_grid.to_json(epoch, left_hoe, left_iters, left_finished).find("\"grid\":") - 8
            );
            json << "},";

            // Right grid data
            json << "\"right\":{";
            json << "\"entropy\":" << std::fixed << std::setprecision(6) << right_hoe << ",";
            json << "\"avg_iters\":" << std::fixed << std::setprecision(3) << right_iters << ",";
            json << "\"finished_ratio\":" << std::fixed << std::setprecision(6) << right_finished << ",";
            json << "\"grid\":" << right_grid.to_json(epoch, right_hoe, right_iters, right_finished).substr(
                right_grid.to_json(epoch, right_hoe, right_iters, right_finished).find("\"grid\":") + 7,
                right_grid.to_json(epoch, right_hoe, right_iters, right_finished).length() -
                right_grid.to_json(epoch, right_hoe, right_iters, right_finished).find("\"grid\":") - 8
            );
            json << "}";

            json << "}";
            ws_server.broadcast(json.str());
        }

        if (epoch % darwin_config.eval_interval == 0) {
            std::cout << "Epoch: " << epoch << std::endl;
            std::cout << "  LEFT:  HOE=" << std::fixed << std::setprecision(3) << left_hoe
                      << ", Avg Iters=" << left_iters
                      << ", Finished=" << left_finished << std::endl;
            std::cout << "  RIGHT: HOE=" << std::fixed << std::setprecision(3) << right_hoe
                      << ", Avg Iters=" << right_iters
                      << ", Finished=" << right_finished << std::endl;
        }
    }

    std::cout << "\n--- BARRIER REMOVED AT EPOCH " << darwin_config.barrier_removal_epoch << " ---\n" << std::endl;

    // PHASE 2: Create merged grid and evolve (t1 to t2)
    Grid merged_grid(2 * darwin_config.grid_width, darwin_config.grid_height, darwin_config.program_size);

    // Copy left and right grids into merged grid
    for (int y = 0; y < darwin_config.grid_height; y++) {
        for (int x = 0; x < darwin_config.grid_width; x++) {
            merged_grid.set_program(x, y, left_grid.get_program(x, y));
            merged_grid.set_program(x + darwin_config.grid_width, y, right_grid.get_program(x, y));
        }
    }

    std::cout << "--- PHASE 2: POPULATIONS MIXING ---" << std::endl;

    for (int epoch = darwin_config.barrier_removal_epoch; epoch < darwin_config.final_epoch; epoch++) {
        // Check if paused
        while (ws_server.is_paused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::vector<EmulatorResult> results;
        double total_iters, total_skips, finished, terminated;

        evolve_grid_epoch(merged_grid, merged_config, results,
                         total_iters, total_skips, finished, terminated, merged_rng);

        // Calculate stats for full merged grid
        std::vector<uint8_t> flat_soup;
        for (const auto& prog : merged_grid.get_all_programs()) {
            flat_soup.insert(flat_soup.end(), prog.begin(), prog.end());
        }
        double merged_hoe = higher_order_entropy(flat_soup);

        // Also calculate entropy for left and right halves of merged grid
        std::vector<uint8_t> left_half, right_half;
        for (int y = 0; y < darwin_config.grid_height; y++) {
            for (int x = 0; x < darwin_config.grid_width; x++) {
                // Left half
                const auto& prog = merged_grid.get_program(x, y);
                left_half.insert(left_half.end(), prog.begin(), prog.end());
            }
            for (int x = darwin_config.grid_width; x < 2 * darwin_config.grid_width; x++) {
                // Right half
                const auto& prog = merged_grid.get_program(x, y);
                right_half.insert(right_half.end(), prog.begin(), prog.end());
            }
        }
        double left_half_hoe = higher_order_entropy(left_half);
        double right_half_hoe = higher_order_entropy(right_half);

        // Track entropy history for Phase 2 (all three grids)
        epochs_phase2.push_back(epoch);
        left_entropy_phase2.push_back(left_half_hoe);
        right_entropy_phase2.push_back(right_half_hoe);
        merged_entropy_phase2.push_back(merged_hoe);

        // Broadcast merged grid
        if (ws_server.has_clients()) {
            std::ostringstream json;
            json << "{";
            json << "\"epoch\":" << epoch << ",";
            json << "\"phase\":2,";
            json << "\"barrier_active\":false,";
            json << "\"barrier_removal_epoch\":" << darwin_config.barrier_removal_epoch << ",";
            json << "\"merged\":" << merged_grid.to_json(epoch, merged_hoe, total_iters, finished);
            json << "}";
            ws_server.broadcast(json.str());
        }

        if (epoch % darwin_config.eval_interval == 0) {
            std::cout << "Epoch: " << epoch << std::endl;
            std::cout << "  MERGED: HOE=" << std::fixed << std::setprecision(3) << merged_hoe
                      << ", Avg Iters=" << total_iters
                      << ", Finished=" << finished << std::endl;
        }
    }

    std::cout << "\n=== DARWIN EXPERIMENT COMPLETE ===" << std::endl;

    // Save entropy histories to CSV files
    std::cout << "\nSaving entropy histories..." << std::endl;

    // Phase 1 entropies
    std::ofstream left_phase1_file("data/visualizations/darwin/left_entropy_phase1.csv");
    if (left_phase1_file.is_open()) {
        left_phase1_file << "epoch,entropy\n";
        for (size_t i = 0; i < epochs_phase1.size(); i++) {
            left_phase1_file << epochs_phase1[i] << "," << std::fixed << std::setprecision(8) << left_entropy_phase1[i] << "\n";
        }
        left_phase1_file.close();
        std::cout << "  Saved left_entropy_phase1.csv (" << epochs_phase1.size() << " epochs)" << std::endl;
    }

    std::ofstream right_phase1_file("data/visualizations/darwin/right_entropy_phase1.csv");
    if (right_phase1_file.is_open()) {
        right_phase1_file << "epoch,entropy\n";
        for (size_t i = 0; i < epochs_phase1.size(); i++) {
            right_phase1_file << epochs_phase1[i] << "," << std::fixed << std::setprecision(8) << right_entropy_phase1[i] << "\n";
        }
        right_phase1_file.close();
        std::cout << "  Saved right_entropy_phase1.csv (" << epochs_phase1.size() << " epochs)" << std::endl;
    }

    std::ofstream merged_phase1_file("data/visualizations/darwin/merged_entropy_phase1.csv");
    if (merged_phase1_file.is_open()) {
        merged_phase1_file << "epoch,entropy\n";
        for (size_t i = 0; i < epochs_phase1.size(); i++) {
            merged_phase1_file << epochs_phase1[i] << "," << std::fixed << std::setprecision(8) << merged_entropy_phase1[i] << "\n";
        }
        merged_phase1_file.close();
        std::cout << "  Saved merged_entropy_phase1.csv (" << epochs_phase1.size() << " epochs)" << std::endl;
    }

    // Phase 2 entropies
    std::ofstream left_phase2_file("data/visualizations/darwin/left_entropy_phase2.csv");
    if (left_phase2_file.is_open()) {
        left_phase2_file << "epoch,entropy\n";
        for (size_t i = 0; i < epochs_phase2.size(); i++) {
            left_phase2_file << epochs_phase2[i] << "," << std::fixed << std::setprecision(8) << left_entropy_phase2[i] << "\n";
        }
        left_phase2_file.close();
        std::cout << "  Saved left_entropy_phase2.csv (" << epochs_phase2.size() << " epochs)" << std::endl;
    }

    std::ofstream right_phase2_file("data/visualizations/darwin/right_entropy_phase2.csv");
    if (right_phase2_file.is_open()) {
        right_phase2_file << "epoch,entropy\n";
        for (size_t i = 0; i < epochs_phase2.size(); i++) {
            right_phase2_file << epochs_phase2[i] << "," << std::fixed << std::setprecision(8) << right_entropy_phase2[i] << "\n";
        }
        right_phase2_file.close();
        std::cout << "  Saved right_entropy_phase2.csv (" << epochs_phase2.size() << " epochs)" << std::endl;
    }

    std::ofstream merged_phase2_file("data/visualizations/darwin/merged_entropy_phase2.csv");
    if (merged_phase2_file.is_open()) {
        merged_phase2_file << "epoch,entropy\n";
        for (size_t i = 0; i < epochs_phase2.size(); i++) {
            merged_phase2_file << epochs_phase2[i] << "," << std::fixed << std::setprecision(8) << merged_entropy_phase2[i] << "\n";
        }
        merged_phase2_file.close();
        std::cout << "  Saved merged_entropy_phase2.csv (" << epochs_phase2.size() << " epochs)" << std::endl;
    }

    std::cout << "\nEntropy tracking complete!" << std::endl;

    return 0;
}
