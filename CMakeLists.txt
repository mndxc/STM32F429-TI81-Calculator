cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#
# Note: STM32CubeMX-generated build config is in cmake/stm32cubemx/CMakeLists.txt
# (HAL libraries, FreeRTOS, USB, startup sources, include dirs, linker flags).
# That file is not hand-edited — regenerate via CubeMX if peripheral config changes.
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Increase warning level for app code
add_compile_options(-Wall -Wextra)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME STM32F429-TI81-Calculator)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# 1. Set the path to the lvgl folder
set(LVGL_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/lvgl)

file(GLOB_RECURSE LVGL_SOURCES
    ${LVGL_ROOT_DIR}/src/*.c
)

# --- APP SOURCES ---
set(APP_SOURCES
    App/Fonts/jetbrains_mono_24.c
    App/Fonts/jetbrains_mono_20.c
    App/HW/Keypad/keypad.c
    App/HW/Keypad/keypad_map.c
    App/Src/app_init.c
    App/Src/calculator_core.c
    App/Src/calc_engine.c
    App/Src/expr_util.c
    App/Src/graph.c
    App/Src/graph_draw.c
    App/Src/persist.c
    App/Src/prgm_exec.c
    App/Src/ui_matrix.c
    App/Src/ui_stat.c
    App/Src/calc_stat.c
    App/Src/ui_prgm.c
    App/Src/ui_draw.c
    App/Src/graph_ui.c
    App/Display/lv_port_disp.c
    App/Display/lv_port_indev.c
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    # --- BSP SOURCES ---
    Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery.c
    Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery_lcd.c
    Drivers/BSP/STM32F429I-Discovery/stm32f429i_discovery_sdram.c
    
    # --- COMPONENT SOURCES ---
    Drivers/BSP/Components/ili9341/ili9341.c
    
    # --- FONT SOURCES ---
    Drivers/BSP/Components/Fonts/font24.c   # Required by BSP LCD driver (Font24 symbol)
    
    # All App Source files
    ${APP_SOURCES}

    # Add all LVGL Source files
    ${LVGL_SOURCES}
)

# Apply -Werror to App sources only
set_source_files_properties(${APP_SOURCES} PROPERTIES COMPILE_OPTIONS "-Werror")

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    # --- ADDED BSP PATHS ---
    Drivers/BSP/STM32F429I-Discovery
    Drivers/BSP/Components/ili9341
    Drivers/BSP/Components/Common
    Drivers/BSP/Components/Fonts

    App/Inc
    App/HW/Keypad
    App/Display

    ${LVGL_ROOT_DIR}
    ${LVGL_ROOT_DIR}/..     # needed for #include "lvgl/lvgl.h" in generated font files
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# --- HOST TESTS ---
if(PROJECT_BUILD_TESTS)
    add_subdirectory(App/Tests)
endif()

