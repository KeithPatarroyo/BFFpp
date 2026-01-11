#include "grid.h"
#include "grid_w_tracer.h"
#include "utils.h"
#include <iostream>
#include <iomanip>

int main() {
    // Test 1: Check if initialize_random produces same results
    std::cout << "=== Testing initialize_random ===" << std::endl;

    // Test with Grid
    seed_random(42);
    Grid grid1(3, 3, 8);
    grid1.initialize_random();

    std::cout << "Grid (no tracer) first program:" << std::endl;
    const auto& prog1 = grid1.get_program(0, 0);
    for (size_t i = 0; i < prog1.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(prog1[i]) << " ";
    }
    std::cout << std::endl;

    // Test with GridWithTracer
    seed_random(42);
    GridWithTracer grid2(3, 3, 8);
    grid2.initialize_random(get_rng());

    std::cout << "GridWithTracer first program:" << std::endl;
    const auto& prog2 = grid2.get_program(0, 0);
    for (size_t i = 0; i < prog2.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(prog2[i].get_char()) << " ";
    }
    std::cout << std::endl;

    // Test 2: Check if create_spatial_pairs produces same results
    std::cout << "\n=== Testing create_spatial_pairs ===" << std::endl;

    seed_random(42);
    Grid grid3(3, 3, 8);
    grid3.initialize_random();
    auto pairs1 = grid3.create_spatial_pairs(2);

    std::cout << "Grid pairs:" << std::endl;
    for (size_t i = 0; i < pairs1.size(); i++) {
        std::cout << "  Pair " << i << ": (" << pairs1[i].first << ", " << pairs1[i].second << ")" << std::endl;
    }

    seed_random(42);
    GridWithTracer grid4(3, 3, 8);
    grid4.initialize_random(get_rng());
    auto pairs2 = grid4.create_spatial_pairs(2, get_rng());

    std::cout << "GridWithTracer pairs:" << std::endl;
    for (size_t i = 0; i < pairs2.size(); i++) {
        std::cout << "  Pair " << i << ": (" << pairs2[i].first << ", " << pairs2[i].second << ")" << std::endl;
    }

    // Test 3: Check if mutate produces same results
    std::cout << "\n=== Testing mutate ===" << std::endl;

    seed_random(42);
    std::vector<uint8_t> test_prog = {100, 101, 102, 103, 104, 105, 106, 107};
    auto mutated1 = mutate(test_prog, 0.5);

    std::cout << "Utils mutate result:" << std::endl;
    for (size_t i = 0; i < mutated1.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(mutated1[i]) << " ";
    }
    std::cout << std::endl;

    seed_random(42);
    std::vector<Token> test_prog2;
    for (size_t i = 0; i < 8; i++) {
        test_prog2.push_back(Token(0, i, 100 + i));
    }
    auto mutated2 = grid4.mutate(test_prog2, 0.5, 1, get_rng());

    std::cout << "GridWithTracer mutate result:" << std::endl;
    for (size_t i = 0; i < mutated2.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(mutated2[i].get_char()) << " ";
    }
    std::cout << std::endl;

    return 0;
}
