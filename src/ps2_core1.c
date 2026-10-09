#include "ps2_core1.h"
#include "ps2_pio.pio.h"
#include "ps2_protocol.h"
#include "shared_state.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"

#define PIN_DATA    5
#define PIN_CLOCK   3
#define PIN_COMMAND 4
#define PIN_ATT     2
#define PIN_ACK     6

void core1_entry(void) {
    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &ps2_slave_program);

    pio_sm_config c = ps2_slave_program_get_default_config(offset);
    sm_config_set_in_pins(&c, PIN_COMMAND);
    sm_config_set_out_pins(&c, PIN_DATA, 1);
    sm_config_set_jmp_pin(&c, PIN_ATT);
    sm_config_set_sideset_pins(&c, PIN_ACK);
    sm_config_set_in_shift(&c, false, true, 8);
    sm_config_set_out_shift(&c, true, true, 8);

    pio_gpio_init(pio, PIN_DATA);
    pio_gpio_init(pio, PIN_ACK);
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_DATA, 1, true);
    gpio_pull_up(PIN_ATT);
    gpio_pull_up(PIN_CLOCK);
    gpio_pull_up(PIN_COMMAND);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    shared_state_t state;
    while (true) {
        shared_state_read(&state);

        uint8_t response[9];
        uint response_len;
        if (state.mode == PS2_MODE_DIGITAL) {
            ps2_build_digital_response(state.button_byte0, state.button_byte1, response);
            response_len = 5;
        } else {
            ps2_build_analog_response(state.button_byte0, state.button_byte1,
                                       state.right_stick_x, state.right_stick_y,
                                       state.left_stick_x, state.left_stick_y, response);
            response_len = 9;
        }

        for (uint i = 0; i < response_len; i++) {
            pio_sm_put_blocking(pio, sm, response[i]);
        }
    }
}
