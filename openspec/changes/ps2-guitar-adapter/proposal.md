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
