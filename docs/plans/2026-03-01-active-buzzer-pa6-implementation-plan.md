# Active Buzzer PA6 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add an active buzzer driver on PA6 (timer PWM) and trigger a non-blocking 80ms beep on button press events.

**Architecture:** Add a board-level PWM alias for buzzer on PA6, add a platform buzzer abstraction (`init/on/off`) backed by Zephyr PWM, and integrate it into `ButtonService` using `k_work_delayable` for auto-off timing. Keep behavior minimal and deterministic.

**Tech Stack:** Zephyr RTOS, Devicetree, PWM driver API (`pwm_dt_spec`, `pwm_set_dt`), Zephyr kernel workqueue (`k_work_delayable`), C++.

---

### Task 1: Board DTS Buzzer Node

**Files:**
- Modify: `boards/lckfb_sky_board_stm32f407.overlay`
- Verify reference: `boards/st/lckfb_sky_board_stm32f407/lckfb_sky_board_stm32f407.dts`

**Step 1: Write the failing test**

Add temporary compile-time use of alias in future driver code:

```cpp
#define BUZZER_PWM_NODE DT_ALIAS(buzzer_pwm0)
static_assert(DT_NODE_HAS_STATUS(BUZZER_PWM_NODE, okay), "buzzer alias missing");
```

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 . -p always`  
Expected: FAIL with devicetree alias missing for `buzzer_pwm0`.

**Step 3: Write minimal implementation**

In overlay, add buzzer alias and timer/pwm pinctrl on PA6:

```dts
/ {
  aliases {
    buzzer-pwm0 = &buzzer_pwm0;
  };
};
```

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 . -p always`  
Expected: PASS for devicetree generation and compile stage.

**Step 5: Commit**

```bash
git add boards/lckfb_sky_board_stm32f407.overlay
git commit -m "feat: add PA6 timer PWM alias for active buzzer"
```

### Task 2: Platform Buzzer Interface

**Files:**
- Create: `include/platform/platform_buzzer.hpp`
- Modify: `app/app_Init.cpp`

**Step 1: Write the failing test**

Add temporary call in `app_Init.cpp`:

```cpp
ret = platform::buzzer_init();
```

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: FAIL with missing declaration/link for `platform::buzzer_init`.

**Step 3: Write minimal implementation**

Create header with API:

```cpp
namespace platform {
int buzzer_init() noexcept;
int buzzer_on() noexcept;
int buzzer_off() noexcept;
}
```

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: compile can resolve symbols after implementation in next task.

**Step 5: Commit**

```bash
git add include/platform/platform_buzzer.hpp app/app_Init.cpp
git commit -m "feat: add platform buzzer API"
```

### Task 3: Zephyr Buzzer Backend

**Files:**
- Create: `subsys/platform/zephyr_buzzer.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the failing test**

Implement API in header only (no source), then build.

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: FAIL at link stage for unresolved `buzzer_*` functions.

**Step 3: Write minimal implementation**

Implement driver with `PWM_DT_SPEC_GET(DT_ALIAS(buzzer_pwm0))`, readiness check, and safe off:

```cpp
int buzzer_on() noexcept { return pwm_set_dt(&buzzer_pwm, buzzer_pwm.period, buzzer_pwm.period / 2U); }
int buzzer_off() noexcept { return pwm_set_dt(&buzzer_pwm, buzzer_pwm.period, 0U); }
```

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: PASS compile/link.

**Step 5: Commit**

```bash
git add subsys/platform/zephyr_buzzer.cpp CMakeLists.txt
git commit -m "feat: implement zephyr active buzzer backend"
```

### Task 4: Integrate with ButtonService (80ms Beep)

**Files:**
- Modify: `include/servers/button_service.hpp`
- Modify: `subsys/servers/button_service.cpp`

**Step 1: Write the failing test**

Add expected call path in button callback handling before implementation:

```cpp
if (pressed) { trigger_beep(); }
```

**Step 2: Run test to verify it fails**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: FAIL with missing `trigger_beep`/delay work symbols.

**Step 3: Write minimal implementation**

Add non-blocking delayed off flow:

```cpp
k_work_reschedule(&buzzer_off_work_, K_MSEC(80));
```

and worker callback calling `platform::buzzer_off()`.

**Step 4: Run test to verify it passes**

Run: `west build -b lckfb_sky_board_stm32f407 .`  
Expected: PASS and no thread-blocking code (`k_msleep`) in button event loop.

**Step 5: Commit**

```bash
git add include/servers/button_service.hpp subsys/servers/button_service.cpp
git commit -m "feat: add 80ms active buzzer beep on button press"
```

### Task 5: Validation and OpenSpec Sync

**Files:**
- Modify: `openspec/changes/add-buzzer-driver-pa6-timer/tasks.md` (mark completed items during execution)
- Optional notes: `OPENSPEC_使用指南.md` (if workflow notes are updated)

**Step 1: Write the failing test**

Run strict OpenSpec validation before final adjustments.

**Step 2: Run test to verify it fails**

Run: `openspec validate --type change add-buzzer-driver-pa6-timer --strict --no-interactive`  
Expected: FAIL if any artifact or wording drift appears during implementation.

**Step 3: Write minimal implementation**

Fix artifact drift, update tasks checkboxes, and align implementation with spec language.

**Step 4: Run test to verify it passes**

Run:
- `openspec validate --type change add-buzzer-driver-pa6-timer --strict --no-interactive`
- `west build -b lckfb_sky_board_stm32f407 . -p always`

Expected: both PASS.

**Step 5: Commit**

```bash
git add openspec/changes/add-buzzer-driver-pa6-timer
git commit -m "docs: sync openspec artifacts for active buzzer feature"
```
