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
