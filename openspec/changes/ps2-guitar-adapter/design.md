## Context

The PS2 controller port is an SPI-like bus where the console is always master and the controller is always slave, polled at ~60Hz with tight response timing (controller must respond within ~15-20us of ATT/CS going low). The controller side must also assert ACK after each byte per the real protocol. The Pico 2W's hardware SPI peripheral only supports master mode, so slave-side behavior must be implemented with the RP2350's PIO blocks, which can bit-bang the protocol in hardware independent of the CPU.

Simultaneously, the Pico must act as USB host to a CRKD Gibson Les Paul guitar controller (PC mode) via TinyUSB, decode its HID input reports, and feed a translated button state into the PIO slave logic.

The user has the physical hardware (Pico 2W, controller, PS2/test harness, powered hub) but has not yet dug it out of storage; the CRKD controller's exact HID report format is unknown and must be captured empirically. The assistant has no way to flash firmware or interact with real hardware directly — all hardware-dependent verification happens through the user running builds and reporting back serial/debug output.

## Goals / Non-Goals

**Goals:**
- Correct, standards-compliant PS2 controller protocol implementation in PIO, including ACK line timing (not just the minimal subset that lenient games tolerate).
- Reliable USB host capture of the CRKD controller's HID reports in PC mode.
- A clean, testable staged bring-up path so hardware issues are isolated one variable at a time.

**Non-Goals:**
- DualShock2-style per-button pressure sensitivity (the 12-byte pressure block / `0x79` ID) — only whammy/tilt as stick-style analog axes (`0x73`, 9-byte frame) are in scope.
- Byte-exact DualShock2 fidelity for every config sub-command, and `0x4F` (custom poll-response format) support.
- Support for other controllers, actual multi-controller support, or the CRKD's Xbox/wireless dongle modes (a data-driven mapping table structure is used so this is easier later, but is not built now).
- Actual Bluetooth controller support (documented as a future-compatible direction, not implemented).
- Rumble/motor feedback to the guitar controller.
- Step-by-step teaching scaffolding — the user wants working firmware, with hardware-in-the-loop checkpoints, not a guided tutorial.

## Decisions

- **PIO over bit-banged GPIO or external SPI slave chip**: PIO can hold exact timing in hardware regardless of CPU load from the USB host stack, which is essential since TinyUSB host processing must run concurrently with PS2 polling. Considered: interrupt-driven GPIO bit-banging (rejected — timing jitter under USB host load risks missing PS2's ~15-20us response window); external shift-register/SPI-slave IC (rejected — adds hardware complexity user wants to avoid).
- **ACK line implemented in PIO, not skipped**: user explicitly prioritized protocol correctness over implementation simplicity. ACK will be asserted low by the PIO program per-byte after DATA is read, matching real controller behavior, rather than treated as optional.
- **TinyUSB host (not Arduino/USB Host Shield)**: consistent with existing decision to use the official Pico SDK; TinyUSB is the SDK's supported USB host stack and integrates directly with PIO-based code without a second toolchain.
- **HID report format determined empirically, not assumed**: CRKD's PC-mode HID descriptor is undocumented publicly. Mapping code will be written against a real captured descriptor/report dump (via `usbhid-dump`, Wireshark USBPcap, or Pico-side raw report logging over serial) rather than guessed from the Xbox-oriented product description.
- **Staged bring-up plan retained from prior planning**: blink → GPIO wiring smoke test → hardcoded PIO button state → manual pushbutton test → generic USB HID host test → full guitar integration. Each stage isolates one subsystem (PIO timing vs. USB host vs. mapping logic) so hardware bugs are localized before the pieces are combined.

## Risks / Trade-offs

- [PIO program cannot meet PS2's ACK + response timing once measured on real hardware] → Stage the bring-up so PIO/PS2 timing is proven with hardcoded button state (stage 3) *before* USB host code is added, isolating timing bugs from USB-load-induced jitter.
- [CRKD controller's PC mode is not standard HID gamepad, or has quirks (e.g. requires an init report/feature report to activate)] → Verify via HID report dump as an explicit early task, before writing `map_guitar_to_ps2()`; do not hardcode assumptions from the Xbox-focused product description.
- [Wire colors on the user's specific cable don't match documented pinout] → Multimeter verification is an explicit task before wiring, per existing project notes.
- [Powered hub does not adequately isolate Pico power draw from PS2 port] → Existing design already avoids drawing any power from the PS2 3.6V line; only signal + shared ground cross that boundary.
- [Assistant cannot directly validate firmware behavior] → Each implementation stage ships with a concrete verification procedure (serial output to check, expected LED/behavior) for the user to run and report back, rather than assuming correctness from code review alone.

## Open Questions

- Exact CRKD HID report layout (bytes for frets, strum, whammy if present, start/select) — resolved once user captures a report dump.
- Whether the CRKD PC-mode enumerates immediately or requires a mode-switch/init sequence recognized by TinyUSB's default HID driver.
- Final guitar→PS2 button mapping (frets to face buttons, strum to d-pad) — draft mapping exists from prior planning, to be confirmed once real report data is available.
