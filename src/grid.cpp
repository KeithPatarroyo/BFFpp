#include "grid.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <random>
#include <algorithm>

Grid::Grid(int width, int height, int program_size)
    : width(width), height(height), program_size(program_size) {
    grid_data.resize(width * height);
    for (auto& program : grid_data) {
        program.resize(program_size);
    }
}

void Grid::initialize_random() {
    for (auto& program : grid_data) {
        program = generate_random_program(program_size);
    }
}

void Grid::initialize_random(std::mt19937& rng) {
    for (auto& program : grid_data) {
        program = generate_random_program(program_size, rng);
    }
}

std::vector<uint8_t>& Grid::get_program(int x, int y) {
    return grid_data[index(x, y)];
}

const std::vector<uint8_t>& Grid::get_program(int x, int y) const {
    return grid_data[index(x, y)];
}

void Grid::set_program(int x, int y, const std::vector<uint8_t>& program) {
    grid_data[index(x, y)] = program;
}

std::vector<std::vector<uint8_t>> Grid::get_all_programs() const {
    return grid_data;
}

void Grid::set_all_programs(const std::vector<std::vector<uint8_t>>& programs) {
    grid_data = programs;
}

RGB Grid::program_to_color(const std::vector<uint8_t>& program) const {
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

void Grid::save_ppm(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    // PPM header
    file << "P3\n";
    file << width << " " << height << "\n";
    file << "255\n";

    // Write pixel data
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            RGB color = program_to_color(get_program(x, y));
            file << static_cast<int>(color.r) << " "
                 << static_cast<int>(color.g) << " "
                 << static_cast<int>(color.b) << " ";
        }
        file << "\n";
    }

    file.close();
}

void Grid::save_html(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filename);
    }

    // Calculate canvas size (scale up for better visibility)
    int scale = std::max(1, 800 / std::max(width, height));
    int canvas_width = width * scale;
    int canvas_height = height * scale;

    file << R"(<!DOCTYPE html>
<html>
<head>
    <title>BFF Grid Visualization</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            background: #1a1a1a;
            color: #fff;
            font-family: monospace;
        }
        canvas {
            border: 1px solid #444;
            image-rendering: pixelated;
            image-rendering: crisp-edges;
        }
        .info {
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
    <div class="info">
        <h2>BFF Grid Visualization</h2>
        <p>Grid Size: )" << width << "x" << height << R"( ()" << get_total_programs() << R"( programs)</p>
        <p>Program Size: )" << program_size << R"( bytes</p>
    </div>
    <canvas id="canvas" width=")" << canvas_width << R"(" height=")" << canvas_height << R"("></canvas>
    <script>
        const canvas = document.getElementById('canvas');
        const ctx = canvas.getContext('2d');
        const width = )" << width << R"(;
        const height = )" << height << R"(;
        const scale = )" << scale << R"(;

        // Grid data (RGB values)
        const gridData = [
)";

    // Write grid data as JavaScript array
    for (int y = 0; y < height; y++) {
        file << "            [";
        for (int x = 0; x < width; x++) {
            RGB color = program_to_color(get_program(x, y));
            file << "[" << static_cast<int>(color.r) << ","
                 << static_cast<int>(color.g) << ","
                 << static_cast<int>(color.b) << "]";
            if (x < width - 1) file << ",";
        }
        file << "]";
        if (y < height - 1) file << ",";
        file << "\n";
    }

    file << R"(        ];

        // Draw grid
        for (let y = 0; y < height; y++) {
            for (let x = 0; x < width; x++) {
                const [r, g, b] = gridData[y][x];
                ctx.fillStyle = `rgb(${r},${g},${b})`;
                ctx.fillRect(x * scale, y * scale, scale, scale);
            }
        }
    </script>
</body>
</html>
)";

    file.close();
}

std::string Grid::to_json(int epoch, double entropy, double avg_iters, double finished_ratio) const {
    std::ostringstream json;

    json << "{";
    json << "\"epoch\":" << epoch << ",";
    json << "\"width\":" << width << ",";
    json << "\"height\":" << height << ",";
    json << "\"entropy\":" << std::fixed << std::setprecision(6) << entropy << ",";
    json << "\"avg_iters\":" << std::fixed << std::setprecision(3) << avg_iters << ",";
    json << "\"finished_ratio\":" << std::fixed << std::setprecision(6) << finished_ratio << ",";
    json << "\"grid\":[";

    // Write grid data as JSON array
    for (int y = 0; y < height; y++) {
        json << "[";
        for (int x = 0; x < width; x++) {
            RGB color = program_to_color(get_program(x, y));
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

std::vector<Grid::Cell> Grid::get_von_neumann_neighbors(int x, int y, int radius) const {
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

std::vector<std::pair<int, int>> Grid::create_spatial_pairs(int neighborhood_radius) {
    int total_cells = width * height;
    std::vector<std::pair<int, int>> pairs;
    std::vector<bool> taken(total_cells, false);

    // Create a random order to visit cells
    std::vector<int> cell_order(total_cells);
    for (int i = 0; i < total_cells; i++) {
        cell_order[i] = i;
    }

    // Get RNG from utils (use shared RNG for reproducibility)
    std::mt19937& rng = get_rng();
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

std::vector<std::pair<int, int>> Grid::create_spatial_pairs(int neighborhood_radius, std::mt19937& custom_rng) {
    int total_cells = width * height;
    std::vector<std::pair<int, int>> pairs;
    std::vector<bool> taken(total_cells, false);

    // Create a random order to visit cells
    std::vector<int> cell_order(total_cells);
    for (int i = 0; i < total_cells; i++) {
        cell_order[i] = i;
    }

    // Use custom RNG instead of shared RNG
    std::shuffle(cell_order.begin(), cell_order.end(), custom_rng);

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
            int chosen_idx = available_neighbors[dist(custom_rng)];

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
