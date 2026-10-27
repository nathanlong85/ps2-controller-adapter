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

void ps2_protocol_state_init(ps2_protocol_state_t *s) {
    s->mode = PS2_MODE_DIGITAL;
    s->config_mode_active = false;
}

uint8_t ps2_identification_byte(const ps2_protocol_state_t *s) {
    if (s->config_mode_active) {
        return 0xF3;
    }
    return (s->mode == PS2_MODE_DIGITAL) ? 0x41 : 0x73;
}

bool ps2_command_is_config_subcommand(uint8_t command_id) {
    switch (command_id) {
        case PS2_CMD_CONFIG_MODE:
        case PS2_CMD_SET_MODE:
        case PS2_CMD_QUERY_45:
        case PS2_CMD_QUERY_46:
        case PS2_CMD_QUERY_47:
        case PS2_CMD_QUERY_4D:
        case PS2_CMD_SET_FORMAT:
            return true;
        default:
            return false;
    }
}

bool ps2_should_use_dummy_response(const ps2_protocol_state_t *s, uint8_t command_id) {
    return s->config_mode_active || ps2_command_is_config_subcommand(command_id);
}

uint8_t ps2_response_length(const ps2_protocol_state_t *s, uint8_t command_id) {
    if (ps2_should_use_dummy_response(s, command_id)) {
        return 9;
    }
    return (s->mode == PS2_MODE_DIGITAL) ? 5 : 9;
}

void ps2_build_config_dummy_response(uint8_t identification_byte, uint8_t out[9]) {
    out[0] = 0xFF;
    out[1] = identification_byte;
    out[2] = 0x5A;
    for (int i = 3; i < 9; i++) {
        out[i] = 0x00;
    }
}

void ps2_apply_command_effect(ps2_protocol_state_t *s, uint8_t command_id, uint8_t data_byte0) {
    if (command_id == PS2_CMD_CONFIG_MODE) {
        s->config_mode_active = (data_byte0 == 0x01);
    } else if (command_id == PS2_CMD_SET_MODE) {
        s->mode = (data_byte0 == 0x01) ? PS2_MODE_ANALOG : PS2_MODE_DIGITAL;
    }
    // All other command IDs (0x45/0x46/0x47/0x4D/0x4F) have no persistent
    // effect - in particular, 0x4F's custom poll-format request is
    // intentionally never honored (ps2_response_length always returns the
    // full analog frame length in analog mode, regardless of 0x4F).
}
