## ADDED Requirements

### Requirement: PCA9555 device node on I2C1 with fixed address

The system SHALL define a PCA9555 device on `i2c1` at address `0x20` in board devicetree and SHALL expose a stable alias for runtime lookup.

#### Scenario: Device is discoverable by alias

- **WHEN** the firmware initializes the PCA9555 platform module
- **THEN** the module SHALL resolve the PCA9555 device via devicetree alias and confirm the device is ready

### Requirement: Fixed port semantics for DIP input and LED output

The system SHALL configure PCA9555 Port0 (`io0_0` to `io0_7`) as input lines for DIP switches and SHALL configure Port1 (`io1_0` to `io1_7`) as output lines for white LEDs.

#### Scenario: Initialization sets port directions and safe output

- **WHEN** the PCA9555 platform module is initialized
- **THEN** Port0 SHALL be configured as input
- **AND** Port1 SHALL be configured as output
- **AND** Port1 output bitmask SHALL default to all-off

### Requirement: DIP switch read interface

The system SHALL provide an API to read the current 8-bit DIP switch snapshot from PCA9555 Port0.

#### Scenario: Caller reads DIP snapshot

- **WHEN** a caller requests DIP state from the platform interface
- **THEN** the system SHALL return one 8-bit value representing `io0_0` to `io0_7`

### Requirement: White LED write interface

The system SHALL provide an API to set the 8-bit white LED output bitmask on PCA9555 Port1.

#### Scenario: Caller updates LED mask

- **WHEN** a caller writes an LED bitmask to the platform interface
- **THEN** the system SHALL apply the bitmask to Port1 outputs `io1_0` to `io1_7`

### Requirement: Non-blocking failure behavior with diagnostics

The system SHALL keep startup running when PCA9555 initialization or runtime access fails and SHALL emit diagnostic logs containing the failure stage and error code.

#### Scenario: PCA9555 is missing or access fails

- **WHEN** initialization or read/write operation returns an error
- **THEN** the system SHALL log a diagnostic error
- **AND** the system SHALL continue startup without terminating `app_Init()`
