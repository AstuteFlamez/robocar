# Phase 5 — Wiring Diagram (cumulative, full system at end of Phase 5)

This shows the **entire robot** as it stands when Phase 5 is finished. New this phase: the **PCA9685** on I²C-1 (0x40), the **UBEC-3A → PCA9685 V+** servo rail with its **bulk cap**, the **two LFD-01 servos**, and the **Camera Module 3** on the **22-pin CSI** port via the **#5820** cable. Everything else (motors, drivers, Pico, buck, IMU on UART, INA219, protection) carries over from Phases 1–4.

Every connection here traces to `_engineering/device_contracts.md` (the contract) and `_engineering/power_budget.md` (the rail sizing).

## Block schematic

```
                      2S LiPo 7.4V (8.4V full -> 6.0V empty), URGENEX 2x1800mAh 35C (~63A), no PCB (protected by main fuse + #2815)
                                      |  Deans T-plug (keyed) -> pigtail
                                      v
                              [ 10 A MAIN FUSE ]            (closest to battery +)
                                      |
                          [ Reverse-polarity + MASTER switch ]   Pololu #2815
                                      |  protected 7.4 V bus
        +-----------------------------+----------------------------------+
        |                             |                                  |
   [ 7.5 A fuse ]                [ 5 A fuse ]                        [ servo branch ]
   MOTOR branch                  Pi branch                          UBEC input
        |                             |                                  |
   [ E-STOP (NC) ]               +----------+                      +-------------+
        |                        | D24V50F5 |  buck 5V/5A          | Hobbywing   |
        |                        +----------+                      | UBEC-3A     |
        v                             | 5V USB-C                   | 5V / 3A     |
  +-------------------+               v                            +-------------+
  | 2x TB6612FNG VM   |       +-------------------------+                | 5V V+
  | (7.4 V)           |       |    Raspberry Pi 5 (16GB)|                v
  |  bulk 470-1000uF  |       |                         |        +-------------------+
  |  +0.1uF/motor     |       |  USB-A   I2C-1   UART0  |        | PCA9685 (0x40)    |
  +---+----+----+--+--+       |  ports   (3/5)  (8/10)  |        |  V+  <-- UBEC 5V  |==[470-1000uF bulk cap to GND]
      |AO/BO outputs |        |  CSI(22) 3V3    GND     |        |  VCC <-- Pi 3.3V  |   (V+ != VCC !)
      v    v    v    v        +--+----+----+-----+---+--+        |  SDA <-- Pi GPIO2 |
   [FL] [RL] [FR] [RR]           |    |    |     |   |           |  SCL <-- Pi GPIO3 |
   310 motors (Mecanum)          |    |    |     |   |           |  GND --- star     |
      ^ encoders (3.3V)          |    |    |     |   |           +----+---------+----+
      |                          |    |    |     |   |           CH0 |         | CH1
      |                          |    |    |     |   |          (S/V+/GND)   (S/V+/GND)
  +---+--------------+           |    |    |     |   |               v             v
  |   Pico 2W        |<==========+    |    |     |   |          +---------+   +---------+
  |  PWM/PIO/PID     | USB-A->microUSB|    |     |   |          | LFD-01  |   | LFD-01  |
  |  3V3 OUT ->      | (power+serial) |    |     |   |          |  PAN    |   |  TILT   |
  |   driver VCC x2  |                |    |     |   |          +----+----+   +----+----+
  |   + 4x enc VCC   |                |    |     |   |               |             |
  +------------------+      (Phase 6) |    |     |   |               +-----+-------+
                            CP2102 -->+    |     |   |                     | servo horns
                            ->lidar USB    |     |   |                     v  move the gimbal
                                           |     |   |              +----------------+
                          I2C-1 SDA/SCL ---+     |   |              |  Camera Mod 3  |
                           to PCA9685 0x40       |   |              |  (on gimbal)   |
                           + INA219  0x41        |   |              +-------+--------+
                                                 |   |                      ^
                          UART0 (8/10) ----------+   |              22-pin CSI |  #5820
                           to BNO055 (PS1=HIGH)      |              22->15 FPC | cable
                                                     |              ----------+ (to Pi CSI)
                          CSI 22-pin ----------------+

   ============================= COMMON GROUND (single-point STAR at LiPo -) =============================
   Separate legs to: TB6612 x2, D24V50F5, UBEC, PCA9685, Pico, Pi. No daisy-chaining.
   The UBEC's 5V return and the PCA9685 GND join the SAME star -- this is what gives the
   servos a return path while keeping their current OUT of the logic ground.
```

> **Reading the new branch:** the UBEC takes 7.4 V from the protected bus and makes its own 5 V, which goes *only* to PCA9685 **V+**. The PCA9685 **VCC** (logic) is a different pin fed by the Pi's **3.3 V**. The servos draw their power from V+ (through the board's V+ rail to each CH header), so their stall/inrush current loops UBEC → V+ → servo → GND-star → UBEC and **never touches the Pi**.

## Wire-by-wire connection list — NEW wires this phase

Check each off as you make it. Endpoints use the canonical pins from `_engineering/device_contracts.md`.

### Servo power rail — UBEC → PCA9685 V+ (the dedicated 5 V actuator rail)

- [ ] `LiPo protected 7.4 V bus (servo branch)` --> `UBEC-3A input +` — feeds the servo regulator from the same battery (contract: UBEC accepts 2–6S) 
- [ ] `LiPo (−) / star ground` --> `UBEC-3A input −` — UBEC ground leg to the star
- [ ] `UBEC-3A 5 V output +` --> `PCA9685 V+` (screw terminal) — **servo power only; 5 V; NEVER a Pi 5 V pin**
- [ ] `UBEC-3A 5 V output −` --> `PCA9685 GND` (and to the star) — servo-side return
- [ ] `470–1000 µF bulk cap (+)` --> `PCA9685 V+`  ·  `cap (−)` --> `PCA9685 GND` — absorbs the 3–4 A servo inrush so the rail doesn't sag (observe polarity!)

### PCA9685 logic side — joins I²C-1 with the INA219

- [ ] `Pi 5 3.3 V` (header pin 1 or 17) --> `PCA9685 VCC` — **logic supply, 3.3 V; references the Pi's I²C pull-ups; this is NOT V+**
- [ ] `Pi 5 GND` (header pin 6 or 9) --> `PCA9685 GND` — logic ground to the star (same GND pad as servo return on the breakout)
- [ ] `Pi 5 GPIO2 / SDA1` (header pin 3) --> `PCA9685 SDA` — I²C data; shares the bus with INA219 (0x41)
- [ ] `Pi 5 GPIO3 / SCL1` (header pin 5) --> `PCA9685 SCL` — I²C clock; shares the bus with INA219 (0x41)

### PCA9685 → servos (3-pin servo cable lands on a channel; V+/GND come from the board rails)

- [ ] `PCA9685 CH0 signal` --> `Pan servo (LFD-01) signal` — 50 Hz, 500–2500 µs PWM; rotates camera left/right
- [ ] `PCA9685 CH0 V+` --> `Pan servo V+` — 5 V from the V+ rail (UBEC-sourced)
- [ ] `PCA9685 CH0 GND` --> `Pan servo GND` — return to the star
- [ ] `PCA9685 CH1 signal` --> `Tilt servo (LFD-01) signal` — rotates camera up/down
- [ ] `PCA9685 CH1 V+` --> `Tilt servo V+` — 5 V from the V+ rail
- [ ] `PCA9685 CH1 GND` --> `Tilt servo GND` — return to the star

### Camera — CSI ribbon (no power wires; the Pi powers the camera over the ribbon)

- [ ] `Pi 5 CSI port (22-pin 0.5 mm)` --> `#5820 cable, 22-pin end` — lift the Pi CSI latch, seat fully, close latch; contacts face the correct way
- [ ] `#5820 cable, 15-pin end` --> `Camera Module 3 (15-pin 1 mm)` — seat in the camera's connector, close its latch
- [ ] *(Optional)* `Nuwa-HP60C depth camera (USB-C)` --> `Pi 5 USB-A` — body-fixed forward; counts ~0.4 A against the 5 V USB budget

> All wires from Phases 1–4 (motors, encoders, drivers, STBY, Pico, buck → Pi, BNO055 on UART, INA219 on I²C) **stay in place.** This phase only adds the rows above.

## Pin tables (the slice that changed this phase)

### Pi 5 — additions and existing I²C-1 bus

| Pi 5 pin | Net | Goes to |
|---|---|---|
| Pin 1 or 17 (3.3 V) | 3V3 logic | **PCA9685 VCC** (+ BNO055 VIN, INA219 VCC) |
| Pin 3 (GPIO2 / SDA1) | I²C-1 SDA | **PCA9685 SDA** + INA219 SDA |
| Pin 5 (GPIO3 / SCL1) | I²C-1 SCL | **PCA9685 SCL** + INA219 SCL |
| Pin 6 or 9 (GND) | star ground | **PCA9685 GND** (logic) |
| CSI (22-pin) | MIPI | **Camera Module 3** via #5820 |
| Pin 2 / Pin 4 (5 V) | — | **UNUSED** (servos never take Pi 5 V) |

### I²C-1 bus map (after this phase)

| Address | Device | Notes |
|---|---|---|
| 0x40 | **PCA9685** (NEW) | servo PWM driver; default address |
| 0x41 | INA219 | pack V/I monitor (Phase 4); set to 0x41 to avoid colliding with 0x40 |

### PCA9685 power pins — the two-rail rule

| Pin | Rail | Source | Voltage |
|---|---|---|---|
| VCC | logic | Pi header | **3.3 V** |
| V+ | servo power | **UBEC** | **5 V** |

## ⚠️ Footguns that apply to THIS phase's wires

- **⚠️ PCA9685 VCC ≠ V+.** VCC is **3.3 V logic from the Pi**; V+ is **5 V servo power from the UBEC**. They are separate pins on the same board — bridging them dumps 5 V onto the 3.3 V logic/I²C net and kills the board (and can backfeed the Pi). Beep-test them apart during the continuity ritual.
- **⚠️ Servo V+ is the UBEC, NOT the Pi's 5 V header.** This is the bug the original plan had. Two LFD-01 servos = ~1.5 A stall + 3–4 A inrush; on the Pi rail that sag (below 4.63 V) reboots the Pi. The Pi's 5 V pins (2, 4) stay unused.
- **⚠️ Put the bulk cap on V+, and observe polarity.** The 470–1000 µF electrolytic across V+↔GND absorbs the servo inrush. Backwards = it pops. The stripe marks the negative leg.
- **⚠️ Common-ground star, not a daisy-chain.** The UBEC return *and* the PCA9685 GND must reach the LiPo-negative star point. If you instead daisy-chain the servo ground through the logic ground, servo current pulses corrupt the 3.3 V reference your I²C signals ride on.
- **⚠️ CSI cable is 22-to-15, and orientation matters.** Use the **#5820** adapter (a generic 15-to-15 will not fit a Pi 5). The 22-pin end goes to the Pi, the 15-pin end to the camera; contacts must face the correct way and both latches fully closed, or the camera won't enumerate.
- **(Carried-over footguns still true on the bot):** TB6612 **STBY must be HIGH**; encoder **VCC = 3.3 V, not 7.4 V**; **BNO055 is on UART, not I²C** (that's *why* I²C is free for the PCA9685); the lidar's PWM pin (Phase 6) ties to **GND**.
