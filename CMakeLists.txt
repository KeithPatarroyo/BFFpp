cmake_minimum_required(VERSION 3.10)
project(BFFpp VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add include directory
include_directories(include)

# Find Brotli library
find_package(PkgConfig REQUIRED)
pkg_check_modules(BROTLI REQUIRED libbrotlienc)

# Source files
set(SOURCES
    src/main.cpp
    src/emulator.cpp
    src/utils.cpp
    src/metrics.cpp
    src/config.cpp
)

# Create executable
add_executable(bffpp ${SOURCES})

# Link libraries
target_link_libraries(bffpp ${BROTLI_LIBRARIES} pthread)
target_include_directories(bffpp PRIVATE ${BROTLI_INCLUDE_DIRS})
target_compile_options(bffpp PRIVATE ${BROTLI_CFLAGS_OTHER})

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bffpp PRIVATE -Wall -Wextra -O3)
endif()

# Test executable
add_executable(test_emulator
    src/test_emulator.cpp
    src/emulator.cpp
    src/utils.cpp
)

# Link libraries for test
target_link_libraries(test_emulator pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_emulator PRIVATE -Wall -Wextra -O3)
endif()

# Test executable for tracer emulator
add_executable(test_emulator_w_tracer
    src/test_emulator_w_tracer.cpp
    src/emulator_w_tracer.cpp
)

# Link libraries for tracer test
target_link_libraries(test_emulator_w_tracer pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_emulator_w_tracer PRIVATE -Wall -Wextra -O3)
endif()

# Verbose test executable for tracer emulator
add_executable(test_emulator_w_tracer_verbose
    src/test_emulator_w_tracer_verbose.cpp
    src/emulator_w_tracer.cpp
    src/utils.cpp
)

# Link libraries for verbose tracer test
target_link_libraries(test_emulator_w_tracer_verbose pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_emulator_w_tracer_verbose PRIVATE -Wall -Wextra -O3)
endif()

# Find OpenSSL for WebSocket
find_package(OpenSSL REQUIRED)

# Grid-based simulation executable
add_executable(bffpp_grid
    src/main_grid.cpp
    src/emulator.cpp
    src/utils.cpp
    src/metrics.cpp
    src/config.cpp
    src/grid.cpp
    src/websocket_server.cpp
)

# Link libraries for grid
target_link_libraries(bffpp_grid ${BROTLI_LIBRARIES} pthread OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(bffpp_grid PRIVATE ${BROTLI_INCLUDE_DIRS})
target_compile_options(bffpp_grid PRIVATE ${BROTLI_CFLAGS_OTHER})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bffpp_grid PRIVATE -Wall -Wextra -O3)
endif()

# Darwin Experiment executable
add_executable(bffpp_darwin
    src/main_darwin.cpp
    src/emulator.cpp
    src/utils.cpp
    src/metrics.cpp
    src/config.cpp
    src/grid.cpp
    src/websocket_server.cpp
)

# Link libraries for darwin
target_link_libraries(bffpp_darwin ${BROTLI_LIBRARIES} pthread OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(bffpp_darwin PRIVATE ${BROTLI_INCLUDE_DIRS})
target_compile_options(bffpp_darwin PRIVATE ${BROTLI_CFLAGS_OTHER})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bffpp_darwin PRIVATE -Wall -Wextra -O3)
endif()

# Grid with tracer executable
add_executable(bffpp_grid_w_tracer
    src/main_grid_w_tracer.cpp
    src/emulator_w_tracer.cpp
    src/utils.cpp
    src/metrics.cpp
    src/config.cpp
    src/grid_w_tracer.cpp
    src/websocket_server.cpp
)

# Link libraries for grid with tracer
target_link_libraries(bffpp_grid_w_tracer ${BROTLI_LIBRARIES} pthread OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(bffpp_grid_w_tracer PRIVATE ${BROTLI_INCLUDE_DIRS})
target_compile_options(bffpp_grid_w_tracer PRIVATE ${BROTLI_CFLAGS_OTHER})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bffpp_grid_w_tracer PRIVATE -Wall -Wextra -O3)
endif()

# Test grid comparison
add_executable(test_grid_comparison
    src/test_grid_comparison.cpp
    src/emulator.cpp
    src/emulator_w_tracer.cpp
    src/utils.cpp
    src/metrics.cpp
    src/config.cpp
    src/grid.cpp
    src/grid_w_tracer.cpp
)

# Link libraries for test
target_link_libraries(test_grid_comparison ${BROTLI_LIBRARIES} pthread)
target_include_directories(test_grid_comparison PRIVATE ${BROTLI_INCLUDE_DIRS})
target_compile_options(test_grid_comparison PRIVATE ${BROTLI_CFLAGS_OTHER})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_grid_comparison PRIVATE -Wall -Wextra -O3)
endif()

# Test RNG debug
add_executable(test_rng_debug
    src/test_rng_debug.cpp
    src/utils.cpp
    src/grid.cpp
    src/grid_w_tracer.cpp
    src/emulator_w_tracer.cpp
)

# Link libraries for RNG debug test
target_link_libraries(test_rng_debug pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_rng_debug PRIVATE -Wall -Wextra -O3)
endif()

# Test epoch by epoch
add_executable(test_epoch_by_epoch
    src/test_epoch_by_epoch.cpp
    src/emulator.cpp
    src/emulator_w_tracer.cpp
    src/utils.cpp
    src/grid.cpp
    src/grid_w_tracer.cpp
)

# Link libraries for epoch by epoch test
target_link_libraries(test_epoch_by_epoch pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_epoch_by_epoch PRIVATE -Wall -Wextra -O3)
endif()

# Test emulator equivalence
add_executable(test_emulator_equivalence
    src/test_emulator_equivalence.cpp
    src/emulator.cpp
    src/emulator_w_tracer.cpp
    src/utils.cpp
)

# Link libraries
target_link_libraries(test_emulator_equivalence pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_emulator_equivalence PRIVATE -Wall -Wextra -O3)
endif()

# Test simple decrement
add_executable(test_simple_decrement
    src/test_simple_decrement.cpp
    src/emulator.cpp
    src/emulator_w_tracer.cpp
    src/utils.cpp
)

# Link libraries
target_link_libraries(test_simple_decrement pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_simple_decrement PRIVATE -Wall -Wextra -O3)
endif()

# Analyze neighborhood HOE
add_executable(analyze_neighborhood_hoe
    src/analyze_neighborhood_hoe.cpp
    src/metrics.cpp
)

# Link libraries
target_link_libraries(analyze_neighborhood_hoe ${BROTLI_LIBRARIES} pthread)
target_include_directories(analyze_neighborhood_hoe PRIVATE ${BROTLI_INCLUDE_DIRS})
target_compile_options(analyze_neighborhood_hoe PRIVATE ${BROTLI_CFLAGS_OTHER})
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(analyze_neighborhood_hoe PRIVATE -Wall -Wextra -O3)
endif()

# Forward pass analysis
add_executable(forward_pass_analysis
    src/forward_pass_analysis.cpp
    src/emulator.cpp
    src/utils.cpp
)

# Link libraries
target_link_libraries(forward_pass_analysis pthread)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(forward_pass_analysis PRIVATE -Wall -Wextra -O3)
endif()
