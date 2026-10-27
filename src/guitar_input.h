#ifndef GUITAR_INPUT_H
#define GUITAR_INPUT_H

#include <stdbool.h>
#include <stdint.h>

// Normalized CRKD guitar input state, decoded from the raw 9-byte HID
// report documented in the progress ledger (Task 5.1-5.3). Byte7 ("Brake"
// usage) is the shortest field this decoder needs, so any report shorter
// than that is treated as malformed.
#define GUITAR_INPUT_MIN_REPORT_LEN 8

typedef struct {
    bool fret_green;
    bool fret_red;
    bool fret_yellow;
    bool fret_blue;
    bool fret_orange;
    bool strum_up;
    bool strum_down;
    bool start;
    bool select;
    uint8_t whammy; // 0x00 rest .. 0xff full depth
    // Hat switch value 6: also fires on the physical CTRL button, which
    // the report format can't distinguish from a genuine tilt.
    bool tilt;
} guitar_input_t;

// Zeroes every field (idle: no frets/strum/start/select, whammy at rest,
// no tilt). Used for malformed reports and on disconnect.
void guitar_input_set_idle(guitar_input_t *state);

// Decodes a raw HID report into state. Reports shorter than
// GUITAR_INPUT_MIN_REPORT_LEN are treated as malformed and decode to idle.
void guitar_input_decode(guitar_input_t *state, const uint8_t *report, uint16_t len);

#endif
