#include "shared_state.h"
#include <assert.h>
#include <stdio.h>

static void test_init_idle_defaults(void) {
    shared_state_init();
    shared_state_t s;
    shared_state_read(&s);
    assert(s.button_byte0 == 0xFF);
    assert(s.button_byte1 == 0xFF);
    assert(s.right_stick_x == 0x80);
    assert(s.right_stick_y == 0x80);
    assert(s.left_stick_x == 0x80);
    assert(s.left_stick_y == 0x80);
    printf("test_init_idle_defaults: PASS\n");
}

static void test_write_read_round_trip(void) {
    shared_state_init();
    shared_state_t written = {
        .button_byte0 = 0x12,
        .button_byte1 = 0x34,
        .right_stick_x = 0x56,
        .right_stick_y = 0x78,
        .left_stick_x = 0x9A,
        .left_stick_y = 0xBC,
    };
    shared_state_write(&written);
    shared_state_t s;
    shared_state_read(&s);
    assert(s.button_byte0 == 0x12);
    assert(s.button_byte1 == 0x34);
    assert(s.right_stick_x == 0x56);
    assert(s.right_stick_y == 0x78);
    assert(s.left_stick_x == 0x9A);
    assert(s.left_stick_y == 0xBC);
    printf("test_write_read_round_trip: PASS\n");
}

int main(void) {
    test_init_idle_defaults();
    test_write_read_round_trip();
    printf("All tests passed.\n");
    return 0;
}
