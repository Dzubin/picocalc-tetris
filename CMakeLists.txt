# PicoCalc Tetris  (by Thomas Dzubin)
#
# Reuses the drivers from ./picocalc-text-starter-main (lcd, audio, southbridge),
# which is picocalc-text-starter by Blair Leduc, plus the Raspberry Pi Pico SDK.
# Build it the same way you build that starter: with the Raspberry Pi Pico VS
# Code extension, or from a terminal that has PICO_SDK_PATH set (see README.md).

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in the Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(picocalc-tetris C CXX ASM)

pico_sdk_init()

# Location of the picocalc-text-starter drivers we build against
set(STARTER ${CMAKE_CURRENT_LIST_DIR}/picocalc-text-starter-main)

add_executable(picocalc-tetris
    tetris.c
    ${STARTER}/drivers/lcd.c
    ${STARTER}/drivers/audio.c
    ${STARTER}/drivers/southbridge.c
    ${STARTER}/drivers/font-8x10.c
    ${STARTER}/drivers/font-5x10.c
)

pico_set_program_name(picocalc-tetris "picocalc-tetris")

# The program version comes from the VERSION line in tetris_config.h, so the
# splash screen and the binary always agree. Changing that header reruns CMake.
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/tetris_config.h GAME_VERSION_LINE
     REGEX "^#define VERSION ")
string(REGEX REPLACE "^#define VERSION \"V?([^\"]*)\".*" "\\1" GAME_VERSION "${GAME_VERSION_LINE}")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_LIST_DIR}/tetris_config.h)
pico_set_program_version(picocalc-tetris "${GAME_VERSION}")

# Build the audio PIO program (generates audio.pio.h that drivers/audio.c needs)
pico_generate_pio_header(picocalc-tetris ${STARTER}/drivers/audio.pio)

# No serial console is needed, so USB stays unused.
pico_enable_stdio_uart(picocalc-tetris 0)
pico_enable_stdio_usb(picocalc-tetris 0)

target_compile_options(picocalc-tetris PRIVATE -Wall)

target_include_directories(picocalc-tetris PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${STARTER}/drivers
)

target_link_libraries(picocalc-tetris
    pico_stdlib
    pico_multicore
    hardware_gpio
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_clocks
    hardware_watchdog
)

# The chip is part of every output file name (tetris-RP2040.uf2 /
# tetris-RP2350.uf2, and the matching .elf/.bin/.hex), so a downloaded file
# always says which board it was built for.
if(PICO_PLATFORM MATCHES "^rp2040")
    set(CHIP_NAME RP2040)
elseif(PICO_PLATFORM MATCHES "^rp2350")
    set(CHIP_NAME RP2350)
else()
    set(CHIP_NAME ${PICO_PLATFORM})
endif()
set_target_properties(picocalc-tetris PROPERTIES OUTPUT_NAME tetris-${CHIP_NAME})

pico_add_extra_outputs(picocalc-tetris)

# Copy the finished .uf2 (same name) to the top-level project folder, so the
# RP2040 and RP2350 builds can sit side by side.
add_custom_command(TARGET picocalc-tetris POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:picocalc-tetris>/$<TARGET_FILE_BASE_NAME:picocalc-tetris>.uf2
            ${CMAKE_CURRENT_LIST_DIR}/
)

