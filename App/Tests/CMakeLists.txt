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
# HOST_TEST=1 guards the cmsis_os.h include in app_common.h (pulled via expr_util.h).

add_executable(test_expr_util
    test_expr_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_util.c
)

target_include_directories(test_expr_util PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad
)

target_compile_options(test_expr_util PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_expr_util PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_expr_util PRIVATE --coverage)
endif()

# --- test_expr_buffer ---
# ExprBuffer_t stateful wrapper; no LVGL/HAL/RTOS dependencies.

add_executable(test_expr_buffer
    test_expr_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_buffer.c
)

target_include_directories(test_expr_buffer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad
)

target_compile_options(test_expr_buffer PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_expr_buffer PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_expr_buffer PRIVATE --coverage)
endif()

# --- test_persist_roundtrip ---
# PersistBlock_t serialization/validation; HOST_TEST guards out HAL code.

add_executable(test_persist_roundtrip
    test_persist_roundtrip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/persist.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

# persist.h now includes app_common.h which pulls in keypad_map.h
target_include_directories(test_persist_roundtrip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad
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
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_util.c
)

# Tests/ directory must be on the path so prgm_exec.c finds prgm_exec_test_stubs.h
target_include_directories(test_prgm_exec PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(test_prgm_exec PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_prgm_exec PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_prgm_exec PRIVATE --coverage)
endif()

# --- test_normal_mode ---
# handle_normal_mode() and sub-handler host tests; HOST_TEST guards out LVGL/HAL/RTOS.

add_executable(test_normal_mode
    test_normal_mode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calculator_core.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/ui_mode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/ui_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/expr_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/prgm_exec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad/keypad_map.c
)

# Tests/ directory (stubs header) and HW/Keypad (keypad_map.h → keypad.h) on path
target_include_directories(test_normal_mode PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad
)

target_compile_options(test_normal_mode PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_normal_mode PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_normal_mode PRIVATE --coverage)
endif()

# --- test_param ---
# Parametric equation preparation and evaluation; no LVGL/HAL/RTOS dependencies.

add_executable(test_param
    test_param.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

target_compile_options(test_param PRIVATE -Wall -Wextra)

if(COVERAGE)
    target_compile_options(test_param PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_param PRIVATE --coverage)
endif()

# --- test_stat ---
# Pure statistical math layer; no LVGL/HAL/RTOS dependencies.

add_executable(test_stat
    test_stat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_stat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

# HW/Keypad path needed for keypad_map.h (pulled in via app_common.h → calc_stat.h)
target_include_directories(test_stat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad
)

target_compile_options(test_stat PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_stat PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_stat PRIVATE --coverage)
endif()

# --- test_yvars ---
# Y-VARS calc_engine integration: Calc_RegisterYEquations, Y₁–Y₄ tokenize/eval.
# Pure engine test — no LVGL/HAL/RTOS dependencies.

add_executable(test_yvars
    test_yvars.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/calc_engine.c
)

target_compile_options(test_yvars PRIVATE -Wall -Wextra)

if(COVERAGE)
    target_compile_options(test_yvars PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_yvars PRIVATE --coverage)
endif()

# --- test_menu_state ---
# Shared MenuState_t navigation helpers; no LVGL/HAL/RTOS dependencies.

add_executable(test_menu_state
    test_menu_state.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../Src/menu_state.c
)

target_include_directories(test_menu_state PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../HW/Keypad
)

target_compile_options(test_menu_state PRIVATE -Wall -Wextra -DHOST_TEST=1)

if(COVERAGE)
    target_compile_options(test_menu_state PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_options(test_menu_state PRIVATE --coverage)
endif()

# CTest integration — run with: ctest --test-dir build/tests
enable_testing()
add_test(NAME calc_engine       COMMAND test_calc_engine)
add_test(NAME expr_util         COMMAND test_expr_util)
add_test(NAME expr_buffer       COMMAND test_expr_buffer)
add_test(NAME persist_roundtrip COMMAND test_persist_roundtrip)
add_test(NAME prgm_exec         COMMAND test_prgm_exec)
add_test(NAME normal_mode       COMMAND test_normal_mode)
add_test(NAME param             COMMAND test_param)
add_test(NAME stat              COMMAND test_stat)
add_test(NAME yvars             COMMAND test_yvars)
add_test(NAME menu_state        COMMAND test_menu_state)
