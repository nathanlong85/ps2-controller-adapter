cmake_minimum_required(VERSION 3.13)
project(ps2_guitar_adapter_tests C)
set(CMAKE_C_STANDARD 11)

add_executable(test_ps2_protocol_bytes
    test_ps2_protocol_bytes.c
    ../../src/ps2_protocol.c
)
target_include_directories(test_ps2_protocol_bytes PRIVATE ../../src)

enable_testing()
add_test(NAME ps2_protocol_bytes COMMAND test_ps2_protocol_bytes)
