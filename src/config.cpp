#include "config.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>

Config load_config(const std::string& filename) {
    Config config;
    std::ifstream file(filename);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + filename);
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the colon separator
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Parse values
        if (key == "random_seed") {
            config.random_seed = std::stoi(value);
        } else if (key == "soup_size") {
            config.soup_size = std::stoi(value);
        } else if (key == "program_size") {
            config.program_size = std::stoi(value);
        } else if (key == "epochs") {
            config.epochs = std::stoi(value);
        } else if (key == "mutation_rate") {
            config.mutation_rate = std::stod(value);
        } else if (key == "read_head_position") {
            config.read_head_position = std::stoi(value);
        } else if (key == "write_head_position") {
            config.write_head_position = std::stoi(value);
        } else if (key == "eval_interval") {
            config.eval_interval = std::stoi(value);
        } else if (key == "num_print_programs") {
            config.num_print_programs = std::stoi(value);
        }
    }

    file.close();
    return config;
}
