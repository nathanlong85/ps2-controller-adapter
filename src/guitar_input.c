#include "guitar_input.h"
#include <string.h>

#define HAT_MASK 0x0F
#define HAT_STRUM_UP 0
#define HAT_STRUM_DOWN 4
#define HAT_TILT 6

void guitar_input_set_idle(guitar_input_t *state) {
    memset(state, 0, sizeof(*state));
}

void guitar_input_decode(guitar_input_t *state, const uint8_t *report, uint16_t len) {
    if (len < GUITAR_INPUT_MIN_REPORT_LEN) {
        guitar_input_set_idle(state);
        return;
    }

    uint8_t byte0 = report[0];
    uint8_t byte1 = report[1];
    uint8_t hat = report[2] & HAT_MASK;

    state->fret_green = (byte0 & (1 << 0)) != 0;
    state->fret_red = (byte0 & (1 << 1)) != 0;
    state->fret_blue = (byte0 & (1 << 3)) != 0;
    state->fret_yellow = (byte0 & (1 << 4)) != 0;
    state->fret_orange = (byte0 & (1 << 6)) != 0;

    state->select = (byte1 & (1 << 2)) != 0;
    state->start = (byte1 & (1 << 3)) != 0;

    state->strum_up = (hat == HAT_STRUM_UP);
    state->strum_down = (hat == HAT_STRUM_DOWN);
    state->tilt = (hat == HAT_TILT);

    state->whammy = report[7];
}
