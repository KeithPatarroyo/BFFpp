#ifndef EMULATOR_H
#define EMULATOR_H

#include <vector>
#include <cstdint>
#include <string>

struct EmulatorResult {
    std::vector<uint8_t> tape;
    std::string state;
    int iteration;
    int skipped;
};

EmulatorResult emulate(
    std::vector<uint8_t> tape,
    int head0_pos = 0,
    int head1_pos = 0,
    int pc_pos = 0,
    int max_iter = 8192,
    int verbose = 0
);

#endif // EMULATOR_H
