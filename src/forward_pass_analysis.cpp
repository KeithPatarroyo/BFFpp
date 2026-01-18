#include "emulator.h"
#include "utils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <future>
#include <algorithm>
#include <iomanip>

// Thread-safe cache for program execution results
class ProgramCache {
public:
    struct CacheEntry {
        bool is_replicator;
        std::vector<uint8_t> program;
    };

    bool has(const std::vector<uint8_t>& program) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache.count(program) > 0;
    }

    bool is_replicator(const std::vector<uint8_t>& program) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(program);
        if (it != cache.end()) {
            return it->second.is_replicator;
        }
        return false;
    }

    void add(const std::vector<uint8_t>& program, bool is_replicator) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache[program] = {is_replicator, program};
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache.size();
    }

private:
    std::map<std::vector<uint8_t>, CacheEntry> cache;
    mutable std::mutex cache_mutex;
};

// Represents a program at a specific position
struct ProgramLocation {
    int epoch;
    int grid_x;
    int grid_y;
    std::vector<uint8_t> program;

    bool operator<(const ProgramLocation& other) const {
        if (epoch != other.epoch) return epoch < other.epoch;
        if (grid_x != other.grid_x) return grid_x < other.grid_x;
        return grid_y < other.grid_y;
    }
};

// Get Von Neumann neighbors with proper boundary checking
std::vector<std::pair<int, int>> get_von_neumann_neighbors(
    int x, int y, int width, int height, int radius = 2
) {
    std::vector<std::pair<int, int>> neighbors;

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Calculate Manhattan distance
            int manhattan_dist = std::abs(dx) + std::abs(dy);

            // Include cell itself and neighbors within radius
            if (manhattan_dist > 0 && manhattan_dist <= radius) {
                int nx = x + dx;
                int ny = y + dy;

                // Check bounds
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    neighbors.push_back({nx, ny});
                }
            }
        }
    }

    return neighbors;
}

// Read program from CSV file at specific position
std::vector<uint8_t> get_program_from_csv(
    const std::string& csv_path,
    int grid_x,
    int grid_y
) {
    std::ifstream file(csv_path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + csv_path);
    }

    std::vector<uint8_t> program;
    std::string line;

    // Skip header
    std::getline(file, line);

    // Read all lines and find matching position
    std::map<int, uint8_t> position_map;

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

        int x = std::stoi(fields[1]);
        int y = std::stoi(fields[2]);
        int pos = std::stoi(fields[3]);
        uint8_t character = static_cast<uint8_t>(std::stoi(fields[6]));

        // If this is the right position, store character at its position
        if (x == grid_x && y == grid_y) {
            position_map[pos] = character;
        }
    }

    // Convert map to vector (in order)
    if (!position_map.empty()) {
        int max_pos = position_map.rbegin()->first;
        program.resize(max_pos + 1);
        for (const auto& [pos, ch] : position_map) {
            program[pos] = ch;
        }
    }

    return program;
}

// Check if a program is a self-replicator
bool check_replicator(const std::vector<uint8_t>& program, int max_iter = 1024) {
    if (program.empty()) return false;

    // Create tape: program1 + program2 (filled with '0')
    std::vector<uint8_t> tape = program;
    tape.insert(tape.end(), program.size(), '0');

    // Run emulation
    EmulatorResult result = emulate(tape, 0, static_cast<int>(program.size()), 0, max_iter, 0);

    // Check if first half equals second half
    size_t mid = result.tape.size() / 2;
    for (size_t i = 0; i < mid; i++) {
        if (result.tape[i] != result.tape[mid + i]) {
            return false;
        }
    }

    return true;
}

// Calculate similarity between two programs
double calculate_similarity(const std::vector<uint8_t>& prog1, const std::vector<uint8_t>& prog2) {
    if (prog1.size() != prog2.size()) return 0.0;
    if (prog1.empty()) return 0.0;

    int matches = 0;
    for (size_t i = 0; i < prog1.size(); i++) {
        if (prog1[i] == prog2[i]) {
            matches++;
        }
    }

    return static_cast<double>(matches) / prog1.size();
}

// Find replicators using forward pass analysis
std::map<int, std::vector<ProgramLocation>> find_replicators(
    const std::string& tokens_dir,
    int start_epoch,
    int grid_x,
    int grid_y,
    int last_epoch,
    int grid_width,
    int grid_height,
    double similarity_threshold = 0.9,
    unsigned int num_threads = 0
) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
    }

    std::cout << "Forward Pass Analysis" << std::endl;
    std::cout << "Start epoch: " << start_epoch << std::endl;
    std::cout << "Start position: (" << grid_x << ", " << grid_y << ")" << std::endl;
    std::cout << "Last epoch: " << last_epoch << std::endl;
    std::cout << "Grid size: " << grid_width << "x" << grid_height << std::endl;
    std::cout << "Similarity threshold: " << similarity_threshold << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << std::endl;

    // Cache for program execution results
    ProgramCache cache;

    // Result storage: epoch -> list of replicators
    std::map<int, std::vector<ProgramLocation>> replicators_by_epoch;

    // Get initial program
    std::stringstream csv_path;
    csv_path << tokens_dir << "/tokens_epoch_" << std::setfill('0') << std::setw(4) << start_epoch << ".csv";

    std::vector<uint8_t> initial_program = get_program_from_csv(csv_path.str(), grid_x, grid_y);

    if (initial_program.empty()) {
        std::cerr << "Error: Could not find program at initial position" << std::endl;
        return replicators_by_epoch;
    }

    // Verify initial program is a replicator
    std::cout << "Verifying initial program is a replicator..." << std::endl;
    bool is_rep = check_replicator(initial_program);
    if (!is_rep) {
        std::cerr << "Warning: Initial program is not a self-replicator!" << std::endl;
    } else {
        std::cout << "Initial program verified as self-replicator" << std::endl;
    }

    cache.add(initial_program, is_rep);

    // Add initial replicator
    ProgramLocation initial;
    initial.epoch = start_epoch;
    initial.grid_x = grid_x;
    initial.grid_y = grid_y;
    initial.program = initial_program;
    replicators_by_epoch[start_epoch].push_back(initial);

    std::cout << std::endl;

    // Forward pass through epochs
    for (int epoch = start_epoch; epoch < last_epoch; epoch++) {
        std::cout << "Processing epoch " << epoch << " -> " << (epoch + 1) << std::endl;

        const auto& current_replicators = replicators_by_epoch[epoch];
        std::cout << "  Current replicators: " << current_replicators.size() << std::endl;

        if (current_replicators.empty()) {
            std::cout << "  No replicators to propagate" << std::endl;
            continue;
        }

        // Candidate programs to check
        std::vector<ProgramLocation> candidates;
        std::set<std::pair<int, int>> checked_positions;

        // Get CSV path for next epoch
        std::stringstream next_csv_path;
        next_csv_path << tokens_dir << "/tokens_epoch_"
                     << std::setfill('0') << std::setw(4) << (epoch + 1) << ".csv";

        // For each replicator, get neighbors
        for (const auto& replicator : current_replicators) {
            auto neighbors = get_von_neumann_neighbors(
                replicator.grid_x, replicator.grid_y,
                grid_width, grid_height, 2
            );

            for (const auto& [nx, ny] : neighbors) {
                // Skip if already checked this position
                if (checked_positions.count({nx, ny})) continue;
                checked_positions.insert({nx, ny});

                // Get program at next epoch
                std::vector<uint8_t> neighbor_program = get_program_from_csv(
                    next_csv_path.str(), nx, ny
                );

                if (neighbor_program.empty()) continue;

                // Check similarity
                double similarity = calculate_similarity(replicator.program, neighbor_program);

                if (similarity >= similarity_threshold) {
                    ProgramLocation candidate;
                    candidate.epoch = epoch + 1;
                    candidate.grid_x = nx;
                    candidate.grid_y = ny;
                    candidate.program = neighbor_program;
                    candidates.push_back(candidate);
                }
            }
        }

        std::cout << "  Candidates (>=" << (similarity_threshold * 100) << "% similar): "
                  << candidates.size() << std::endl;

        // Check candidates in parallel
        std::vector<std::future<std::pair<ProgramLocation, bool>>> futures;
        std::mutex results_mutex;

        for (const auto& candidate : candidates) {
            // Skip if already in cache
            if (cache.has(candidate.program)) {
                if (cache.is_replicator(candidate.program)) {
                    replicators_by_epoch[epoch + 1].push_back(candidate);
                }
                continue;
            }

            // Submit to thread pool
            if (futures.size() >= num_threads) {
                // Wait for one to complete
                auto result = futures.front().get();
                futures.erase(futures.begin());

                cache.add(result.first.program, result.second);
                if (result.second) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    replicators_by_epoch[epoch + 1].push_back(result.first);
                }
            }

            futures.push_back(std::async(std::launch::async, [candidate]() {
                bool is_rep = check_replicator(candidate.program);
                return std::make_pair(candidate, is_rep);
            }));
        }

        // Wait for remaining futures
        for (auto& future : futures) {
            auto result = future.get();
            cache.add(result.first.program, result.second);
            if (result.second) {
                replicators_by_epoch[epoch + 1].push_back(result.first);
            }
        }

        std::cout << "  Found " << replicators_by_epoch[epoch + 1].size()
                  << " replicators at epoch " << (epoch + 1) << std::endl;
        std::cout << "  Cache size: " << cache.size() << " programs" << std::endl;
        std::cout << std::endl;
    }

    return replicators_by_epoch;
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0]
                  << " <tokens_dir> <start_epoch> <grid_x> <grid_y> <last_epoch> <grid_width> <grid_height> [similarity_threshold] [num_threads]"
                  << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " data/tokens 16324 14 27 16327 240 135 0.9 8"
                  << std::endl;
        return 1;
    }

    std::string tokens_dir = argv[1];
    int start_epoch = std::atoi(argv[2]);
    int grid_x = std::atoi(argv[3]);
    int grid_y = std::atoi(argv[4]);
    int last_epoch = std::atoi(argv[5]);
    int grid_width = std::atoi(argv[6]);
    int grid_height = std::atoi(argv[7]);

    double similarity_threshold = 0.9;
    if (argc > 8) {
        similarity_threshold = std::atof(argv[8]);
    }

    unsigned int num_threads = 0;
    if (argc > 9) {
        num_threads = std::atoi(argv[9]);
    }

    // Run forward pass analysis
    auto replicators = find_replicators(
        tokens_dir,
        start_epoch,
        grid_x,
        grid_y,
        last_epoch,
        grid_width,
        grid_height,
        similarity_threshold,
        num_threads
    );

    // Print summary
    std::cout << "=== Summary ===" << std::endl;
    int total_replicators = 0;
    for (const auto& [epoch, reps] : replicators) {
        std::cout << "Epoch " << epoch << ": " << reps.size() << " replicators" << std::endl;
        total_replicators += reps.size();
    }
    std::cout << "\nTotal replicators found: " << total_replicators << std::endl;

    // Save results to file
    std::string output_path = tokens_dir + "/forward_pass_results.csv";
    std::ofstream out(output_path);
    if (out.is_open()) {
        out << "epoch,grid_x,grid_y,program\n";
        for (const auto& [epoch, reps] : replicators) {
            for (const auto& rep : reps) {
                out << rep.epoch << "," << rep.grid_x << "," << rep.grid_y << ",\"";
                for (uint8_t ch : rep.program) {
                    out << static_cast<char>(ch);
                }
                out << "\"\n";
            }
        }
        std::cout << "\nResults saved to: " << output_path << std::endl;
    }

    return 0;
}
