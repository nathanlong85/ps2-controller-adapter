# Brainstorm Summary

- Change: ps2-guitar-adapter
- Date: 2026-07-11

## Confirmed Technical Approach

- **PIO scope narrowed**: PIO is a pure clocked shift-register + ACK-timing engine (drive/sample bits per SPI Mode 3, pulse ACK low via pindirs for open-drain emulation, resync on ATT-abort). All protocol/mode semantics (response content, response length, config-mode command dispatch) live in C, which feeds the PIO TX FIFO before/at each exchange.
- **Core split**: core1 dedicated to PS2/PIO servicing (deterministic, isolated from USB/BT jitter, kept heap-free with no blocking/shared SDK locks); core0 runs TinyUSB host + HID decode + the mapping layer.
- **Shared state**: a small struct (button bits + analog axes + mode/config state) shared core0->core1 via a lock-free double-buffer/seqlock pattern (not a single atomic word, not a mutex).
- **Power**: powered USB hub, as originally planned (Pico + USB host powered independently of the PS2 port; only signal + shared ground cross to the console).
- **Test harness**: serial-print debugging first; logic analyzer treated as a fallback if PIO/ACK timing issues prove too hard to isolate that way.
- **Analog mode (new scope, was previously a Non-Goal)**: real `0x43` config-mode entry/exit and `0x44` mode switching; boots in digital mode, switches to analog only via `0x44` (matches real hardware). `0x45` gets a plausible identification response. `0x46`/`0x47`/`0x4D` get benign dummy responses (not DualShock2-exact fidelity). `0x4F` (variable poll format) is explicitly skipped — always sends the full analog frame.
- **Axis mapping**: whammy -> right stick Y, tilt -> left stick Y (if tilt is analog; if it's a digital bit instead, it becomes an extra button bit and skips analog mode for that signal). Exact rest value/polarity/bit-width for both wait on the CRKD HID report capture (task 5).
- **Mapping layer is data-driven**: a config table/struct maps HID fields -> PS2 buttons/axes for the CRKD specifically. No multi-controller support is being built now, but the structure avoids hardcoding so a future controller means a new table, not a protocol rewrite.
- **Future Bluetooth extensibility (documented, not built)**: BT HID latency (~7.5-30ms depending on BLE/Classic) is looser than USB but within the ~16.67ms PS2 poll budget most of the time; a late/missed report is handled the same as a dropped USB frame (re-present last-known state). The normalized-input-struct + mapping-table design already generalizes to a second input source. Zero-cost provisions made now: don't hardcode a single-input-source assumption in consumer logic (use a source/connection-state enum alongside the struct), reserve a PIO block + DMA channel that PS2 doesn't claim, keep core1 lock-free/heap-free.

## Key Trade-offs and Risks

- Analog mode roughly doubles protocol-implementation surface vs. the original digital-only design and introduces a new C<->PIO timing coupling (response content must be ready before the FIFO needs it) that didn't exist before.
- Skipping `0x4F` variable poll format is an accepted, documented gap (rare games might customize poll format and misbehave).
- Whammy/tilt HID format (rest value, polarity, analog-vs-digital) is unconfirmed until the real report capture in task 5.
- No logic analyzer for initial bring-up — serial-only debugging accepted as slower for isolating PIO/ACK timing issues; logic analyzer is a fallback if needed.
- BT extensibility numbers (real latency/jitter on cyw43439, exact PIO/DMA allocation for the wireless chip) are deferred and cannot be resolved without actually building that path later.

## Testing Strategy

- Pure logic (guitar-to-PS2 mapping table, bit-packing/byte-order, HID report decoding) is structured to compile and unit-test on the host dev machine, independent of the Pico SDK/hardware — this is where correctness can be verified directly during development.
- HID decode tests use real captured CRKD report bytes as fixtures once captured (task 5).
- PIO SPI-slave timing, ACK electrical behavior, TinyUSB enumeration, and full end-to-end integration remain hardware-bring-up-only, verified via the existing staged plan with the user running builds and reporting serial output back.

## Spec Patches

- New capability `ps2-analog-mode`: config-mode entry/exit, `0x44` mode switching, analog poll response format (`0x73` ID, 9-byte frame), dummy sub-command responses (`0x45/0x46/0x47/0x4D`), explicit `0x4F` non-support.
- `guitar-to-ps2-mapping`: add whammy/tilt analog axis requirements (rest-value/centering to `0x80`, mapping to right/left stick Y respectively), and a requirement that the mapping is implemented as a data-driven config table (not inline conditionals), scoped to the CRKD only.
- `ps2-protocol`: add open-drain ACK electrical detail (Hi-Z release via pindirs, not push-pull), aborted-transaction/ATT-abort resync requirement, non-`0x42`/unrecognized-command handling (benign fixed-length dummy response), power-on idle-state requirement (valid idle pad presented before USB/input source is available).
- `usb-guitar-input`: add disconnect/reconnect fail-safe requirement (normalized state reverts to idle on controller disconnect).
