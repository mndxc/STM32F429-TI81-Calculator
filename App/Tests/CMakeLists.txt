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

# --- test_calc_engine ---

add_executable(test_calc_engine
    test_calc_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

# Enable warnings for tests too
target_compile_options(test_calc_engine PRIVATE -Wall -Wextra)

# Code coverage — enable with: cmake -DCOVERAGE=ON ...
# Then run tests, then: gcovr -r . --filter App/Src/calc_engine.c build/tests
option(COVERAGE "Enable gcov branch coverage instrumentation" OFF)
if(COVERAGE)
    target_compile_options(test_calc_engine PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_calc_engine PRIVATE --coverage)
endif()

# --- test_expr_util ---
# Pure expression-buffer utility functions; no LVGL/HAL/RTOS dependencies.

add_executable(test_expr_util
    test_expr_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_util.c
)

target_compile_options(test_expr_util PRIVATE -Wall -Wextra)

if(COVERAGE)
    target_compile_options(test_expr_util PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_expr_util PRIVATE --coverage)
endif()

# --- test_persist_roundtrip ---
# PersistBlock_t serialization/validation; HOST_TEST guards out HAL code.

add_executable(test_persist_roundtrip
    test_persist_roundtrip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/persist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

target_compile_options(test_persist_roundtrip PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_persist_roundtrip PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_persist_roundtrip PRIVATE --coverage)
endif()

# --- test_prgm_exec ---
# Program executor host tests; HOST_TEST guards out LVGL/HAL/RTOS code.

add_executable(test_prgm_exec
    test_prgm_exec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/prgm_exec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

# Tests/ directory must be on the path so prgm_exec.c finds prgm_exec_test_stubs.h
target_include_directories(test_prgm_exec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(test_prgm_exec PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_prgm_exec PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_prgm_exec PRIVATE --coverage)
endif()

# CTest integration — run with: ctest --test-dir build/tests
enable_testing()
add_test(NAME calc_engine       COMMAND test_calc_engine)
add_test(NAME expr_util         COMMAND test_expr_util)
add_test(NAME persist_roundtrip COMMAND test_persist_roundtrip)
add_test(NAME prgm_exec         COMMAND test_prgm_exec)
