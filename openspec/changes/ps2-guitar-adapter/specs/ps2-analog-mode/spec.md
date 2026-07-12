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
