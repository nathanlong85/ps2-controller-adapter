#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "shared_state.h"
#include "ps2_core1.h"
#include "tusb.h"
#include <stdio.h>

int main(void) {
    stdio_init_all();
    shared_state_init();

    // Start answering PS2 polls immediately; the sleep below only gives a
    // human time to open a serial monitor before the first printf.
    ps2_pio_setup();
    multicore_launch_core1(core1_entry);

    tuh_init(BOARD_TUH_RHPORT);

    sleep_ms(2000);
    printf("PS2 guitar adapter starting\n");

    // Task 5+ will add mapping updates alongside the HID report logging.
    while (true) {
        tuh_task();
    }
}
