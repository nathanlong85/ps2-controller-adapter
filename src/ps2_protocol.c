#include "ps2_protocol.h"

void ps2_build_digital_response(uint8_t button_byte0, uint8_t button_byte1, uint8_t out[5]) {
    out[0] = 0xFF;
    out[1] = 0x41;
    out[2] = 0x5A;
    out[3] = button_byte0;
    out[4] = button_byte1;
}

void ps2_build_analog_response(uint8_t button_byte0, uint8_t button_byte1,
                                uint8_t rx, uint8_t ry, uint8_t lx, uint8_t ly,
                                uint8_t out[9]) {
    out[0] = 0xFF;
    out[1] = 0x73;
    out[2] = 0x5A;
    out[3] = button_byte0;
    out[4] = button_byte1;
    out[5] = rx;
    out[6] = ry;
    out[7] = lx;
    out[8] = ly;
}
