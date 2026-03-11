cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


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
    Drivers/BSP/Components/Fonts/font24.c
    Drivers/BSP/Components/Fonts/font20.c
    Drivers/BSP/Components/Fonts/font16.c
    Drivers/BSP/Components/Fonts/font12.c
    Drivers/BSP/Components/Fonts/font8.c

    # --- KEYPAD SOURCES ---
    Drivers/Keypad/keypad.c
    Drivers/Keypad/keypad_map.c

    # --- LOGIC SOURCES ---
    Core/Src/calculator_core.c
    Core/Src/calc_engine.c

    # Add all LVGL Source files 
    ${LVGL_SOURCES}
    Middlewares/Third_Party/lv_port_disp.c
    Middlewares/Third_Party/lv_port_indev.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    # --- ADDED BSP PATHS ---
    Drivers/BSP/STM32F429I-Discovery
    Drivers/BSP/Components/ili9341
    Drivers/BSP/Components/Common
    Drivers/BSP/Components/Fonts

    Drivers/Keypad

    ${LVGL_ROOT_DIR}

    Middlewares/Third_Party
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
