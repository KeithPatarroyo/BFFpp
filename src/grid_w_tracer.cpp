#include "grid_w_tracer.h"
#include "utils.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

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

std::string GridWithTracer::to_json(int epoch, double entropy) const {
    std::ostringstream json;

    json << "{";
    json << "\"epoch\":" << epoch << ",";
    json << "\"width\":" << width << ",";
    json << "\"height\":" << height << ",";
    json << "\"entropy\":" << std::fixed << std::setprecision(6) << entropy << ",";
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
