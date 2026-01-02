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
