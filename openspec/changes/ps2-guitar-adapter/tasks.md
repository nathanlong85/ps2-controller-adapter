## 1. Hardware Prep & Toolchain Bring-Up

- [x] 1.1 Retrieve Pico 2W, CRKD Gibson Les Paul controller, PS2 console/test harness, powered USB hub, and PS2 controller cable from storage
- [x] 1.2 Verify VS Code + official Raspberry Pi Pico extension toolchain still works: build and flash the "blink" example to the Pico 2W
- [x] 1.3 Verify PS2 cable wire colors against the documented pinout using a multimeter; record any deviations (matched standard convention, no deviations)

## 2. PS2 Protocol Wiring Smoke Test (no PIO yet)

- [x] 2.1 Write minimal GPIO polling code to detect ATT/CS going low and CLOCK pulses from a real PS2 console, printing observations over serial
- [x] 2.2 Confirm via serial output that wiring (DATA/COMMAND/ATT/CLOCK/GND) is correct before proceeding to PIO (16 clock pulses/transaction over serial, matching a logic-analyzer capture — see commit for root causes found and fixed along the way)

## 3. Shared-State Design & PIO PS2 SPI-Slave Implementation (`ps2-protocol`)

- [x] 3.1 Design and implement the core0/core1 shared-state struct (button bits, analog axes, mode/config state) using a lock-free double-buffer/seqlock pattern; keep core1 heap-free with no blocking/shared SDK locks
- [x] 3.2 Implement PIO program as a pure clocked shift-register/ACK engine: sample COMMAND on clock rising edge, drive DATA on clock falling edge, respond to ATT/CS going low; response content/length supplied by C, not hardcoded in PIO
- [x] 3.3 Implement ACK line assertion in PIO after each byte except the last, releasing to high-impedance (not driving high) to correctly emulate the open-drain line
- [x] 3.4 Implement ATT-abort recovery: resync the PIO state machine to await the next poll if ATT/CS rises before a response completes
- [x] 3.5 Implement the standard digital-controller poll/response sequence (`0x01`/`0xFF`, `0x42`/`0x41`, `0x5A`, button byte 0, button byte 1) with a hardcoded fixed button_state, including power-on idle response before any input source connects
- [x] 3.6 Flash to Pico, verify against real PS2 that it's recognized as a connected digital controller (confirmed via logic analyzer against a real PS2: full 5-byte digital-pad response `ff 41 5a ff ff` with correct ACK handshaking on every poll; a secondary USB-PS2 adapter used for convenience testing truncated reads regardless of controller ID reported — isolated to that adapter's own limitation, not our firmware)
- [x] 3.7 Wire a physical pushbutton to a spare GPIO, toggle a single bit in button_state on press, and confirm the PS2 sees the corresponding button press end-to-end (confirmed via logic analyzer against the real PS2: `button_byte1` correctly read as `0xbf` [Cross/X pressed] across 3 consecutive polls while the test button was held, `0xff` idle otherwise)

## 4. USB Host Bring-Up (`usb-guitar-input`, generic)

- [x] 4.1 Integrate TinyUSB host into the project alongside the PIO code; confirm `tuh_task()` runs without disrupting PIO PS2 timing
- [x] 4.2 Verify basic USB host function using a simple HID device (e.g. a mouse or keyboard) — confirm `tuh_hid_mount_cb`/`tuh_hid_report_received_cb` fire and raw report bytes can be printed over serial

## 5. CRKD Controller Report Capture & Decoding

- [x] 5.1 Connect the CRKD Gibson Les Paul controller to a PC in PC mode; capture its USB HID report descriptor and sample input reports (e.g. via `usbhid-dump`, Wireshark USBPcap, or equivalent) (captured via a custom hidapi tool on macOS, VID 0x0351; "PC" slider position is actually XInput (PID 0x1000, bDeviceClass=0xFF) — the standard-HID mode is "Android" slider + dial 1-8 (PID 0x5500, bDeviceClass=0x00); see progress ledger for full descriptor)
- [x] 5.2 Confirm the CRKD enumerates as a standard HID gamepad in PC mode (not XInput); document actual report byte layout for frets, strum, start/select (confirmed HID in Android-slider mode, not "PC" slider mode which is XInput; frets=byte0 bits 0/1/3/4/6, strum=byte2 hat switch 0/4, start/select=byte1 bits 3/2)
- [x] 5.3 Capture and document the whammy bar's report format (rest value, direction/polarity, bit width) and tilt's report format (analog axis vs. digital bit) (whammy = byte7, 8-bit unsigned analog 0x00 rest to 0xff full depth, shares axis with LT; tilt = byte2 hat switch value 6, digital/non-directional, NOT analog — initial analog-looking readings were cross-talk from the left analog stick)
- [x] 5.4 Connect the CRKD controller to the Pico's USB host (via powered hub) and print raw HID reports over serial to confirm the same report format holds on-device (confirmed on real hardware; the original hang was a bad powered hub, not firmware — see progress ledger for the full isolation and root-cause writeup)
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
