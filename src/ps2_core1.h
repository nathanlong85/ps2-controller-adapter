#ifndef PS2_CORE1_H
#define PS2_CORE1_H

// Loads the PS2 PIO program and starts its state machine. Must run on core0
// before multicore_launch_core1(core1_entry): pio_add_program takes the SDK
// hardware-claim lock, which core1 must never touch.
void ps2_pio_setup(void);

void core1_entry(void);

#endif
