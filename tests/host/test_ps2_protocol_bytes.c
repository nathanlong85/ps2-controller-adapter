#include "ps2_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_digital_response_idle(void) {
    uint8_t out[5];
    ps2_build_digital_response(0xFF, 0xFF, out);
    uint8_t expected[5] = {0xFF, 0x41, 0x5A, 0xFF, 0xFF};
    assert(memcmp(out, expected, 5) == 0);
    printf("test_digital_response_idle: PASS\n");
}

static void test_digital_response_button_pressed(void) {
    uint8_t out[5];
    ps2_build_digital_response(0xFE, 0xFF, out); // bit 0 of byte0 pressed
    uint8_t expected[5] = {0xFF, 0x41, 0x5A, 0xFE, 0xFF};
    assert(memcmp(out, expected, 5) == 0);
    printf("test_digital_response_button_pressed: PASS\n");
}

int main(void) {
    test_digital_response_idle();
    test_digital_response_button_pressed();
    printf("All tests passed.\n");
    return 0;
}
