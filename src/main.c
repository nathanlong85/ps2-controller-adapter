#include "pico/stdlib.h"
#include <stdio.h>

// GPIO assignments — adjust after Task 1 Step 3 wire verification if needed.
#define PIN_ATT    2
#define PIN_CLOCK  3
#define PIN_COMMAND 4
#define PIN_DATA   5
#define PIN_ACK    6

int main(void) {
    stdio_init_all();

    gpio_init(PIN_ATT);
    gpio_set_dir(PIN_ATT, GPIO_IN);
    gpio_pull_up(PIN_ATT);

    gpio_init(PIN_CLOCK);
    gpio_set_dir(PIN_CLOCK, GPIO_IN);
    gpio_pull_up(PIN_CLOCK);

    gpio_init(PIN_COMMAND);
    gpio_set_dir(PIN_COMMAND, GPIO_IN);
    gpio_pull_up(PIN_COMMAND);

    sleep_ms(2000);
    printf("PS2 wiring smoke test starting\n");

    bool last_att = true;
    uint32_t clock_pulses = 0;
    bool counting = false;

    while (true) {
        bool att = gpio_get(PIN_ATT);

        if (last_att && !att) {
            printf("ATT asserted (low)\n");
            clock_pulses = 0;
            counting = true;
        }
        if (!last_att && att) {
            printf("ATT released (high) - saw %u clock pulses, COMMAND last-bit=%d\n",
                   clock_pulses, gpio_get(PIN_COMMAND));
            counting = false;
        }
        last_att = att;

        if (counting) {
            static bool last_clock = true;
            bool clk = gpio_get(PIN_CLOCK);
            if (last_clock && !clk) {
                clock_pulses++;
            }
            last_clock = clk;
        }

        sleep_us(5);
    }
}
