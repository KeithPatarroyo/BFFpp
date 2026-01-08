#ifndef GRID_H
#define GRID_H

#include <vector>
#include <cstdint>
#include <string>
#include <random>

struct RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class Grid {
public:
    Grid(int width, int height, int program_size);

    // Initialize grid with random programs
    void initialize_random();
    void initialize_random(std::mt19937& rng);

    // Get program at position
    std::vector<uint8_t>& get_program(int x, int y);
    const std::vector<uint8_t>& get_program(int x, int y) const;

    // Set program at position
    void set_program(int x, int y, const std::vector<uint8_t>& program);

    // Get all programs as a flat vector (for fully-connected topology)
    std::vector<std::vector<uint8_t>> get_all_programs() const;

    // Set all programs from flat vector
    void set_all_programs(const std::vector<std::vector<uint8_t>>& programs);

    // Convert program to RGB color for visualization
    RGB program_to_color(const std::vector<uint8_t>& program) const;

    // Save grid as PPM image
    void save_ppm(const std::string& filename) const;

    // Generate HTML visualization
    void save_html(const std::string& filename) const;

    // Serialize grid to JSON for WebSocket
    std::string to_json(int epoch, double entropy, double avg_iters, double finished_ratio) const;

    // Spatial pairing methods
    struct Cell {
        int x;
        int y;
        int index() const { return -1; } // Placeholder, actual index computed by Grid
    };

    // Get Von Neumann neighborhood (Manhattan distance <= r)
    std::vector<Cell> get_von_neumann_neighbors(int x, int y, int radius = 2) const;

    // Create spatial pairs using Von Neumann neighborhoods
    // Returns vector of pairs: (cell_index1, cell_index2)
    // Cells without available neighbors return (-1, cell_index) to indicate mutation-only
    std::vector<std::pair<int, int>> create_spatial_pairs(int neighborhood_radius = 2);
    std::vector<std::pair<int, int>> create_spatial_pairs(int neighborhood_radius, std::mt19937& rng);

    // Getters
    int get_width() const { return width; }
    int get_height() const { return height; }
    int get_program_size() const { return program_size; }
    int get_total_programs() const { return width * height; }

private:
    int width;
    int height;
    int program_size;
    std::vector<std::vector<uint8_t>> grid_data; // Flattened 2D array

    int index(int x, int y) const { return y * width + x; }
};

#endif // GRID_H
