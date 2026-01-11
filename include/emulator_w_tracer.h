#ifndef EMULATOR_W_TRACER_H
#define EMULATOR_W_TRACER_H

#include <vector>
#include <cstdint>
#include <string>

// Token structure: 64-bit packed (epoch, position, char)
// Bits 0-7:   char (8 bits) - the actual character value
// Bits 8-23:  position (16 bits) - original position in tape
// Bits 24-63: epoch (40 bits) - epoch when token was created

struct Token {
    uint64_t value;

    Token() : value(0) {}
    Token(uint64_t v) : value(v) {}
    Token(uint64_t epoch, uint16_t position, uint8_t character);

    // Extract components
    uint8_t get_char() const { return static_cast<uint8_t>(value & 0xFF); }
    uint16_t get_position() const { return static_cast<uint16_t>((value >> 8) & 0xFFFF); }
    uint64_t get_epoch() const { return (value >> 24); }

    // Set only the character part (preserving epoch and position)
    void set_char(uint8_t character) {
        value = (value & 0xFFFFFFFFFFFFFF00ULL) | character;
    }

    // Create a new token with same epoch/position but different char
    Token with_char(uint8_t character) const {
        return Token((value & 0xFFFFFFFFFFFFFF00ULL) | character);
    }
};

struct EmulatorResultWithTracer {
    std::vector<Token> tape;      // Tape with tokens
    int head0_pos;                 // Final position of head 0
    int head1_pos;                 // Final position of head 1
    int pc_pos;                    // Final position of program counter
    int iteration;                 // Number of iterations executed
    int skipped;                   // Number of skipped instructions
    std::string state;            // Final state: "Finished", "Terminated", or "Running"
};

// Emulator with tracer - tracks character lineage through tokens
EmulatorResultWithTracer emulate_w_tracer(
    std::vector<Token> tape,
    int head0_pos = 0,
    int head1_pos = 0,
    int pc_pos = 0,
    int max_iter = 8192,
    int verbose = 0
);

// Helper function: Initialize tokens for a tape (epoch 0)
std::vector<Token> initialize_tokens(const std::vector<uint8_t>& byte_tape);

// Helper function: Initialize tokens with specific epoch
std::vector<Token> initialize_tokens_with_epoch(const std::vector<uint8_t>& byte_tape, uint64_t epoch);

// Helper function: Extract character values from tokens
std::vector<uint8_t> tokens_to_bytes(const std::vector<Token>& token_tape);

// Helper function: Create new token for mutation
Token create_mutation_token(uint64_t epoch, uint16_t position, uint8_t new_char);

#endif // EMULATOR_W_TRACER_H
