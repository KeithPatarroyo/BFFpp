/*
 * Brain Fuck Family Emulator
 * An attempt to reproduce the results from: https://arxiv.org/pdf/2406.19108
 *
 * Instruction set:
 * head0: read head
 * head1: write head
 * jump changes to position of the instruction head
 *
 *     < head0 = head0 - 1
 *     > head0 = head0 + 1
 *     { head1 = head1 - 1
 *     } head1 = head1 + 1
 *     - tape[head0] = tape[head0] - 1
 *     + tape[head0] = tape[head0] + 1
 *     . tape[head1] = tape[head0]
 *     , tape[head0] = tape[head1]
 *     [ if (tape[head0] == 0): jump forwards to matching ] command.
 *     ] if (tape[head0] != 0): jump backwards to matching [ command.
 */

#include "emulator.h"
#include "utils.h"
#include <iostream>

EmulatorResult emulate(
    std::vector<uint8_t> tape,
    int head0_pos,
    int head1_pos,
    int pc_pos,
    int max_iter,
    int verbose
) {
    const std::string instructions = "<>{}-+.,[]";
    const uint8_t zero = '0';
    const uint8_t open_bracket = '[';
    const uint8_t close_bracket = ']';

    int tape_size = tape.size();
    int iteration = 0;
    int skipped = 0;
    std::string state = "Terminated";

    while (iteration < max_iter) {
        uint8_t instr = tape[pc_pos];

        if (instr == '<') {
            head0_pos = (head0_pos - 1 + tape_size) % tape_size;
        }
        else if (instr == '>') {
            head0_pos = (head0_pos + 1) % tape_size;
        }
        else if (instr == '{') {
            head1_pos = (head1_pos - 1 + tape_size) % tape_size;
        }
        else if (instr == '}') {
            head1_pos = (head1_pos + 1) % tape_size;
        }
        else if (instr == '-') {
            tape[head0_pos] = (tape[head0_pos] - 1) % 256;
        }
        else if (instr == '+') {
            tape[head0_pos] = (tape[head0_pos] + 1) % 256;
        }
        else if (instr == '.') {
            tape[head1_pos] = tape[head0_pos];
        }
        else if (instr == ',') {
            tape[head0_pos] = tape[head1_pos];
        }
        else if (instr == '[') {
            if (tape[head0_pos] == zero) {
                int diff = 1;
                for (int i = pc_pos + 1; i < tape_size; i++) {
                    if (tape[i] == open_bracket) {
                        diff++;
                    } else if (tape[i] == close_bracket) {
                        diff--;
                    }

                    if (diff == 0) {
                        pc_pos = i;
                        break;
                    }
                }

                if (diff != 0) {
                    state = "Error, Unmatched [";
                    break;
                }
            }
        }
        else if (instr == ']') {
            if (tape[head0_pos] != zero) {
                int diff = 1;
                for (int i = pc_pos - 1; i >= 0; i--) {
                    if (tape[i] == close_bracket) {
                        diff++;
                    } else if (tape[i] == open_bracket) {
                        diff--;
                    }

                    if (diff == 0) {
                        pc_pos = i;
                        break;
                    }
                }

                if (diff != 0) {
                    state = "Error, Unmatched ]";
                    break;
                }
            }
        }
        else {
            skipped++;
        }

        if (verbose > 0) {
            std::cout << "Iteration: " << iteration << "\t\t";
            print_tape(tape, head0_pos, head1_pos, pc_pos, false);
        }

        iteration++;
        pc_pos = pc_pos + 1;
        if (pc_pos >= tape_size) {
            state = "Finished";
            break;
        }
    }

    return EmulatorResult{tape, state, iteration, skipped};
}
