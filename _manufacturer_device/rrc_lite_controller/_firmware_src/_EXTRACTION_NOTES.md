# RRC Lite firmware — source-only extraction notes

`RosRobotControllerLite_ros/` is the **STM32F407 firmware that runs on the RRC Lite** — the controller
this project replaces. It is the implementation behind the Lesson-13/14/15 protocol docs one level up.

## Provenance
- **Snapshot:** the `RosRobotControllerLite_ros.zip` bundled with **Lesson 14** ("Communication with the
  Host Computer – Sending"). Hiwonder ships a full project copy with each lesson; Lesson 14 is among the
  latest, so it carries the complete feature set — host-comm protocol **plus** motor / encoder / PID /
  servo / IMU / chassis / battery code.
- **Origin:** `MentorPi A1 & M1 2024-…-3-004.zip` → `…/RRC Lite Controller/3. RRCLite Program Analysis/
  Lesson 14 …/RosRobotControllerLite_ros.zip` (a 100 MB nested zip; 2357 files inc. `.git` + build output).
- Code comments are in Chinese (author tag: *LuYongPing*, 2023–2024).

## What was kept vs dropped
**Kept (3.0 MB, 147 files)** — everything vendor-specific + what makes the project legible:
- `Hiwonder/` — **all of Hiwonder's own code** (the high-value part; see map below)
- `Core/` — CubeMX-generated `main.c`, `stm32f4xx_it.c` (ISRs), `freertos.c`, `Core/Inc/main.h` (pin `#define`s), `FreeRTOSConfig.h`
- `Middlewares/` — FreeRTOS source (1.5 MB)
- `MDK-ARM/` — Keil project + linker scatter file
- `RosRobotControllerM4.ioc` — **STM32CubeMX pin/peripheral map** (open in CubeMX to see the canonical pinout)
- `Doc/`, `Doxyfile`, `README.md`, `.mxproject`, `.gitignore`, `armclang.bat`, `clean_keil.bat`, `ros_robot_controller_m4.jflash`, images

**Dropped (≈151 MB, generic & regenerable):**
- `Drivers/` (118 MB) — ST STM32F4xx HAL + CMSIS (incl. the huge CMSIS-DSP/NN libs). Stock ST/ARM code.
- `Third_Party/` (33 MB) — u8g2 (OLED), lwmem, lwrb, etc. Stock upstream libraries.
- `.git/` history and all build artifacts (`.o .crf .d .axf .map .htm .lnp …`, JLink logs).

**To rebuild:** open `RosRobotControllerM4.ioc` in STM32CubeMX to regenerate `Drivers/` (and the project
skeleton), or drop in ST's STM32F4 HAL + CMSIS; pull the `Third_Party` libs from their upstreams; build
with Keil MDK-ARM (`MDK-ARM/`) or `armclang.bat`. Nothing *vendor-specific* was removed.

## Where to look first (high-value file map)
| Concern | Files |
|---|---|
| **Host↔controller serial protocol** (the interface we replace) | `Hiwonder/System/packet_handle.c/.h`, `Hiwonder/Misc/packet.c/.h`, `Hiwonder/Portings/packet_porting.c`, `Hiwonder/Portings/packet_reports.h` |
| Frame **checksum/CRC** | `Hiwonder/Misc/checksum.c/.h` |
| **Mecanum kinematics** (body twist → wheel speeds) | `Hiwonder/Chassis/mecanum_chassis.c/.h`, `Hiwonder/Portings/chassis_porting.c` |
| **Motor drive + encoder + per-motor params** | `Hiwonder/Peripherals/encoder_motor.c`, `Hiwonder/Portings/motor_porting.c`, `Hiwonder/Portings/motors_param.h` |
| **PID** | `Hiwonder/Misc/pid.c/.h` |
| **IMU** (QMI8658) | `Hiwonder/Peripherals/QMI8658.c`, `QMI8658reg.h`, `Hiwonder/Portings/imu_porting.c` |
| **Servos** (PWM + serial bus) | `Hiwonder/Peripherals/pwm_servo.c`, `serial_servo.c`, `Hiwonder/Portings/*servo*` |
| **Battery / ADC voltage** | `Hiwonder/System/battery_handle.c` |
| **App init + board config** | `Hiwonder/System/app.c`, `global.h`, `global_conf.h` |
| **Pin map / clocks / peripherals** | `RosRobotControllerM4.ioc` (CubeMX), `Core/Inc/main.h`, `Core/Src/stm32f4xx_hal_msp.c` |

## Facts already harvested from the source (primary-source confirmations)
- **`Hiwonder/Portings/motors_param.h`** — encoder ticks/rev and stock PID gains by motor type:
  - **`JGA27` (this kit's "310" motor): 13 pulses × 4 (quadrature) × 20 (gearbox) = `1040.0` ticks/rev** —
    matches `_engineering/`'s `COUNTS_PER_REV ≈ 1040`. Stock PID: `Kp −36, Ki −1, Kd −1`, RPS limit 6.0.
  - JGB520 → 3960 (11×4×90); JGB37 → 1980 (11×4×45); JGB528 → 5764 (11×4×131).
- **`Hiwonder/Misc/packet.h`** — authoritative `enum PACKET_FUNCTION` (the Lesson-13 PDF only documented 1–7):
  `SYS=0, LED=1, BUZZER=2, MOTOR=3, PWM_SERVO=4, BUS_SERVO=5, KEY=6, IMU=7, GAMEPAD=8, SBUS=9, OLED=10, RGB=11`.
  Frame start bytes `0xAA 0x55`; DMA double-buffered state-machine parser.

> These are the **vendor's** values. Per the project's golden rules they remain *claims to verify on the
> bench* (the JGA27 1040 figure in particular is still to be measured empirically in Phase 2) — but they
> agree with `_engineering/`, which is a strong cross-check.
