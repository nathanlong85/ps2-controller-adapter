---
comet_change: ps2-guitar-adapter
role: technical-design
canonical_spec: openspec
---

# PS2 Guitar Adapter — Technical Design

## Context

A Raspberry Pi Pico 2W bridges a CRKD Gibson Les Paul USB guitar controller (PC mode) into a real PS2 console's controller port, acting as USB host on one side (TinyUSB) and a PS2 SPI-slave peripheral (RP2350 PIO) on the other. The PS2 console polls at ~60Hz and requires a response beginning within ~15-20us of ATT/CS going low, including a correctly-timed ACK pulse after each byte. The Pico's hardware SPI peripheral is master-only, so slave timing must be implemented in PIO.

During design brainstorming, scope expanded from a digital-only pad (frets/strum/start/select) to include analog whammy-bar and tilt (star power) support, which pulls in the PS2 console's analog/DualShock config-mode protocol — a materially larger surface than the original digital-only design. This document reflects that expanded scope. See OpenSpec canonical specs (`ps2-protocol`, `usb-guitar-input`, `guitar-to-ps2-mapping`, `ps2-analog-mode`) for requirement-level detail; this document covers the "how."

The assistant cannot flash firmware or interact with real hardware. All hardware-dependent verification happens through the user running builds on real hardware and reporting back serial output.

## Goals / Non-Goals

**Goals:**
- Correct, standards-compliant PS2 digital *and* analog controller protocol, including proper ACK line timing and a real (if not maximally DualShock2-exact) config-mode negotiation.
- Whammy bar and tilt (star power) transmitted via PS2 analog axes, in addition to frets/strum/start/select as digital buttons.
- A guitar-to-PS2 mapping layer structured as a data-driven config table (CRKD-specific for now), not hardcoded conditionals, so a future controller doesn't require a protocol rewrite.
- A staged, hardware-in-the-loop bring-up path that isolates PIO timing, USB host behavior, and mapping logic as independent variables.

**Non-Goals (this change):**
- DualShock2-style per-button pressure sensitivity (the 12-byte pressure block / `0x79` ID) — only stick-style analog axes (`0x73`, 9-byte frame) are in scope.
- Byte-exact DualShock2 fidelity for every config sub-command (`0x46`/`0x47`/`0x4D` get benign dummy responses, not real DS2 identification data).
- `0x4F` (custom poll-response format) support — the adapter always returns the full analog frame.
- Actual multi-controller or Bluetooth-controller support — only the CRKD over USB is implemented; the design avoids precluding these later (see Future Extensibility) but does not build them now.
- Rumble/motor feedback to the guitar controller.

## Decisions

### PIO scope: pure shift-register + ACK engine, not protocol owner

Originally, PIO was expected to own more of the poll/response sequencing directly (viable for a fixed 5-byte digital-only response). With analog mode's variable response length (5 bytes digital, 9 bytes analog, distinct config-mode reply lengths) and command dispatch (`0x43`/`0x44`/`0x45`/`0x46`/`0x47`/`0x4D`), PIO's limited instruction memory and lack of real branching make it unsuitable as the protocol-logic owner. Decision: PIO handles only the electrical/timing layer — sampling COMMAND on clock rising edge, driving DATA on clock falling edge, pulsing ACK low (via toggling pin direction to emulate open-drain, since PIO has no native open-drain output and the console provides the pull-up), and resyncing to poll-start on ATT-abort. C decides *what* bytes to load into the PIO TX FIFO before each exchange based on current mode/config state.

### Core split: core1 = PS2/PIO servicing, core0 = USB host + mapping

Core1 is dedicated to time-critical PS2 servicing: reacting to ATT-rising to resync the state machine, keeping the TX FIFO fed with the correct response for the current mode, and nothing else. It is kept heap-free and free of blocking/shared SDK locks so nothing on core0 (TinyUSB host processing, future BT stack processing) can stall it. Core0 owns TinyUSB host, HID decode, and the mapping layer, which is less timing-sensitive and can tolerate more jitter.

Alternative considered: single-core, since PIO offloads bit-level timing. Rejected — the failure mode (a long USB-stack operation delaying an ATT-abort resync or FIFO refill) is exactly the kind of intermittent glitch that's difficult to debug from serial output alone, and the user has already prioritized protocol correctness over implementation simplicity.

### Shared state: struct with lock-free double-buffer/seqlock, not a single atomic word

The original digital-only design's plan (a single atomic 32-bit word for 2 button bytes) is insufficient once analog axes and mode/config state exist. The shared state now includes: button bitmask (2 bytes), analog axes (whammy, tilt — up to 4 bytes for two 16-bit-capable or 2 bytes if 8-bit axes suffice), and mode/config state (digital/analog, locked/unlocked, config-mode-active). This is carried in a small struct written by core0 and read by core1 via a lock-free double-buffer or seqlock pattern — chosen over a mutex specifically to guarantee core1 never blocks waiting on core0.

### Analog mode compliance: "config-mode-real, DS2-fidelity-approximate" middle ground

Three options were considered:
1. **Fixed-analog shortcut** (always claim `0x73`, never implement real config-mode negotiation) — rejected as too weak given the user's demonstrated preference for correctness (mirrors the earlier ACK-line decision); breaks games that explicitly probe mode-switching state.
2. **Full DualShock2-exact fidelity** (byte-exact `0x45/0x46/0x47/0x4D` responses, full `0x4F` variable-format support) — rejected as disproportionate effort; these sub-commands exist mostly for pressure-sensitive-button detection and precise model identification, neither needed here.
3. **Recommended middle ground (chosen)**: real `0x43` config-mode entry/exit and `0x44` mode switching (so mode state is genuinely negotiated, defaulting to digital on boot like real hardware); a plausible `0x45` identification response; benign fixed-length dummy responses for `0x46/0x47/0x4D`; `0x4F` explicitly unsupported (always full analog frame). This covers what the large majority of games actually check while avoiding low-value, high-effort DS2-exact fidelity work.

### Boot mode: digital, switch to analog via `0x44`

Matches real DualShock hardware behavior and correctly handles games that explicitly test mode-switching, rather than an analog-locked boot state that would deviate from expected hardware behavior.

### Axis mapping: whammy → right stick Y, tilt → left stick Y

Both sticks are otherwise unused by the digital button mapping (frets occupy face buttons, strum occupies the d-pad, start/select map directly). Keeping whammy and tilt on independent axes avoids blend/priority logic if both move simultaneously. If tilt turns out to be reported as a digital bit rather than an analog axis (unconfirmed until the HID capture), it is instead mapped as an extra digital button bit (e.g. onto the otherwise-unmapped R3) and analog mode's scope shrinks to whammy alone.

### Mapping layer: data-driven config table, CRKD-specific

The guitar-to-PS2 mapping is implemented as a config table/struct (HID field → PS2 button/axis), not inline conditional logic. This is scoped to the CRKD controller only in this change — no multi-controller support is implemented — but the structure means a future controller requires a new table, not a rewrite of protocol or mapping code.

### Future extensibility: Bluetooth controller input (documented, not built)

Not implemented in this change, but the design deliberately avoids precluding it:
- BT HID gamepad latency (~7.5-15ms BLE, ~10-30ms BT Classic, depending on connection/sniff interval) is looser than USB's 1-8ms but generally within the ~16.67ms PS2 poll budget; an occasional late/missed report is handled identically to a dropped USB frame — core1 re-presents the last-known state for that poll cycle. This is not a new failure mode.
- No PIO/DMA conflict is expected: the RP2350 has 3 PIO blocks, and the CYW43439 wireless chip's driver needs its own PIO state machine + DMA channel, independent of the block already dedicated to PS2 servicing.
- Zero-cost provisions made now: consumer-side logic uses a source/connection-state enum rather than assuming exactly one input source exists; a PIO block and DMA channel are left unclaimed by the PS2 implementation; core1 remains heap-free with no shared SDK locks (already required for PS2 timing reasons, and this also protects it from future BT-stack activity on core0).
- Explicitly deferred: real BTstack/cyw43439 latency measurement, multi-source arbitration logic (if BT and USB could ever be connected simultaneously), and any actual Bluetooth implementation work.

## Risks / Trade-offs

- [Analog mode roughly doubles protocol-implementation surface and introduces a new C↔PIO timing coupling that didn't exist in the pure-digital design] → Mitigated by keeping PIO's role minimal (pure shift-register/ACK) and deciding all mode/response content in C ahead of each exchange; verified via the staged bring-up plan before combining with USB input.
- [Skipping `0x4F` variable poll-format support] → Accepted, documented gap; revisit only if real-hardware testing surfaces a game that requires it.
- [CRKD whammy/tilt HID report format — rest value, polarity, bit width, analog vs. digital — is unconfirmed] → Resolved via empirical capture (existing task 5), not assumed from the product's Xbox-oriented description.
- [No logic analyzer for initial bring-up] → Accepted; serial-print diagnostics first, logic analyzer treated as a fallback if PIO/ACK timing issues prove too hard to isolate otherwise.
- [Power topology: Pico as USB host needs 5V it can't safely draw from the PS2 port] → Powered USB hub, as originally planned; only PS2 signal lines + shared ground cross to the console.
- [Assistant cannot directly validate firmware behavior] → Each implementation stage ships with a concrete verification procedure for the user to run and report back.

## Testing Strategy

- **Unit-testable on the dev machine, independent of the Pico SDK/hardware**: the guitar-to-PS2 mapping table (fret/strum/start-select → bits, whammy/tilt → axis scaling and centering to `0x80`, idle → `0xFF/0xFF` with no stuck bits, simultaneous-input non-interference), the button-byte bit-packing/byte-order, and HID report decoding (once real report fixtures are captured in task 5 — malformed/short reports must decode to idle, not crash). These are structured as pure functions with no `pico/` SDK includes, buildable and testable on the host machine.
- **Hardware-bring-up-only (cannot be verified by the assistant directly)**: PIO SPI-slave shift timing, ACK electrical behavior (open-drain release timing), ATT-abort resync, TinyUSB enumeration of the CRKD in PC mode, config-mode negotiation against a real PS2/game, and full end-to-end integration. Verified via the existing staged plan (blink → wiring smoke test → hardcoded PIO response → manual pushbutton → generic USB HID host → CRKD capture/decode → full integration), with the user running builds and reporting serial output back at each stage.

## Migration Plan

Not applicable — greenfield project, no existing deployed system to migrate from.

## Open Questions

- Exact CRKD HID report layout for whammy and tilt (rest value, direction/polarity, bit width, and whether tilt is analog or digital) — resolved once the user captures a report dump (task 5).
- Whether the CRKD's PC mode requires an init/feature report to activate, or enumerates immediately.
- Real-world BT HID latency/jitter on the CYW43439 + BTstack combination, and exact PIO/DMA allocation for the wireless chip — deferred until an actual future Bluetooth-support change is undertaken.
