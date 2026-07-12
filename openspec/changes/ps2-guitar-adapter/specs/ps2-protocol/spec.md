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
