# Comet Design Handoff

- Change: ps2-guitar-adapter
- Phase: design
- Mode: compact
- Context hash: 80125765d49d9186ef678f53e3d5a13c9790025436be4b851cc9d82547e7444e

Generated-by: comet-handoff.sh

OpenSpec remains the canonical capability spec. This handoff is a deterministic, source-traceable context pack, not an agent-authored summary.

## openspec/changes/ps2-guitar-adapter/proposal.md

- Source: openspec/changes/ps2-guitar-adapter/proposal.md
- Lines: 1-32
- SHA256: 0af7143af18ebca6f3101386b2f9d792f4b80de09ad37a94b9e08f0145562f58

```md
## Why

The PS2 has no native support for USB guitar controllers. A Raspberry Pi Pico 2W can bridge a modern USB HID guitar controller (CRKD Gibson Les Paul, PC mode) into a real PS2 controller port by acting as a USB host on one side and a PS2 SPI-slave peripheral (via PIO) on the other, making the controller usable on original PS2 hardware.

## What Changes

- New Pico SDK (C/C++) firmware project targeting Pico 2W, using PIO (not the Pico's SPI peripheral, which only supports master mode) to implement the PS2 controller-side SPI-slave protocol, including the ACK line and analog-mode config negotiation.
- TinyUSB host integration to enumerate and read HID reports from the CRKD Gibson Les Paul controller (PC mode).
- A guitar-to-PS2 mapping layer, implemented as a data-driven config table, translating the controller's frets/strum/start/select into the PS2 digital controller button bitmask, and whammy bar/tilt into PS2 analog stick axes.
- PS2 analog/DualShock-style config-mode protocol support (mode switching, analog poll response format) sufficient for whammy bar and tilt (star power) to be transmitted as analog input, without full DualShock2 byte-exact fidelity.
- Wiring/hardware documentation: PS2 connector pinout, powered-hub power scheme (Pico must not draw power from the PS2 port), wire verification procedure.
- Firmware is validated through a staged bring-up plan (blink → wiring/GPIO smoke test → hardcoded PIO responses → manual button test → generic USB HID host test → full guitar integration) since implementation can only be verified against real hardware the user runs and reports back on.

## Capabilities

### New Capabilities
- `ps2-protocol`: PIO-based PS2 SPI-slave implementation — poll/response sequencing, digital controller identification bytes, ACK line timing (including open-drain electrical behavior and aborted-transaction recovery), button bitmask output.
- `usb-guitar-input`: TinyUSB host integration that enumerates the CRKD Gibson Les Paul controller in PC mode and decodes its HID input reports into a normalized button/strum/whammy/tilt state, including disconnect fail-safe behavior.
- `guitar-to-ps2-mapping`: Data-driven translation layer mapping normalized guitar input state (buttons and analog axes) to the PS2 controller output consumed by `ps2-protocol`/`ps2-analog-mode`, scoped to the CRKD controller.
- `ps2-analog-mode`: PS2 config-mode negotiation (entry/exit, digital/analog mode switching) and analog poll response format, enabling whammy bar and tilt to be transmitted as analog stick axes.

### Modified Capabilities
(none — greenfield project, no existing specs)

## Impact

- New firmware project under this repo (CMakeLists.txt, `.pio` source, `main.c`, supporting headers).
- New hardware documentation (wiring diagrams, power notes, bring-up test procedure).
- No existing code or specs affected (repo currently has no implementation).
- External dependency: Pico SDK + TinyUSB host stack (via `pico-sdk`/`tinyusb` submodules or vendored libs).
- Requires physical hardware (Pico 2W, CRKD controller, PS2 console or test harness, powered USB hub) in the user's possession to validate; the assistant cannot execute or flash firmware directly.
- Core0/core1 responsibility split and shared-state design (struct with lock-free double-buffer, not a single atomic word) affect the shape of the PIO/main-loop implementation planned for tasks 3 and 6.
```

## openspec/changes/ps2-guitar-adapter/design.md

- Source: openspec/changes/ps2-guitar-adapter/design.md
- Lines: 1-44
- SHA256: c6271b3a5546ff676a09bdcd578f9172f34ee9d1cd7b016e3e38351c5ae1aaa9

```md
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
```

## openspec/changes/ps2-guitar-adapter/tasks.md

- Source: openspec/changes/ps2-guitar-adapter/tasks.md
- Lines: 1-54
- SHA256: a7b5659b0ec6a585b3f9792f7bf4b507a1e5786837700ba896ed2c2c6673140d

```md
## 1. Hardware Prep & Toolchain Bring-Up

- [ ] 1.1 Retrieve Pico 2W, CRKD Gibson Les Paul controller, PS2 console/test harness, powered USB hub, and PS2 controller cable from storage
- [ ] 1.2 Verify VS Code + official Raspberry Pi Pico extension toolchain still works: build and flash the "blink" example to the Pico 2W
- [ ] 1.3 Verify PS2 cable wire colors against the documented pinout using a multimeter; record any deviations

## 2. PS2 Protocol Wiring Smoke Test (no PIO yet)

- [ ] 2.1 Write minimal GPIO polling code to detect ATT/CS going low and CLOCK pulses from a real PS2 console, printing observations over serial
- [ ] 2.2 Confirm via serial output that wiring (DATA/COMMAND/ATT/CLOCK/GND) is correct before proceeding to PIO

## 3. Shared-State Design & PIO PS2 SPI-Slave Implementation (`ps2-protocol`)

- [ ] 3.1 Design and implement the core0/core1 shared-state struct (button bits, analog axes, mode/config state) using a lock-free double-buffer/seqlock pattern; keep core1 heap-free with no blocking/shared SDK locks
- [ ] 3.2 Implement PIO program as a pure clocked shift-register/ACK engine: sample COMMAND on clock rising edge, drive DATA on clock falling edge, respond to ATT/CS going low; response content/length supplied by C, not hardcoded in PIO
- [ ] 3.3 Implement ACK line assertion in PIO after each byte except the last, releasing to high-impedance (not driving high) to correctly emulate the open-drain line
- [ ] 3.4 Implement ATT-abort recovery: resync the PIO state machine to await the next poll if ATT/CS rises before a response completes
- [ ] 3.5 Implement the standard digital-controller poll/response sequence (`0x01`/`0xFF`, `0x42`/`0x41`, `0x5A`, button byte 0, button byte 1) with a hardcoded fixed button_state, including power-on idle response before any input source connects
- [ ] 3.6 Flash to Pico, verify against real PS2 that it's recognized as a connected digital controller (e.g. via a PS2 test/homebrew tool or a game's controller test screen)
- [ ] 3.7 Wire a physical pushbutton to a spare GPIO, toggle a single bit in button_state on press, and confirm the PS2 sees the corresponding button press end-to-end

## 4. USB Host Bring-Up (`usb-guitar-input`, generic)

- [ ] 4.1 Integrate TinyUSB host into the project alongside the PIO code; confirm `tuh_task()` runs without disrupting PIO PS2 timing
- [ ] 4.2 Verify basic USB host function using a simple HID device (e.g. a mouse or keyboard) — confirm `tuh_hid_mount_cb`/`tuh_hid_report_received_cb` fire and raw report bytes can be printed over serial

## 5. CRKD Controller Report Capture & Decoding

- [ ] 5.1 Connect the CRKD Gibson Les Paul controller to a PC in PC mode; capture its USB HID report descriptor and sample input reports (e.g. via `usbhid-dump`, Wireshark USBPcap, or equivalent)
- [ ] 5.2 Confirm the CRKD enumerates as a standard HID gamepad in PC mode (not XInput); document actual report byte layout for frets, strum, start/select
- [ ] 5.3 Capture and document the whammy bar's report format (rest value, direction/polarity, bit width) and tilt's report format (analog axis vs. digital bit)
- [ ] 5.4 Connect the CRKD controller to the Pico's USB host (via powered hub) and print raw HID reports over serial to confirm the same report format holds on-device
- [ ] 5.5 Implement HID report decoding (frets, strum, start/select, whammy, tilt) into the normalized guitar input state per the `usb-guitar-input` spec, including malformed-report and disconnect fail-safe handling

## 6. PS2 Analog Mode Protocol (`ps2-analog-mode`)

- [ ] 6.1 Implement config-mode entry/exit (`0x43`) and the escape-mode (`0xF3`) poll response
- [ ] 6.2 Implement `0x44` mode switching between digital (`0x41`, 5-byte) and analog (`0x73`, 9-byte) response, defaulting to digital at boot
- [ ] 6.3 Implement the analog poll response format (button bytes + 4 analog axis bytes, centered at `0x80`)
- [ ] 6.4 Implement benign dummy responses for `0x45`/`0x46`/`0x47`/`0x4D`; confirm `0x4F` is ignored and the full analog frame is always returned
- [ ] 6.5 Flash to Pico, verify against real PS2/game that mode-switching and analog stick bytes are recognized correctly

## 7. Guitar-to-PS2 Mapping & Full Integration (`guitar-to-ps2-mapping`)

- [ ] 7.1 Implement the guitar-to-PS2 mapping as a data-driven config table (frets, strum, start/select -> buttons; whammy -> right stick Y, tilt -> left stick Y or a button bit if digital), per the `guitar-to-ps2-mapping` spec
- [ ] 7.2 Wire the normalized guitar input state into the shared state struct from task 3.1, replacing the hardcoded value from task 3.5
- [ ] 7.3 End-to-end test: with the Pico wired to a real PS2 and the CRKD controller connected via powered hub, confirm each fret/strum/start/select/whammy/tilt maps to the correct PS2 input in a game or controller test screen
- [ ] 7.4 Confirm idle state (no input) produces no stuck/phantom button presses and centered (`0x80`) analog axes

## 8. Documentation

- [ ] 8.1 Write wiring documentation (pinout, verified wire colors, power scheme diagram) under `docs/`
- [ ] 8.2 Write build/flash instructions and the bring-up/test procedure used for verification
- [ ] 8.3 Document the future Bluetooth-extensibility provisions made (reserved PIO block/DMA channel, source/connection-state enum, core1 lock-free constraint) so a later change can build on them without rediscovering the rationale
```

## openspec/changes/ps2-guitar-adapter/specs/guitar-to-ps2-mapping/spec.md

- Source: openspec/changes/ps2-guitar-adapter/specs/guitar-to-ps2-mapping/spec.md
- Lines: 1-46
- SHA256: de46df65c7d8d114f10761d0ae5dccf24461c6f80ec787a8c6aec2dd114ab06d

```md
## ADDED Requirements

### Requirement: Guitar-to-PS2 Button Translation
The system SHALL translate the normalized guitar input state (from `usb-guitar-input`) into the PS2 button bitmask (from `ps2-protocol`) using a fixed mapping: Green fret -> X, Red fret -> Circle, Yellow fret -> Triangle, Blue fret -> Square, Orange fret -> L1, Strum up -> Up, Strum down -> Down, Start -> Start, Select -> Select.

#### Scenario: Fret mapped to correct face button
- **WHEN** the normalized guitar input state shows the Green fret active
- **THEN** the PS2 button bitmask has the X button bit cleared (pressed) and all other bits unaffected

#### Scenario: Strum direction mapped to d-pad
- **WHEN** the normalized guitar input state shows strum-down active
- **THEN** the PS2 button bitmask has the Down button bit cleared (pressed)

#### Scenario: Idle guitar state produces idle PS2 state
- **WHEN** the normalized guitar input state is fully idle (no frets, no strum, no start/select)
- **THEN** the PS2 button bitmask is `0xFF, 0xFF` (no buttons pressed)

#### Scenario: Multiple simultaneous inputs
- **WHEN** more than one guitar input is active at once (e.g. a fret held plus a strum)
- **THEN** all corresponding PS2 button bits are cleared simultaneously without interfering with each other

### Requirement: Whammy and Tilt Analog Mapping
The system SHALL translate the normalized guitar input state's whammy bar value onto the PS2 analog controller's right stick Y axis, and the tilt (star power) value onto the left stick Y axis when tilt is reported as an analog value, scaling and centering both so that the guitar's rest position corresponds to the PS2 analog rest value `0x80`. If tilt is instead reported as a digital (on/off) value, the system SHALL map it as an additional digital button bit rather than an analog axis.

#### Scenario: Whammy at rest maps to centered axis
- **WHEN** the normalized guitar input state's whammy value is at rest
- **THEN** the PS2 right stick Y axis byte is `0x80`

#### Scenario: Whammy fully engaged maps to axis extreme
- **WHEN** the normalized guitar input state's whammy value is at its maximum
- **THEN** the PS2 right stick Y axis byte reflects the corresponding extreme of its range

#### Scenario: Analog tilt at rest maps to centered axis
- **WHEN** tilt is reported as an analog value and the normalized guitar input state's tilt value is at rest
- **THEN** the PS2 left stick Y axis byte is `0x80`

#### Scenario: Digital tilt maps to a button bit
- **WHEN** tilt is reported as a digital (on/off) value rather than analog
- **THEN** the system maps it to an unused PS2 button bit instead of an analog axis

### Requirement: Data-Driven Mapping Table
The system SHALL implement the guitar-to-PS2 mapping (buttons and analog axes) as a data-driven configuration table associating each normalized guitar input field with its corresponding PS2 button or axis, rather than as inline conditional logic, scoped to the CRKD Gibson Les Paul controller for this change.

#### Scenario: Mapping defined by table lookup
- **WHEN** the system translates a normalized guitar input state into PS2 output
- **THEN** each field's translation is determined by consulting the configuration table rather than a hardcoded conditional expression specific to that field
```

## openspec/changes/ps2-guitar-adapter/specs/ps2-analog-mode/spec.md

- Source: openspec/changes/ps2-guitar-adapter/specs/ps2-analog-mode/spec.md
- Lines: 1-56
- SHA256: 50627f7cdcc989f3c41b44267b2c7a4fe0a0366be4e9f3e28ca12f73e92b764b

```md
## ADDED Requirements

### Requirement: Config Mode Entry and Exit
The system SHALL enter config (escape) mode upon receiving the command sequence `0x43 0x00 0x01`, during which subsequent polls are answered with escape-mode identification (`0xF3`) rather than the normal digital/analog response, and SHALL exit config mode upon receiving `0x43 0x00 0x00`, resuming normal poll responses.

#### Scenario: Console enters config mode
- **WHEN** the console sends `0x43 0x00 0x01`
- **THEN** the system responds appropriately and subsequent `0x42` polls are answered with the `0xF3` escape-mode identification rather than a normal digital or analog response

#### Scenario: Console exits config mode
- **WHEN** the console sends `0x43 0x00 0x00` while in config mode
- **THEN** the system exits config mode and subsequent `0x42` polls resume the normal digital or analog response per current mode

### Requirement: Mode Switching via 0x44
The system SHALL support the `0x44` config sub-command to switch the controller's identification and poll response between digital (`0x41`, 5-byte response) and analog (`0x73`, 9-byte response) mode, defaulting to digital mode at power-on.

#### Scenario: Switch to analog mode
- **WHEN** the console sends `0x44` with a data byte selecting analog mode while in config mode
- **THEN** subsequent `0x42` polls (after exiting config mode) are answered with `0x73` identification and the 9-byte analog response format

#### Scenario: Switch to digital mode
- **WHEN** the console sends `0x44` with a data byte selecting digital mode while in config mode
- **THEN** subsequent `0x42` polls are answered with `0x41` identification and the 5-byte digital response format

#### Scenario: Default mode at power-on
- **WHEN** the system powers on and no `0x44` command has yet been received
- **THEN** the system responds in digital mode by default

### Requirement: Analog Poll Response Format
The system SHALL, while in analog mode, respond to `0x42` polls with `0xFF, 0x73, 0x5A`, followed by the two button bytes, followed by four analog stick bytes (right stick X, right stick Y, left stick X, left stick Y), each centered at `0x80` at rest.

#### Scenario: Analog poll response includes stick bytes
- **WHEN** the system is in analog mode and receives a standard poll
- **THEN** the response includes the button bytes followed by four analog axis bytes in right-X, right-Y, left-X, left-Y order

#### Scenario: Unused analog axes remain centered
- **WHEN** an analog axis has no corresponding mapped input active
- **THEN** its byte value in the poll response is `0x80`

### Requirement: Config Sub-Command Dummy Responses
The system SHALL respond to the `0x45`, `0x46`, `0x47`, and `0x4D` config sub-commands with a well-formed, fixed-length response while in config mode, without necessarily matching exact DualShock2 firmware identification data, so that games probing these sub-commands do not stall or misbehave.

#### Scenario: 0x45 identification query
- **WHEN** the console sends `0x45` while in config mode
- **THEN** the system responds with a plausible, well-formed identification response reflecting current digital/analog mode state

#### Scenario: 0x46/0x47/0x4D queried
- **WHEN** the console sends `0x46`, `0x47`, or `0x4D` while in config mode
- **THEN** the system responds with a well-formed, fixed-length dummy response rather than stalling or NAKing

### Requirement: Unsupported Variable Poll Format
The system SHALL NOT support the `0x4F` command's custom poll-response formatting; it SHALL always respond with the full analog frame (button bytes plus all four analog axis bytes) regardless of any `0x4F` configuration attempt.

#### Scenario: Console attempts to customize poll format
- **WHEN** the console sends `0x4F` with a custom format bitmask while in config mode
- **THEN** the system continues to respond to subsequent polls with the full analog frame, ignoring the requested customization
```

## openspec/changes/ps2-guitar-adapter/specs/ps2-protocol/spec.md

- Source: openspec/changes/ps2-guitar-adapter/specs/ps2-protocol/spec.md
- Lines: 1-66
- SHA256: 3de29c0f4741511d69cce1305d97fb5ab228f095897f5454765cfa32f356a507

```md
## ADDED Requirements

### Requirement: PS2 SPI-Slave Poll Response
The system SHALL respond to the PS2 console's standard digital-controller poll sequence (`0x01`, `0x42`, `0x00`, `0x00`, `0x00`) with the correct response bytes (`0xFF`, `0x41`, `0x5A`, button byte 0, button byte 1), sampling COMMAND/clock and driving DATA per SPI Mode 3 (CPOL=1, CPHA=1) using PIO so timing is independent of CPU load.

#### Scenario: Console polls and controller identifies as digital pad
- **WHEN** the PS2 console asserts ATT/CS low and clocks the standard poll sequence
- **THEN** the system drives `0xFF` in response to `0x01`, `0x41` in response to `0x42`, and `0x5A` in response to the following byte, followed by the two button bytes

#### Scenario: Response begins within required latency
- **WHEN** ATT/CS goes low
- **THEN** the system begins driving the DATA line within the PS2-required response window (~15-20us) so the console does not time out the poll

### Requirement: ACK Line Assertion
The system SHALL assert the ACK line low after each byte transferred (except the final byte of the exchange) to indicate the controller is present and has more data to send, matching real PS2 controller behavior rather than omitting ACK.

#### Scenario: ACK pulsed after each byte
- **WHEN** a byte has been fully clocked in/out during an active poll exchange
- **THEN** the system pulses ACK low for the PS2-specified duration before releasing it, for every byte except the last

#### Scenario: No ACK after final byte
- **WHEN** the last button byte of the poll response has been sent
- **THEN** the system does not assert ACK, signaling end of the exchange

### Requirement: Button Bitmask Encoding
The system SHALL encode controller button state into the two-byte active-low bitmask matching the standard PS2 digital controller layout (byte 0: Select/L3/R3/Start/Up/Right/Down/Left; byte 1: L2/R2/L1/R1/Triangle/Circle/X/Square), where a 0 bit indicates the button is pressed.

#### Scenario: No buttons pressed
- **WHEN** no mapped input is active
- **THEN** both button bytes are `0xFF`

#### Scenario: Single button pressed
- **WHEN** exactly one mapped button's input is active
- **THEN** only that bit is cleared to 0 in the corresponding byte, all others remain 1

### Requirement: ACK Line Electrical Behavior
The system SHALL assert ACK by driving the line low and release it to a high-impedance (input) state rather than driving it high, since ACK is an open-collector line relying on the console's pull-up resistor for its released state.

#### Scenario: ACK released to high-impedance
- **WHEN** the system releases ACK after a pulse
- **THEN** the corresponding GPIO is set to input (Hi-Z), not driven high, allowing the console's pull-up to restore the line

### Requirement: Aborted Transaction Recovery
The system SHALL resynchronize to the start of the response sequence whenever ATT/CS deasserts before a poll response has fully completed, so that the next poll is handled correctly regardless of how the previous exchange ended.

#### Scenario: ATT rises mid-byte
- **WHEN** ATT/CS goes high before all response bytes have been clocked out
- **THEN** the system abandons the in-progress response and resets its state machine to await the next ATT/CS assertion

#### Scenario: Next poll after an aborted transaction succeeds normally
- **WHEN** a new poll sequence begins after a previous aborted transaction
- **THEN** the system responds correctly per the standard poll response requirement, with no residual state from the aborted exchange

### Requirement: Power-On Idle State
The system SHALL present a valid, idle (no buttons pressed) controller response to PS2 polls from power-on, before any USB (or other) input source has connected, rather than failing to respond or responding with undefined data.

#### Scenario: Poll received before input source connects
- **WHEN** the PS2 console polls the controller before the guitar controller (or any input source) has been detected
- **THEN** the system responds as a valid digital controller with both button bytes `0xFF`

### Requirement: Unrecognized Command Handling
The system SHALL respond to any command byte it does not specifically implement (other than the standard poll and defined config-mode commands) with a benign, fixed-length dummy response rather than hanging or corrupting the exchange's byte count.

#### Scenario: Unknown command byte received
- **WHEN** the console sends a command byte the system does not recognize
- **THEN** the system responds with a well-formed but inert response of the expected length, and remains ready for the next poll
```

## openspec/changes/ps2-guitar-adapter/specs/usb-guitar-input/spec.md

- Source: openspec/changes/ps2-guitar-adapter/specs/usb-guitar-input/spec.md
- Lines: 1-38
- SHA256: b34cbd481e30313c26d07c62faf0661cb9001d0a30124cd8a26314eaf38f93ec

```md
## ADDED Requirements

### Requirement: CRKD Controller Enumeration
The system SHALL enumerate the CRKD Gibson Les Paul guitar controller (in PC mode) as a USB HID device via TinyUSB host and successfully mount it for report reception.

#### Scenario: Controller mounted in PC mode
- **WHEN** the CRKD controller is connected in PC mode to the Pico's USB host port (via powered hub)
- **THEN** TinyUSB's `tuh_hid_mount_cb` fires and the system begins requesting/receiving input reports from the device

#### Scenario: Controller not in PC mode
- **WHEN** the CRKD controller is connected in a non-PC mode (e.g. Xbox mode)
- **THEN** the system SHALL NOT crash or hang; it MAY fail to decode meaningful input but must remain otherwise operational (PS2 side still responds to polls)

### Requirement: HID Report Decoding
The system SHALL decode the CRKD controller's HID input reports into a normalized in-memory guitar input state (fret buttons, strum up/down, start, select, whammy bar, tilt), based on the device's actual captured report format rather than an assumed layout.

#### Scenario: Fret press reflected in normalized state
- **WHEN** a HID input report is received with a fret button bit set
- **THEN** the corresponding field in the normalized guitar input state is marked active

#### Scenario: Report with no input
- **WHEN** a HID input report indicates no buttons/strum active
- **THEN** the normalized guitar input state reflects a fully released/idle state

#### Scenario: Malformed or short report
- **WHEN** a HID input report is received that is shorter than expected or otherwise malformed
- **THEN** the system does not crash and treats the normalized guitar input state as idle for that update

### Requirement: Controller Disconnect Fail-Safe
The system SHALL reset the normalized guitar input state to fully idle when the CRKD controller disconnects, and SHALL resume normal decoding if it reconnects.

#### Scenario: Controller disconnects mid-session
- **WHEN** the CRKD controller is unmounted (e.g. `tuh_hid_umount_cb` fires)
- **THEN** the normalized guitar input state is set to fully idle (no frets, no strum, no start/select, whammy and tilt at rest)

#### Scenario: Controller reconnects
- **WHEN** the CRKD controller mounts again after a disconnect
- **THEN** the system resumes decoding HID reports into the normalized guitar input state as normal
```

