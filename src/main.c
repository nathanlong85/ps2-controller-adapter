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
    bool last_clock = true;
    uint32_t clock_pulses = 0;
    bool counting = false;
    uint32_t idle_print_counter = 0;

    while (true) {
        bool att = gpio_get(PIN_ATT);
        bool clk = gpio_get(PIN_CLOCK);

        if (last_att && !att) {
            // No printf here: the UART print can block for >1ms at the
            // default baud rate, which is longer than an entire transaction
            // (~66us observed on a logic analyzer) — it was swallowing every
            // clock pulse before counting ever saw them. Report everything
            // once, at ATT release, after counting is done.
            clock_pulses = 0;
            counting = true;
        }
        if (!last_att && att) {
            printf("ATT released (high) - saw %u clock pulses, COMMAND last-bit=%d\n",
                   clock_pulses, gpio_get(PIN_COMMAND));
            counting = false;
        }
        last_att = att;

        if (counting && last_clock && !clk) {
            clock_pulses++;
        }
        last_clock = clk;

        if (!counting) {
            if (++idle_print_counter >= 2000000) {
                idle_print_counter = 0;
                printf("idle levels: ATT=%d CLOCK=%d COMMAND=%d\n",
                       att, clk, gpio_get(PIN_COMMAND));
            }
        }
    }
}
