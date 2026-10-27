cmake_minimum_required(VERSION 3.13)
project(ps2_guitar_adapter_tests C)
set(CMAKE_C_STANDARD 11)

add_executable(test_ps2_protocol_bytes
    test_ps2_protocol_bytes.c
    ../../src/ps2_protocol.c
)
target_include_directories(test_ps2_protocol_bytes PRIVATE ../../src)

add_executable(test_shared_state
    test_shared_state.c
    ../../src/shared_state.c
)
target_include_directories(test_shared_state PRIVATE ../../src)

add_executable(test_guitar_input
    test_guitar_input.c
    ../../src/guitar_input.c
)
target_include_directories(test_guitar_input PRIVATE ../../src)

enable_testing()
add_test(NAME ps2_protocol_bytes COMMAND test_ps2_protocol_bytes)
add_test(NAME shared_state COMMAND test_shared_state)
add_test(NAME guitar_input COMMAND test_guitar_input)
