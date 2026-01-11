#include "grid_w_tracer.h"
#include <fstream>
#include <iomanip>
#include <iostream>

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
