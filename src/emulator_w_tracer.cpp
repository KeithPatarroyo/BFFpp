#include "emulator_w_tracer.h"
#include <iostream>
#include <algorithm>

// Token constructor
Token::Token(uint64_t epoch, uint16_t position, uint8_t character) {
    value = (epoch << 24) | (static_cast<uint64_t>(position) << 8) | character;
}

// Initialize tokens for a byte tape (epoch 0)
std::vector<Token> initialize_tokens(const std::vector<uint8_t>& byte_tape) {
    return initialize_tokens_with_epoch(byte_tape, 0);
}

// Initialize tokens with specific epoch
std::vector<Token> initialize_tokens_with_epoch(const std::vector<uint8_t>& byte_tape, uint64_t epoch) {
    std::vector<Token> tokens;
    tokens.reserve(byte_tape.size());
    for (size_t i = 0; i < byte_tape.size(); i++) {
        tokens.push_back(Token(epoch, static_cast<uint16_t>(i), byte_tape[i]));
    }
    return tokens;
}

// Extract character values from tokens
std::vector<uint8_t> tokens_to_bytes(const std::vector<Token>& token_tape) {
    std::vector<uint8_t> bytes;
    bytes.reserve(token_tape.size());
    for (const auto& token : token_tape) {
        bytes.push_back(token.get_char());
    }
    return bytes;
}

// Create new token for mutation
Token create_mutation_token(uint64_t epoch, uint16_t position, uint8_t new_char) {
    return Token(epoch, position, new_char);
}

// Emulator with tracer implementation
EmulatorResultWithTracer emulate_w_tracer(
    std::vector<Token> tape,
    int head0_pos,
    int head1_pos,
    int pc_pos,
    int max_iter,
    int verbose
) {
    // Bounds checking
    int tape_size = static_cast<int>(tape.size());
    head0_pos = std::max(0, std::min(head0_pos, tape_size - 1));
    head1_pos = std::max(0, std::min(head1_pos, tape_size - 1));
    pc_pos = std::max(0, std::min(pc_pos, tape_size - 1));

    int iteration = 0;
    int skipped = 0;
    std::string state = "Running";

    // Valid BFF instructions
    const std::string instructions = "<>{}-+.,[]";
    const uint8_t zero = '0';  // ASCII 48

    while (iteration < max_iter) {
        iteration++;

        // Get current instruction (char part of token)
        char instruction = static_cast<char>(tape[pc_pos].get_char());

        // Skip non-instruction characters
        if (instructions.find(instruction) == std::string::npos) {
            skipped++;
            pc_pos = pc_pos + 1;
            if (pc_pos >= tape_size) {
                state = "Finished";
                break;
            }
            continue;
        }

        if (verbose > 0) {
            std::cout << "Iter " << iteration << ": PC=" << pc_pos
                      << " H0=" << head0_pos << " H1=" << head1_pos
                      << " Inst=" << instruction << std::endl;
        }

        // Execute instruction
        switch (instruction) {
            case '<':  // Move head 0 left
                head0_pos = (head0_pos - 1 + tape_size) % tape_size;
                break;

            case '>':  // Move head 0 right
                head0_pos = (head0_pos + 1) % tape_size;
                break;

            case '{':  // Move head 1 left
                head1_pos = (head1_pos - 1 + tape_size) % tape_size;
                break;

            case '}':  // Move head 1 right
                head1_pos = (head1_pos + 1) % tape_size;
                break;

            case '+': {  // Increment value at head 0 (only char part)
                uint8_t current = tape[head0_pos].get_char();
                tape[head0_pos].set_char(current + 1);
                break;
            }

            case '-': {  // Decrement value at head 0 (only char part)
                uint8_t current = tape[head0_pos].get_char();
                tape[head0_pos].set_char(current - 1);
                break;
            }

            case '.': {  // Copy from head 0 to head 1 (copy entire token)
                Token source_token = tape[head0_pos];
                tape[head1_pos] = source_token;
                break;
            }

            case ',': {  // Copy from head 1 to head 0 (copy entire token)
                Token source_token = tape[head1_pos];
                tape[head0_pos] = source_token;
                break;
            }

            case '[': {  // Jump forward if head 0 is '0' (ASCII 48)
                uint8_t value = tape[head0_pos].get_char();
                if (value == zero) {
                    int depth = 1;
                    for (int i = pc_pos + 1; i < tape_size; i++) {
                        char c = static_cast<char>(tape[i].get_char());
                        if (c == '[') depth++;
                        else if (c == ']') depth--;

                        if (depth == 0) {
                            pc_pos = i;
                            break;
                        }
                    }

                    if (depth != 0) {
                        state = "Error, Unmatched [";
                        return EmulatorResultWithTracer{
                            tape, head0_pos, head1_pos, pc_pos, iteration, skipped, state
                        };
                    }
                }
                break;
            }

            case ']': {  // Jump backward if head 0 is not '0' (ASCII 48)
                uint8_t value = tape[head0_pos].get_char();
                if (value != zero) {
                    int depth = 1;
                    for (int i = pc_pos - 1; i >= 0; i--) {
                        char c = static_cast<char>(tape[i].get_char());
                        if (c == ']') depth++;
                        else if (c == '[') depth--;

                        if (depth == 0) {
                            pc_pos = i;
                            break;
                        }
                    }

                    if (depth != 0) {
                        state = "Error, Unmatched ]";
                        return EmulatorResultWithTracer{
                            tape, head0_pos, head1_pos, pc_pos, iteration, skipped, state
                        };
                    }
                }
                break;
            }

            default:
                // Should never reach here due to earlier check
                break;
        }

        // Move to next instruction
        pc_pos = pc_pos + 1;

        // Check termination condition (reached end of tape)
        if (pc_pos >= tape_size) {
            state = "Finished";
            break;
        }
    }

    if (iteration >= max_iter && state == "Running") {
        state = "Terminated";
    }

    return EmulatorResultWithTracer{
        tape,
        head0_pos,
        head1_pos,
        pc_pos,
        iteration,
        skipped,
        state
    };
}
