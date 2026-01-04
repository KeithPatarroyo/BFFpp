#include "grid.h"
#include "utils.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <iomanip>

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
    // Simple color mapping: use first 3 bytes as RGB
    // This creates a unique color fingerprint for each program
    RGB color;

    if (program.size() >= 3) {
        color.r = program[0];
        color.g = program[1];
        color.b = program[2];
    } else {
        // Fallback for very small programs
        color.r = program.size() > 0 ? program[0] : 0;
        color.g = program.size() > 1 ? program[1] : 0;
        color.b = 0;
    }

    return color;
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
