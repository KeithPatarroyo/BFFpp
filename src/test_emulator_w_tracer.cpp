#include "emulator_w_tracer.h"
#include <iostream>
#include <iomanip>

void print_tokens(const std::vector<Token>& tokens, const std::string& label) {
    std::cout << "\n" << label << ":\n";
    std::cout << "Pos | Char | Epoch | OrigPos\n";
    std::cout << "----+------+-------+--------\n";

    for (size_t i = 0; i < tokens.size() && i < 20; i++) {
        char c = static_cast<char>(tokens[i].get_char());
        if (c < 32 || c > 126) c = '.';  // Non-printable chars

        std::cout << std::setw(3) << i << " | "
                  << std::setw(4) << static_cast<int>(tokens[i].get_char())
                  << " | "
                  << std::setw(5) << tokens[i].get_epoch()
                  << " | "
                  << std::setw(6) << tokens[i].get_position()
                  << " (" << c << ")"
                  << std::endl;
    }
    if (tokens.size() > 20) {
        std::cout << "... (" << tokens.size() << " total)\n";
    }
}

void test_copy_operation() {
    std::cout << "\n=== TEST 1: Copy Operation (.) ===\n";
    std::cout << "Program: >+++.  (Move head0 right, increment 3 times, copy to head1)\n";

    std::vector<uint8_t> program = {
        '>', '+', '+', '+', '.', 0, 0, 0
    };

    auto tokens = initialize_tokens(program);
    print_tokens(tokens, "Initial tokens");

    auto result = emulate_w_tracer(tokens, 0, 5, 0, 100, 0);

    print_tokens(result.tape, "After execution");

    std::cout << "\nFinal state: " << result.state << std::endl;
    std::cout << "Iterations: " << result.iteration << std::endl;
    std::cout << "Head0 at: " << result.head0_pos << std::endl;
    std::cout << "Head1 at: " << result.head1_pos << std::endl;

    // Check if token was copied
    Token src = result.tape[result.head0_pos];
    Token dst = result.tape[result.head1_pos];

    std::cout << "\nToken at head0 pos " << result.head0_pos << ": "
              << "char=" << static_cast<int>(src.get_char())
              << ", epoch=" << src.get_epoch()
              << ", origpos=" << src.get_position() << std::endl;

    std::cout << "Token at head1 pos " << result.head1_pos << ": "
              << "char=" << static_cast<int>(dst.get_char())
              << ", epoch=" << dst.get_epoch()
              << ", origpos=" << dst.get_position() << std::endl;

    if (src.get_char() == dst.get_char() &&
        src.get_epoch() == dst.get_epoch() &&
        src.get_position() == dst.get_position()) {
        std::cout << "✓ Copy operation preserved token!" << std::endl;
    } else {
        std::cout << "✗ Token mismatch!" << std::endl;
    }
}

void test_increment_decrement() {
    std::cout << "\n=== TEST 2: Increment/Decrement (+/-) ===\n";
    std::cout << "Program: >+++--  (Move right, modify '+' char)\n";

    std::vector<uint8_t> program = {
        '>', '+', '+', '+', '-', '-', 0, 0
    };

    auto tokens = initialize_tokens(program);
    auto result = emulate_w_tracer(tokens, 0, 5, 0, 100, 0);

    // After >, head0 is at position 1
    Token modified = result.tape[1];

    std::cout << "\nToken at position 1 after +++ and --:" << std::endl;
    std::cout << "  char=" << static_cast<int>(modified.get_char())
              << " ('" << static_cast<char>(modified.get_char()) << "')" << std::endl;
    std::cout << "  epoch=" << modified.get_epoch() << std::endl;
    std::cout << "  origpos=" << modified.get_position() << std::endl;

    // Position 1 starts with '+' (43), +3-2 = 44 (',')
    // Origin (epoch, position) should be preserved
    if (modified.get_char() == 44 && modified.get_epoch() == 0 && modified.get_position() == 1) {
        std::cout << "✓ Increment/decrement preserved origin!" << std::endl;
    } else {
        std::cout << "✗ Expected char=44, epoch=0, origpos=1" << std::endl;
    }
}

void test_replication_with_tokens() {
    std::cout << "\n=== TEST 3: Simple Replication with Token Tracking ===\n";
    std::cout << "Program: A simple self-copier\n";

    // Simple program that copies itself
    std::vector<uint8_t> program = {
        '>', '+', '+', '+', '.',  // Program A: write 3 to position 1
        '{', '}', '}', '}', ',',  // Program B: copy from position 1 to position 0
        0, 0
    };

    auto tokens = initialize_tokens_with_epoch(program, 100);  // Start at epoch 100

    print_tokens(tokens, "Initial tokens (epoch 100)");

    auto result = emulate_w_tracer(tokens, 0, 5, 0, 200, 0);

    print_tokens(result.tape, "After execution");

    std::cout << "\nIterations: " << result.iteration << std::endl;
    std::cout << "State: " << result.state << std::endl;

    // Check if we can trace where the copied values came from
    std::cout << "\nToken lineage analysis:\n";
    for (size_t i = 0; i < std::min(size_t(10), result.tape.size()); i++) {
        std::cout << "Pos " << i << ": originated from pos "
                  << result.tape[i].get_position()
                  << " in epoch " << result.tape[i].get_epoch()
                  << " (char=" << static_cast<int>(result.tape[i].get_char()) << ")"
                  << std::endl;
    }
}

int main() {
    std::cout << "=== EMULATOR WITH TRACER TEST ===" << std::endl;
    std::cout << "\nToken format: 64-bit (epoch[40], position[16], char[8])\n";

    test_copy_operation();
    test_increment_decrement();
    test_replication_with_tokens();

    std::cout << "\n=== ALL TESTS COMPLETE ===" << std::endl;

    return 0;
}
