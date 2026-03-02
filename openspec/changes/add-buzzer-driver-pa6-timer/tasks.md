## 1. Board and Configuration

- [x] 1.1 Add buzzer PWM node for PA6 timer channel in board DTS/overlay and define a stable alias
- [x] 1.2 Confirm required PWM-related Kconfig options are enabled for the target board

## 2. Platform Buzzer Driver

- [x] 2.1 Add platform buzzer interface header in `include/platform/` (init, on(freq,duty), off)
- [x] 2.2 Implement Zephyr buzzer driver in `subsys/platform/` using `pwm_dt_spec` and `pwm_set_dt`
- [x] 2.3 Implement parameter clipping for out-of-range `freq_hz` and `duty_percent`
- [x] 2.4 Add safe output-off behavior and error logging on all failure paths

## 3. Integration

- [x] 3.1 Expose a global platform accessor for buzzer consistent with existing platform modules
- [x] 3.2 Keep scope driver-only: no ButtonService integration and no dip-switch runtime read
- [x] 3.3 Provide minimal call-site demonstration path (manual test call) outside business service logic if needed

## 4. Verification

- [x] 4.1 Build firmware for `lckfb_sky_board_stm32f407` and confirm no DTS/PWM compile errors
- [ ] 4.2 Validate runtime behavior: buzzer on/off works and out-of-range parameters are clipped
- [x] 4.3 Run `openspec validate --type change add-buzzer-driver-pa6-timer --strict` and resolve any issues
