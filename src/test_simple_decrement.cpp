#include "emulator.h"
#include "emulator_w_tracer.h"
#include <iostream>
#include <iomanip>

int main() {
    // Simple test: just decrement '0' to see if it becomes '/'
    std::string program = "---0";  // Three decrements, then a '0'

    std::vector<uint8_t> tape1(program.begin(), program.end());
    std::vector<Token> tape2 = initialize_tokens_with_epoch(std::vector<uint8_t>(program.begin(), program.end()), 0);

    std::cout << "Initial tape:" << std::endl;
    for (size_t i = 0; i < tape1.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(tape1[i]) << " ";
    }
    std::cout << std::endl;

    // Run emulators (head0=0, head1=0)
    EmulatorResult result1 = emulate(tape1, 0, 0, 0, 100, 0);
    EmulatorResultWithTracer result2 = emulate_w_tracer(tape2, 0, 0, 0, 100, 0);

    std::cout << "\nRegular emulator result:" << std::endl;
    for (size_t i = 0; i < result1.tape.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(result1.tape[i]) << " ";
    }
    std::cout << std::endl;

    std::cout << "\nTracer emulator result:" << std::endl;
    for (size_t i = 0; i < result2.tape.size(); i++) {
        std::cout << std::setw(3) << static_cast<int>(result2.tape[i].get_char()) << " ";
    }
    std::cout << std::endl;

    std::cout << "\nExpected: 45 45 45 45 (three '-' characters and one '-' at position 3)" << std::endl;
    std::cout << "Position 3 should be: 48 - 3 = 45" << std::endl;

    return 0;
}
