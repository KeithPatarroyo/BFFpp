#ifndef GRID_W_TRACER_H
#define GRID_W_TRACER_H

#include "emulator_w_tracer.h"
#include <vector>
#include <cstdint>
#include <string>
#include <random>

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class GridWithTracer {
public:
    struct Cell {
        int x;
        int y;
    };

    GridWithTracer(int width, int height, int program_size);

    // Initialize grid with random programs and tokens (epoch 0)
    void initialize_random();
    void initialize_random(std::mt19937& rng);

    // Get program tokens at position
    std::vector<Token>& get_program(int x, int y);
    const std::vector<Token>& get_program(int x, int y) const;

    // Set program tokens at position
    void set_program(int x, int y, const std::vector<Token>& program);

    // Get program as bytes (for compatibility)
    std::vector<uint8_t> get_program_bytes(int x, int y) const;

    // Convert program to RGB color for visualization
    RGB program_to_color(const std::vector<uint8_t>& program) const;

    // Save all tokens to CSV file
    void save_tokens_to_csv(const std::string& filepath, int epoch_num) const;

    // Generate JSON for visualization
    std::string to_json(int epoch, double entropy, double finished_ratio) const;

    // Von Neumann neighborhood
    std::vector<Cell> get_von_neumann_neighbors(int x, int y, int radius) const;

    // Create spatial pairs for pairing programs
    std::vector<std::pair<int, int>> create_spatial_pairs(int neighborhood_radius, std::mt19937& rng);

    // Mutate a program (creates new token with given epoch)
    std::vector<Token> mutate(const std::vector<Token>& program, double mutation_rate,
                              uint64_t epoch, std::mt19937& rng);

    // Getters
    int get_width() const { return width; }
    int get_height() const { return height; }
    int get_program_size() const { return program_size; }
    int get_total_programs() const { return width * height; }

    // Get all tokens (flattened)
    const std::vector<std::vector<Token>>& get_all_programs() const { return grid_data; }

private:
    int width;
    int height;
    int program_size;
    std::vector<std::vector<Token>> grid_data; // Flattened 2D array

    int index(int x, int y) const { return y * width + x; }
};

#endif // GRID_W_TRACER_H
