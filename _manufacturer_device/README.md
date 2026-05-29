# `_manufacturer_device/` — Hiwonder MentorPi M1 (manufacturer hardware reference)

This folder documents the **as-shipped Hiwonder MentorPi M1**, from the *manufacturer's* sources — both
the whole-robot hardware picture and a deep dive on the **RRC Lite** controller (the STM32 board this
project replaces). The robot is a **two-board design**: a **Raspberry Pi 5** (host / ROS2) plus the **RRC
Lite** (real-time motor / servo / IMU I/O), joined by one USB-serial link.

**Start here:**
- **[`mentorpi_m1_hardware.md`](mentorpi_m1_hardware.md)** ★ — the **whole robot**: every component plus
  the **interface / connection map** (what wires to the RRC Lite vs. the Pi, and over which bus). Read first.
- **[`rrc_lite_controller/`](rrc_lite_controller/)** — deep dive on the controller: vendor course
  (PDF→text), chip datasheets, and the STM32 firmware source. Covers what's on the board, what its firmware
  does, and the exact Pi↔controller serial protocol we replace.
- **[`robot_reference/`](robot_reference/)** — the manufacturer's robot PDFs (introduction, assembly,
  charging, startup, lidar, depth camera), each as PDF + `.txt`.

> ⚠️ **Raw vendor reference, not project truth.** `_engineering/` remains the source of truth (per the
> top-level `CLAUDE.md`). Treat every number here as a **manufacturer claim to verify** before it shapes a
> phase doc. Where vendor and our design differ on purpose (e.g. IMU, motor driver), that's flagged below.

---

## Navigation — folder layout

A synthesis doc (`mentorpi_m1_hardware.md`), the manufacturer's robot PDFs (`robot_reference/`), and the
controller deep-dive (`rrc_lite_controller/`, incl. a lean `_firmware_src/`). `★` = most load-bearing.

> **Every `*.pdf`/`*.PDF` below has a co-located `*.txt`** (`pdftotext -layout`). Read/grep the `.txt`;
> open the `.pdf` for figures, schematics and pin drawings (images don't survive text conversion).

```
_manufacturer_device/
├── README.md                         ← index (you are here)
├── mentorpi_m1_hardware.md           ★ WHOLE-ROBOT overview + interface/connection map — start here
├── robot_reference/                  manufacturer robot PDFs (each + .txt):
│   ├── 00. Important Notice Before Using the MentorPi Robot.pdf
│   ├── 1. Getting Ready/             1.1 Introduction · 1.2 Charging + Assembly · 1.3 Startup & Status
│   ├── 10. Lidar Lesson/             Lesson 1 Introduction · Lesson 2 Working & Ranging Principle
│   └── 11. Depth Camera Basic Lesson/  Lesson 1 Configuration
└── rrc_lite_controller/
    │
    ├── 1. RRCLite Hardware Course/
    │   ├── Lesson 1 RRCLite Hardware Introduction.pdf            ★ board map: chips, ports, power
    │   └── Lesson 2 RRCLite Schematic Diagram Explanation/
    │       ├── Lesson 2 RRCLite Schematic Explanation.pdf        ★ schematic walk-through
    │       └── Chip Manuals/                                     (datasheets — PDF + .txt)
    │           ├── STM32F407VET6_Datasheet.PDF                     the MCU
    │           ├── CH9102F_Datasheet.PDF                           USB-UART bridge
    │           └── RT8289GSP_Datasheet.PDF                         buck regulator
    │
    ├── 2. STM32 Development Fundamentals/                        toolchain (build/flash/debug the STM32)
    │   ├── 1. Set Development Environment/Development Environment Setup.pdf
    │   ├── 2. First Program Blinking LED Light/First Program - Light up LED.pdf
    │   ├── 3. Project Compilation and Download.pdf
    │   ├── 4. SWD Simulation Debugging.pdf
    │   └── 5. Using a LED Blinking Program with FreeRTOS/….pdf
    │
    ├── 3. RRCLite Program Analysis/                              what the firmware does, lesson by lesson
    │   ├── Lesson 1 … Program Architecture Analysis/
    │   ├── Lesson 2 … LED and Buzzer …/  (2.1 Buzzer, 2.2 LED)
    │   ├── Lesson 3 … Button Detection …/
    │   ├── Lesson 4 … ADC Power Supply Voltage Detection/        ★ battery/voltage sensing
    │   ├── Lesson 5 … QMI8658 Accelerometer and Gyroscope …/     ★ IMU
    │   ├── Lesson 6 … RGB LED Display …/
    │   ├── Lesson 7 … PWM Servo Control Principle …/
    │   ├── Lesson 8 … PWM Servo Motion Speed Control …/
    │   ├── Lesson 9 … Bus Servo Control …/  (+ Hiwonder Bus Servo Communication Protocol.pdf)
    │   ├── Lesson 10 … Brushed DC Motor Program Analysis/        ★ motor drive
    │   ├── Lesson 11 … AB Quadrature Encoder …/                  ★ encoder
    │   ├── Lesson 12 … DC Brushed Motor PID Control …/           ★ PID  (+ PID Basic Learning ×4)
    │   ├── Lesson 13 … Communication Protocol with the Host …/   ★★ THE protocol we replace
    │   ├── Lesson 14 … Communication … Sending …/                ★ protocol C source  (+ "MUST READ" note)
    │   └── Lesson 15 … Communication … Receiving …/              ★ protocol C source  (+ "MUST READ" note)
    │
    ├── Appendix/Reference Tutorial/                              extra chip datasheets (PDF + .txt)
    │   ├── stm32f407ve.pdf            MCU (reference manual)
    │   ├── CH9102F.PDF                USB-UART (dup of Chip Manuals copy)
    │   ├── DC Power Chip.PDF          power chip
    │   └── VP230.pdf                  CAN transceiver
    │
    └── _firmware_src/                                            ★ the RRC Lite STM32 firmware (source only)
        ├── _EXTRACTION_NOTES.md       provenance, what was kept/dropped, high-value file map, harvested facts
        └── RosRobotControllerLite_ros/
            ├── RosRobotControllerM4.ioc   ★ STM32CubeMX pin/peripheral map (the canonical pinout)
            ├── Core/                       main.c, ISRs, FreeRTOS task setup, Core/Inc/main.h (pin #defines)
            ├── Middlewares/                FreeRTOS source
            ├── MDK-ARM/                    Keil project + linker scatter
            └── Hiwonder/                   ★ ALL vendor-specific code:
                ├── Chassis/     mecanum_chassis.c ★, differential_chassis.c, chassis.h
                ├── Misc/        packet.c/.h ★ (protocol), checksum.c ★ (CRC), pid.c ★, sbus.c, log, object
                ├── Peripherals/ encoder_motor.c ★, QMI8658.c ★ (IMU), pwm_servo.c, serial_servo.c,
                │                button.c, buzzer.c, led.c, rgb_spi.c, key.c
                ├── Portings/    motor_porting.c, motors_param.h ★ (ticks/rev + PID gains), chassis_porting.c,
                │                imu_porting.c, packet_porting.c ★, packet_reports.h, *servo*_porting.c, …
                ├── System/      packet_handle.c/.h ★ (protocol dispatch), app.c, battery_handle.c ★,
                │                global.h, global_conf.h, oled_handle.c
                └── USB_HOST/    usbh_hid_gamepad.c, usbh_hiwonder_hid.c  (USB game-pad host)
```
*(`Drivers/` = ST HAL + CMSIS and `Third_Party/` = u8g2/lwmem were dropped from the firmware copy —
generic, ~151 MB, regenerable from the `.ioc`. See `_firmware_src/_EXTRACTION_NOTES.md`.)*

---

## Provenance

| | |
|---|---|
| **Extracted** | 2026-05-29 |
| **Controller docs + datasheets** | `MentorPi A1 & M1 2024-…-3-001.zip` → `…/Hardware Tutorial/RRC Lite Controller/` → `rrc_lite_controller/` |
| **Robot docs** | same zip → `00. Important Notice`, `1. Getting Ready`, `10. Lidar Lesson`, `11. Depth Camera` → `robot_reference/` |
| **Firmware source** | `MentorPi A1 & M1 2024-…-3-004.zip` → Lesson 14's nested `RosRobotControllerLite_ros.zip` → `_firmware_src/` |
| **Whole-robot synthesis** | `mentorpi_m1_hardware.md` — from the above + web research (Hiwonder pages, shipping ROS2 driver), 2026-05-29 |
| **Inventory** | 38 controller PDFs (31 lesson + 7 datasheet) + 8 robot PDFs → all converted to `.txt`; firmware = 3.0 MB / 147 files |
| **Text conversion** | `pdftotext -layout -enc UTF-8` |

The English in the PDFs is Hiwonder's machine translation from Chinese; phrasing is rough but the numbers,
hex, pin names and code are intact. Firmware code comments are in Chinese.

---

## Key vendor facts surfaced (verify before trusting)

Spot-checked against the converted text and the firmware source:

**Board** (Lesson 1)
- MCU **STM32F407VET6** — 168 MHz Cortex-M4, 512 KB Flash, 192 KB RAM.
- IMU **QMI8658** (accel + gyro) on-board. *(Our rebuild instead uses a BNO055 over UART — different part.)*
- **4× encoder-motor** ports, **4× PWM-servo** ports (two groups; jumper picks servo rail = Vin or 5 V),
  **2× bus-servo** ports, 2× RGB, 2× button, buzzer, reset, I²C expansion, GPIO+SWD header.
- Power input **DC 5 V – 12.6 V**; a dedicated **5 V / 5 A** output powers the Pi/Jetson (Hiwonder calls it
  "serial port 2," but it is a **power port, not a UART**). USB-serial via CH9102F on **UART1**.

**Host ↔ controller serial protocol** (Lesson 13 + `Hiwonder/Misc/packet.h`)
- Link: **UART1, 1 000 000 baud, 8N1, little-endian, HEX.**
- Frame: `0xAA 0x55` | `func` (u8) | `len` (u8) | `data…` | `CRC` (u8 = low byte of CRC over func+len+data).
- Function codes (authoritative enum from firmware; the PDF only listed 1–7):
  `SYS=0, LED=1, BUZZER=2, MOTOR=3, PWM_SERVO=4, BUS_SERVO=5, KEY=6, IMU=7, GAMEPAD=8, SBUS=9, OLED=10, RGB=11`.
- Motor speed commanded as a **float in rev/s**; IMU telemetry uploads **6 floats** (accel xyz, gyro xyz).
- Worked hex examples for every command: Lesson 13 `.txt`, lines ~1268+. Both C sides: Lessons 14/15.

**Motors / encoders** (`Hiwonder/Portings/motors_param.h`)
- **JGA27 = this kit's "310" motor: 13 × 4 × 20 = `1040` ticks/rev** — matches `_engineering/`'s
  `COUNTS_PER_REV ≈ 1040`. Stock PID `Kp −36, Ki −1, Kd −1`. (Other types: JGB37→1980, JGB520→3960, JGB528→5764.)

---

## What was deliberately **excluded** (and where it still lives)

In the **`…-3-004.zip`** part, under the same `RRC Lite Controller/` path — bulk, not documentation:
- **`MDK536.rar`** — 850 MB Keil MDK-ARM IDE installer.
- **Build artifacts** (`.o .crf .d .axf .htm .lnp .map`, JLink logs) and **`.git/`** history.
- **Seven ~100 MB per-lesson copies** of the firmware project — we kept one (Lesson 14) and stripped it.
- From the firmware copy itself: `Drivers/` (ST HAL + CMSIS) and `Third_Party/` (u8g2/lwmem) — generic.

The **two ~12.5 GB ROS2 archives** in Downloads are **VMware VM exports** (`.ovf` + `.vmdk` + `.iso`) — the
Pi-side OS/software, containing **zero PDFs**. See below for the ROS2 driver.

---

## Not included — ROS2 host driver (`ros_robot_controller`)

The Pi/ROS2 side of the serial protocol. **Decision (2026-05-29): skipped** — Lessons 13–15 plus the
firmware's `packet_handle.c` / `packet.c` already document *both* sides of the wire protocol, so its
marginal value is low. It is **not** present as plain source in any MentorPi zip (only the STM32
`.jflash` config matches); it exists **only inside the `.vmdk` VM images** in Downloads.

To retrieve it later: either (a) install `qemu-utils`/`libguestfs-tools` (needs sudo + network + ~10–20 GB
temp) and copy the package out of the virtual disk — caveat: Hiwonder runs ROS2 in **Docker**, so it may
be nested inside a container layer — or (b) pull `ros_robot_controller` from Hiwonder's public source
(verify it matches the MentorPi M1 first).

---

## How this relates to the rebuild

Use it as the **"what the vendor actually built"** reference while writing/auditing the phase docs and
`_engineering/`. Most directly relevant to:
- **Phase 1–2** (powertrain, encoders, PID) — Lessons 10/11/12 + `Hiwonder/Peripherals/encoder_motor.c`,
  `Misc/pid.c`, `Portings/motors_param.h` show how Hiwonder drives the same 310 motors and reads the 13-line
  Hall encoders (and confirm the 1040 ticks/rev figure).
- **Phase 3** (teleop, Mecanum) — Lessons 13/14/15 + `Hiwonder/System/packet_handle.c`, `Misc/packet.c`,
  `Chassis/mecanum_chassis.c` define the serial contract and kinematics our design replaces.
- **Phase 4** (IMU) — Lesson 5 + `Hiwonder/Peripherals/QMI8658.c`; note the vendor IMU is **QMI8658**, *not*
  the BNO055 our design specifies.

When a fact here contradicts `_engineering/`, that is usually **intentional** (changed on purpose) or a
**claim to verify on the bench** — never silently copy a number from this folder into a phase doc.
