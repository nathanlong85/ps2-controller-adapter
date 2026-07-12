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
