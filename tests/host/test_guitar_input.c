#include "guitar_input.h"
#include <assert.h>
#include <stdio.h>

static void test_idle_report(void) {
    guitar_input_t s;
    const uint8_t report[9] = {0x00, 0x00, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    guitar_input_decode(&s, report, sizeof(report));
    assert(!s.fret_green && !s.fret_red && !s.fret_yellow && !s.fret_blue && !s.fret_orange);
    assert(!s.strum_up && !s.strum_down);
    assert(!s.start && !s.select);
    assert(!s.tilt);
    assert(s.whammy == 0x00);
    printf("test_idle_report: PASS\n");
}

static void test_fret_bits(void) {
    guitar_input_t s;
    const uint8_t report[9] = {0x5B, 0x00, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    // 0x5B = bits 0,1,3,4,6 -> green, red, blue, yellow, orange all set
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.fret_green && s.fret_red && s.fret_blue && s.fret_yellow && s.fret_orange);
    printf("test_fret_bits: PASS\n");
}

static void test_strum_up_down(void) {
    guitar_input_t s;
    uint8_t report[9] = {0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.strum_up && !s.strum_down);

    report[2] = 0x04;
    guitar_input_decode(&s, report, sizeof(report));
    assert(!s.strum_up && s.strum_down);
    printf("test_strum_up_down: PASS\n");
}

static void test_start_select(void) {
    guitar_input_t s;
    const uint8_t report[9] = {0x00, 0x0C, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    // byte1 bits 2,3 -> select, start
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.select && s.start);
    printf("test_start_select: PASS\n");
}

static void test_whammy_full_range(void) {
    guitar_input_t s;
    uint8_t report[9] = {0x00, 0x00, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.whammy == 0x00);

    report[7] = 0xFF;
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.whammy == 0xFF);
    printf("test_whammy_full_range: PASS\n");
}

static void test_tilt_hat_value(void) {
    guitar_input_t s;
    const uint8_t report[9] = {0x00, 0x00, 0x06, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00};
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.tilt);
    assert(!s.strum_up && !s.strum_down);
    printf("test_tilt_hat_value: PASS\n");
}

static void test_malformed_short_report_is_idle(void) {
    guitar_input_t s;
    // Pre-seed with non-idle values to confirm decode() actually resets them.
    s.fret_green = true;
    s.start = true;
    s.whammy = 0xFF;
    s.tilt = true;

    const uint8_t short_report[3] = {0xFF, 0xFF, 0xFF};
    guitar_input_decode(&s, short_report, sizeof(short_report));

    assert(!s.fret_green && !s.fret_red && !s.fret_yellow && !s.fret_blue && !s.fret_orange);
    assert(!s.strum_up && !s.strum_down);
    assert(!s.start && !s.select);
    assert(!s.tilt);
    assert(s.whammy == 0x00);
    printf("test_malformed_short_report_is_idle: PASS\n");
}

static void test_set_idle_resets_non_idle_state(void) {
    guitar_input_t s;
    const uint8_t report[9] = {0x5B, 0x0C, 0x06, 0x80, 0x80, 0x80, 0x80, 0xFF, 0x00};
    guitar_input_decode(&s, report, sizeof(report));
    assert(s.fret_green && s.start && s.select && s.tilt && s.whammy == 0xFF);

    guitar_input_set_idle(&s);
    assert(!s.fret_green && !s.fret_red && !s.fret_yellow && !s.fret_blue && !s.fret_orange);
    assert(!s.strum_up && !s.strum_down);
    assert(!s.start && !s.select);
    assert(!s.tilt);
    assert(s.whammy == 0x00);
    printf("test_set_idle_resets_non_idle_state: PASS\n");
}

int main(void) {
    test_idle_report();
    test_fret_bits();
    test_strum_up_down();
    test_start_select();
    test_whammy_full_range();
    test_tilt_hat_value();
    test_malformed_short_report_is_idle();
    test_set_idle_resets_non_idle_state();
    printf("All tests passed.\n");
    return 0;
}
