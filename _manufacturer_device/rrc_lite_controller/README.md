# `rrc_lite_controller/` — RRC Lite deep-dive (docs + datasheets + firmware)

Everything about the **RRC Lite** (`RosRobotControllerLite`, STM32F407VET6) — the controller this project
replaces — from Hiwonder's own course + the shipped firmware. Extracted from the MentorPi 2024 archive.
Each `.pdf` has a co-located `.txt` (`pdftotext -layout`); read/grep the `.txt`, open the `.pdf` for
figures and the schematic.

> For the whole-robot view (how this board wires to the motors, servos, lidar, camera and the Pi), see
> **[`../mentorpi_m1_hardware.md`](../mentorpi_m1_hardware.md)**.

## What's here

- **`1. RRCLite Hardware Course/`**
  - `Lesson 1 … Hardware Introduction` — ★ board map: chips, ports, power, the 5 V/5 A Pi feed.
  - `Lesson 2 … Schematic Diagram Explanation/` — ★ schematic walk-through (names the **YX-4055AM** motor
    driver) + `Chip Manuals/` datasheets (**STM32F407VET6**, **CH9102F** USB-UART, **RT8289GSP** buck).
- **`2. STM32 Development Fundamentals/`** — toolchain: environment, build, flash, SWD, FreeRTOS.
- **`3. RRCLite Program Analysis/`** — what the firmware does, lesson by lesson:
  LED/buzzer (2), button (3), **ADC battery sense (4)**, **QMI8658 IMU (5)**, RGB (6), PWM servo (7–8),
  bus servo (9), **DC motor (10)**, **encoder (11)**, **PID (12)**, and the **host-comms protocol
  (13 ★★, 14, 15)** — the interface we replace.
- **`Appendix/Reference Tutorial/`** — extra chip datasheets (CH9102F, DC power chip, **VP230** CAN
  transceiver, **stm32f407ve** reference manual). PDF-only.
- **`_firmware_src/`** — ★ the STM32 firmware source (vendor `Hiwonder/` code, `Core/`, `.ioc` pin map),
  with provenance + a high-value file map in **[`_firmware_src/_EXTRACTION_NOTES.md`](_firmware_src/_EXTRACTION_NOTES.md)**.

## Highest-value for "replace the controller"
1. **Protocol:** Lesson 13 (`.txt`) + firmware `_firmware_src/.../Hiwonder/System/packet_handle.c`,
   `Misc/packet.c` (frame `0xAA 0x55 | func | len | data | CRC16`; func enum `SYS0…RGB11`; **1,000,000**
   baud; `/dev/ttyACM0`).
2. **Motors/encoders/PID:** Lessons 10–12 + `Hiwonder/Portings/motors_param.h` (JGA27 = **1040** ticks/rev;
   stock PID `Kp −36, Ki −1, Kd −1`), `Peripherals/encoder_motor.c`, `Misc/pid.c`.
3. **Pin/peripheral map:** `_firmware_src/.../RosRobotControllerM4.ioc` (open in STM32CubeMX) +
   `Core/Inc/main.h`.

*(All numbers here are **vendor facts to verify**, not project canon — `../README.md` and `_engineering/`
explain the rebuild's intentional divergences.)*
