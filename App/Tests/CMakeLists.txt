cmake_minimum_required(VERSION 3.22)
project(Neo81Tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add math library
link_libraries(m)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../Inc
)

# Test executable
add_executable(test_calc_engine
    test_calc_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

# Enable warnings for tests too
target_compile_options(test_calc_engine PRIVATE -Wall -Wextra)
