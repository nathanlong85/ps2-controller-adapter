#ifndef SHARED_STATE_H
#define SHARED_STATE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PS2_MODE_DIGITAL = 0,
    PS2_MODE_ANALOG = 1,
} ps2_mode_t;

typedef struct {
    uint8_t button_byte0;   // active-low bitmask, byte 0
    uint8_t button_byte1;   // active-low bitmask, byte 1
    uint8_t right_stick_x;  // 0x80 = centered
    uint8_t right_stick_y;
    uint8_t left_stick_x;
    uint8_t left_stick_y;
    ps2_mode_t mode;
    bool config_mode_active;
} shared_state_t;

// Seqlock-protected shared state. Core0 is the sole writer, core1 the sole
// reader. shared_state_read() takes no lock and retries locally on a torn
// read, so core1 never waits on a lock held by core0. This is lock-free, not
// wait-free: if core0 were delayed mid-write, core1 would keep retrying until
// the write completes. Today core0 never stalls inside its short write
// window; Task 4 should revisit this once USB interrupts exist on core0 (an
// IRQ landing mid-write could stretch that window).
void shared_state_init(void);
void shared_state_write(const shared_state_t *src);
void shared_state_read(shared_state_t *dst);

#endif
