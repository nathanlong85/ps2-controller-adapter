#include "shared_state.h"
#include <string.h>
#include <stdatomic.h>

static shared_state_t g_state;
static _Atomic uint32_t g_seq = 0;

void shared_state_init(void) {
    shared_state_t idle = {
        .button_byte0 = 0xFF,
        .button_byte1 = 0xFF,
        .right_stick_x = 0x80,
        .right_stick_y = 0x80,
        .left_stick_x = 0x80,
        .left_stick_y = 0x80,
    };
    shared_state_write(&idle);
}

void shared_state_write(const shared_state_t *src) {
    uint32_t seq = atomic_load_explicit(&g_seq, memory_order_relaxed);
    atomic_store_explicit(&g_seq, seq + 1, memory_order_relaxed);   // odd = writing
    // Keeps the data stores below from being reordered above the odd store.
    atomic_thread_fence(memory_order_release);
    memcpy(&g_state, src, sizeof(g_state));
    atomic_store_explicit(&g_seq, seq + 2, memory_order_release);   // even = stable
}

void shared_state_read(shared_state_t *dst) {
    uint32_t seq_before, seq_after;
    do {
        seq_before = atomic_load_explicit(&g_seq, memory_order_acquire);
        memcpy(dst, &g_state, sizeof(*dst));
        // Keeps the data loads above from being reordered below seq_after.
        atomic_thread_fence(memory_order_acquire);
        seq_after = atomic_load_explicit(&g_seq, memory_order_relaxed);
    } while (seq_before != seq_after || (seq_before & 1));
}
