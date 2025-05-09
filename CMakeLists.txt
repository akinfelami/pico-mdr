# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(4760FinalProject C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

# name anything you want
add_executable(4760FinalProject)

# must match with pio filename and executable name from above
pico_generate_pio_header(4760FinalProject ${CMAKE_CURRENT_LIST_DIR}/hsync.pio)
pico_generate_pio_header(4760FinalProject ${CMAKE_CURRENT_LIST_DIR}/vsync.pio)
pico_generate_pio_header(4760FinalProject ${CMAKE_CURRENT_LIST_DIR}/rgb.pio)

# must match with executable name and source file names
target_sources(4760FinalProject PRIVATE
	vga16_graphics.c 
	main.c
	game_state.c
)

# must match with executable name
target_link_libraries(4760FinalProject
	pico_stdlib 
	pico_bootsel_via_double_reset 
	hardware_pio 
	hardware_dma 
	hardware_adc 
	hardware_sync
	hardware_irq
	pico_multicore
	hardware_spi
	hardware_clocks
)

# must match with executable name
pico_add_extra_outputs(4760FinalProject)

add_compile_options(-Ofast)
