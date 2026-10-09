#ifndef PS2_PROTOCOL_H
#define PS2_PROTOCOL_H

#include <stdint.h>

// Assembles the 5-byte digital-controller poll response:
// {0xFF, 0x41, 0x5A, button_byte0, button_byte1}
void ps2_build_digital_response(uint8_t button_byte0, uint8_t button_byte1, uint8_t out[5]);

#endif
