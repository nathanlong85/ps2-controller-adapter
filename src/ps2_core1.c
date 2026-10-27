#include "ps2_core1.h"
#include "ps2_pio.pio.h"
#include "ps2_protocol.h"
#include "shared_state.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include <assert.h>

#define PIN_DATA    5
#define PIN_CLOCK   3
#define PIN_COMMAND 4
#define PIN_ATT     2
#define PIN_ACK     6

// The PIO program waits on CLOCK/ATT by absolute GPIO number.
static_assert(PIN_ATT == ps2_slave_PIN_ATT, "PIN_ATT must match ps2_pio.pio");
static_assert(PIN_CLOCK == ps2_slave_PIN_CLOCK, "PIN_CLOCK must match ps2_pio.pio");

// TX FIFO word: bits 7:0 = byte, inverted (DATA is open-drain: the program
// shifts out pindirs, 1 = pull low), bit 8 = ACK after this byte (see
// ps2_pio.pio). Bit 8 is a control bit and is never inverted.
#define TX_ACK_FLAG (1u << 8)
#define TX_DATA(b)  ((uint32_t)(uint8_t)~(b))

// Byte 0 of every response is 0xFF (sent while the console sends 0x01).
#define TX_FIRST_WORD (TX_DATA(0xFFu) | TX_ACK_FLAG)

// Puts the SM back at entry_point (waiting for ATT low) with empty FIFOs,
// ACK and DATA both released to Hi-Z with their output values latched low
// (both are open-drain: the program only toggles their pindirs), then
// preloads byte 0 so it is ready the instant ATT falls. Takes no SDK locks.
static void ps2_sm_reset(PIO pio, uint sm, uint offset) {
    const uint32_t mask = (1u << PIN_DATA) | (1u << PIN_ACK);

    pio_sm_set_enabled(pio, sm, false);
    pio_sm_clear_fifos(pio, sm);
    pio_sm_restart(pio, sm);
    pio_sm_set_pins_with_mask(pio, sm, 0, mask);
    pio_sm_set_pindirs_with_mask(pio, sm, 0, mask);
    pio_sm_exec(pio, sm, pio_encode_jmp(offset + ps2_slave_offset_entry_point));
    pio_sm_put(pio, sm, TX_FIRST_WORD);
    pio_sm_set_enabled(pio, sm, true);
}

// Written once by ps2_pio_setup() on core0 before core1 is launched.
static PIO ps2_pio;
static uint ps2_sm;
static uint ps2_offset;

// Pops one received COMMAND byte from the RX FIFO (bits 31:24 of the word,
// per ps2_pio.pio), blocking only until it arrives or ATT rises. The CPU
// runs orders of magnitude faster than the ~32-64us/byte SPI clock, so by
// the time core1's C code reaches these reads the bytes have almost always
// already arrived; the ATT check is a safety net if the console aborts
// mid-transaction, not the expected path.
static bool ps2_rx_read_byte(PIO pio, uint sm, uint8_t *out) {
    while (pio_sm_is_rx_fifo_empty(pio, sm)) {
        if (gpio_get(PIN_ATT)) {
            return false;
        }
    }
    *out = (uint8_t)(pio_sm_get(pio, sm) >> 24);
    return true;
}

void ps2_pio_setup(void) {
    PIO pio = pio0;
    uint sm = 0;
    // Claim SM 0 so a later PIO user (e.g. the CYW43 driver) can't take it.
    pio_sm_claim(pio, sm);
    uint offset = pio_add_program(pio, &ps2_slave_program);

    pio_sm_config c = ps2_slave_program_get_default_config(offset);
    sm_config_set_in_pins(&c, PIN_COMMAND);
    sm_config_set_out_pins(&c, PIN_DATA, 1);
    sm_config_set_jmp_pin(&c, PIN_ATT);
    sm_config_set_sideset_pins(&c, PIN_ACK);
    // LSB first in both directions; explicit push/pull in the program.
    sm_config_set_in_shift(&c, true, false, 32);
    sm_config_set_out_shift(&c, true, false, 32);

    pio_gpio_init(pio, PIN_DATA);
    pio_gpio_init(pio, PIN_ACK);
    // gpio_init (via gpio_set_function) enables the input buffer and, on
    // RP2350, clears pad isolation; gpio_pull_up alone does neither. PIO
    // reads these pins' inputs regardless of their function select.
    gpio_init(PIN_ATT);
    gpio_init(PIN_CLOCK);
    gpio_init(PIN_COMMAND);
    gpio_pull_up(PIN_ATT);
    gpio_pull_up(PIN_CLOCK);
    gpio_pull_up(PIN_COMMAND);
    // DATA is open-drain (Hi-Z = logic high) but, unlike ACK, nothing on
    // this test console was found pulling it up externally: the PIO
    // program's Hi-Z state floated low instead of high. A weak internal
    // pull-up fixes this and is harmless even if a memory card is also
    // sharing this line with its own driver.
    gpio_pull_up(PIN_DATA);

    pio_sm_init(pio, sm, offset, &c);
    ps2_sm_reset(pio, sm, offset);

    ps2_pio = pio;
    ps2_sm = sm;
    ps2_offset = offset;
}

void core1_entry(void) {
    PIO pio = ps2_pio;
    uint sm = ps2_sm;
    uint offset = ps2_offset;

    ps2_protocol_state_t protocol_state;
    ps2_protocol_state_init(&protocol_state);

    shared_state_t state;
    uint8_t response[9];
    while (true) {
        while (gpio_get(PIN_ATT)) {
            tight_loop_contents();
        }

        // The SM is already shifting out the preloaded byte 0 (>=16us per
        // byte at 500kHz, ~32us at the console's typical 250kHz). Drain the
        // matching RX bytes - index 0 (always 0x01, discarded) and index 1
        // (the command ID) - before deciding how to build the rest of the
        // response; see ps2_rx_read_byte's comment for why this essentially
        // never blocks in practice. If the console aborts before the
        // command ID arrives, fail safe to a normal poll using current
        // state rather than hanging or sending garbage.
        uint8_t discard;
        uint8_t command_id = PS2_CMD_POLL;
        bool have_command_id =
            ps2_rx_read_byte(pio, sm, &discard) && ps2_rx_read_byte(pio, sm, &command_id);
        if (!have_command_id) {
            command_id = PS2_CMD_POLL;
        }

        // Response content is built from protocol_state as of the START of
        // this transaction: a config command's effect (entering/exiting
        // config mode, switching digital/analog) is only visible starting
        // with the *next* poll, per spec.
        ps2_protocol_state_t state_for_response = protocol_state;
        shared_state_read(&state);

        uint response_len = ps2_response_length(&state_for_response, command_id);
        if (ps2_should_use_dummy_response(&state_for_response, command_id)) {
            ps2_build_config_dummy_response(ps2_identification_byte(&state_for_response), response);
        } else if (state_for_response.mode == PS2_MODE_DIGITAL) {
            ps2_build_digital_response(state.button_byte0, state.button_byte1, response);
        } else {
            ps2_build_analog_response(state.button_byte0, state.button_byte1,
                                       state.right_stick_x, state.right_stick_y,
                                       state.left_stick_x, state.left_stick_y, response);
        }

        for (uint i = 1; i < response_len; i++) {
            uint32_t word = TX_DATA(response[i]);
            if (i < response_len - 1) {
                word |= TX_ACK_FLAG;
            }
            bool aborted = false;
            while (pio_sm_is_tx_fifo_full(pio, sm)) {
                if (gpio_get(PIN_ATT)) {
                    aborted = true;
                    break;
                }
            }
            if (aborted) {
                break;
            }
            pio_sm_put(pio, sm, word);
        }

        // Command byte index 3 (the first data byte) only matters for
        // PS2_CMD_CONFIG_MODE/PS2_CMD_SET_MODE, and only takes effect for
        // the *next* transaction, so reading it any time before the next
        // ATT fall is fine - no need to race the TX loop above for it.
        if (have_command_id &&
            (command_id == PS2_CMD_CONFIG_MODE || command_id == PS2_CMD_SET_MODE)) {
            uint8_t data_byte0;
            if (ps2_rx_read_byte(pio, sm, &discard) && ps2_rx_read_byte(pio, sm, &data_byte0)) {
                ps2_apply_command_effect(&protocol_state, command_id, data_byte0);
            }
        }

        while (!gpio_get(PIN_ATT)) {
            tight_loop_contents();
        }
        ps2_sm_reset(pio, sm, offset);
    }
}
