# Robotic Car — Project Overview

## End goal
A fully autonomous 4WD robot car. You can drive it remotely over WiFi with live camera and pan/tilt, but it can also localize itself (IMU + wheel odometry), build a 2D map of the room with lidar (SLAM), and autonomously navigate to a clicked point on the map with obstacle avoidance.

## Two-brain architecture
- **Raspberry Pi 5** — the brain. Vision, SLAM, navigation, sensor fusion, web server, WiFi. Runs Linux.
- **Pi Pico 2W** — the spinal cord. Real-time motor PWM, encoder counting, closed-loop wheel speed control. Runs MicroPython or C SDK.

The two connect over a single USB cable that carries 5V power AND serial data. The Pi sends high-level velocity commands; the Pico runs the PID loops locally with deterministic timing.

## Sensors and modules in the final build
- 4× JGA27-310R-74 DC motors w/ quadrature encoders *(have)*
- 2× DRV8833 dual H-bridge motor drivers
- Pi Pico 2W (motor co-processor) *(have)*
- Raspberry Pi 5 *(have)*
- BNO055 IMU (9-axis with built-in fusion, I2C)
- Pi Camera v3 (CSI)
- 2× SG90 micro servos (pan/tilt mechanism)
- PCA9685 16-channel PWM/servo driver (I2C)
- Off-the-shelf pan/tilt bracket
- LDROBOT STL-19P 360° DTOF lidar *(have)*
- 2S LiPo battery 7.4V *(have)*
- USB power bank 5V/3A+ *(have)*
- 4WD chassis (acrylic plates, motor mounts, wheels)

## Master block diagram

```
                ┌──────────────────┐
                │  USB power bank  │  5V / 3A+
                │  (Pi 5 power)    │
                └────────┬─────────┘
                         │ USB-C
                         ▼
       ┌───────────────────────────────────────┐
       │           Raspberry Pi 5              │
       │                                       │
       │  CSI port   USB ports   I2C-1 (GPIO)  │
       │                         UART (GPIO)   │
       └─────┬────────┬─┬──────────┬───────────┘
             │        │ │          │
   ┌─────────┘    ┌───┘ └──┐       │
   │              │        │       │
   ▼              ▼        ▼       ▼
[Pi Camera v3] [Pico 2W][STL-19P] I2C bus 1 (3.3V)
 (CSI ribbon)   (USB)   (UART or      │
                         USB adapter) ├─► BNO055 IMU        (0x28)
                │                     └─► PCA9685 servo PWM (0x40)
                │                            ├─► SG90 #1 (pan)
                │                            └─► SG90 #2 (tilt)
                │
                │ GP2–GP9   (motor control out)
                │ GP10–GP17 (encoder A/B in)
                │ 3V3 / GND (logic + encoder power)
                ▼
       ┌──────────────────┐         ┌──────────────────┐
   VM─►│  DRV8833 #1      │     VM─►│  DRV8833 #2      │
       │  (left side)     │         │  (right side)    │
       └──┬────────┬──────┘         └──┬────────┬──────┘
       M1,M2    M1,M2                M1,M2    M1,M2
          ▼        ▼                    ▼        ▼
       [FL mtr][RL mtr]              [FR mtr][RR mtr]
        encoders A,B ──────────────► back to Pico GP10–GP17
        VCC, GND ◄─────── 3.3V from Pico

       ┌──────────────────┐
       │  2S LiPo 7.4V    │  + ──► both DRV8833 VM
       │                  │  − ──► common ground rail
       └──────────────────┘

   Common ground:  LiPo− ── DRV GND ── Pico GND ── Pi 5 GND
                   (Pi 5 ↔ Pico link via USB cable)
```

## I2C address map (Pi 5 bus 1)

| Address | Device                | Notes                        |
|---------|-----------------------|------------------------------|
| 0x28    | BNO055 IMU            | Factory default              |
| 0x40    | PCA9685 servo driver  | Factory default              |

Far fewer I2C devices than before, so the bus is uncontested and easy to debug.

## Phase summary

| #  | Title                       | Effort   | Difficulty | Deliverable                                     |
|----|-----------------------------|----------|------------|-------------------------------------------------|
| 1  | Powertrain                  | 1 wkend  | Medium     | All 4 motors spin under Pico control            |
| 2  | Odometry & closed-loop speed| 1 wkend  | Medium     | Wheels hold commanded velocity                  |
| 3  | Pi↔Pico bridge + WiFi teleop| 1 wkend  | Medium     | Drive the car from a web browser                |
| 4  | IMU + pose tracking         | 1 wkend  | Medium     | Live heading & x/y pose in web UI               |
| 5  | Camera + pan/tilt           | 1 wkend  | Medium     | Live video stream, browser pan/tilt control     |
| 6  | Lidar setup & visualization | 1 wkend  | Medium     | Live 360° scan visible in web UI                |
| 7  | SLAM mapping                | 2 wkends | Hard       | Drive around, watch a 2D map build itself       |
| 8  | Autonomous navigation       | 2 wkends | Hard       | Click a point on the map, the bot drives there  |

Each phase has its own document. Work through them in order — every phase builds directly on the one before.

## How to use these docs

1. Order all parts first using `01_parts_list.md`
2. Work through phases 1 → 8 in order, one per session
3. Each phase doc contains: goal, new parts, current full schematic, pin tables, software tasks, verification checklist, common pitfalls
4. **Do not skip the verification checklists.** They are the difference between "I think it works" and "I know it works"
5. After each phase, the schematic is updated to show the full system as it exists at that point

## Naming conventions used across all docs

- `FL` / `RL` / `FR` / `RR` = front-left / rear-left / front-right / rear-right
- Pico pins: `GP0`, `GP1`, … (the chip's own GPIO numbers, not header pin numbers)
- Pi 5 pins: BCM numbering (e.g., `GPIO2`, `GPIO3`), not physical header pin numbers
- All voltages are nominal — verify with a multimeter before connecting anything
- "Common ground" means LiPo− and Pi 5 GND and Pico GND and driver GND are all electrically tied together. If anything is flaky, check grounds first.
