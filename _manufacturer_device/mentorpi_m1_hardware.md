# MentorPi M1 — as-shipped hardware & interface map (manufacturer reference)

What the **Hiwonder MentorPi M1** physically is, and **how every device wires to the two compute
boards** — the Raspberry Pi 5 and the RRC Lite controller. The point is to make clear *what the RRC Lite
talks to, and over what bus*, since this project replaces that controller.

> ⚠️ **Vendor reference, not project canon.** `_engineering/` is the source of truth for the *rebuild*.
> Numbers here are **manufacturer facts**. Each is tagged with its source:
> **[HW1]** RRC Lite Lesson 1 · **[L2]** RRC Lite Lesson 2 (schematic) · **[L13]** Lesson 13 protocol ·
> **[L5]** Lesson 5 (IMU) · **[Lidar1]** Lidar Lesson 1 · **[Notice]** Important Notice · **[Startup]**
> Startup & Status · **[FW]** RRC Lite firmware (`.ioc` / `motors_param.h` / `*_porting.c`) · **[web]**
> Hiwonder published specs + the shipping ROS2 driver source (see Sources) · **[ENG]** this project's
> `_engineering/` cross-reference. Unconfirmed items are marked **(verify)**.

---

## 1. Architecture at a glance

The MentorPi M1 is a **two-board robot**, which is exactly why the RRC Lite matters:

- **Raspberry Pi 5** — the "host / upper computer." Runs Ubuntu + ROS2 (in Docker): perception, SLAM,
  navigation, camera, lidar. It has **no direct wiring to the motors** [HW1][Startup][web].
- **RRC Lite** (`RosRobotControllerLite`, STM32F407VET6) — the "lower computer," a real-time I/O board.
  Drives the 4 motors, reads the 4 encoders, runs per-wheel PID, drives the servos, reads the on-board
  IMU, reports battery voltage. It is the **only board touching the drivetrain** [HW1][FW][web].
- The two are joined by **one USB-serial link** (the Lesson-13 packet protocol), and the RRC Lite also
  **feeds 5 V/5 A power up to the Pi** over USB-C [HW1][web].

```
                            ┌───────────────────────── Raspberry Pi 5  (host, ROS2) ──────────────────────────┐
   Nuwa-HP60C depth cam ─USB│▶ USB ports                                                                       │
   (or GC0308 mono cam) ─USB│▶                                                                                 │
   STL-19P lidar ─USB dongle│▶ /dev/ttyUSB0 @ 230400                                                           │
                            │           ▲ /dev/ttyACM0 (USB-CDC, packet protocol §5)    ▲ 5V/5A USB-C (up)     │
                            └───────────┼───────────────────────────────────────────┼──────────────────────-─┘
                                        │ TX/RX                                       │
                            ┌───────────┼───────────────────────────────────────────┼──────────────────────-─┐
                            │ CH9102F USB-UART ── USART1 ──┐         5V/5A PD output ─┘   (RRC Lite)            │
                            │                              │   ┌───────────────────────────────────────────┐  │
   4× DC encoder motor ─────│ YX-4055AM ◀ TIM1/9/10/11 PWM─┘   │ STM32F407VET6 + QMI8658 IMU (I²C2)         │  │
   (FL/RL/FR/RR) ◀─encoder──│ TIM2/3/4/5 quadrature            │   (2 PWM inputs per motor, sign-magnitude) │  │
   2× PWM servo (pan-tilt) ◀│ PA11/PA12 (PC8/PC9 spare)        └───────────────────────────────────────────┘  │
   2× serial bus servo ◀────│ UART5 (half-duplex, single-wire)                                                 │
   2× RGB (WS2812) ◀────────│ SPI1                                                                             │
                            │ Power in: 7.4 V LiPo ─▶ RT8289 buck(s) ─▶ 5 V rails (logic / servo / Pi)         │
                            └────────────────────────────────────────────────────────────────────────────────┘
```
*(ASCII is schematic, not pin-exact — see §5 for the precise port/bus table.)*

---

## 2. Component inventory (M1, depth-camera version)

| Subsystem | Part | Key spec | Source |
|---|---|---|---|
| Compute (high level) | **Raspberry Pi 5** | quad Cortex-A76; needs **5.1 V/5 A (~27 W)** USB-C; RAM 2/4/8/16 GB per order | [web][Notice] |
| Compute (real-time) | **RRC Lite** controller | STM32F407VET6 @168 MHz, 512 KB Flash, 192 KB RAM | [HW1][web] |
| Chassis | Mecanum 4WD | aluminum-alloy (anodized); **212 × 171 × 147 mm**, **1.2 kg** | [web] |
| Wheels | 4× **Mecanum** wheels | angled rollers; diameter/material **not published (verify)** | [HW1][web] |
| Drive | 4× **JGA27-310** gearmotor | 7.4 V, **1:20**, **450 ±10 RPM**, run ≤0.65 A / **stall ≤1.4 A**, 4.8 W | [web][FW][ENG] |
| Feedback | 4× **13-line Hall** quadrature encoder | **≈1040 counts / output rev** (13×4×20 — derived, measure) | [FW][web] |
| Orientation | **QMI8658** 6-axis IMU | accel + gyro; on the RRC Lite, **I²C2** | [L5][FW] |
| Ranging | **STL-19P** lidar (LDROBOT LD19/D500 engine) | DTOF, 0.02–12 m, 5–13 Hz (def. 10), **UART 230400** | [Lidar1][web] |
| Vision (3D) | **Nuwa-HP60C** depth camera (driver `ascamera`) | USB-C 2.0; RGB 1080p, depth 640×480, 0.2–4 m | [web][Notice] |
| Vision (mono variant) | **GC0308** USB cam (module HS-256-650) on **2-axis pan-tilt** | USB UVC, 640×480, 170° FOV | [web][Notice] |
| Actuated mount | 2× **LFD-01** PWM servo (pan-tilt) | PWM hobby servo, 0–180°, 500–2500 µs, ≥1.4 kg·cm | [web][L13] |
| Power | **7.4 V 2S LiPo, 2200 mAh** | 10C (resellers say 20C); charge <6.4 V; **8.4 V/2 A** DC5.5×2.5 charger | [web][Notice][ENG] |

> The M1 ships in **two camera variants** [Notice][Startup][web]: a **depth-camera** version (Nuwa-HP60C
> over USB) and a **monocular** version (GC0308 USB cam on a 2-servo pan-tilt). You have the **3D/depth**
> version. The camera mount is marketed as **2-DOF** (2 PWM servos on the RRC Lite), but the app exposes
> pan-tilt steering on the **monocular** version [Startup] — so **verify whether your depth unit actually
> populates the 2 pan-tilt servos** (§8).

---

## 3. Subsystems

### Chassis, wheels, motors, encoders
- **Mecanum 4WD** → the robot is **holonomic** (translate sideways + rotate). Project wheel order is
  `FL, RL, FR, RR`. Chassis: anodized aluminum, 212 × 171 × 147 mm, 1.2 kg [web].
- Each wheel: a **JGA27-310** brushed gearmotor — **7.4 V, 1:20 gearbox, 450 ±10 RPM** output, rated
  **≤0.65 A**, **stall ≤1.4 A**, 4.8 W, ~70 g, 3 mm D-shaft [web]; gearbox 1:20 also confirmed in the
  firmware [FW]. (Exact SKU suffix, e.g. `-310R20`, is a `[web]` detail — verify.)
- **13-line AB Hall encoder** per motor. ×4 quadrature × 1:20 ⇒ **≈1040 counts / output rev**. This is a
  **derived** figure — Hiwonder prints "13 lines" + "1:20" but not "1040"; the firmware hard-codes
  `MOTOR_JGA27_TICKS_PER_CIRCLE 1040.0` [FW]. Still, **measure it** (the firmware exposes raw counts).
  The RRC Lite runs a velocity **PID per wheel** (stock JGA27 gains `Kp −36, Ki −1, Kd −1` — negative
  because the firmware folds the motor/encoder sign convention into the gains) [FW].
- 4 motor+encoder leads on a combined **JST-PH 2.0 6-pin** connector per motor [web].
- **Motor driver: `YX-4055AM` H-bridge** per the RRC Lite schematic [L2] (×2 implied — a YX-4055AM is a
  dual H-bridge, so two chips cover four motors). Hiwonder's RRC Lite *product page* instead lists
  **"SA8870C"** [web] — likely a board-revision/naming difference; **verify against your board**. Either
  way, the firmware drives **two PWM inputs per motor** (sign-magnitude; there is **no** separate direction
  GPIO) [FW]. *(The rebuild swaps this for a TB6612FNG — intentional; see `_engineering/`.)*

### RRC Lite controller
Full detail (datasheets, schematic, protocol, firmware source) is in **[`rrc_lite_controller/`](rrc_lite_controller/)**.
Summary: STM32F407VET6; YX-4055AM/"SA8870C" motor driver; on-board **QMI8658** IMU; **CH9102F** USB-UART;
RT8289 buck(s); VP230 CAN transceiver; buzzer, 2 RGB, 3 status LEDs, 2 buttons; a **5 V/5 A USB-C** output
that powers the Pi [HW1][web]. It owns the drivetrain, servos and IMU; the Pi commands it over serial (§5).

### IMU
**QMI8658** (6-axis) lives on the RRC Lite, not the Pi — confirmed by the firmware (`Peripherals/QMI8658.c`)
and Lesson 5 [L5][FW]. *(Web/marketing doesn't name the RRC Lite IMU, but the shipped firmware does — trust
the firmware.)* It sits on **I²C2 (`PB10`/`PB11`), address 0x6A**, with a data-ready interrupt on `PB12`
(the board's other bus, I²C1 on `PB6`/`PB7`, serves the OLED/expansion) [FW][L5]. The STM32 reads it and
**streams it to the Pi** as protocol function 7 = 6× float (accel x/y/z, gyro x/y/z) [L13][FW]. *(The
rebuild uses a BNO055 on the Pi instead — a different part; that divergence is intentional.)*

### Lidar — STL-19P / LD19
- **DTOF (direct time-of-flight) pulse lidar**, BLDC-spun, 360° planar [Lidar1][web].
- Range **0.02–12 m** [Lidar1] (web/LD19 sheet: 0.03–12 m, 12 m white / 8 m black); ranging **~4500 Hz**
  [Lidar1]; scan **5–13 Hz** (default 10 Hz), set by a **PWM** input (40 % duty → 10 Hz); angular
  resolution **~1°** (typ., at 10 Hz) [Lidar1]; ambient-light immunity **~30 kLux** [Lidar1]; 905 nm,
  IEC-60825 **Class 1** [Lidar1].
- **Device interface: UART, 230400 baud, 8N1**, 3.3 V logic, **ZH1.5T-4P** connector (Tx, PWM, GND, P5V)
  [Lidar1].
- Reaches the Pi through a small **USB-to-UART dongle** (the kit's "lidar data cable"); shows up as
  **`/dev/ttyUSB0` @ 230400** in the shipping driver [web]. **The dongle's chip is NOT confirmed** —
  Hiwonder doesn't name it; the project's "CP2102" note is **unverified** [ENG]. **It does not go through
  the RRC Lite** (different device node, different baud) [web].

### Camera + pan-tilt
- **Depth version:** **Nuwa-HP60C** — Hiwonder's structured-light depth module (IR + RGB + dot projector).
  RGB 1920×1080@20, depth 640×480@20, depth range **0.2–4 m**, depth FOV ≈ 74°×59°, **USB-C 2.0** [web].
  ROS driver/topic namespace is **`ascamera`**; the on-robot config tool labels this option **"Orbbec
  depth camera"** [Notice] — so the menu name says Orbbec while the product is branded Nuwa-HP60C; the
  underlying sensor vendor is **not fully resolved (verify)**.
- **Monocular version:** **GC0308** sensor (GalaxyCore), module **HS-256-650**, 640×480, 170° FOV, USB
  UVC, driven by `usb_cam` [web].
- Both cameras connect by **USB to the Pi** — **not the Pi CSI port, not via the RRC Lite** [web].
- The **pan-tilt** (when present) is **2× LFD-01 PWM servos driven by the RRC Lite** PWM ports, not the
  Pi [web][HW1][FW].

### Battery & charging
- **7.4 V 2S LiPo, 2200 mAh**, with a protection board; **10C** on Hiwonder's M1 page (some resellers say
  20C — treat as reseller error) [web][ENG]. Charge promptly below **6.4 V**; ~1 h charge [Notice].
- Charged via an **on-board DC charge port** with an **8.4 V / 2 A** charger (**DC 5.5 × 2.5 mm** barrel)
  [web]. The pack feeds the RRC Lite, which bucks to the 5 V rails (logic, servos, Pi).

---

## 4. Raspberry Pi 5 side
The Pi runs the ROS2 stack (in Docker) and connects to: the **lidar** (`/dev/ttyUSB0`, USB dongle), the
**camera** (USB), **Wi-Fi** (boots an AP hotspot `HW…`, password `hiwonder`) [Startup], and the **RRC
Lite** (`/dev/ttyACM0`, §5). The Pi 5 wants **5.1 V/5 A (~27 W)**; that power comes **from the RRC Lite's
5 V/5 A USB-C output**, not a separate brick [web][HW1].

---

## 5. Interface / connection map  ★ (the core)

**Driven by the RRC Lite (drivetrain + servos + IMU + status I/O):**

| Device | RRC Lite resource | Bus / signal | Notes | Source |
|---|---|---|---|---|
| 4× DC motor (drive) | **YX-4055AM** H-bridge ← PWM: M1=TIM1 CH3/4, M2=TIM1 CH1/2, M3=**TIM9** CH1/2, M4=**TIM10** CH1 + **TIM11** CH1 | PWM into H-bridge | **2 PWM inputs/motor**, sign-magnitude (**no dir GPIO**); TIM1 alone covers only M1+M2 | [L2][FW][web] |
| 4× encoder | **TIM2/TIM3/TIM4/TIM5** (encoder mode) | quadrature (A/B) | ≈1040 cnt/rev | [FW] |
| 2× PWM servo (pan-tilt) | `PA11`,`PA12` (+ `PC8`,`PC9` spare; 4 ports, 2 groups) | 50 Hz, 500–2500 µs (timer-ISR generated) | LFD-01 | [HW1][FW][web] |
| 2× serial bus servo | **UART5** (`PC12`) | half-duplex **single-wire** | Hiwonder bus protocol | [FW] |
| QMI8658 IMU | **I²C2** (`PB10`/`PB11`), addr 0x6A (+ INT `PB12`) | I²C | reported to Pi as func 7 | [L5][FW] |
| 2× RGB LED | **SPI1** (`PA5`/`PA7`) | WS2812 over SPI-DMA | — | [FW] |
| Battery sense | `PB0` → **ADC1_IN8** | analog divider (÷11) | low-voltage telemetry | [FW] |
| Buzzer / 3 LEDs / 2 buttons | `PA6` / `PD9-11` / `PE0-1` | GPIO | status + user input | [FW] |
| **Host (Raspberry Pi)** | **USART1** (`PA9`/`PA10`) ◀▶ **CH9102F** USB-UART | packet protocol | enumerates as USB-CDC → **/dev/ttyACM0** | [HW1][FW][web] |
| **Power to Pi** | **5 V / 5 A USB-C** output | power only | sized for Pi 5 (27 W) | [HW1][web] |

**Host ↔ RRC Lite serial protocol** (the interface this project replaces): frame `0xAA 0x55 | func | len |
data | CRC` (CRC16 over func+len+data); functions `SYS0 LED1 BUZZER2 MOTOR3 PWM_SERVO4 BUS_SERVO5 KEY6
IMU7 GAMEPAD8 SBUS9 OLED10 RGB11`; motor speed as `float` rev/s; IMU as 6 floats; little-endian [L13][FW].
**Baud = 1,000,000** — both Lesson 13 and the shipping ROS driver use it (`Board(device="/dev/ttyACM0",
baudrate=1000000)`) [L13][web]. ⚠️ *Caveat:* this firmware **snapshot**'s CubeMX init (`Core/Src/usart.c`)
sets USART1 to **115200** and contains no visible runtime override — so the **shipped** firmware likely
differs from this teaching snapshot (or, since the link is USB-CDC, the host-set baud may not be physically
enforced end-to-end). Treat 1 M as the operational/host setting; **measure if it matters.** Full detail:
`rrc_lite_controller/` Lessons 13–15 + `Hiwonder/System/packet_handle.c`, `Misc/packet.c`.

**On the Pi directly (NOT through the RRC Lite):**

| Device | Pi node | Bus | Notes | Source |
|---|---|---|---|---|
| STL-19P lidar | **/dev/ttyUSB0** | UART 230400 via **USB dongle** (chip unconfirmed) | PWM pin sets scan Hz | [Lidar1][web] |
| Nuwa-HP60C depth cam | USB | USB-C 2.0 (`ascamera`) | depth version | [web][Notice] |
| GC0308 mono cam | USB | UVC (`usb_cam`) | mono version, on pan-tilt | [web][Notice] |
| Wi-Fi / hotspot | on-board | — | AP `HW…`, pw `hiwonder` | [Startup] |

---

## 6. Power tree (as shipped)

```
7.4 V 2S LiPo (2200 mAh) ─▶ on-board DC charge port (8.4 V/2 A charger; charge <6.4 V)
            ├──▶ RRC Lite power input (5–12.6 V) ─▶ RT8289 buck(s) ─▶ 5 V logic rail (STM32, sensors)
            │                                                      ─▶ 5 V servo rail (jumper: Vin or 5 V)
            │                                                      ─▶ 5 V/5 A USB-C ─▶ Raspberry Pi 5
            └──▶ YX-4055AM motor-driver supply (battery voltage) ─▶ 4× DC motors
```
Sources: [HW1][Notice][web]. The Pi 5's 5 A-at-5 V need is a non-standard (Pi-specific) USB profile; the RRC
Lite's PD output is built to satisfy it. *(The rebuild's power tree — 10 A main fuse, branch fuses, e-stop,
Pololu D24V50F5 for the Pi — is a deliberate redesign; see `_engineering/power_budget.md`. Don't conflate.)*

---

## 7. Power-on behaviour
~1 min after power-on: **LED1 steady + one buzzer beep** = ROS up; **LED2 blinks 1 Hz** = AP mode; a
**`HW…` Wi-Fi hotspot** appears (password `hiwonder`); the **lidar starts spinning**; motors then respond
to app / wireless-controller commands via the STM32 controller [Startup].

---

## 8. Verify on YOUR robot (it's the real authority)
Your robot runs on a Pi 5, so ground-truth the Pi-side interfaces directly. On the Pi:
```bash
ls -l /dev/ttyACM* /dev/ttyUSB*        # expect: ttyACM0 = RRC Lite, ttyUSB0 = lidar dongle
lsusb                                  # CH9102/CH343 controller (e.g. 1a86:55d4), lidar dongle, USB camera
udevadm info -q property -n /dev/ttyUSB0   # the lidar dongle's actual chip (CP2102? CH340? other)
dmesg | grep -iE 'tty|cp210|ch34|9102|uvc|video'
v4l2-ctl --list-devices                # camera nodes (depth vs mono)
i2cdetect -y 1                         # any I²C devices on the Pi header
ros2 node list ; ros2 topic list       # controller node, /scan, /imu, /odom, camera topics
cat /proc/device-tree/model ; free -h  # Pi 5 model + RAM
# is the pan-tilt populated on your depth unit?  watch the 2 PWM servo ports / try the app pan-tilt
```
Anything those print **overrides** a "(verify)" above — feed corrections back in.

---

## Open / to-confirm
- **Motor-driver chip**: schematic says **YX-4055AM**, product page says **SA8870C** — confirm on your board.
- **Lidar USB-UART dongle chip** (CP2102 vs CH340 vs other) and exact lidar SKU label (STL-19P = LD19/D500).
- **Depth-camera sensor vendor** — product is "Nuwa-HP60C", config menu says "Orbbec"; underlying module unresolved.
- **Operational host baud** (1 M vs the snapshot's 115200) — measure end-to-end.
- Whether the **depth** unit populates the **2 pan-tilt PWM servos** (app steering is documented for mono).
- **Mecanum wheel diameter/material**, chassis ground clearance — unpublished.
- **Battery C-rating** — Hiwonder 10C vs resellers 20C.
- **≈1040 counts/rev** — derived from 13×4×20; measure empirically (project open item #3).

## Sources
**Manufacturer PDFs (in this repo):** `robot_reference/` (Introduction, Important Notice, Assembly,
Charging, Startup, Lidar Lessons 1–2, Depth-Camera Lesson 1) and `rrc_lite_controller/` (Hardware Lesson 1,
Schematic Lesson 2, IMU Lesson 5, protocol Lesson 13, firmware).
**[web] (2026-05-29):** Hiwonder product pages — `hiwonder.com/products/mentorpi-m1`, `/rrc-lite`,
`/310-dc-gear-encoder-motor`, `/ld19-d300-lidar`; `docs.hiwonder.com/projects/MentorPi`; Hiwonder
ROS-Robot-Control-Board wiki; CNX-Software MentorPi review (2024-10-23); Raspberry Pi 27 W PSU datasheet.
Port/baud/topology (`/dev/ttyACM0` @ 1 M, `/dev/ttyUSB0` @ 230400) read from the **shipping ROS2 driver
source** (community mirror `github.com/Matzefritz/HiWonder_MentorPi`, packaging Hiwonder's stock
`ros_robot_controller` + `ldrobot-lidar-ros2`). Project cross-reference: `../_engineering/`.
