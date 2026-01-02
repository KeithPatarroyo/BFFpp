#include "emulator.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <cstdint>

int main() {
    // Test case from original Python implementation
    std::string program1_str = "[[{.>]-]                ]-]>.{[[";
    std::vector<uint8_t> program1(program1_str.begin(), program1_str.end());

    // Create program2 filled with '0' characters
    std::vector<uint8_t> program2(program1.size(), '0');

    // Concatenate programs to create tape
    std::vector<uint8_t> tape = program1;
    tape.insert(tape.end(), program2.begin(), program2.end());

    std::cout << "Running emulator test with verbose output:\n" << std::endl;
    std::cout << "Initial tape:" << std::endl;
    print_tape(tape, 0, 0, 0, false);
    std::cout << std::endl;

    // Run emulation with verbose output
    EmulatorResult result = emulate(tape, 0, 0, 0, 1024, 1);

    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "State: " << result.state << std::endl;
    std::cout << "Iterations: " << result.iteration << std::endl;
    std::cout << "Skipped: " << result.skipped << std::endl;
    std::cout << "\nFinal tape:" << std::endl;
    print_tape(result.tape, -1, -1, -1, false);
    std::cout << std::endl;

    return 0;
}
