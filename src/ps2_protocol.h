#ifndef PS2_PROTOCOL_H
#define PS2_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PS2_MODE_DIGITAL = 0,
    PS2_MODE_ANALOG = 1,
} ps2_mode_t;

// Assembles the 5-byte digital-controller poll response:
// {0xFF, 0x41, 0x5A, button_byte0, button_byte1}
void ps2_build_digital_response(uint8_t button_byte0, uint8_t button_byte1, uint8_t out[5]);

void ps2_build_analog_response(uint8_t button_byte0, uint8_t button_byte1,
                                uint8_t rx, uint8_t ry, uint8_t lx, uint8_t ly,
                                uint8_t out[9]);

// Config-mode sub-command IDs (byte index 1 of the command frame).
#define PS2_CMD_POLL 0x42
#define PS2_CMD_CONFIG_MODE 0x43
#define PS2_CMD_SET_MODE 0x44
#define PS2_CMD_QUERY_45 0x45
#define PS2_CMD_QUERY_46 0x46
#define PS2_CMD_QUERY_47 0x47
#define PS2_CMD_QUERY_4D 0x4D
#define PS2_CMD_SET_FORMAT 0x4F

// Persistent PS2-side protocol state, owned and mutated only by core1 (the
// side that observes the console's command stream). Not part of
// shared_state_t: that struct's single-writer seqlock contract assumes
// core0 is the sole writer, and this state is inherently core1's to own.
typedef struct {
    ps2_mode_t mode;
    bool config_mode_active;
} ps2_protocol_state_t;

// mode = PS2_MODE_DIGITAL, config_mode_active = false (power-on default).
void ps2_protocol_state_init(ps2_protocol_state_t *s);

// Response byte index 1 (identification) for the current state: 0xF3 while
// in config mode, else 0x41 (digital) / 0x73 (analog).
uint8_t ps2_identification_byte(const ps2_protocol_state_t *s);

// True if command_id is one of the 0x43-0x4F config sub-commands.
bool ps2_command_is_config_subcommand(uint8_t command_id);

// True if this transaction's response body (bytes 3+) should be the
// well-formed dummy filler rather than a normal digital/analog poll
// response: either the command itself is a config sub-command, or the
// system is currently in config mode (so even a 0x42 poll gets the
// escape-mode response per spec).
bool ps2_should_use_dummy_response(const ps2_protocol_state_t *s, uint8_t command_id);

// Total response length (including the 3-byte header) for this
// transaction: always 9 when ps2_should_use_dummy_response() is true,
// otherwise 5 (digital) or 9 (analog) per the current mode.
uint8_t ps2_response_length(const ps2_protocol_state_t *s, uint8_t command_id);

// Builds the 9-byte dummy response for a config sub-command (or a poll
// received while in config mode): {0xFF, identification_byte, 0x5A, then
// six 0x00 filler bytes}.
void ps2_build_config_dummy_response(uint8_t identification_byte, uint8_t out[9]);

// Applies a received command's effect on persistent state, once the
// command's first data byte (command frame index 3) is known. Only
// PS2_CMD_CONFIG_MODE (data_byte0: 0x01 = enter, else exit) and
// PS2_CMD_SET_MODE (data_byte0: 0x01 = analog, else digital) have any
// effect; all other command IDs, including PS2_CMD_SET_FORMAT, are no-ops
// (0x4F's custom format request is intentionally never honored).
void ps2_apply_command_effect(ps2_protocol_state_t *s, uint8_t command_id, uint8_t data_byte0);

#endif
