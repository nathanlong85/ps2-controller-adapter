# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(ps2_guitar_adapter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico2_w)

pico_sdk_init()

add_executable(ps2_guitar_adapter
    src/main.c
)

target_link_libraries(ps2_guitar_adapter
    pico_stdlib
)

pico_enable_stdio_usb(ps2_guitar_adapter 0)
pico_enable_stdio_uart(ps2_guitar_adapter 1)

pico_add_extra_outputs(ps2_guitar_adapter)

