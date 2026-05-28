# Robocar — 4WD autonomous robot car

A fully autonomous 4-wheel-drive robot car built around a **Raspberry Pi 5** (high-level brain) and a **Pi Pico 2W** (real-time motor co-processor). It drives over WiFi with live camera and pan/tilt, tracks its own pose with an IMU and wheel odometry, builds a 2D map of the room with lidar (SLAM), and autonomously navigates to clicked map points with obstacle avoidance.

This repo is the **build plan** — eight phases of escalating capability, each with full ASCII schematics, line-by-line wire lists, pin tables, software task lists, verification checklists, and common pitfalls.

## Architecture at a glance

```
[USB power bank] → [Pi 5] ←─USB─→ [Pico 2W] → [2× DRV8833] → [4 motors w/ encoders]
                     │                                          │
                     ├── CSI ─→ [Pi Camera v3 on pan/tilt]      │
                     ├── USB ─→ [STL-19P lidar]                 │
                     └── I²C ─→ [BNO055 IMU] + [PCA9685 servos] │
                                                                │
                              encoders feed back ───────────────┘
                                              ↓
                                       [2S LiPo 7.4V] → motor power
```

Two brains. The Pi 5 handles SLAM, vision, web UI, planning. The Pico handles real-time PID motor control and encoder counting. They talk over a single USB cable that carries both 5V power and serial data.

## Build phases

Work through these in order. Each phase produces a working, demonstrable milestone.

| #   | Phase                                                                   | Difficulty | Time     |
|-----|-------------------------------------------------------------------------|------------|----------|
| 0   | [Project overview](./00_README.md)                                      | —          | Read this first |
| —   | [Parts list](./01_parts_list.md)                                        | —          | Order before starting |
| 1   | [Powertrain — motors + drivers + Pico](./02_phase_1_powertrain.md)      | Medium     | 1 weekend |
| 2   | [Odometry & closed-loop wheel speed](./03_phase_2_odometry.md)          | Medium     | 1 weekend |
| 3   | [Pi ↔ Pico bridge & WiFi teleop](./04_phase_3_teleop.md)                | Medium     | 1 weekend |
| 4   | [IMU + pose tracking](./05_phase_4_imu.md)                              | Medium     | 1 weekend |
| 5   | [Camera + pan/tilt](./06_phase_5_camera.md)                             | Medium     | 1 weekend |
| 6   | [Lidar setup & visualization](./07_phase_6_lidar.md)                    | Medium     | 1 weekend |
| 7   | [SLAM mapping](./08_phase_7_slam.md)                                    | Hard       | 2 weekends |
| 8   | [Autonomous navigation](./09_phase_8_autonomy.md)                       | Hard       | 2 weekends |

## What each phase doc contains

- A goal statement and difficulty/time estimate
- An ASCII schematic showing the full system state at that point
- Pin assignment tables (BCM number AND physical header pin)
- **Wire-by-wire connection list** — checkable boxes for every wire, with named endpoints
- Software task list with code skeletons where useful
- Verification checklist — don't skip these
- Common pitfalls section based on what typically goes wrong

## Hardware

Already on hand:
- Raspberry Pi 5
- Pi Pico 2W
- 4× JGA27-310R-74 DC motors with quadrature encoders (7.4V, 450 RPM, 20:1)
- 2S LiPo battery (7.4V)
- USB power bank (5V, 3A+)
- LDROBOT STL-19P 360° lidar

To buy:
- 2× DRV8833 dual H-bridge motor drivers
- BNO055 IMU
- PCA9685 16-channel PWM driver
- 2× SG90 micro servos + pan/tilt bracket
- Pi Camera v3 + longer CSI cable
- 4WD chassis kit
- Misc wiring, capacitors, fuse, switch

Full BOM with quantities and ~prices in [`01_parts_list.md`](./01_parts_list.md).

## Conventions used across all docs

- `FL` / `RL` / `FR` / `RR` = front-left / rear-left / front-right / rear-right
- Pico pins: `GP<n>` is the GPIO number; `(pin <m>)` is the physical board position
- Pi 5 pins: `GPIO<n>` is BCM number; `(header pin <m>)` is the physical position
- All voltages are nominal — verify with a multimeter before connecting anything
- "Common ground" means LiPo−, both driver GNDs, Pico GND, and Pi 5 GND are all electrically tied together
