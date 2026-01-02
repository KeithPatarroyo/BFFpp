# BFFpp

C++ Implementation of the "BrainFuck Family" Programming Languages

A high-performance C++ port of [PyBFF](https://github.com/TheDevilWillBeBee/PyBFF), designed to reproduce results from the paper "Computational Life: How Well-formed, Self-replicating Programs Emerge from Simple Interaction" (https://arxiv.org/pdf/2406.19108).

This project simulates a "primordial soup" environment where BrainFuck-family programs evolve and develop self-replication capabilities through random mutation and selection.

## Features

- Fast BrainFuck-family interpreter with dual read/write heads
- Multithreaded simulation using C++ threads
- Shannon entropy and Kolmogorov complexity analysis
- Configurable simulation parameters via YAML files
- ANSI color-coded tape visualization

## Project Structure

```
BFFpp/
├── include/          # Header files
│   ├── emulator.h    # BrainFuck interpreter
│   ├── metrics.h     # Entropy and compression metrics
│   ├── utils.h       # Utility functions
│   └── config.h      # Configuration parser
├── src/              # Source files
│   ├── main.cpp      # Main simulation loop
│   ├── emulator.cpp
│   ├── metrics.cpp
│   ├── utils.cpp
│   └── config.cpp
├── configs/          # Configuration files
│   ├── base_config.yaml    # Standard configuration (240-250 epochs)
│   └── small_config.yaml   # Quick test configuration
├── data/             # Output data (generated during runs)
└── CMakeLists.txt    # Build configuration
```

## Dependencies

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.10 or higher
- Brotli compression library
- pthread (usually included with your compiler)

### Installing Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libbrotli-dev pkg-config
```

**Fedora/RHEL:**
```bash
sudo dnf install gcc-c++ cmake brotli-devel pkgconfig
```

**macOS (with Homebrew):**
```bash
brew install cmake brotli pkg-config
```

## Building

```bash
# Create build directory
mkdir build
cd build

# Configure
cmake ..

# Build
make

# The executable 'bffpp' will be created in the build directory
```

## Usage

Run the simulation with a configuration file:

```bash
./build/bffpp --config configs/base_config.yaml
```

Or use the default configuration:

```bash
./build/bffpp --config configs/small_config.yaml
```

## Configuration Parameters

Configuration files (YAML format) support the following parameters:

- `random_seed`: Random seed for reproducibility
- `soup_size`: Number of programs in the soup (must be even)
- `program_size`: Size of each program in bytes
- `epochs`: Number of simulation epochs to run
- `mutation_rate`: Probability of mutation per byte (0.0 to 1.0)
- `read_head_position`: Initial position of read head
- `write_head_position`: Initial position of write head
- `eval_interval`: Print statistics every N epochs
- `num_print_programs`: Number of programs to print when HOE > 1.0

## Instruction Set

The BrainFuck Family language uses the following instructions:

- `<` : Move read head left
- `>` : Move read head right
- `{` : Move write head left
- `}` : Move write head right
- `-` : Decrement value at read head
- `+` : Increment value at read head
- `.` : Copy from read head to write head
- `,` : Copy from write head to read head
- `[` : Jump forward to matching `]` if value at read head is 0
- `]` : Jump backward to matching `[` if value at read head is not 0

## Expected Results

- **base_config.yaml**: Self-replicating programs emerge around epoch 240-250
- **small_config.yaml**: Faster testing configuration, emergence around epoch 13,900-14,000

When Higher Order Entropy (HOE) exceeds 1.0, self-replicating programs have likely emerged.

## Performance Notes

This C++ implementation uses:
- Native multithreading for parallel program execution
- Efficient Brotli compression for Kolmogorov complexity estimation
- Optimized data structures (std::vector<uint8_t>)
- Compiler optimizations (-O3)

Expected performance improvement over Python version: 10-50x depending on configuration.

## License

This project is a C++ port of PyBFF. Please refer to the original [PyBFF repository](https://github.com/TheDevilWillBeBee/PyBFF) for licensing information.

## References

- Original Paper: "Computational Life: How Well-formed, Self-replicating Programs Emerge from Simple Interaction" (https://arxiv.org/pdf/2406.19108)
- Python Implementation: [PyBFF](https://github.com/TheDevilWillBeBee/PyBFF)