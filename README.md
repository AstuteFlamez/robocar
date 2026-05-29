# Robocar — rebuilding a Hiwonder MentorPi M1 from the wheels up

Take a commercial **Hiwonder MentorPi M1** (a Mecanum-wheel, Raspberry-Pi-5 robot that ships with a vendor controller and a ROS2 image), set the vendor stack aside, and **rebuild the entire low-level robot yourself** — motor control, odometry, sensor fusion, mapping, and autonomous navigation — on a two-brain architecture you understand end to end. Drive it over WiFi with live camera and pan/tilt, watch it track its own pose, build a 2D map of a room with lidar (SLAM), and click a point to send it there autonomously.

> **Why rebuild a robot that already works?** Because the goal is *learning*. Running the vendor's software teaches you to operate someone else's robot; rebuilding the stack from first principles teaches you to *be the engineer who could have built it.* This repo is an 8-phase course that does exactly that — and produces a portfolio project you can defend line-by-line in an interview.

This is built for an **ECE sophomore**: it assumes Ohm's law, basic circuits, and intro Python, and explains everything else — H-bridges, PWM, quadrature encoders, PID, sensor fusion, Mecanum kinematics, SLAM, path planning — from the ground up, with real numbers and engaging prose.

---

## The two-brain architecture

```
[2S LiPo 7.4V] ─► fuse ─► reverse-protect/switch ─► e-stop ─┬─► motor rail ─► 2× TB6612FNG ─► 4 Mecanum motors
                                                            ├─► D24V50F5 buck (5V/5A) ─► Raspberry Pi 5 ──┐
                                                            └─► UBEC (5V) ─► PCA9685 ─► pan/tilt servos    │
                                                                                                          │
   Raspberry Pi 5  (the high-level brain: vision, SLAM, navigation, web UI, WiFi)                         │
        │                                                                                                 │
        ├── USB ──► Pi Pico 2W  (the real-time co-processor: 20 kHz PWM, PIO encoder decode, per-wheel PID)│
        │              └── encoders + motor drivers                                                       │
        ├── USB ──► STL-19P lidar (via CP2102 3.3 V USB-UART)                                              │
        ├── CSI ──► Camera Module 3 (on the pan/tilt gimbal)                                               │
        ├── UART ─► BNO055 IMU (UART mode — dodges the Pi I²C clock-stretch bug)                           │
        └── I²C ──► PCA9685 servo driver (0x40) + INA219 power monitor (0x41) ◄───────────────────────────┘
```

**Pi 5** does the soft-real-time, compute-heavy work. **Pico 2W** does the hard-real-time, deterministic work. They talk over one USB cable. Splitting computation by its timing requirements is the single most important systems idea in robotics — and it's exactly how the vendor controller you're replacing is built.

---

## Repository layout

```
robocar/
├── README.md                ← you are here
├── _engineering/            ← the SOURCE OF TRUTH (read this first)
│   ├── device_contracts.md      every part's voltage/current/logic/connector + canonical pin map + connection matrix
│   ├── products_to_buy.md       master BOM: have-vs-buy, links, prices, the reason for each part
│   ├── power_budget.md          power tree + current budget + fuse/buck/UBEC sizing
│   ├── engineering_decisions.md why each part/design was chosen (with alternatives + learning payoff)
│   └── safety.md                LiPo rules, protection layers, power-up order, continuity ritual
├── phase_1_powertrain/      ← each phase folder = { README.md, wiring_diagram.md, bom.md }
├── phase_2_odometry/
├── phase_3_teleop/
├── phase_4_imu/
├── phase_5_camera/
├── phase_6_lidar/
├── phase_7_slam/
├── phase_8_autonomy/
└── _archive/original_plan/  ← the first-draft plan, preserved (superseded — see note inside)
```

Each phase folder is **self-contained** so you can focus on one milestone, debug it, and not get lost: a `README.md` (goal, wiring tables, software tasks, verification checklist, pitfalls, a measurement lab, and a résumé/interview wrap-up), a `wiring_diagram.md` (cumulative schematic + a checkable wire-by-wire list), and a `bom.md` (just the parts that phase needs, with links).

---

## The eight phases

Work them in order — each builds directly on the last.

| # | Phase | You can… | Difficulty | Time |
|---|---|---|---|---|
| 1 | [Powertrain](./phase_1_powertrain/) | spin all 4 motors under Pico control, safely powered | Medium | 1 wkend |
| 2 | [Odometry & PID](./phase_2_odometry/) | hold a commanded wheel velocity (closed loop) | Medium | 1 wkend |
| 3 | [Teleop + Mecanum](./phase_3_teleop/) | drive — *and strafe* — from your phone | Medium | 1 wkend |
| 4 | [IMU + pose](./phase_4_imu/) | see live (x, y, heading) in the browser | Medium | 1 wkend |
| 5 | [Camera + pan/tilt](./phase_5_camera/) | stream video and look around | Medium | 1 wkend |
| 6 | [Lidar](./phase_6_lidar/) | see a live 360° scan of the room | Medium | 1 wkend |
| 7 | [SLAM](./phase_7_slam/) | build a 2D map by driving around | Hard | 2 wkends |
| 8 | [Autonomy](./phase_8_autonomy/) | click a point → the bot drives there itself | Hard | 2 wkends |

---

## What's different from a generic "4WD car" build

This plan was reconciled against the **actual** MentorPi M1 hardware and hardened with datasheet research + adversarial electrical verification. The headline changes (full rationale in [`_engineering/engineering_decisions.md`](./_engineering/engineering_decisions.md)):

- **Mecanum, not differential.** Real 3-DOF kinematics (`vx, vy, ωz`) so the bot can strafe — modeling Mecanum wheels as left/right is physically wrong.
- **Power redesigned for safety.** A 5 V/3 A power bank *can't* run a Pi 5 (it caps USB to 600 mA and browns out); servos on the Pi rail reboot it. Fixed with a single-LiPo power tree: a 5 V/5 A buck for the Pi, a dedicated UBEC for servos, reverse-polarity protection, coordinated fusing, and a fail-safe e-stop.
- **TB6612FNG drivers** (real headroom over the motor's 1.4 A stall) instead of a driver sitting on its thermal knee.
- **BNO055 over UART**, because it clock-stretches and the Pi's hardware I²C silently corrupts the data.
- **Correct cables and connectors** (the Pi 5's 22-pin CSI; the lidar's 3.3 V UART; the motors' factory JST-PH harness) — the details that stall a build.
- **Learning scaffolding everywhere:** a measurement lab and a résumé/interview wrap-up in every phase, a PID step-response plotting lab, a logic-analyzer in the toolkit, and an optional ROS2-bridge capstone.

---

## Start here
1. Read [`_engineering/safety.md`](./_engineering/safety.md) — **before** you touch a battery.
2. Skim [`_engineering/device_contracts.md`](./_engineering/device_contracts.md) and [`_engineering/products_to_buy.md`](./_engineering/products_to_buy.md).
3. Confirm what your kit actually contains (Mecanum wheels? LFD-01 servos? depth camera?) — see the verify list at the bottom of the BOM.
4. Open [`phase_1_powertrain/`](./phase_1_powertrain/) and build.

*All voltages are nominal — measure with a multimeter before connecting anything. The documents describe the design; the bench is the authority.*
