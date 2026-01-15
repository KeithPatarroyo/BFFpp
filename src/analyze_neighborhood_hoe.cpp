#include "metrics.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

struct TokenData {
    int epoch;
    int grid_x;
    int grid_y;
    int pos_in_program;
    uint64_t token_epoch;
    uint16_t token_orig_pos;
    uint8_t character;
};

struct GridData {
    int epoch;
    int width;
    int height;
    std::map<std::pair<int, int>, std::vector<uint8_t>> programs;
};

struct HOEResult {
    int epoch;
    int grid_x;
    int grid_y;
    double hoe;
    int neighborhood_size;
    int total_bytes;
};

std::mutex results_mutex;

std::vector<std::pair<int, int>> get_von_neumann_neighbors(int x, int y, int width, int height, int radius) {
    std::vector<std::pair<int, int>> neighbors;

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int manhattan_dist = std::abs(dx) + std::abs(dy);

            if (manhattan_dist == 0 || manhattan_dist > radius) {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;

            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                neighbors.push_back({nx, ny});
            }
        }
    }

    return neighbors;
}

GridData read_token_csv(const std::string& csv_path) {
    std::cout << "Reading " << csv_path << "..." << std::endl;

    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + csv_path);
    }

    GridData data;
    data.epoch = -1;
    data.width = 0;
    data.height = 0;

    std::string line;
    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> fields;

        // Parse CSV line
        while (std::getline(ss, field, ',')) {
            // Remove quotes if present
            if (!field.empty() && field.front() == '"') {
                field = field.substr(1, field.size() - 2);
            }
            fields.push_back(field);
        }

        if (fields.size() < 7) continue;

        TokenData token;
        token.epoch = std::stoi(fields[0]);
        token.grid_x = std::stoi(fields[1]);
        token.grid_y = std::stoi(fields[2]);
        token.pos_in_program = std::stoi(fields[3]);
        token.character = static_cast<uint8_t>(std::stoi(fields[6]));

        if (data.epoch == -1) {
            data.epoch = token.epoch;
        }

        data.width = std::max(data.width, token.grid_x + 1);
        data.height = std::max(data.height, token.grid_y + 1);

        auto key = std::make_pair(token.grid_x, token.grid_y);
        if (data.programs[key].size() <= static_cast<size_t>(token.pos_in_program)) {
            data.programs[key].resize(token.pos_in_program + 1);
        }
        data.programs[key][token.pos_in_program] = token.character;
    }

    std::cout << "  Grid size: " << data.width << "x" << data.height << ", Epoch: " << data.epoch << std::endl;

    return data;
}

void analyze_cell_range(
    const GridData& grid_data,
    int start_idx,
    int end_idx,
    int radius,
    std::vector<HOEResult>& results
) {
    for (int idx = start_idx; idx < end_idx; idx++) {
        int y = idx / grid_data.width;
        int x = idx % grid_data.width;

        // Get Von Neumann neighbors
        auto neighbors = get_von_neumann_neighbors(x, y, grid_data.width, grid_data.height, radius);

        // Collect bytes from neighborhood
        std::vector<uint8_t> neighborhood_bytes;

        // Add cell itself
        auto cell_key = std::make_pair(x, y);
        if (grid_data.programs.count(cell_key)) {
            const auto& program = grid_data.programs.at(cell_key);
            neighborhood_bytes.insert(neighborhood_bytes.end(), program.begin(), program.end());
        }

        // Add neighbors
        for (const auto& [nx, ny] : neighbors) {
            auto neighbor_key = std::make_pair(nx, ny);
            if (grid_data.programs.count(neighbor_key)) {
                const auto& program = grid_data.programs.at(neighbor_key);
                neighborhood_bytes.insert(neighborhood_bytes.end(), program.begin(), program.end());
            }
        }

        // Compute HOE
        double hoe = higher_order_entropy(neighborhood_bytes);

        // Store result
        HOEResult result;
        result.epoch = grid_data.epoch;
        result.grid_x = x;
        result.grid_y = y;
        result.hoe = hoe;
        result.neighborhood_size = neighbors.size() + 1;  // +1 for cell itself
        result.total_bytes = neighborhood_bytes.size();

        std::lock_guard<std::mutex> lock(results_mutex);
        results.push_back(result);
    }
}

std::vector<HOEResult> analyze_epoch(const GridData& grid_data, int radius, unsigned int num_threads) {
    int total_cells = grid_data.width * grid_data.height;

    std::cout << "  Analyzing " << total_cells << " cells with " << num_threads << " threads..." << std::endl;

    std::vector<HOEResult> results;
    results.reserve(total_cells);

    // Divide work among threads
    std::vector<std::thread> threads;
    int cells_per_thread = (total_cells + num_threads - 1) / num_threads;

    for (unsigned int t = 0; t < num_threads; t++) {
        int start_idx = t * cells_per_thread;
        int end_idx = std::min(start_idx + cells_per_thread, total_cells);

        if (start_idx >= total_cells) break;

        threads.emplace_back(analyze_cell_range, std::cref(grid_data), start_idx, end_idx, radius, std::ref(results));
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Sort results by position
    std::sort(results.begin(), results.end(), [](const HOEResult& a, const HOEResult& b) {
        if (a.epoch != b.epoch) return a.epoch < b.epoch;
        if (a.grid_y != b.grid_y) return a.grid_y < b.grid_y;
        return a.grid_x < b.grid_x;
    });

    // Compute statistics
    double min_hoe = results[0].hoe;
    double max_hoe = results[0].hoe;
    double sum_hoe = 0.0;

    for (const auto& result : results) {
        min_hoe = std::min(min_hoe, result.hoe);
        max_hoe = std::max(max_hoe, result.hoe);
        sum_hoe += result.hoe;
    }

    std::cout << "  HOE range: [" << std::fixed << std::setprecision(4)
              << min_hoe << ", " << max_hoe << "]" << std::endl;
    std::cout << "  HOE mean: " << (sum_hoe / results.size()) << std::endl;

    return results;
}

void save_results(const std::vector<HOEResult>& results, const std::string& output_path) {
    std::ofstream file(output_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open output file: " + output_path);
    }

    // Write header
    file << "epoch,grid_x,grid_y,hoe,neighborhood_size,total_bytes\n";

    // Write data
    for (const auto& result : results) {
        file << result.epoch << ","
             << result.grid_x << ","
             << result.grid_y << ","
             << std::fixed << std::setprecision(10) << result.hoe << ","
             << result.neighborhood_size << ","
             << result.total_bytes << "\n";
    }

    std::cout << "Saved results to " << output_path << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int radius = 10;
    std::string tokens_dir = "data/tokens";

    if (argc > 1) {
        radius = std::atoi(argv[1]);
    }
    if (argc > 2) {
        tokens_dir = argv[2];
    }

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 4;

    std::cout << "Neighborhood HOE Analysis" << std::endl;
    std::cout << "Von Neumann radius: " << radius << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Tokens directory: " << tokens_dir << std::endl;
    std::cout << std::endl;

    // Find all CSV files
    std::vector<std::string> csv_files;
    for (const auto& entry : fs::directory_iterator(tokens_dir)) {
        if (entry.path().extension() == ".csv" &&
            entry.path().filename().string().find("tokens_epoch_") == 0) {
            csv_files.push_back(entry.path().string());
        }
    }

    std::sort(csv_files.begin(), csv_files.end());

    if (csv_files.empty()) {
        std::cerr << "No token CSV files found in " << tokens_dir << std::endl;
        return 1;
    }

    std::cout << "Found " << csv_files.size() << " token files" << std::endl;
    std::cout << std::endl;

    // Analyze each epoch
    std::vector<HOEResult> all_results;

    for (const auto& csv_file : csv_files) {
        GridData grid_data = read_token_csv(csv_file);
        auto results = analyze_epoch(grid_data, radius, num_threads);
        all_results.insert(all_results.end(), results.begin(), results.end());
        std::cout << std::endl;
    }

    // Save results
    std::string output_path = tokens_dir + "/neighborhood_hoe_analysis.csv";
    save_results(all_results, output_path);

    // Print summary statistics by epoch
    std::cout << "\n=== Summary Statistics by Epoch ===" << std::endl;

    std::map<int, std::vector<double>> hoe_by_epoch;
    for (const auto& result : all_results) {
        hoe_by_epoch[result.epoch].push_back(result.hoe);
    }

    for (const auto& [epoch, hoe_values] : hoe_by_epoch) {
        double sum = 0.0;
        double min_val = hoe_values[0];
        double max_val = hoe_values[0];

        for (double val : hoe_values) {
            sum += val;
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }

        std::cout << "\nEpoch " << epoch << ":" << std::endl;
        std::cout << "  Count: " << hoe_values.size() << std::endl;
        std::cout << "  Mean:  " << std::fixed << std::setprecision(6) << (sum / hoe_values.size()) << std::endl;
        std::cout << "  Min:   " << min_val << std::endl;
        std::cout << "  Max:   " << max_val << std::endl;
    }

    return 0;
}
