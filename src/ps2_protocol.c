#include "ps2_protocol.h"

void ps2_build_digital_response(uint8_t button_byte0, uint8_t button_byte1, uint8_t out[5]) {
    out[0] = 0xFF;
    out[1] = 0x41;
    out[2] = 0x5A;
    out[3] = button_byte0;
    out[4] = button_byte1;
}
