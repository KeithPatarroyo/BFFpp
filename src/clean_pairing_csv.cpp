#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Check if character is a valid BFF instruction
bool is_instruction(char ch) {
    const std::string instructions = ",.[]{}()<>+-";
    return instructions.find(ch) != std::string::npos;
}

// Clean program by replacing non-instructions with spaces
std::string clean_program(const std::string& program) {
    std::string cleaned = program;
    for (size_t i = 0; i < cleaned.size(); i++) {
        if (!is_instruction(cleaned[i])) {
            cleaned[i] = ' ';
        }
    }
    return cleaned;
}

// Parse CSV field (handles quoted fields)
std::string parse_csv_field(const std::string& field) {
    std::string result = field;
    // Remove surrounding quotes if present
    if (!result.empty() && result.front() == '"' && result.back() == '"') {
        result = result.substr(1, result.size() - 2);
    }
    return result;
}

// Process a single CSV file
void process_csv_file(const std::string& input_path, const std::string& output_path) {
    std::ifstream infile(input_path);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open " << input_path << std::endl;
        return;
    }

    std::ofstream outfile(output_path);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not create " << output_path << std::endl;
        return;
    }

    std::string line;
    bool is_header = true;
    int line_count = 0;

    while (std::getline(infile, line)) {
        line_count++;

        // Write header as-is
        if (is_header) {
            outfile << line << "\n";
            is_header = false;
            continue;
        }

        // Parse CSV line manually to handle quoted programs with commas
        std::vector<std::string> fields;
        std::string current_field;
        bool in_quotes = false;

        for (size_t i = 0; i < line.size(); i++) {
            char ch = line[i];

            if (ch == '"') {
                in_quotes = !in_quotes;
                current_field += ch;
            } else if (ch == ',' && !in_quotes) {
                fields.push_back(current_field);
                current_field.clear();
            } else {
                current_field += ch;
            }
        }
        // Add last field
        if (!current_field.empty() || !fields.empty()) {
            fields.push_back(current_field);
        }

        // Need at least 6 fields: epoch,position_x,position_y,program,combined_x,combined_y
        if (fields.size() < 6) {
            std::cerr << "Warning: Line " << line_count << " has only " << fields.size() << " fields, skipping" << std::endl;
            continue;
        }

        // Extract fields
        std::string epoch = fields[0];
        std::string position_x = fields[1];
        std::string position_y = fields[2];
        std::string program = parse_csv_field(fields[3]);
        std::string combined_x = fields[4];
        std::string combined_y = fields[5];

        // Clean the program
        std::string cleaned_program = clean_program(program);

        // Write cleaned line
        outfile << epoch << ","
                << position_x << ","
                << position_y << ",\""
                << cleaned_program << "\","
                << combined_x << ","
                << combined_y << "\n";
    }

    infile.close();
    outfile.close();

    std::cout << "Processed " << line_count << " lines from " << input_path << std::endl;
    std::cout << "  Output: " << output_path << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <pairings_directory> [output_directory]" << std::endl;
        std::cerr << "Example: " << argv[0] << " data/pairings data/pairings_cleaned" << std::endl;
        return 1;
    }

    std::string input_dir = argv[1];
    std::string output_dir = (argc > 2) ? argv[2] : input_dir + "_cleaned";

    // Create output directory
    if (!fs::exists(output_dir)) {
        fs::create_directories(output_dir);
        std::cout << "Created output directory: " << output_dir << std::endl;
    }

    std::cout << "Cleaning pairing CSV files..." << std::endl;
    std::cout << "Input:  " << input_dir << std::endl;
    std::cout << "Output: " << output_dir << std::endl;
    std::cout << std::endl;

    int file_count = 0;

    // Process all CSV files in input directory
    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (entry.path().extension() == ".csv") {
            std::string filename = entry.path().filename().string();
            std::string input_path = entry.path().string();
            std::string output_path = output_dir + "/" + filename;

            std::cout << "Processing: " << filename << std::endl;
            process_csv_file(input_path, output_path);
            std::cout << std::endl;

            file_count++;
        }
    }

    std::cout << "Done! Processed " << file_count << " files." << std::endl;

    return 0;
}
