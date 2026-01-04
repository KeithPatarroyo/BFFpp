#ifndef CONFIG_H
#define CONFIG_H

#include <string>

struct Config {
    int random_seed;
    int soup_size;
    int program_size;
    int epochs;
    double mutation_rate;
    int read_head_position;
    int write_head_position;
    int eval_interval;
    int num_print_programs;

    // Grid parameters
    int grid_width;
    int grid_height;
    bool use_grid;
    int visualization_interval;
};

Config load_config(const std::string& filename);

#endif // CONFIG_H
