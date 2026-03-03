## ADDED Requirements

### Requirement: Active buzzer timer output on PA6

The system SHALL provide active buzzer output using a timer-backed PWM signal on pin PA6 through
board devicetree configuration.

#### Scenario: Buzzer device is ready after boot

- **WHEN** the platform initializes the buzzer driver
- **THEN** the driver SHALL resolve the configured PWM device for PA6 and report ready state

### Requirement: Active buzzer control interface with parameters

The system SHALL provide APIs to initialize, enable, and disable buzzer output. The enable API SHALL
accept `freq_hz` and `duty_percent` parameters.

#### Scenario: Enable and disable buzzer with requested parameters

- **WHEN** a caller enables buzzer output with `freq_hz` and `duty_percent` and then disables it
- **THEN** the driver SHALL output PWM while enabled and SHALL stop output after disable is called

### Requirement: Out-of-range parameter clipping

The system SHALL automatically clip out-of-range `freq_hz` and `duty_percent` values to
implementation-defined safe ranges.

#### Scenario: Caller passes out-of-range values

- **WHEN** a caller passes `freq_hz` or `duty_percent` outside supported limits
- **THEN** the driver SHALL apply clipped values and proceed with PWM output

### Requirement: Safe failure behavior

The system SHALL keep buzzer output disabled on initialization or runtime control failures.

#### Scenario: Buzzer init or control failure

- **WHEN** PWM device readiness check fails or on/off operation returns an error
- **THEN** the system SHALL keep buzzer output disabled and SHALL emit diagnostic logs
