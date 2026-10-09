#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "shared_state.h"
#include "ps2_core1.h"
#include <stdio.h>

int main(void) {
    stdio_init_all();
    shared_state_init();

    sleep_ms(2000);
    printf("PS2 guitar adapter starting\n");

    multicore_launch_core1(core1_entry);

    // Task 4+ will add tuh_task() and mapping updates here.
    while (true) {
        tight_loop_contents();
    }
}
