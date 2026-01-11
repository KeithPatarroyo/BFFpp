#include "emulator_w_tracer.h"
#include "utils.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>

void print_tape_with_tokens(const std::vector<Token>& tape, int head0_pos, int head1_pos, int pc_pos) {
    std::cout << "Tape state (showing tokens):" << std::endl;
    std::cout << "Pos | Char | Epoch | OrigPos | H0 | H1 | PC" << std::endl;
    std::cout << "----+------+-------+---------+----+----+----" << std::endl;

    for (size_t i = 0; i < tape.size(); i++) {
        const Token& t = tape[i];
        uint8_t ch = t.get_char();

        std::cout << std::setw(3) << i << " | ";
        std::cout << std::setw(4) << static_cast<int>(ch) << " | ";
        std::cout << std::setw(5) << t.get_epoch() << " | ";
        std::cout << std::setw(7) << t.get_position() << " | ";

        // Mark head positions
        if (static_cast<int>(i) == head0_pos) std::cout << " H0 ";
        else std::cout << "    ";

        if (static_cast<int>(i) == head1_pos) std::cout << " H1 ";
        else std::cout << "    ";

        if (static_cast<int>(i) == pc_pos) std::cout << " PC";
        else std::cout << "   ";

        // Show character representation
        if (ch >= 32 && ch <= 126) {
            std::cout << "  '" << static_cast<char>(ch) << "'";
        }

        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void print_execution_state(int iteration, int head0_pos, int head1_pos, int pc_pos,
                           const std::vector<Token>& tape) {
    std::cout << "=== Iteration " << iteration << " ===" << std::endl;
    std::cout << "Head0 at: " << head0_pos << ", Head1 at: " << head1_pos << ", PC at: " << pc_pos << std::endl;

    if (pc_pos >= 0 && pc_pos < static_cast<int>(tape.size())) {
        uint8_t current_instr = tape[pc_pos].get_char();
        if (current_instr >= 32 && current_instr <= 126) {
            std::cout << "Current instruction: '" << static_cast<char>(current_instr) << "'" << std::endl;
        } else {
            std::cout << "Current instruction: " << static_cast<int>(current_instr) << std::endl;
        }
    }

    // Show tokens at head positions
    if (head0_pos >= 0 && head0_pos < static_cast<int>(tape.size())) {
        const Token& t0 = tape[head0_pos];
        std::cout << "Token at Head0: char=" << static_cast<int>(t0.get_char())
                  << ", epoch=" << t0.get_epoch()
                  << ", origpos=" << t0.get_position() << std::endl;
    }

    if (head1_pos >= 0 && head1_pos < static_cast<int>(tape.size())) {
        const Token& t1 = tape[head1_pos];
        std::cout << "Token at Head1: char=" << static_cast<int>(t1.get_char())
                  << ", epoch=" << t1.get_epoch()
                  << ", origpos=" << t1.get_position() << std::endl;
    }

    std::cout << std::endl;
}

int main() {
    std::cout << "=== EMULATOR WITH TRACER - STEP BY STEP TEST ===" << std::endl;
    std::cout << std::endl;

    // Test program
    std::string program1_str = "[[{.>]-]                ]-]>.{[[";
    std::cout << "Testing program: \"" << program1_str << "\"" << std::endl;
    std::cout << "Program length: " << program1_str.length() << std::endl;
    std::cout << std::endl;

    std::vector<uint8_t> program1(program1_str.begin(), program1_str.end());

    // Create program2 filled with '0' characters
    std::vector<uint8_t> program2(program1.size(), '0');

    // Concatenate programs to create byte tape
    std::vector<uint8_t> byte_tape = program1;
    byte_tape.insert(byte_tape.end(), program2.begin(), program2.end());

    // Initialize tokens (epoch 0)
    std::vector<Token> tape = initialize_tokens_with_epoch(byte_tape, 0);

    std::cout << "=== INITIAL STATE ===" << std::endl;
    print_tape_with_tokens(tape, 0, program1.size(), 0);

    // Run emulation with verbose tracking
    std::cout << "=== STARTING EXECUTION ===" << std::endl;
    std::cout << std::endl;

    int head0_pos = 0;
    int head1_pos = program1.size();
    int pc_pos = 0;
    int max_iter = 100;
    int iteration = 0;

    const std::string instructions = "<>{}-+.,[]";
    int tape_size = tape.size();
    std::string state = "Running";

    while (iteration < max_iter && pc_pos < tape_size) {
        // Get current instruction
        char instruction = static_cast<char>(tape[pc_pos].get_char());

        // Skip non-instruction characters
        if (instructions.find(instruction) == std::string::npos) {
            pc_pos++;
            continue;
        }

        // Print state before executing instruction
        print_execution_state(iteration, head0_pos, head1_pos, pc_pos, tape);

        // Execute instruction (simplified version for tracking)
        switch (instruction) {
            case '<':
                head0_pos = (head0_pos - 1 + tape_size) % tape_size;
                break;
            case '>':
                head0_pos = (head0_pos + 1) % tape_size;
                break;
            case '{':
                head1_pos = (head1_pos - 1 + tape_size) % tape_size;
                break;
            case '}':
                head1_pos = (head1_pos + 1) % tape_size;
                break;
            case '+': {
                uint8_t current = tape[head0_pos].get_char();
                tape[head0_pos].set_char(current + 1);
                break;
            }
            case '-': {
                uint8_t current = tape[head0_pos].get_char();
                tape[head0_pos].set_char(current - 1);
                break;
            }
            case '.': {
                // Copy entire token from head0 to head1
                Token source_token = tape[head0_pos];
                tape[head1_pos] = source_token;
                std::cout << "  >> COPY: Token from pos " << head0_pos << " to pos " << head1_pos << std::endl;
                std::cout << "     Token: epoch=" << source_token.get_epoch()
                          << ", origpos=" << source_token.get_position()
                          << ", char=" << static_cast<int>(source_token.get_char()) << std::endl;
                break;
            }
            case ',': {
                // Copy entire token from head1 to head0
                Token source_token = tape[head1_pos];
                tape[head0_pos] = source_token;
                std::cout << "  >> COPY: Token from pos " << head1_pos << " to pos " << head0_pos << std::endl;
                std::cout << "     Token: epoch=" << source_token.get_epoch()
                          << ", origpos=" << source_token.get_position()
                          << ", char=" << static_cast<int>(source_token.get_char()) << std::endl;
                break;
            }
            case '[': {
                uint8_t value = tape[head0_pos].get_char();
                if (value == 0) {
                    // Jump forward to matching ]
                    int depth = 1;
                    for (int i = pc_pos + 1; i < tape_size; i++) {
                        char c = static_cast<char>(tape[i].get_char());
                        if (c == '[') depth++;
                        else if (c == ']') depth--;
                        if (depth == 0) {
                            pc_pos = i;
                            std::cout << "  >> JUMP FORWARD to position " << pc_pos << std::endl;
                            break;
                        }
                    }
                }
                break;
            }
            case ']': {
                uint8_t value = tape[head0_pos].get_char();
                if (value != 0) {
                    // Jump backward to matching [
                    int depth = 1;
                    for (int i = pc_pos - 1; i >= 0; i--) {
                        char c = static_cast<char>(tape[i].get_char());
                        if (c == ']') depth++;
                        else if (c == '[') depth--;
                        if (depth == 0) {
                            pc_pos = i;
                            std::cout << "  >> JUMP BACKWARD to position " << pc_pos << std::endl;
                            break;
                        }
                    }
                }
                break;
            }
        }

        pc_pos++;
        iteration++;

        // Stop if we've exceeded tape size
        if (pc_pos >= tape_size) {
            state = "Finished";
            break;
        }
    }

    std::cout << "=== EXECUTION COMPLETE ===" << std::endl;
    std::cout << "State: " << state << std::endl;
    std::cout << "Total iterations: " << iteration << std::endl;
    std::cout << std::endl;

    std::cout << "=== FINAL STATE ===" << std::endl;
    print_tape_with_tokens(tape, head0_pos, head1_pos, pc_pos);

    // Show which tokens have been copied
    std::cout << "=== TOKEN LINEAGE ANALYSIS ===" << std::endl;
    std::cout << "Tokens that originated from the first program (pos 0-31):" << std::endl;
    for (size_t i = 0; i < tape.size(); i++) {
        const Token& t = tape[i];
        if (t.get_position() < 32) {
            std::cout << "Position " << i << ": originated from position "
                      << t.get_position() << " in epoch " << t.get_epoch()
                      << ", char=" << static_cast<int>(t.get_char());
            if (t.get_char() >= 32 && t.get_char() <= 126) {
                std::cout << " ('" << static_cast<char>(t.get_char()) << "')";
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
