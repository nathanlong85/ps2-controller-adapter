---
change: ps2-guitar-adapter
design-doc: docs/superpowers/specs/2026-07-11-ps2-guitar-adapter-design.md
base-ref: 13b8ac4ad9e123d7cdbf67b15d4adb335ec60df0
---

# PS2 Guitar Adapter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Pico 2W firmware (C/C++, Pico SDK) that bridges a CRKD Gibson Les Paul USB guitar controller into a real PS2 console's controller port, presenting itself as a standards-compliant PS2 digital/analog controller.

**Architecture:** Core1 runs a PIO-driven PS2 SPI-slave state machine (pure shift-register + ACK electrical layer, response bytes supplied by C) and stays heap-free/lock-free so nothing on core0 can stall its ~15-20us response window. Core0 runs TinyUSB host, decodes CRKD HID reports into a normalized guitar-input struct, and runs a data-driven mapping table that translates that struct into PS2 button/axis bytes. The two cores communicate through a lock-free double-buffer/seqlock shared-state struct (buttons, analog axes, mode/config state) written by core0, read by core1.

**Tech Stack:** Raspberry Pi Pico SDK (C/C++), RP2350 PIO assembly (`.pio` programs via `pico_generate_pio_header`), TinyUSB host stack (`tuh_hid_*` callbacks), CMake build, host-side C unit tests (plain C, no `pico/` includes) for the pure-logic pieces, physical PS2 console + CRKD controller + powered USB hub + multimeter for hardware-in-the-loop verification.

## Global Constraints

- Target board: Raspberry Pi Pico 2W (RP2350), built via the official Pico SDK + VS Code Raspberry Pi Pico extension toolchain (per proposal.md Impact and tasks.md 1.2).
- PS2 protocol timing: response must begin within ~15-20us of ATT/CS going low (ps2-protocol spec, "Response begins within required latency").
- SPI mode: sample COMMAND/clock and drive DATA per SPI Mode 3 (CPOL=1, CPHA=1) (ps2-protocol spec).
- ACK is open-collector: assert low by driving, release to Hi-Z (input), never drive high (ps2-protocol spec, "ACK Line Electrical Behavior").
- Core1 (PS2/PIO servicing) MUST remain heap-free and free of blocking/shared SDK locks for the entire life of the program (design doc, "Core split" decision) — this applies to every task that touches core1 code, not just the initial implementation.
- Shared state between core0 and core1 MUST use a lock-free double-buffer/seqlock, never a mutex (design doc, "Shared state" decision).
- Mapping layer MUST be a data-driven config table (HID field -> PS2 button/axis), not inline conditionals (guitar-to-ps2-mapping spec, "Data-Driven Mapping Table"; design doc "Mapping layer" decision).
- Power topology: the Pico must not draw 5V from the PS2 port; USB host power comes from a powered hub; only PS2 signal lines + shared ground cross to the console (proposal.md Impact, design doc Risks).
- Pure-logic code (mapping table, bit-packing, HID decode once fixtures exist) must be buildable/testable on the host machine with no `pico/` SDK includes (design doc, "Testing Strategy").
- Boot default is digital mode; analog mode is entered only via `0x44` (ps2-analog-mode spec, "Default mode at power-on").
- `0x4F` variable poll-format is explicitly unsupported — always return the full analog frame (ps2-analog-mode spec, "Unsupported Variable Poll Format").
- Reserve one PIO block + one DMA channel unclaimed by the PS2 implementation, and keep core1 free of shared SDK locks, as zero-cost provisions for future Bluetooth support (design doc, "Future extensibility" decision) — do not consume all three PIO blocks or all DMA channels for PS2+USB.

---

## File Structure

```
CMakeLists.txt                      # Top-level Pico SDK project file, pulls in pico-sdk + tinyusb
pico_sdk_import.cmake                # Standard Pico SDK CMake bootstrap (copied from SDK)
src/
  main.c                             # Entry point: multicore launch, tuh_task() loop on core0
  shared_state.h / shared_state.c    # Task 3.1: lock-free double-buffer struct + accessors
  ps2_pio.pio                        # Task 3.2-3.4: PIO program (shift register + ACK + resync)
  ps2_protocol.h / ps2_protocol.c    # Task 3.5, 6.x: response-byte assembly, config-mode state machine
  ps2_core1.c                        # Task 3.5: core1 entry point, PIO FIFO feed loop
  usb_hid_host.c                     # Task 4.1-4.2: TinyUSB host glue (tuh_hid_mount_cb etc.)
  crkd_decode.h / crkd_decode.c      # Task 5.5: CRKD HID report -> normalized guitar_input_t
  guitar_input.h                     # Shared normalized guitar input struct definition
  mapping_table.h / mapping_table.c  # Task 7.1: data-driven guitar_input_t -> ps2_output_t table
  tusb_config.h                      # TinyUSB host configuration (HID host enabled)
tests/
  host/
    test_mapping_table.c             # Task 7.1 unit tests (host-buildable, no pico/ includes)
    test_ps2_protocol_bytes.c        # Task 3.5/6.x unit tests for response-byte assembly
    test_crkd_decode.c               # Task 5.5 unit tests using captured fixtures
    fixtures/crkd_reports.h          # Task 5.1/5.3 captured raw HID report bytes as test fixtures
    CMakeLists.txt                   # Separate host-only test build (gcc, no arm toolchain)
docs/
  wiring.md                          # Task 8.1
  build-and-bringup.md               # Task 8.2
  future-bluetooth-extensibility.md  # Task 8.3
```

Rationale: PIO timing/electrical code (`ps2_pio.pio`, `ps2_core1.c`) is isolated from protocol *content* decisions (`ps2_protocol.c`) per the design doc's "PIO scope" decision — PIO never decides what bytes to send, C does. `mapping_table.c` and `crkd_decode.c` are split from `usb_hid_host.c` so the CRKD-specific parsing and mapping logic (host-testable, no SDK includes) stay separate from the TinyUSB callback glue (SDK-dependent, hardware-only).

---

## Hardware-in-the-Loop Legend

Each task is tagged:
- **[CODE]** — implementable and self-verifiable by an agent (build, host-run unit tests, static review). No physical hardware required.
- **[HW]** — requires the user to physically perform steps (wiring, flashing, pressing buttons, observing a console/game) and report back results (serial output, pass/fail, LED behavior). The agent prepares exact instructions and what to look for; the user executes and reports.

---

## Task 1: Hardware Prep & Toolchain Bring-Up

**Files:** none (no code yet)

**Interfaces:** N/A — this is a prerequisite-verification task.

- [ ] **Step 1: [HW] Retrieve hardware**

Ask the user to retrieve and have on hand: Pico 2W, CRKD Gibson Les Paul controller, PS2 console (or test harness), powered USB hub, PS2 controller cable (for splicing/breakout), multimeter.

Expected: user confirms all items are physically available.

- [ ] **Step 2: [HW] Verify toolchain with blink example**

Ask the user to build and flash the standard Pico SDK "blink" example (from the official `pico-examples` repo, `blink/blink.c`) to the Pico 2W using the VS Code + Raspberry Pi Pico extension workflow.

Expected: onboard LED blinks after flashing. If it fails, resolve toolchain setup before continuing (report back the specific error).

- [ ] **Step 3: [HW] Verify PS2 cable pinout**

Ask the user to use a multimeter (continuity/resistance mode) to trace each wire in the PS2 controller cable back to its pin on the PS2 connector, and compare against the documented PS2 controller port pinout:

```
Pin 1: DATA (controller -> console)
Pin 2: COMMAND (console -> controller)
Pin 3: 9V (unused — do not connect)
Pin 4: GND
Pin 5: 3.6V (unused — do not connect; Pico is not powered from here)
Pin 6: ATT/CS (console -> controller, active low)
Pin 7: CLOCK (console -> controller)
Pin 8: (no connection)
Pin 9: ACK (controller -> console, open-collector)
```

Expected: user reports actual wire colors mapped to each pin, and flags any deviation from common documented color conventions. Record the verified mapping in a scratch note for use in Task 8.1 (wiring docs).

- [ ] **Step 4: Commit toolchain verification notes**

No code changes yet in this task (blink example lives in `pico-examples`, not this repo). Skip commit — proceed to Task 2, which creates the first repo commit.

---

## Task 2: PS2 Protocol Wiring Smoke Test (no PIO yet)

**Files:**
- Create: `CMakeLists.txt`
- Create: `pico_sdk_import.cmake`
- Create: `src/main.c` (temporary GPIO-polling smoke test version; will be replaced by multicore entry point in Task 3)

**Interfaces:**
- Produces: repo scaffolding (`CMakeLists.txt` targeting `pico2_w` board) that later tasks extend.

- [x] **Step 1: [CODE] Scaffold the CMake project**

Create `pico_sdk_import.cmake` by copying the standard boilerplate from the Pico SDK (`$PICO_SDK_PATH/external/pico_sdk_import.cmake`).

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(ps2_guitar_adapter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico2_w)

pico_sdk_init()

add_executable(ps2_guitar_adapter
    src/main.c
)

target_link_libraries(ps2_guitar_adapter
    pico_stdlib
)

pico_enable_stdio_usb(ps2_guitar_adapter 0)
pico_enable_stdio_uart(ps2_guitar_adapter 1)

pico_add_extra_outputs(ps2_guitar_adapter)
```

Note: `pico_enable_stdio_usb` is disabled because the Pico's USB port will shortly become the TinyUSB *host* port (Task 4) — diagnostic output uses UART instead. Ask the user to confirm which UART pins/adapter they'll use for serial monitoring before Task 2 Step 3 (needed for them to see the smoke test output).

- [x] **Step 2: [CODE] Write the GPIO smoke-test program**

Create `src/main.c`:

```c
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
```

This code intentionally does no timing-critical work (busy-polls GPIO in a tight loop); it exists only to confirm wiring, not to implement the protocol. PIO implementation is Task 3.

- [ ] **Step 3: [HW] Build, flash, and confirm wiring against a real PS2**

Ask the user to:
1. Build the project (`cmake -B build && cmake --build build` or via the VS Code extension).
2. Flash `ps2_guitar_adapter.uf2` to the Pico 2W.
3. Wire Pico GPIO pins 2 (ATT), 3 (CLOCK), 4 (COMMAND) to the corresponding PS2 cable wires identified in Task 1 Step 3, plus a shared GND connection. Do NOT wire DATA/ACK yet — this stage is read-only/listen-only.
4. Power on the PS2 console with the modified controller cable connected, and observe serial output over UART.

Expected: user reports serial lines like `ATT asserted (low)` / `ATT released (high) - saw N clock pulses` appearing repeatedly (~60Hz) once the console is on. If ATT toggles but clock_pulses is always 0, COMMAND/CLOCK wiring is likely swapped or disconnected — user reports the actual wire-to-pin mapping used so it can be corrected before Task 3.

- [x] **Step 4: Commit**

```bash
git add CMakeLists.txt pico_sdk_import.cmake src/main.c
git commit -m "Add PS2 wiring smoke-test firmware"
```

---

## Task 3: Shared-State Design & PIO PS2 SPI-Slave Implementation

**Files:**
- Create: `src/shared_state.h`
- Create: `src/shared_state.c`
- Create: `src/ps2_pio.pio`
- Create: `src/ps2_protocol.h`
- Create: `src/ps2_protocol.c`
- Create: `src/ps2_core1.c`
- Modify: `src/main.c` (multicore launch, replaces Task 2's smoke-test body)
- Modify: `CMakeLists.txt` (add PIO header generation, new sources)
- Test: `tests/host/test_ps2_protocol_bytes.c`
- Test: `tests/host/CMakeLists.txt`

**Interfaces:**
- Produces: `shared_state_t` struct, `shared_state_write(const shared_state_t *src)` (core0 writer), `shared_state_read(shared_state_t *dst)` (core1 reader) — lock-free seqlock.
- Produces: `ps2_build_digital_response(uint8_t button0, uint8_t button1, uint8_t out[5])` and related response-assembly functions in `ps2_protocol.c`, consumed by Task 6 (analog) and Task 7 (mapping wiring).
- Produces: PIO program `ps2_slave_program` (from `ps2_pio.pio`) exposing a byte-in/byte-out/ack-request interface via TX/RX FIFOs, consumed by `ps2_core1.c`.
- Consumes: nothing from earlier tasks (this is the protocol foundation).

### Step group A: Shared state struct (host-testable)

- [x] **Step 1: [CODE] Write the shared_state seqlock header**

Create `src/shared_state.h`:

```c
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
// reader. Core1 must never block: shared_state_read() retries locally on a
// torn read instead of waiting on core0.
void shared_state_init(void);
void shared_state_write(const shared_state_t *src);
void shared_state_read(shared_state_t *dst);

#endif
```

- [x] **Step 2: [CODE] Implement the seqlock**

Create `src/shared_state.c`:

```c
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
        .mode = PS2_MODE_DIGITAL,
        .config_mode_active = false,
    };
    shared_state_write(&idle);
}

void shared_state_write(const shared_state_t *src) {
    uint32_t seq = atomic_load_explicit(&g_seq, memory_order_relaxed);
    atomic_store_explicit(&g_seq, seq + 1, memory_order_release);   // odd = writing
    memcpy(&g_state, src, sizeof(g_state));
    atomic_store_explicit(&g_seq, seq + 2, memory_order_release);   // even = stable
}

void shared_state_read(shared_state_t *dst) {
    uint32_t seq_before, seq_after;
    do {
        seq_before = atomic_load_explicit(&g_seq, memory_order_acquire);
        memcpy(dst, &g_state, sizeof(*dst));
        seq_after = atomic_load_explicit(&g_seq, memory_order_acquire);
    } while (seq_before != seq_after || (seq_before & 1));
}

#endif
```

(Remove the stray `#endif` at the end — it's a `.c` file, not a header; final file must not include it.)

- [x] **Step 3: [CODE] Write host-buildable unit test for the seqlock**

Create `tests/host/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.13)
project(ps2_guitar_adapter_tests C)
set(CMAKE_C_STANDARD 11)

add_executable(test_ps2_protocol_bytes
    test_ps2_protocol_bytes.c
    ../../src/ps2_protocol.c
)
target_include_directories(test_ps2_protocol_bytes PRIVATE ../../src)

enable_testing()
add_test(NAME ps2_protocol_bytes COMMAND test_ps2_protocol_bytes)
```

Note: the seqlock itself (`shared_state.c`) is exercised indirectly by the mapping/protocol tests in later tasks rather than directly here, since single-threaded host tests cannot exercise cross-core race behavior meaningfully — its correctness under real concurrency is verified in Task 3 Step 9 (hardware pushbutton test) and Task 7's end-to-end test.

### Step group B: Protocol response-byte assembly (host-testable)

- [ ] **Step 4: [CODE] Write failing test for digital response assembly**

Create `tests/host/test_ps2_protocol_bytes.c`:

```c
#include "ps2_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_digital_response_idle(void) {
    uint8_t out[5];
    ps2_build_digital_response(0xFF, 0xFF, out);
    uint8_t expected[5] = {0xFF, 0x41, 0x5A, 0xFF, 0xFF};
    assert(memcmp(out, expected, 5) == 0);
    printf("test_digital_response_idle: PASS\n");
}

static void test_digital_response_button_pressed(void) {
    uint8_t out[5];
    ps2_build_digital_response(0xFE, 0xFF, out); // bit 0 of byte0 pressed
    uint8_t expected[5] = {0xFF, 0x41, 0x5A, 0xFE, 0xFF};
    assert(memcmp(out, expected, 5) == 0);
    printf("test_digital_response_button_pressed: PASS\n");
}

int main(void) {
    test_digital_response_idle();
    test_digital_response_button_pressed();
    printf("All tests passed.\n");
    return 0;
}
```

- [ ] **Step 5: [CODE] Run test to verify it fails (function not yet defined)**

Run: `cd tests/host && cmake -B build && cmake --build build`
Expected: FAIL — compile error, `ps2_build_digital_response` and `ps2_protocol.h` do not exist yet.

- [ ] **Step 6: [CODE] Implement ps2_protocol.h / .c (digital response only for now)**

Create `src/ps2_protocol.h`:

```c
#ifndef PS2_PROTOCOL_H
#define PS2_PROTOCOL_H

#include <stdint.h>

// Assembles the 5-byte digital-controller poll response:
// {0xFF, 0x41, 0x5A, button_byte0, button_byte1}
void ps2_build_digital_response(uint8_t button_byte0, uint8_t button_byte1, uint8_t out[5]);

#endif
```

Create `src/ps2_protocol.c`:

```c
#include "ps2_protocol.h"

void ps2_build_digital_response(uint8_t button_byte0, uint8_t button_byte1, uint8_t out[5]) {
    out[0] = 0xFF;
    out[1] = 0x41;
    out[2] = 0x5A;
    out[3] = button_byte0;
    out[4] = button_byte1;
}
```

- [ ] **Step 7: [CODE] Run test to verify it passes**

Run: `cd tests/host/build && ctest --output-on-failure`
Expected: `1/1 Test #1: ps2_protocol_bytes ..... Passed`, output includes both `PASS` lines and `All tests passed.`

- [ ] **Step 8: [CODE] Write the PIO program (shift-register + ACK + resync)**

Create `src/ps2_pio.pio`. This implements only the electrical/timing layer per the design doc's "PIO scope" decision: it samples COMMAND on CLOCK rising edge, shifts DATA out on CLOCK falling edge, pulses ACK low after each byte (except when C signals "last byte"), and resyncs whenever ATT rises. Byte content comes from the TX FIFO; received COMMAND bytes go to the RX FIFO so C can dispatch based on command.

```pio
; ps2_pio.pio
; PS2 controller-side SPI-slave shift register + ACK engine.
; SPI Mode 3: CPOL=1, CPHA=1. Console is master, we are slave.
;
; Pin mapping (set via pio_sm_config in ps2_core1.c):
;   IN pin 0  = COMMAND (sampled on CLOCK rising edge)
;   OUT pin 0 = DATA (driven on CLOCK falling edge)
;   side-set  = ACK (open-drain emulated via pindirs; see ps2_core1.c)
;   jmp pin   = ATT (active low; used to detect abort/resync)

.program ps2_slave
.side_set 1 opt

public entry_point:
await_att_low:
    wait 0 pin 2            ; wait for ATT low (pin index 2 = ATT in the SM's pin group)

byte_loop:
    set x, 7                 ; 8 bits per byte
bit_loop:
    wait 1 pin 1              ; wait CLOCK rising edge (pin index 1 = CLOCK)
    in pins, 1                ; sample COMMAND on rising edge
    wait 0 pin 1              ; wait CLOCK falling edge
    out pins, 1                ; drive DATA on falling edge
    jmp x-- bit_loop

    push                      ; push received COMMAND byte to RX FIFO
    pull                      ; pull next TX byte (or ACK-control word) for next cycle

    jmp pin, no_ack           ; if ATT has gone high (jmp pin = ATT), abort without ACK
    nop             side 0     ; drive ACK low
    nop [7]                    ; hold ACK low for required pulse width (tune via ACK-electrical test)
no_ack:
    nop             side 1     ; release ACK (side-set drives pindirs, see C wrapper)

    jmp pin, await_att_low     ; if ATT high, resync to start
    jmp byte_loop               ; else continue to next byte in this exchange
```

Note: this is a first-pass PIO skeleton. Exact instruction-level ACK pulse width and side-set-to-pindir wiring (Hi-Z release, not drive-high) must be tuned against the timing verified in Task 3 Step 11 — record any instruction changes made during hardware bring-up back into this file so the source matches what was flashed.

- [ ] **Step 9: [CODE] Implement ps2_core1.c: PIO setup + FIFO feed loop**

Create `src/ps2_core1.c`:

```c
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
```

Create `src/ps2_core1.h`:

```c
#ifndef PS2_CORE1_H
#define PS2_CORE1_H

void core1_entry(void);

#endif
```

Note: `ps2_build_analog_response` does not exist until Task 6 — for this task, stub it in `ps2_protocol.c`/`.h` to return the idle 9-byte analog frame (`0xFF, 0x73, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80`) so the project builds; Task 6 Step group will replace the stub with the real config-mode-aware implementation. Add to `ps2_protocol.h`:

```c
void ps2_build_analog_response(uint8_t button_byte0, uint8_t button_byte1,
                                uint8_t rx, uint8_t ry, uint8_t lx, uint8_t ly,
                                uint8_t out[9]);
```

and to `ps2_protocol.c`:

```c
void ps2_build_analog_response(uint8_t button_byte0, uint8_t button_byte1,
                                uint8_t rx, uint8_t ry, uint8_t lx, uint8_t ly,
                                uint8_t out[9]) {
    out[0] = 0xFF;
    out[1] = 0x73;
    out[2] = 0x5A;
    out[3] = button_byte0;
    out[4] = button_byte1;
    out[5] = rx;
    out[6] = ry;
    out[7] = lx;
    out[8] = ly;
}
```

- [ ] **Step 10: [CODE] Wire multicore launch into main.c and update CMakeLists.txt**

Modify `src/main.c`, replacing the Task 2 smoke-test body:

```c
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
```

Modify `CMakeLists.txt`: add PIO header generation and new sources.

```cmake
add_executable(ps2_guitar_adapter
    src/main.c
    src/shared_state.c
    src/ps2_protocol.c
    src/ps2_core1.c
)

pico_generate_pio_header(ps2_guitar_adapter ${CMAKE_CURRENT_LIST_DIR}/src/ps2_pio.pio)

target_link_libraries(ps2_guitar_adapter
    pico_stdlib
    pico_multicore
    hardware_pio
)
```

- [ ] **Step 11: [CODE] Build the firmware target and confirm it compiles**

Run: `cmake -B build -S . && cmake --build build`
Expected: build succeeds, `build/ps2_guitar_adapter.uf2` produced. Fix any compile errors (PIO header naming, pin conflicts) before proceeding to hardware steps.

- [ ] **Step 12: [HW] Flash and verify PS2 recognizes the adapter as a digital controller**

Ask the user to:
1. Wire DATA and ACK (in addition to ATT/CLOCK/COMMAND/GND from Task 2) per the verified pinout from Task 1 Step 3.
2. Flash the built `.uf2` to the Pico 2W.
3. Connect to a real PS2 console (or homebrew test tool / a game's controller test screen) and observe whether the console recognizes a connected digital controller with no buttons pressed.

Expected: user reports the PS2 shows a connected digital pad, idle state (no phantom button presses). If not recognized, ask the user to report serial diagnostics (add temporary printf instrumentation to `core1_entry`'s response loop if needed, being mindful this printf must not run on the time-critical path in the final build) and iterate on PIO timing (Step 8) before proceeding.

- [ ] **Step 13: [HW] Wire pushbutton and confirm end-to-end button press**

Ask the user to wire a physical pushbutton to a spare GPIO (e.g. GPIO 7) with a pull-up, add a temporary polling loop in `main.c` that calls `shared_state_write` with `button_byte0 = gpio_get(7) ? 0xFF : 0xFE` (bit 0 cleared when pressed) each iteration, rebuild, reflash, and press the button while watching the PS2's controller test screen.

Expected: user reports the corresponding button (bit 0 of byte 0 = Select, per the ps2-protocol spec's byte layout) shows pressed on the PS2 side exactly while the physical button is held, with no stuck/phantom presses. Once confirmed, this temporary polling code is removed in Task 7 Step 2 when the real mapping wiring replaces it — leave it in place until then since it is the only input source until Task 7.

- [ ] **Step 14: Commit**

```bash
git add src/shared_state.h src/shared_state.c src/ps2_pio.pio src/ps2_protocol.h src/ps2_protocol.c src/ps2_core1.h src/ps2_core1.c src/main.c CMakeLists.txt tests/host/CMakeLists.txt tests/host/test_ps2_protocol_bytes.c
git commit -m "Implement PIO PS2 SPI-slave digital response and shared-state seqlock"
```

---

## Task 4: USB Host Bring-Up (generic HID)

**Files:**
- Create: `src/usb_hid_host.c`
- Create: `src/tusb_config.h`
- Modify: `src/main.c` (add `tuh_init`/`tuh_task()` loop)
- Modify: `CMakeLists.txt` (add `tinyusb_host` sources/config)

**Interfaces:**
- Consumes: nothing new from earlier tasks.
- Produces: `tuh_hid_mount_cb`/`tuh_hid_report_received_cb` wiring pattern that Task 5 extends for CRKD-specific decoding.

- [ ] **Step 1: [CODE] Add TinyUSB host configuration**

Create `src/tusb_config.h`:

```c
#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

#define CFG_TUSB_MCU OPT_MCU_RP2040
#define CFG_TUSB_OS OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_HOST

#define CFG_TUH_HID 4
#define CFG_TUH_HUB 1
#define CFG_TUH_DEVICE_MAX 4

#endif
```

- [ ] **Step 2: [CODE] Write generic HID host glue**

Create `src/usb_hid_host.c`:

```c
#include "tusb.h"
#include <stdio.h>

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    printf("HID mounted: dev_addr=%u instance=%u desc_len=%u\n", dev_addr, instance, desc_len);
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("HID unmounted: dev_addr=%u instance=%u\n", dev_addr, instance);
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    printf("HID report: dev_addr=%u instance=%u len=%u bytes=", dev_addr, instance, len);
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", report[i]);
    }
    printf("\n");
    tuh_hid_receive_report(dev_addr, instance);
}
```

- [ ] **Step 3: [CODE] Wire tuh_init/tuh_task into main.c**

Modify `src/main.c`'s main loop:

```c
#include "tusb.h"
// ... existing includes ...

int main(void) {
    stdio_init_all();
    shared_state_init();

    sleep_ms(2000);
    printf("PS2 guitar adapter starting\n");

    multicore_launch_core1(core1_entry);

    tuh_init(BOARD_TUH_RHPORT);

    while (true) {
        tuh_task();
    }
}
```

- [ ] **Step 4: [CODE] Update CMakeLists.txt for TinyUSB host**

```cmake
add_executable(ps2_guitar_adapter
    src/main.c
    src/shared_state.c
    src/ps2_protocol.c
    src/ps2_core1.c
    src/usb_hid_host.c
)

target_include_directories(ps2_guitar_adapter PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

target_link_libraries(ps2_guitar_adapter
    pico_stdlib
    pico_multicore
    hardware_pio
    tinyusb_host
    tinyusb_board
)
```

- [ ] **Step 5: [CODE] Build and confirm it compiles**

Run: `cmake -B build -S . && cmake --build build`
Expected: build succeeds. Resolve any missing TinyUSB config symbols (commonly `CFG_TUH_*` values) before proceeding.

- [ ] **Step 6: [HW] Verify basic USB host function with a simple HID device**

Ask the user to flash the build, connect a simple USB HID device (mouse or keyboard) to the Pico's USB host port via the powered hub, and observe serial output.

Expected: user reports `HID mounted: ...` followed by repeated `HID report: ... bytes=...` lines as they move the mouse or press keys, confirming `tuh_task()` runs correctly. Also ask the user to confirm the PS2 side (from Task 3) still shows a connected idle digital controller at the same time — this confirms USB host activity doesn't disrupt PIO PS2 timing, per the design doc's stated risk.

- [ ] **Step 7: Commit**

```bash
git add src/usb_hid_host.c src/tusb_config.h src/main.c CMakeLists.txt
git commit -m "Add TinyUSB host bring-up with generic HID report logging"
```

---

## Task 5: CRKD Controller Report Capture & Decoding

**Files:**
- Create: `tests/host/fixtures/crkd_reports.h`
- Create: `src/guitar_input.h`
- Create: `src/crkd_decode.h`
- Create: `src/crkd_decode.c`
- Create: `tests/host/test_crkd_decode.c`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/usb_hid_host.c` (route CRKD reports to `crkd_decode`)

**Interfaces:**
- Consumes: raw HID report bytes captured in Steps 1-4 (host-side and on-device).
- Produces: `guitar_input_t` struct (`src/guitar_input.h`) and `crkd_decode_report(const uint8_t *report, uint16_t len, guitar_input_t *out)`, consumed by Task 7's mapping table.

- [ ] **Step 1: [HW] Capture CRKD HID report descriptor and sample reports on a PC**

Ask the user to connect the CRKD Gibson Les Paul controller to a PC in PC mode and capture:
- The USB HID report descriptor (via `usbhid-dump`, Wireshark + USBPcap, or OS-equivalent tooling).
- Raw input report byte dumps for: idle (no input), each fret pressed individually, strum up, strum down, start, select, whammy at rest / partially engaged / fully engaged, tilt at rest / engaged.

Expected: user provides the raw descriptor and report byte sequences (paste or file) back to the agent.

- [ ] **Step 2: [HW] Confirm CRKD enumerates as standard HID gamepad in PC mode**

Ask the user to confirm from the captured descriptor whether the device identifies as a standard HID gamepad (not XInput), and to note the report ID (if any) and total report byte length.

Expected: user confirms standard HID gamepad, reports byte length and report ID (or "no report ID").

- [ ] **Step 3: [HW] Document whammy and tilt report format**

From the Step 1 capture, ask the user to identify: which byte(s)/bit(s) encode whammy, whether it's a ranged analog value (report byte range) or a digital bit, its rest value, and its direction of travel (does the value increase or decrease as whammy is engaged). Same for tilt: analog axis or digital on/off.

Expected: user reports exact byte offsets, bit positions (if digital), rest values, and value ranges for whammy and tilt. This directly resolves the design doc's flagged Open Question and determines whether Task 7's tilt mapping targets an analog axis or a digital button bit.

- [ ] **Step 4: [CODE] Encode the capture into host-side test fixtures**

Using the exact byte sequences reported in Steps 1-3, create `tests/host/fixtures/crkd_reports.h`:

```c
#ifndef CRKD_REPORTS_FIXTURES_H
#define CRKD_REPORTS_FIXTURES_H

#include <stdint.h>

// NOTE: replace these placeholder byte sequences with the exact bytes
// captured in Task 5 Steps 1-3 before running tests. Lengths and offsets
// below are illustrative until real capture data is available.

static const uint8_t CRKD_REPORT_IDLE[] = {0x00, 0x00, 0x00, 0x80, 0x80};
static const uint8_t CRKD_REPORT_GREEN_FRET[] = {0x01, 0x00, 0x00, 0x80, 0x80};
static const uint8_t CRKD_REPORT_STRUM_UP[] = {0x00, 0x01, 0x00, 0x80, 0x80};
static const uint8_t CRKD_REPORT_WHAMMY_FULL[] = {0x00, 0x00, 0x00, 0x80, 0xFF};
static const uint8_t CRKD_REPORT_TOO_SHORT[] = {0x00, 0x01};

#endif
```

This file is intentionally a placeholder shape (agent cannot know real values without hardware capture) — its purpose is to define the fixture-file location and format so it's a one-line data swap once Steps 1-3 produce real numbers, not a rewrite. Flag to the user explicitly: **this fixture file must be updated with real captured bytes before Step 5's tests are meaningful.**

- [ ] **Step 5: [CODE] Write failing test for CRKD decode**

Create `src/guitar_input.h`:

```c
#ifndef GUITAR_INPUT_H
#define GUITAR_INPUT_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool fret_green;
    bool fret_red;
    bool fret_yellow;
    bool fret_blue;
    bool fret_orange;
    bool strum_up;
    bool strum_down;
    bool start;
    bool select;
    uint8_t whammy;       // 0 = rest, 255 = fully engaged (normalized)
    bool tilt_is_analog;  // resolved by Task 5 Step 3 capture
    uint8_t tilt_analog;  // valid when tilt_is_analog is true
    bool tilt_digital;    // valid when tilt_is_analog is false
} guitar_input_t;

// Returns a fully idle/released state — used on malformed report and disconnect.
guitar_input_t guitar_input_idle(void);

#endif
```

Create `src/guitar_input.c` is not needed as a separate file; keep `guitar_input_idle` in `crkd_decode.c` since it's the only user for now (avoid an empty translation unit) — declare it in `guitar_input.h` as shown above, define it in `crkd_decode.c`.

Create `src/crkd_decode.h`:

```c
#ifndef CRKD_DECODE_H
#define CRKD_DECODE_H

#include "guitar_input.h"
#include <stdint.h>

// Decodes a raw CRKD HID input report into a normalized guitar_input_t.
// Malformed or too-short reports decode to guitar_input_idle() rather than
// crashing or reading out of bounds.
void crkd_decode_report(const uint8_t *report, uint16_t len, guitar_input_t *out);

#endif
```

Create `tests/host/test_crkd_decode.c` (placeholder assertions to be tightened once real fixture bytes replace the Step 4 placeholders — the malformed-report and idle-report assertions are real regardless of exact byte layout):

```c
#include "crkd_decode.h"
#include "../fixtures/crkd_reports.h"
#include <assert.h>
#include <stdio.h>

static void test_idle_report_is_idle(void) {
    guitar_input_t g;
    crkd_decode_report(CRKD_REPORT_IDLE, sizeof(CRKD_REPORT_IDLE), &g);
    assert(!g.fret_green && !g.fret_red && !g.fret_yellow && !g.fret_blue && !g.fret_orange);
    assert(!g.strum_up && !g.strum_down && !g.start && !g.select);
    printf("test_idle_report_is_idle: PASS\n");
}

static void test_malformed_report_decodes_to_idle(void) {
    guitar_input_t g;
    crkd_decode_report(CRKD_REPORT_TOO_SHORT, sizeof(CRKD_REPORT_TOO_SHORT), &g);
    guitar_input_t idle = guitar_input_idle();
    assert(g.fret_green == idle.fret_green);
    assert(g.strum_up == idle.strum_up);
    printf("test_malformed_report_decodes_to_idle: PASS\n");
}

static void test_null_report_decodes_to_idle(void) {
    guitar_input_t g;
    crkd_decode_report(NULL, 0, &g);
    guitar_input_t idle = guitar_input_idle();
    assert(g.fret_green == idle.fret_green);
    printf("test_null_report_decodes_to_idle: PASS\n");
}

int main(void) {
    test_idle_report_is_idle();
    test_malformed_report_decodes_to_idle();
    test_null_report_decodes_to_idle();
    printf("All tests passed.\n");
    return 0;
}
```

- [ ] **Step 6: [CODE] Run test to verify it fails**

Run: `cd tests/host && cmake -B build && cmake --build build`
Expected: FAIL — `crkd_decode.c` not yet added to the test build / functions undefined.

- [ ] **Step 7: [CODE] Implement crkd_decode.c against captured report layout**

Create `src/crkd_decode.c`, using the exact byte/bit offsets reported by the user in Steps 1-3. The structure below assumes an example layout (byte 0 = fret bitmask, byte 1 = strum/start/select bitmask, byte 2 = padding, byte 3 = whammy analog, byte 4 = tilt analog) — **replace the offsets/masks with the real captured layout before this step is considered complete**:

```c
#include "crkd_decode.h"
#include <stddef.h>

#define CRKD_MIN_REPORT_LEN 5

#define FRET_GREEN_BIT  (1 << 0)
#define FRET_RED_BIT    (1 << 1)
#define FRET_YELLOW_BIT (1 << 2)
#define FRET_BLUE_BIT   (1 << 3)
#define FRET_ORANGE_BIT (1 << 4)

#define STRUM_UP_BIT   (1 << 0)
#define STRUM_DOWN_BIT (1 << 1)
#define START_BIT      (1 << 2)
#define SELECT_BIT     (1 << 3)

guitar_input_t guitar_input_idle(void) {
    guitar_input_t g = {0};
    g.whammy = 0;
    g.tilt_is_analog = true;
    g.tilt_analog = 0;
    g.tilt_digital = false;
    return g;
}

void crkd_decode_report(const uint8_t *report, uint16_t len, guitar_input_t *out) {
    if (report == NULL || len < CRKD_MIN_REPORT_LEN) {
        *out = guitar_input_idle();
        return;
    }

    uint8_t frets = report[0];
    uint8_t buttons = report[1];

    out->fret_green  = (frets & FRET_GREEN_BIT) != 0;
    out->fret_red    = (frets & FRET_RED_BIT) != 0;
    out->fret_yellow = (frets & FRET_YELLOW_BIT) != 0;
    out->fret_blue   = (frets & FRET_BLUE_BIT) != 0;
    out->fret_orange = (frets & FRET_ORANGE_BIT) != 0;

    out->strum_up   = (buttons & STRUM_UP_BIT) != 0;
    out->strum_down = (buttons & STRUM_DOWN_BIT) != 0;
    out->start       = (buttons & START_BIT) != 0;
    out->select      = (buttons & SELECT_BIT) != 0;

    out->whammy = report[3];
    out->tilt_is_analog = true;
    out->tilt_analog = report[4];
    out->tilt_digital = false;
}
```

- [ ] **Step 8: [CODE] Update host test CMakeLists.txt and run tests**

Modify `tests/host/CMakeLists.txt`, adding:

```cmake
add_executable(test_crkd_decode
    test_crkd_decode.c
    ../../src/crkd_decode.c
)
target_include_directories(test_crkd_decode PRIVATE ../../src)
add_test(NAME crkd_decode COMMAND test_crkd_decode)
```

Run: `cd tests/host && cmake -B build && cmake --build build && cd build && ctest --output-on-failure`
Expected: both `ps2_protocol_bytes` and `crkd_decode` tests pass.

- [ ] **Step 9: [HW] Confirm on-device report format matches PC capture**

Ask the user to connect the CRKD controller to the Pico's USB host (via the powered hub, using the Task 4 build extended with a print of `dev_addr`/report bytes already present in `usb_hid_host.c`), and compare the printed raw bytes against the PC capture from Step 1 while pressing the same inputs (frets, strum, whammy, tilt).

Expected: user confirms byte layout matches; if it differs (e.g. different report ID prefix over the Pico's TinyUSB stack vs. the PC capture tool), report the actual on-device bytes so `crkd_decode.c` (Step 7) and the fixtures (Step 4) are corrected to match reality.

- [ ] **Step 10: [CODE] Route CRKD reports through crkd_decode in usb_hid_host.c**

Modify `src/usb_hid_host.c`:

```c
#include "tusb.h"
#include "crkd_decode.h"
#include <stdio.h>

static guitar_input_t g_latest_guitar_input;

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    printf("HID mounted: dev_addr=%u instance=%u desc_len=%u\n", dev_addr, instance, desc_len);
    tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    printf("HID unmounted: dev_addr=%u instance=%u\n", dev_addr, instance);
    g_latest_guitar_input = guitar_input_idle();
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    crkd_decode_report(report, len, &g_latest_guitar_input);
    tuh_hid_receive_report(dev_addr, instance);
}

const guitar_input_t *usb_hid_host_get_latest_guitar_input(void) {
    return &g_latest_guitar_input;
}
```

Add the accessor declaration to a new small header `src/usb_hid_host.h`:

```c
#ifndef USB_HID_HOST_H
#define USB_HID_HOST_H

#include "guitar_input.h"

const guitar_input_t *usb_hid_host_get_latest_guitar_input(void);

#endif
```

This satisfies the usb-guitar-input spec's "Controller Disconnect Fail-Safe" requirement — `tuh_hid_umount_cb` resets state to idle via `guitar_input_idle()`.

- [ ] **Step 11: [CODE] Build and confirm it compiles**

Run: `cmake -B build -S . && cmake --build build`
Expected: build succeeds.

- [ ] **Step 12: Commit**

```bash
git add tests/host/fixtures/crkd_reports.h src/guitar_input.h src/crkd_decode.h src/crkd_decode.c tests/host/test_crkd_decode.c tests/host/CMakeLists.txt src/usb_hid_host.c src/usb_hid_host.h
git commit -m "Decode CRKD HID reports into normalized guitar input state"
```

---

## Task 6: PS2 Analog Mode Protocol

**Files:**
- Modify: `src/ps2_protocol.h`
- Modify: `src/ps2_protocol.c`
- Modify: `src/ps2_core1.c` (dispatch config-mode commands, replace analog stub)
- Test: `tests/host/test_ps2_protocol_bytes.c` (extend)

**Interfaces:**
- Consumes: `shared_state_t.mode`, `shared_state_t.config_mode_active` (from Task 3).
- Produces: `ps2_dispatch_command(uint8_t cmd_byte0, ps2_protocol_state_t *state, uint8_t *out, uint *out_len)` — the config-mode-aware command dispatcher consumed by `ps2_core1.c`; `ps2_protocol_state_t` tracks mode + config-mode-active + which sub-command is in progress, separate from `shared_state_t` (which only carries the *button/axis content* crossing cores — protocol/config sequencing state lives entirely on core1, since it is driven by console byte sequences core1 alone observes).

- [ ] **Step 1: [CODE] Write failing tests for config-mode entry/exit and mode switching**

Extend `tests/host/test_ps2_protocol_bytes.c`, adding before `main`:

```c
static void test_config_mode_entry_response(void) {
    ps2_protocol_state_t state = {0};
    uint8_t out[9];
    uint out_len;
    // 0x43 0x00 0x01 sequence: dispatch each byte in order.
    ps2_dispatch_command(0x43, &state, out, &out_len);
    assert(state.config_mode_active == false); // not yet — needs full 3-byte sequence
    printf("test_config_mode_entry_response: PASS (stub check)\n");
}

static void test_analog_response_idle(void) {
    uint8_t out[9];
    ps2_build_analog_response(0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80, out);
    uint8_t expected[9] = {0xFF, 0x73, 0x5A, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80};
    assert(memcmp(out, expected, 9) == 0);
    printf("test_analog_response_idle: PASS\n");
}

static void test_default_mode_is_digital(void) {
    ps2_protocol_state_t state = {0};
    ps2_protocol_state_init(&state);
    assert(state.mode == PS2_MODE_DIGITAL);
    printf("test_default_mode_is_digital: PASS\n");
}
```

Add corresponding calls in `main()`. Note: `test_config_mode_entry_response` above is intentionally a minimal placeholder pending the real 3-byte command-dispatch design finalized in Step 3 below — it will be tightened once `ps2_dispatch_command`'s exact multi-byte-sequence signature is implemented.

- [ ] **Step 2: [CODE] Run test to verify it fails**

Run: `cd tests/host && cmake -B build && cmake --build build`
Expected: FAIL — `ps2_protocol_state_t`, `ps2_dispatch_command`, `ps2_protocol_state_init` undefined.

- [ ] **Step 3: [CODE] Implement config-mode state machine in ps2_protocol.h/.c**

Add to `src/ps2_protocol.h`:

```c
typedef enum {
    PS2_MODE_DIGITAL = 0,
    PS2_MODE_ANALOG = 1,
} ps2_response_mode_t;

typedef struct {
    ps2_response_mode_t mode;
    bool config_mode_active;
    uint8_t current_command;   // command byte this exchange is responding to
    uint8_t byte_index;         // which byte of the response we're on
} ps2_protocol_state_t;

void ps2_protocol_state_init(ps2_protocol_state_t *state);

// Called once per exchange when the command byte (second byte after 0x01) is
// known. Fills out/out_len with the full response for that command given
// current state, and updates state for config sub-commands (0x43, 0x44).
// data_bytes/data_len are the command's payload bytes beyond the command byte
// itself (e.g. for 0x43 0x00 0x01, data_bytes = {0x00, 0x01}).
void ps2_dispatch_command(uint8_t command,
                           const uint8_t *data_bytes, uint8_t data_len,
                           ps2_protocol_state_t *state,
                           uint8_t *out, uint8_t *out_len);
```

Add to `src/ps2_protocol.c`:

```c
void ps2_protocol_state_init(ps2_protocol_state_t *state) {
    state->mode = PS2_MODE_DIGITAL;
    state->config_mode_active = false;
    state->current_command = 0;
    state->byte_index = 0;
}

static void build_current_poll_response(const ps2_protocol_state_t *state,
                                         uint8_t button0, uint8_t button1,
                                         uint8_t rx, uint8_t ry, uint8_t lx, uint8_t ly,
                                         uint8_t *out, uint8_t *out_len) {
    if (state->config_mode_active) {
        out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A;
        *out_len = 3;
        return;
    }
    if (state->mode == PS2_MODE_ANALOG) {
        ps2_build_analog_response(button0, button1, rx, ry, lx, ly, out);
        *out_len = 9;
    } else {
        ps2_build_digital_response(button0, button1, out);
        *out_len = 5;
    }
}

void ps2_dispatch_command(uint8_t command,
                           const uint8_t *data_bytes, uint8_t data_len,
                           ps2_protocol_state_t *state,
                           uint8_t *out, uint8_t *out_len) {
    switch (command) {
        case 0x43: // config mode entry/exit
            if (data_len >= 2 && data_bytes[0] == 0x00) {
                state->config_mode_active = (data_bytes[1] == 0x01);
            }
            out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A; out[3] = 0x00; out[4] = 0x00;
            out[5] = 0x00; out[6] = 0x00; out[7] = 0x00; out[8] = 0x00;
            *out_len = 9;
            break;

        case 0x44: // mode switch (only meaningful while config_mode_active)
            if (state->config_mode_active && data_len >= 2 && data_bytes[0] == 0x00) {
                state->mode = (data_bytes[1] == 0x01) ? PS2_MODE_ANALOG : PS2_MODE_DIGITAL;
            }
            out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A; out[3] = 0x00; out[4] = 0x00;
            out[5] = 0x00; out[6] = 0x00; out[7] = 0x00; out[8] = 0x00;
            *out_len = 9;
            break;

        case 0x45: // identification query — benign, mode-reflecting dummy
            out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A;
            out[3] = (state->mode == PS2_MODE_ANALOG) ? 0x01 : 0x00;
            out[4] = 0x02; out[5] = 0x00; out[6] = 0x00; out[7] = 0x00; out[8] = 0x00;
            *out_len = 9;
            break;

        case 0x46:
        case 0x47:
        case 0x4D:
            out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A; out[3] = 0x00; out[4] = 0x00;
            out[5] = 0x00; out[6] = 0x00; out[7] = 0x00; out[8] = 0x00;
            *out_len = 9;
            break;

        case 0x4F: // unsupported variable poll format — ignore requested customization
            out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A; out[3] = 0x00; out[4] = 0x00;
            out[5] = 0x00; out[6] = 0x00; out[7] = 0x00; out[8] = 0x00;
            *out_len = 9;
            break;

        default: // unrecognized command — benign fixed-length dummy
            out[0] = 0xFF; out[1] = 0xF3; out[2] = 0x5A; out[3] = 0x00; out[4] = 0x00;
            out[5] = 0x00; out[6] = 0x00; out[7] = 0x00; out[8] = 0x00;
            *out_len = 9;
            break;
    }
}
```

Note: `0x42` (the standard poll) is deliberately NOT handled in `ps2_dispatch_command` — it's handled directly in `ps2_core1.c`'s main loop via `build_current_poll_response`-equivalent logic, since it's the hot path and reads button/axis bytes from `shared_state_t` each cycle rather than from a fixed dummy table. Expose `build_current_poll_response` in the header (remove `static`) so `ps2_core1.c` can call it:

```c
void ps2_build_current_poll_response(const ps2_protocol_state_t *state,
                                      uint8_t button0, uint8_t button1,
                                      uint8_t rx, uint8_t ry, uint8_t lx, uint8_t ly,
                                      uint8_t *out, uint8_t *out_len);
```

(rename `build_current_poll_response` to `ps2_build_current_poll_response` and drop `static` in the `.c` file to match.)

- [ ] **Step 4: [CODE] Fix the placeholder config-mode-entry test now that the real signature exists**

Replace `test_config_mode_entry_response` in `tests/host/test_ps2_protocol_bytes.c`:

```c
static void test_config_mode_entry_and_exit(void) {
    ps2_protocol_state_t state;
    ps2_protocol_state_init(&state);
    uint8_t out[9];
    uint8_t out_len;
    uint8_t enter_data[2] = {0x00, 0x01};

    ps2_dispatch_command(0x43, enter_data, 2, &state, out, &out_len);
    assert(state.config_mode_active == true);

    uint8_t poll_out[9];
    uint8_t poll_len;
    ps2_build_current_poll_response(&state, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80, poll_out, &poll_len);
    assert(poll_out[1] == 0xF3);
    assert(poll_len == 3);

    uint8_t exit_data[2] = {0x00, 0x00};
    ps2_dispatch_command(0x43, exit_data, 2, &state, out, &out_len);
    assert(state.config_mode_active == false);

    ps2_build_current_poll_response(&state, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80, poll_out, &poll_len);
    assert(poll_out[1] == 0x41); // back to digital mode response
    assert(poll_len == 5);

    printf("test_config_mode_entry_and_exit: PASS\n");
}

static void test_mode_switch_to_analog(void) {
    ps2_protocol_state_t state;
    ps2_protocol_state_init(&state);
    uint8_t out[9];
    uint8_t out_len;
    uint8_t enter_data[2] = {0x00, 0x01};
    ps2_dispatch_command(0x43, enter_data, 2, &state, out, &out_len);

    uint8_t switch_data[2] = {0x00, 0x01}; // select analog
    ps2_dispatch_command(0x44, switch_data, 2, &state, out, &out_len);

    uint8_t exit_data[2] = {0x00, 0x00};
    ps2_dispatch_command(0x43, exit_data, 2, &state, out, &out_len);

    uint8_t poll_out[9];
    uint8_t poll_len;
    ps2_build_current_poll_response(&state, 0xFF, 0xFF, 0x80, 0x80, 0x80, 0x80, poll_out, &poll_len);
    assert(poll_out[1] == 0x73);
    assert(poll_len == 9);

    printf("test_mode_switch_to_analog: PASS\n");
}
```

Update `main()` to call these instead of the removed stub test.

- [ ] **Step 5: [CODE] Run tests to verify they pass**

Run: `cd tests/host/build && cmake --build . && ctest --output-on-failure`
Expected: all tests pass, including `test_config_mode_entry_and_exit`, `test_mode_switch_to_analog`, `test_analog_response_idle`, `test_default_mode_is_digital`.

- [ ] **Step 6: [CODE] Wire command dispatch into ps2_core1.c**

Modify `src/ps2_core1.c`'s main loop to track `ps2_protocol_state_t` and use `ps2_dispatch_command`/`ps2_build_current_poll_response` instead of the Task 3 stub logic. This requires the PIO program (Task 3 Step 8) to expose the received COMMAND byte (and subsequent data bytes) via its RX FIFO so C can branch on it before choosing what to load into the TX FIFO — replace the Task 3 placeholder loop that always sent digital/analog based only on `shared_state_t.mode`:

```c
    ps2_protocol_state_t proto_state;
    ps2_protocol_state_init(&proto_state);
    shared_state_t state;

    while (true) {
        shared_state_read(&state);
        proto_state.mode = (state.mode == PS2_MODE_ANALOG) ? PS2_MODE_ANALOG : PS2_MODE_DIGITAL;

        uint8_t command = (uint8_t)pio_sm_get_blocking(pio, sm); // first byte after 0x01, per PIO RX FIFO contract
        uint8_t data_bytes[2] = {0, 0};
        uint8_t data_len = 0;
        uint8_t response[9];
        uint8_t response_len;

        if (command == 0x42) {
            ps2_build_current_poll_response(&proto_state, state.button_byte0, state.button_byte1,
                                             state.right_stick_x, state.right_stick_y,
                                             state.left_stick_x, state.left_stick_y,
                                             response, &response_len);
        } else {
            // 0x43/0x44 exchanges carry 2 payload bytes after the command byte;
            // read them from the RX FIFO before dispatching.
            data_bytes[0] = (uint8_t)pio_sm_get_blocking(pio, sm);
            data_bytes[1] = (uint8_t)pio_sm_get_blocking(pio, sm);
            data_len = 2;
            ps2_dispatch_command(command, data_bytes, data_len, &proto_state, response, &response_len);
        }

        for (uint i = 0; i < response_len; i++) {
            pio_sm_put_blocking(pio, sm, response[i]);
        }
    }
```

Note to implementer: the exact FIFO read/write ordering above depends on how many bytes the PIO program has already latched into its RX FIFO by the time C reaches this point (the PIO's `push`/`pull` placement from Task 3 Step 8 determines this). Validate this ordering against real captured behavior in Step 7 below and adjust either the PIO program or this dispatch loop's byte-count assumptions if the console's actual byte cadence differs — this is exactly the kind of PIO/C coupling flagged as a risk in the design doc, and is why Step 7 (hardware verification) exists before declaring this task done.

- [ ] **Step 7: [HW] Flash and verify analog mode + config-mode negotiation against a real PS2/game**

Ask the user to build, flash, and test against a real PS2 console or a game/homebrew tool known to probe analog mode (e.g. a PS2 controller test screen, or a game that supports DualShock analog input). Specifically verify:
1. Console recognizes the controller as digital at boot.
2. If the game/tool sends `0x44` to switch to analog, the controller correctly reports `0x73` and 9-byte responses, and the analog stick indicators move when whammy/tilt values change (drive `state.right_stick_y`/`left_stick_y` with test values temporarily if no CRKD is wired yet).
3. No hangs, dropped polls, or console-side "controller disconnected" errors during config-mode entry/exit.

Expected: user reports pass/fail per point above, with serial diagnostics for any failure. Iterate on Step 6's dispatch logic based on findings.

- [ ] **Step 8: Commit**

```bash
git add src/ps2_protocol.h src/ps2_protocol.c src/ps2_core1.c tests/host/test_ps2_protocol_bytes.c
git commit -m "Implement PS2 analog mode and config-mode command dispatch"
```

---

## Task 7: Guitar-to-PS2 Mapping & Full Integration

**Files:**
- Create: `src/mapping_table.h`
- Create: `src/mapping_table.c`
- Create: `tests/host/test_mapping_table.c`
- Modify: `tests/host/CMakeLists.txt`
- Modify: `src/main.c` (replace Task 3's temporary pushbutton polling with real mapping wiring)

**Interfaces:**
- Consumes: `guitar_input_t` (Task 5), `shared_state_t`/`shared_state_write` (Task 3).
- Produces: `map_guitar_to_ps2(const guitar_input_t *in, shared_state_t *out)`, the final integration point.

- [ ] **Step 1: [CODE] Write failing tests for the mapping table**

Create `tests/host/test_mapping_table.c`:

```c
#include "mapping_table.h"
#include "guitar_input.h"
#include "shared_state.h"
#include <assert.h>
#include <stdio.h>

static void test_idle_maps_to_idle(void) {
    guitar_input_t g = guitar_input_idle();
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    assert(s.button_byte0 == 0xFF);
    assert(s.button_byte1 == 0xFF);
    assert(s.right_stick_y == 0x80);
    assert(s.left_stick_y == 0x80);
    printf("test_idle_maps_to_idle: PASS\n");
}

static void test_green_fret_maps_to_x(void) {
    guitar_input_t g = guitar_input_idle();
    g.fret_green = true;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    // X button is byte1 bit 6 in the standard PS2 layout (byte1: L2/R2/L1/R1/Triangle/Circle/X/Square)
    assert((s.button_byte1 & (1 << 6)) == 0);
    printf("test_green_fret_maps_to_x: PASS\n");
}

static void test_strum_down_maps_to_dpad_down(void) {
    guitar_input_t g = guitar_input_idle();
    g.strum_down = true;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    // Down is byte0 bit 6 in the standard layout (byte0: Select/L3/R3/Start/Up/Right/Down/Left)
    assert((s.button_byte0 & (1 << 6)) == 0);
    printf("test_strum_down_maps_to_dpad_down: PASS\n");
}

static void test_simultaneous_fret_and_strum(void) {
    guitar_input_t g = guitar_input_idle();
    g.fret_green = true;
    g.strum_down = true;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    assert((s.button_byte1 & (1 << 6)) == 0); // X still pressed
    assert((s.button_byte0 & (1 << 6)) == 0); // Down still pressed
    printf("test_simultaneous_fret_and_strum: PASS\n");
}

static void test_whammy_at_rest_centers_right_stick_y(void) {
    guitar_input_t g = guitar_input_idle();
    g.whammy = 0;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    assert(s.right_stick_y == 0x80);
    printf("test_whammy_at_rest_centers_right_stick_y: PASS\n");
}

static void test_whammy_full_maps_to_extreme(void) {
    guitar_input_t g = guitar_input_idle();
    g.whammy = 255;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    assert(s.right_stick_y == 0xFF);
    printf("test_whammy_full_maps_to_extreme: PASS\n");
}

static void test_analog_tilt_at_rest_centers_left_stick_y(void) {
    guitar_input_t g = guitar_input_idle();
    g.tilt_is_analog = true;
    g.tilt_analog = 0;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    assert(s.left_stick_y == 0x80);
    printf("test_analog_tilt_at_rest_centers_left_stick_y: PASS\n");
}

static void test_digital_tilt_maps_to_button_bit(void) {
    guitar_input_t g = guitar_input_idle();
    g.tilt_is_analog = false;
    g.tilt_digital = true;
    shared_state_t s;
    map_guitar_to_ps2(&g, &s);
    // Digital tilt maps to R3 (byte0 bit 2) per design doc's fallback decision.
    assert((s.button_byte0 & (1 << 2)) == 0);
    assert(s.left_stick_y == 0x80); // stays centered, not used as an axis
    printf("test_digital_tilt_maps_to_button_bit: PASS\n");
}

int main(void) {
    test_idle_maps_to_idle();
    test_green_fret_maps_to_x();
    test_strum_down_maps_to_dpad_down();
    test_simultaneous_fret_and_strum();
    test_whammy_at_rest_centers_right_stick_y();
    test_whammy_full_maps_to_extreme();
    test_analog_tilt_at_rest_centers_left_stick_y();
    test_digital_tilt_maps_to_button_bit();
    printf("All tests passed.\n");
    return 0;
}
```

- [ ] **Step 2: [CODE] Run test to verify it fails**

Run: `cd tests/host && cmake -B build && cmake --build build`
Expected: FAIL — `mapping_table.h`/`map_guitar_to_ps2` undefined.

- [ ] **Step 3: [CODE] Implement the data-driven mapping table**

Create `src/mapping_table.h`:

```c
#ifndef MAPPING_TABLE_H
#define MAPPING_TABLE_H

#include "guitar_input.h"
#include "shared_state.h"

// Translates a normalized guitar input state into PS2 output (button bytes +
// analog axes) via a data-driven table, per the guitar-to-ps2-mapping spec.
// Does not touch s->mode or s->config_mode_active — caller preserves those.
void map_guitar_to_ps2(const guitar_input_t *in, shared_state_t *out);

#endif
```

Create `src/mapping_table.c`:

```c
#include "mapping_table.h"
#include <string.h>

// PS2 button bit layout (active-low):
// byte0: bit0=Select bit1=L3 bit2=R3 bit3=Start bit4=Up bit5=Right bit6=Down bit7=Left
// byte1: bit0=L2 bit1=R2 bit2=L1 bit3=R1 bit4=Triangle bit5=Circle bit6=X bit7=Square

typedef struct {
    int byte_index;   // 0 or 1 in shared_state_t's button bytes
    uint8_t bit_mask;
} ps2_button_bit_t;

typedef struct {
    const bool *guitar_field_offset; // not used directly; table is applied procedurally below
} mapping_entry_t;

// A literal data table mapping each guitar field to its PS2 bit location.
// Implemented as a lookup applied by map_guitar_to_ps2 below, rather than a
// chain of if/else per field, satisfying the "table lookup, not inline
// conditional" requirement.
typedef struct {
    size_t guitar_field_byte_offset_within_struct; // computed via offsetof
    ps2_button_bit_t target;
} button_mapping_row_t;

#define FIELD_OFFSET(field) offsetof(guitar_input_t, field)

static const button_mapping_row_t BUTTON_MAP[] = {
    { FIELD_OFFSET(fret_green),  { 1, (1 << 6) } }, // X
    { FIELD_OFFSET(fret_red),    { 1, (1 << 5) } }, // Circle
    { FIELD_OFFSET(fret_yellow), { 1, (1 << 4) } }, // Triangle
    { FIELD_OFFSET(fret_blue),   { 1, (1 << 7) } }, // Square
    { FIELD_OFFSET(fret_orange), { 1, (1 << 2) } }, // L1
    { FIELD_OFFSET(strum_up),    { 0, (1 << 4) } }, // Up
    { FIELD_OFFSET(strum_down),  { 0, (1 << 6) } }, // Down
    { FIELD_OFFSET(start),       { 0, (1 << 3) } }, // Start
    { FIELD_OFFSET(select),      { 0, (1 << 0) } }, // Select
};
#define BUTTON_MAP_LEN (sizeof(BUTTON_MAP) / sizeof(BUTTON_MAP[0]))

#define DIGITAL_TILT_BUTTON_BYTE 0
#define DIGITAL_TILT_BUTTON_MASK (1 << 2) // R3

void map_guitar_to_ps2(const guitar_input_t *in, shared_state_t *out) {
    out->button_byte0 = 0xFF;
    out->button_byte1 = 0xFF;
    out->right_stick_x = 0x80;
    out->left_stick_x = 0x80;

    for (size_t i = 0; i < BUTTON_MAP_LEN; i++) {
        const bool *field = (const bool *)((const uint8_t *)in + BUTTON_MAP[i].guitar_field_byte_offset_within_struct);
        if (*field) {
            uint8_t *target_byte = (BUTTON_MAP[i].target.byte_index == 0) ? &out->button_byte0 : &out->button_byte1;
            *target_byte &= (uint8_t)~BUTTON_MAP[i].target.bit_mask;
        }
    }

    out->right_stick_y = in->whammy; // whammy already normalized 0-255 with 0=rest; PS2 rest is 0x80, so remap:
    out->right_stick_y = (uint8_t)(0x80 + ((int)in->whammy - 0) / 2); // maps 0..255 rest-to-full onto 0x80..0xFF

    if (in->tilt_is_analog) {
        out->left_stick_y = (uint8_t)(0x80 + ((int)in->tilt_analog) / 2);
    } else {
        out->left_stick_y = 0x80;
        if (in->tilt_digital) {
            out->button_byte0 &= (uint8_t)~DIGITAL_TILT_BUTTON_MASK;
        }
    }
}
```

Note: the whammy/tilt scaling above (`0x80 + value/2`) is a placeholder linear mapping from a 0-255 "rest-to-full-engagement" normalized range onto the upper half of the PS2 axis byte (`0x80`-`0xFF`), consistent with "rest = centered, engaged = one direction" (whammy/tilt are one-directional controls, not bidirectional sticks). If Task 5's real capture data shows a different normalized range for `guitar_input_t.whammy`/`tilt_analog` (e.g. signed or full 0-255 bidirectional), adjust this formula accordingly — the test in Step 1 (`test_whammy_full_maps_to_extreme`, expecting `0xFF` at `whammy=255`) pins the intended endpoint behavior regardless of the exact intermediate formula.

- [ ] **Step 4: [CODE] Update host test CMakeLists.txt and run tests**

Modify `tests/host/CMakeLists.txt`, adding:

```cmake
add_executable(test_mapping_table
    test_mapping_table.c
    ../../src/mapping_table.c
    ../../src/crkd_decode.c
)
target_include_directories(test_mapping_table PRIVATE ../../src)
add_test(NAME mapping_table COMMAND test_mapping_table)
```

Run: `cd tests/host/build && cmake --build . && ctest --output-on-failure`
Expected: all four test executables (`ps2_protocol_bytes`, `crkd_decode`, `mapping_table`) pass, all listed assertions print `PASS`.

- [ ] **Step 5: [CODE] Wire mapping into main.c, replacing the Task 3 pushbutton stub**

Modify `src/main.c`, removing the Task 3 Step 13 temporary pushbutton-polling code and replacing the main loop body:

```c
#include "mapping_table.h"
#include "usb_hid_host.h"
// ... existing includes retained ...

int main(void) {
    stdio_init_all();
    shared_state_init();

    sleep_ms(2000);
    printf("PS2 guitar adapter starting\n");

    multicore_launch_core1(core1_entry);

    tuh_init(BOARD_TUH_RHPORT);

    while (true) {
        tuh_task();

        const guitar_input_t *guitar = usb_hid_host_get_latest_guitar_input();
        shared_state_t out;
        map_guitar_to_ps2(guitar, &out);
        out.mode = PS2_MODE_DIGITAL; // preserved/updated by ps2_core1's config-mode handling; see note below
        shared_state_write(&out);
    }
}
```

Note: `out.mode` and `out.config_mode_active` are set here from a fixed default because `shared_state_t` (Task 3) is the core0-to-core1 direction only — the actual live mode negotiated via `0x44` lives in `ps2_core1.c`'s local `ps2_protocol_state_t` (Task 6), which is not fed back to core0. This is intentional: core0 always offers the current button/axis content; core1 decides, based on its own config-mode state, whether to actually use it (digital vs analog framing) per Task 6 Step 6's dispatch logic, which already reads `state.mode` for content shaping but the response *format* (5 vs 9 byte, `0x41` vs `0x73`) is governed by `proto_state.mode`, which core1 owns and evolves independently via `0x44` -- `shared_state_t.mode` as written by core0 is therefore vestigial once Task 6 lands. Simplify by removing `mode`/`config_mode_active` from `shared_state_t` in this step's implementation (they were only needed as Task 3's initial placeholder before Task 6 introduced the real state machine) and update `shared_state.h`/`shared_state_init`/`ps2_core1.c` accordingly, so shared_state_t carries only button bytes + 4 axis bytes. Confirm this simplification by re-running the full host test suite (Step 4) after making the change, since `mapping_table.c` and its tests do not reference `mode`/`config_mode_active` on `shared_state_t`.

- [ ] **Step 6: [CODE] Build and confirm it compiles**

Run: `cmake -B build -S . && cmake --build build`
Expected: build succeeds after the `shared_state_t` field removal from Step 5 is propagated (check `ps2_core1.c`'s references to `state.mode`/`state.config_mode_active` are removed, since mode now lives solely in `ps2_protocol_state_t`).

- [ ] **Step 7: [HW] End-to-end test with real PS2 and CRKD controller**

Ask the user to flash the final build, wire the Pico fully to the PS2 (per Task 1/3's verified pinout) and connect the CRKD controller via the powered USB hub, then in a game or controller test screen:
1. Press each fret individually and confirm the correct PS2 face button/L1 lights up.
2. Strum up/down and confirm D-pad up/down.
3. Press start/select and confirm they register.
4. Move the whammy bar through its range and confirm the mapped analog axis moves smoothly from center to the expected extreme.
5. Engage tilt (star power) and confirm either the mapped axis moves (if analog) or the mapped button registers (if digital), per Task 5 Step 3's determination.
6. Hold multiple inputs simultaneously (e.g. a fret + strum) and confirm both register without interference.

Expected: user reports pass/fail for each of the six checks above, with any mismatches (wrong button, inverted axis, stuck input) reported for follow-up fixes to `mapping_table.c`.

- [ ] **Step 8: [HW] Confirm idle state produces no stuck inputs**

Ask the user to release all inputs (including disconnecting/reconnecting the CRKD mid-session) and confirm on the PS2's controller test screen that all buttons show unpressed and both analog axes (if in analog mode) sit at center, with no phantom presses appearing spontaneously.

Expected: user confirms clean idle state matches the guitar-to-ps2-mapping spec's "Idle guitar state produces idle PS2 state" scenario and the ps2-analog-mode spec's "Unused analog axes remain centered" scenario.

- [ ] **Step 9: Commit**

```bash
git add src/mapping_table.h src/mapping_table.c tests/host/test_mapping_table.c tests/host/CMakeLists.txt src/main.c src/shared_state.h src/shared_state.c src/ps2_core1.c
git commit -m "Wire guitar-to-PS2 mapping table into full integration"
```

---

## Task 8: Documentation

**Files:**
- Create: `docs/wiring.md`
- Create: `docs/build-and-bringup.md`
- Create: `docs/future-bluetooth-extensibility.md`

**Interfaces:** N/A — documentation only, no code interfaces.

- [ ] **Step 1: [CODE] Write wiring documentation**

Create `docs/wiring.md` covering: the PS2 connector pinout table (from Task 1 Step 3's verified mapping, filled in with the user's actual wire colors), the Pico GPIO pin assignments used in `ps2_core1.c` (`PIN_ATT=2, PIN_CLOCK=3, PIN_COMMAND=4, PIN_DATA=5, PIN_ACK=6`, updated to match whatever pins the hardware bring-up steps actually settled on), the power scheme (powered USB hub feeds the Pico's 5V/USB, only signal + shared GND cross to the PS2 port, PS2's 3.6V/9V pins are explicitly not connected), and a labeled wiring diagram (ASCII or embedded image if the user provides a photo).

- [ ] **Step 2: [CODE] Write build/flash and bring-up documentation**

Create `docs/build-and-bringup.md` covering: prerequisites (Pico SDK path, VS Code extension, arm toolchain), build commands (`cmake -B build -S . && cmake --build build`), flashing procedure (BOOTSEL + drag `.uf2`, or picotool), how to run the host-only unit tests (`cd tests/host && cmake -B build && cmake --build build && cd build && ctest`), and the staged bring-up/verification procedure used across Tasks 1-7 (blink -> wiring smoke test -> PIO digital response -> pushbutton test -> generic USB HID -> CRKD capture/decode -> analog mode -> full integration), so a future contributor can re-run the same verification sequence after making changes.

- [ ] **Step 3: [CODE] Document future Bluetooth-extensibility provisions**

Create `docs/future-bluetooth-extensibility.md`, documenting the zero-cost provisions made per the design doc's "Future extensibility" decision: which PIO block and DMA channel are left unclaimed by the PS2 implementation (confirm actual PIO block used in `ps2_core1.c` — currently `pio0` — so the doc states `pio1`/`pio2` and specific DMA channels remain free), the source/connection-state enum pattern used in `usb_hid_host.c` (or note if this wasn't literally implemented as an enum and should be before BT work begins), and the core1 heap-free/lock-free constraint that must be preserved. State explicitly that no BT implementation work was done in this change — this document exists so a future change doesn't have to rediscover the rationale from the original design doc.

- [ ] **Step 4: Commit**

```bash
git add docs/wiring.md docs/build-and-bringup.md docs/future-bluetooth-extensibility.md
git commit -m "Add wiring, build/bring-up, and future-extensibility documentation"
```

---

## Summary of Hardware-in-the-Loop Checkpoints

For quick reference, every `[HW]` step across this plan, in order:

1. Task 1 Step 1 — retrieve hardware
2. Task 1 Step 2 — blink example flash/verify
3. Task 1 Step 3 — multimeter pinout verification
4. Task 2 Step 3 — wiring smoke test (ATT/CLOCK observed on real PS2)
5. Task 3 Step 12 — PIO digital response recognized by real PS2
6. Task 3 Step 13 — physical pushbutton end-to-end test
7. Task 4 Step 6 — generic USB HID device mount/report test
8. Task 5 Steps 1-3 — CRKD HID capture on a PC (descriptor, byte layout, whammy/tilt format)
9. Task 5 Step 9 — on-device CRKD report capture matches PC capture
10. Task 6 Step 7 — analog mode / config-mode negotiation against real PS2/game
11. Task 7 Step 7 — full end-to-end guitar-to-PS2 mapping test
12. Task 7 Step 8 — idle-state / no-stuck-input verification

All other steps are `[CODE]` and can be executed and self-verified (via build + host-side `ctest`) without physical hardware.
