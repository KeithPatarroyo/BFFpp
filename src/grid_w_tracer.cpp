#include "grid_w_tracer.h"
#include "utils.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <algorithm>

GridWithTracer::GridWithTracer(int width, int height, int program_size)
    : width(width), height(height), program_size(program_size) {
    grid_data.resize(width * height);
    for (auto& program : grid_data) {
        program.reserve(program_size);
    }
}

void GridWithTracer::initialize_random() {
    std::random_device rd;
    std::mt19937 gen(rd());
    initialize_random(gen);
}

void GridWithTracer::initialize_random(std::mt19937& rng) {
    std::uniform_int_distribution<> dis(0, 255);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = index(x, y);
            grid_data[idx].clear();

            // Create random program with tokens
            // Each token's initial position is its index in the program
            // Epoch is 0 for initialization
            for (int i = 0; i < program_size; i++) {
                uint8_t random_char = static_cast<uint8_t>(dis(rng));
                Token token(0, static_cast<uint16_t>(i), random_char);
                grid_data[idx].push_back(token);
            }
        }
    }
}

std::vector<Token>& GridWithTracer::get_program(int x, int y) {
    return grid_data[index(x, y)];
}

const std::vector<Token>& GridWithTracer::get_program(int x, int y) const {
    return grid_data[index(x, y)];
}

void GridWithTracer::set_program(int x, int y, const std::vector<Token>& program) {
    grid_data[index(x, y)] = program;
}

std::vector<uint8_t> GridWithTracer::get_program_bytes(int x, int y) const {
    const auto& tokens = get_program(x, y);
    return tokens_to_bytes(tokens);
}

RGB GridWithTracer::program_to_color(const std::vector<uint8_t>& program) const {
    // Semantic color mapping based on CuBFF implementation
    // Colors programs based on instruction type frequencies

    if (program.empty()) {
        return RGB{0, 0, 0};
    }

    // Count instruction types
    int loop_ops = 0;      // [ ]
    int arith_ops = 0;     // + - . ,
    int head_ops = 0;      // < > { }
    int null_ops = 0;      // non-instruction bytes

    const std::string instructions = "<>{}-+.,[]";

    for (uint8_t byte : program) {
        char ch = static_cast<char>(byte);
        if (ch == '[' || ch == ']') {
            loop_ops++;
        } else if (ch == '+' || ch == '-' || ch == '.' || ch == ',') {
            arith_ops++;
        } else if (ch == '<' || ch == '>' || ch == '{' || ch == '}') {
            head_ops++;
        } else {
            null_ops++;
        }
    }

    // Calculate dominant instruction type
    int total_instructions = loop_ops + arith_ops + head_ops;

    if (total_instructions == 0) {
        // All null/invalid - red tint
        return RGB{255, 0, 0};
    }

    // Mix colors based on instruction composition
    float loop_ratio = static_cast<float>(loop_ops) / total_instructions;
    float arith_ratio = static_cast<float>(arith_ops) / total_instructions;
    float head_ratio = static_cast<float>(head_ops) / total_instructions;

    // Base colors (from CuBFF):
    // Loop operations: Green {0, 192, 0}
    // Arithmetic/copy: Magenta {200, 0, 200}
    // Head movement: Light purple {200, 128, 220}

    uint8_t r = static_cast<uint8_t>(
        loop_ratio * 0 +
        arith_ratio * 200 +
        head_ratio * 200
    );

    uint8_t g = static_cast<uint8_t>(
        loop_ratio * 192 +
        arith_ratio * 0 +
        head_ratio * 128
    );

    uint8_t b = static_cast<uint8_t>(
        loop_ratio * 0 +
        arith_ratio * 200 +
        head_ratio * 220
    );

    return RGB{r, g, b};
}

void GridWithTracer::save_tokens_to_csv(const std::string& filepath, int epoch_num) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return;
    }

    // Write header
    file << "epoch_snapshot,grid_x,grid_y,pos_in_program,token_epoch,token_orig_pos,char,char_ascii\n";

    // Write all tokens
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto& program = get_program(x, y);
            for (size_t i = 0; i < program.size(); i++) {
                const Token& token = program[i];
                file << epoch_num << ","
                     << x << ","
                     << y << ","
                     << i << ","
                     << token.get_epoch() << ","
                     << token.get_position() << ","
                     << static_cast<int>(token.get_char()) << ",";

                // Add printable char representation
                char c = static_cast<char>(token.get_char());
                if (c >= 32 && c <= 126) {
                    file << "\"" << c << "\"";
                } else {
                    file << "\"\"";
                }
                file << "\n";
            }
        }
    }

    file.close();
}

std::string GridWithTracer::to_json(int epoch, double entropy, double finished_ratio) const {
    std::ostringstream json;

    json << "{";
    json << "\"epoch\":" << epoch << ",";
    json << "\"width\":" << width << ",";
    json << "\"height\":" << height << ",";
    json << "\"entropy\":" << std::fixed << std::setprecision(6) << entropy << ",";
    json << "\"finished_ratio\":" << std::fixed << std::setprecision(6) << finished_ratio << ",";
    json << "\"grid\":[";

    // Write grid data as JSON array
    for (int y = 0; y < height; y++) {
        json << "[";
        for (int x = 0; x < width; x++) {
            // Convert tokens to bytes, then to color
            std::vector<uint8_t> program_bytes = get_program_bytes(x, y);
            RGB color = program_to_color(program_bytes);
            json << "[" << static_cast<int>(color.r) << ","
                 << static_cast<int>(color.g) << ","
                 << static_cast<int>(color.b) << "]";
            if (x < width - 1) json << ",";
        }
        json << "]";
        if (y < height - 1) json << ",";
    }

    json << "]}";

    return json.str();
}

std::vector<GridWithTracer::Cell> GridWithTracer::get_von_neumann_neighbors(int x, int y, int radius) const {
    std::vector<Cell> neighbors;

    // Iterate through all possible cells within Manhattan distance
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            // Calculate Manhattan distance
            int manhattan_dist = std::abs(dx) + std::abs(dy);

            // Skip if outside radius or is the cell itself
            if (manhattan_dist == 0 || manhattan_dist > radius) {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;

            // Check bounds
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                neighbors.push_back(Cell{nx, ny});
            }
        }
    }

    return neighbors;
}

std::vector<std::pair<int, int>> GridWithTracer::create_spatial_pairs(int neighborhood_radius, std::mt19937& rng) {
    int total_cells = width * height;
    std::vector<std::pair<int, int>> pairs;
    std::vector<bool> taken(total_cells, false);

    // Create a random order to visit cells
    std::vector<int> cell_order(total_cells);
    for (int i = 0; i < total_cells; i++) {
        cell_order[i] = i;
    }

    // Shuffle cells
    std::shuffle(cell_order.begin(), cell_order.end(), rng);

    // Process cells in random order
    for (int cell_idx : cell_order) {
        // Skip if already taken
        if (taken[cell_idx]) {
            continue;
        }

        // Convert flat index to x, y
        int y = cell_idx / width;
        int x = cell_idx % width;

        // Get neighbors
        std::vector<Cell> neighbors = get_von_neumann_neighbors(x, y, neighborhood_radius);

        // Filter to only untaken neighbors
        std::vector<int> available_neighbors;
        for (const Cell& neighbor : neighbors) {
            int neighbor_idx = index(neighbor.x, neighbor.y);
            if (!taken[neighbor_idx]) {
                available_neighbors.push_back(neighbor_idx);
            }
        }

        // If there are available neighbors, pick one randomly
        if (!available_neighbors.empty()) {
            std::uniform_int_distribution<int> dist(0, available_neighbors.size() - 1);
            int chosen_idx = available_neighbors[dist(rng)];

            // Mark both as taken and add pair
            taken[cell_idx] = true;
            taken[chosen_idx] = true;
            pairs.push_back({cell_idx, chosen_idx});
        } else {
            // No available neighbors - mark as mutation-only
            taken[cell_idx] = true;
            pairs.push_back({-1, cell_idx});
        }
    }

    return pairs;
}

std::vector<Token> GridWithTracer::mutate(const std::vector<Token>& program, double mutation_rate,
                                          uint64_t epoch, std::mt19937& rng) {
    std::vector<Token> mutated = program;

    std::uniform_real_distribution<> mutation_dist(0.0, 1.0);
    std::uniform_int_distribution<> byte_dist(0, 255);
    std::uniform_int_distribution<> pos_dist(0, program_size - 1);

    if (mutation_dist(rng) < mutation_rate) {
        int mut_pos = pos_dist(rng);
        uint8_t new_char = static_cast<uint8_t>(byte_dist(rng));
        // Create new token with given epoch and mutation position
        mutated[mut_pos] = Token(epoch, mut_pos, new_char);
    }

    return mutated;
}
