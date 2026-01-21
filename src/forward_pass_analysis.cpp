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
    bool has(const std::string& program) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache.count(program) > 0;
    }

    bool is_replicator(const std::string& program) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(program);
        if (it != cache.end()) {
            return it->second;
        }
        return false;
    }

    void add(const std::string& program, bool is_replicator) {
        std::lock_guard<std::mutex> lock(cache_mutex);
        cache[program] = is_replicator;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(cache_mutex);
        return cache.size();
    }

private:
    std::map<std::string, bool> cache;
    mutable std::mutex cache_mutex;
};

// Cell data from CSV
struct CellData {
    std::string program;
    int combined_x;
    int combined_y;
};

// Represents a program at a specific position
struct ProgramLocation {
    int epoch;
    int grid_x;
    int grid_y;
    std::string program;

    bool operator<(const ProgramLocation& other) const {
        if (epoch != other.epoch) return epoch < other.epoch;
        if (grid_x != other.grid_x) return grid_x < other.grid_x;
        if (grid_y != other.grid_y) return grid_y < other.grid_y;
        return program < other.program;
    }

    bool operator==(const ProgramLocation& other) const {
        return epoch == other.epoch && grid_x == other.grid_x &&
               grid_y == other.grid_y && program == other.program;
    }
};

// Get neighbors (including middle, r=1, corners, r=2) matching Python's get_neighboors
std::vector<std::pair<int, int>> get_neighboors(int grid_x, int grid_y) {
    std::vector<std::pair<int, int>> neighbors;

    // Middle
    neighbors.push_back({grid_x, grid_y});

    // r_one
    neighbors.push_back({grid_x - 1, grid_y});
    neighbors.push_back({grid_x + 1, grid_y});
    neighbors.push_back({grid_x, grid_y - 1});
    neighbors.push_back({grid_x, grid_y + 1});

    // r_two
    neighbors.push_back({grid_x - 2, grid_y});
    neighbors.push_back({grid_x + 2, grid_y});
    neighbors.push_back({grid_x, grid_y - 2});
    neighbors.push_back({grid_x, grid_y + 2});

    // corners
    neighbors.push_back({grid_x - 1, grid_y - 1});
    neighbors.push_back({grid_x + 1, grid_y + 1});
    neighbors.push_back({grid_x + 1, grid_y - 1});
    neighbors.push_back({grid_x - 1, grid_y + 1});

    return neighbors;
}

// Parse a CSV line with proper quote handling
std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            fields.push_back(field);
            field.clear();
        } else {
            field += c;
        }
    }
    fields.push_back(field);

    return fields;
}

// Check if a character is a valid BF instruction
bool is_valid_instruction(char ch) {
    const std::string instructions = ",.[]{}<>+-";
    return instructions.find(ch) != std::string::npos;
}

// Clean program by replacing non-instructions with spaces
std::string clean_program(const std::string& program) {
    std::string cleaned = program;
    for (size_t i = 0; i < cleaned.size(); i++) {
        if (!is_valid_instruction(cleaned[i])) {
            cleaned[i] = ' ';
        }
    }
    return cleaned;
}

// Read all cell data from a pairing CSV file
std::map<std::pair<int, int>, CellData> read_pairing_csv(const std::string& csv_path) {
    std::map<std::pair<int, int>, CellData> cells;
    std::ifstream file(csv_path);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + csv_path);
    }

    std::string line;
    // Skip header
    std::getline(file, line);

    while (std::getline(file, line)) {
        auto fields = parse_csv_line(line);

        if (fields.size() < 6) continue;

        int x = std::stoi(fields[1]);
        int y = std::stoi(fields[2]);
        std::string program = clean_program(fields[3]);  // Clean program
        int combined_x = std::stoi(fields[4]);
        int combined_y = std::stoi(fields[5]);

        CellData cell_data;
        cell_data.program = program;
        cell_data.combined_x = combined_x;
        cell_data.combined_y = combined_y;

        cells[{x, y}] = cell_data;
    }

    return cells;
}

// Convert string program to vector<uint8_t>
std::vector<uint8_t> string_to_program(const std::string& str) {
    std::vector<uint8_t> program;
    for (char c : str) {
        program.push_back(static_cast<uint8_t>(c));
    }
    return program;
}

// Check if a program is a self-replicator
bool check_replicator(const std::string& program_str, int max_iter = 1024) {
    if (program_str.empty()) return false;

    // Convert to vector
    std::vector<uint8_t> program = string_to_program(program_str);

    // Create tape: program1 + program2 (filled with '0')
    std::vector<uint8_t> tape = program;
    tape.insert(tape.end(), program.size(), '0');

    // Run emulation
    EmulatorResult result = emulate(tape, 0, static_cast<int>(program.size()), 0, max_iter, 0);

    // Check if first half equals second half
    if (result.tape.size() < program.size() * 2) return false;

    size_t mid = result.tape.size() / 2;
    for (size_t i = 0; i < mid; i++) {
        if (result.tape[i] != result.tape[mid + i]) {
            return false;
        }
    }

    return true;
}

// Calculate similarity between two programs
double calculate_similarity(const std::string& prog1, const std::string& prog2) {
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

// Find replicators using forward pass analysis with pairing data
std::map<int, std::set<ProgramLocation>> find_replicators(
    const std::string& pairings_dir,
    int start_epoch,
    int grid_x,
    int grid_y,
    int last_epoch,
    int grid_width,
    int grid_height,
    unsigned int num_threads = 0
) {
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
    }

    std::cout << "Forward Pass Analysis (Pairing-based)" << std::endl;
    std::cout << "Start epoch: " << start_epoch << std::endl;
    std::cout << "Start position: (" << grid_x << ", " << grid_y << ")" << std::endl;
    std::cout << "Last epoch: " << last_epoch << std::endl;
    std::cout << "Grid size: " << grid_width << "x" << grid_height << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << std::endl;

    // Cache for program execution results
    ProgramCache cache;

    // Result storage: epoch -> set of replicators (using set to avoid duplicates)
    std::map<int, std::set<ProgramLocation>> replicators_by_epoch;

    // Get initial program from pairing CSV
    std::stringstream csv_path;
    csv_path << pairings_dir << "/pairings_epoch_"
             << std::setfill('0') << std::setw(4) << start_epoch << ".csv";

    auto cells = read_pairing_csv(csv_path.str());
    auto it = cells.find({grid_x, grid_y});

    if (it == cells.end()) {
        std::cerr << "Error: Could not find program at initial position" << std::endl;
        return replicators_by_epoch;
    }

    std::string initial_program = it->second.program;

    // Verify initial program is a replicator
    std::cout << "Verifying initial program is a replicator..." << std::endl;
    std::cout << "Program: " << initial_program << std::endl;
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
    replicators_by_epoch[start_epoch].insert(initial);

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

        // Get CSV data for next epoch
        std::stringstream next_csv_path;
        next_csv_path << pairings_dir << "/pairings_epoch_"
                     << std::setfill('0') << std::setw(4) << (epoch + 1) << ".csv";

        // Read next epoch cell data
        std::map<std::pair<int, int>, CellData> next_cells;
        try {
            next_cells = read_pairing_csv(next_csv_path.str());
        } catch (const std::exception& e) {
            std::cerr << "  Error reading next epoch: " << e.what() << std::endl;
            break;
        }

        // Candidate programs to check
        std::vector<ProgramLocation> candidates;
        std::mutex candidates_mutex;

        // For each replicator at time t
        for (const auto& replicator : current_replicators) {
            int rep_x = replicator.grid_x;
            int rep_y = replicator.grid_y;
            std::string rep_program = replicator.program;

            // Get neighbors (including middle)
            auto neighbors = get_neighboors(rep_x, rep_y);

            for (const auto& [neigh_x, neigh_y] : neighbors) {
                // Check bounds
                if (neigh_x < 0 || neigh_x >= grid_width ||
                    neigh_y < 0 || neigh_y >= grid_height) {
                    continue;
                }

                // Get cell data at neighbor position in next epoch
                auto cell_it = next_cells.find({neigh_x, neigh_y});
                if (cell_it == next_cells.end()) continue;

                const CellData& neighbor_cell = cell_it->second;
                int combined_x = neighbor_cell.combined_x;
                int combined_y = neighbor_cell.combined_y;

                // Case 1: Neighbor paired with the replicator's position
                if (combined_x == rep_x && combined_y == rep_y) {
                    // Check the neighbor position
                    std::string next_program = neighbor_cell.program;
                    double similarity = calculate_similarity(rep_program, next_program);

                    if (similarity > 0.9) {
                        ProgramLocation candidate;
                        candidate.epoch = epoch + 1;
                        candidate.grid_x = neigh_x;
                        candidate.grid_y = neigh_y;
                        candidate.program = next_program;

                        std::lock_guard<std::mutex> lock(candidates_mutex);
                        candidates.push_back(candidate);
                    }

                    // Also check the replicator's own position (both get updated in pairing)
                    auto rep_cell_it = next_cells.find({rep_x, rep_y});
                    if (rep_cell_it != next_cells.end()) {
                        std::string next_same = rep_cell_it->second.program;
                        double similarity_same = calculate_similarity(rep_program, next_same);

                        if (similarity_same > 0.9) {
                            ProgramLocation candidate;
                            candidate.epoch = epoch + 1;
                            candidate.grid_x = rep_x;
                            candidate.grid_y = rep_y;
                            candidate.program = next_same;

                            std::lock_guard<std::mutex> lock(candidates_mutex);
                            candidates.push_back(candidate);
                        }
                    }
                }

                // Case 2: Mutation-only (no pairing) at the replicator's position
                if (combined_x == -1 && combined_y == -1 &&
                    neigh_x == rep_x && neigh_y == rep_y) {
                    std::string next_program = neighbor_cell.program;
                    double similarity = calculate_similarity(rep_program, next_program);

                    if (similarity > 0.9) {
                        ProgramLocation candidate;
                        candidate.epoch = epoch + 1;
                        candidate.grid_x = neigh_x;
                        candidate.grid_y = neigh_y;
                        candidate.program = next_program;

                        std::lock_guard<std::mutex> lock(candidates_mutex);
                        candidates.push_back(candidate);
                    }
                }
            }
        }

        std::cout << "  Candidates (>90% similar): " << candidates.size() << std::endl;

        // Check candidates in parallel
        std::vector<std::future<std::pair<ProgramLocation, bool>>> futures;
        std::mutex results_mutex;

        for (const auto& candidate : candidates) {
            // Skip if already in cache
            if (cache.has(candidate.program)) {
                if (cache.is_replicator(candidate.program)) {
                    replicators_by_epoch[epoch + 1].insert(candidate);
                }
                continue;
            }

            // Wait if too many threads are running
            if (futures.size() >= num_threads) {
                // Wait for one to complete
                auto result = futures.front().get();
                futures.erase(futures.begin());

                cache.add(result.first.program, result.second);
                if (result.second) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    replicators_by_epoch[epoch + 1].insert(result.first);
                }
            }

            // Submit to thread pool
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
                replicators_by_epoch[epoch + 1].insert(result.first);
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
    if (argc < 8) {
        std::cerr << "Usage: " << argv[0]
                  << " <pairings_dir> <start_epoch> <grid_x> <grid_y> <last_epoch> <grid_width> <grid_height> [num_threads]"
                  << std::endl;
        std::cerr << "Example: " << argv[0]
                  << " python/test_data 16324 14 27 16327 64 64 8"
                  << std::endl;
        return 1;
    }

    std::string pairings_dir = argv[1];
    int start_epoch = std::atoi(argv[2]);
    int grid_x = std::atoi(argv[3]);
    int grid_y = std::atoi(argv[4]);
    int last_epoch = std::atoi(argv[5]);
    int grid_width = std::atoi(argv[6]);
    int grid_height = std::atoi(argv[7]);

    unsigned int num_threads = 0;
    if (argc > 8) {
        num_threads = std::atoi(argv[8]);
    }

    // Run forward pass analysis
    auto replicators = find_replicators(
        pairings_dir,
        start_epoch,
        grid_x,
        grid_y,
        last_epoch,
        grid_width,
        grid_height,
        num_threads
    );

    // Print summary
    std::cout << "=== Summary ===" << std::endl;
    int total_replicators = 0;
    std::set<std::string> unique_programs;
    std::map<std::string, ProgramLocation> first_appearance;

    for (const auto& [epoch, reps] : replicators) {
        std::cout << "Epoch " << epoch << ": " << reps.size() << " replicators" << std::endl;
        total_replicators += reps.size();

        // Collect unique programs and track first appearance
        for (const auto& rep : reps) {
            unique_programs.insert(rep.program);

            // Record first appearance of this program
            if (first_appearance.find(rep.program) == first_appearance.end()) {
                first_appearance[rep.program] = rep;
            }
        }
    }

    std::cout << "\nTotal replicators found: " << total_replicators << std::endl;
    std::cout << "Unique replicator programs: " << unique_programs.size() << std::endl;

    std::cout << "\nUnique replicator programs:" << std::endl;
    int prog_num = 1;
    for (const auto& program : unique_programs) {
        const auto& first = first_appearance[program];
        std::cout << "  [" << prog_num++ << "] " << program << std::endl;
        std::cout << "      First appeared at epoch " << first.epoch
                  << ", position (" << first.grid_x << ", " << first.grid_y << ")" << std::endl;
    }

    // Save results to file
    std::string output_path = pairings_dir + "/forward_pass_results.csv";
    std::ofstream out(output_path);
    if (out.is_open()) {
        out << "epoch,grid_x,grid_y,program\n";
        for (const auto& [epoch, reps] : replicators) {
            for (const auto& rep : reps) {
                out << rep.epoch << "," << rep.grid_x << "," << rep.grid_y << ",\"";
                out << rep.program;
                out << "\"\n";
            }
        }
        std::cout << "\nResults saved to: " << output_path << std::endl;
    }

    return 0;
}
