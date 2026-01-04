#ifndef GRID_H
#define GRID_H

#include <vector>
#include <cstdint>
#include <string>

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
