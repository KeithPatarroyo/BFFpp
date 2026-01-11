#include "emulator.h"
#include "emulator_w_tracer.h"
#include "utils.h"
#include <iostream>
#include <iomanip>

int main() {
    // Create test programs
    std::string prog1_str = "[[{.>]-]                ]-]>.{[[";
    std::vector<uint8_t> prog1(prog1_str.begin(), prog1_str.end());
    std::vector<uint8_t> prog2(prog1.size(), '0');

    // Concatenate to create tape
    std::vector<uint8_t> tape_bytes = prog1;
    tape_bytes.insert(tape_bytes.end(), prog2.begin(), prog2.end());

    // Create token tape
    std::vector<Token> tape_tokens = initialize_tokens_with_epoch(tape_bytes, 0);

    std::cout << "Testing emulator equivalence..." << std::endl;
    std::cout << "Initial tape size: " << tape_bytes.size() << std::endl;

    // Run both emulators
    EmulatorResult result1 = emulate(tape_bytes, 0, static_cast<int>(prog1.size()));
    EmulatorResultWithTracer result2 = emulate_w_tracer(tape_tokens, 0, static_cast<int>(prog1.size()));

    std::cout << "\nEmulator (no tracer):" << std::endl;
    std::cout << "  State: " << result1.state << std::endl;
    std::cout << "  Iterations: " << result1.iteration << std::endl;
    std::cout << "  Tape size: " << result1.tape.size() << std::endl;

    std::cout << "\nEmulator with tracer:" << std::endl;
    std::cout << "  State: " << result2.state << std::endl;
    std::cout << "  Iterations: " << result2.iteration << std::endl;
    std::cout << "  Tape size: " << result2.tape.size() << std::endl;

    // Compare tape contents
    bool tapes_match = true;
    if (result1.tape.size() != result2.tape.size()) {
        std::cout << "\nERROR: Tape sizes differ!" << std::endl;
        tapes_match = false;
    } else {
        for (size_t i = 0; i < result1.tape.size(); i++) {
            uint8_t byte1 = result1.tape[i];
            uint8_t byte2 = result2.tape[i].get_char();

            if (byte1 != byte2) {
                if (tapes_match) {
                    std::cout << "\nTape differences found:" << std::endl;
                    tapes_match = false;
                }
                std::cout << "  Position " << i << ": "
                          << static_cast<int>(byte1) << " vs "
                          << static_cast<int>(byte2) << std::endl;
            }
        }
    }

    if (tapes_match) {
        std::cout << "\nSUCCESS: Tapes match perfectly!" << std::endl;
    } else {
        std::cout << "\nFAILURE: Tapes do not match!" << std::endl;

        // Print first 64 bytes of each
        std::cout << "\nFirst 64 bytes of regular emulator tape:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(64), result1.tape.size()); i++) {
            if (i > 0 && i % 16 == 0) std::cout << std::endl;
            std::cout << std::setw(3) << static_cast<int>(result1.tape[i]) << " ";
        }
        std::cout << std::endl;

        std::cout << "\nFirst 64 bytes of tracer emulator tape:" << std::endl;
        for (size_t i = 0; i < std::min(size_t(64), result2.tape.size()); i++) {
            if (i > 0 && i % 16 == 0) std::cout << std::endl;
            std::cout << std::setw(3) << static_cast<int>(result2.tape[i].get_char()) << " ";
        }
        std::cout << std::endl;
    }

    return tapes_match ? 0 : 1;
}
