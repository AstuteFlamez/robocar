# Products to Buy (Master BOM)

> **The big picture.** You own a **Hiwonder MentorPi M1** kit and a **Raspberry Pi 5 (16 GB)**. This project deliberately *bypasses* the kit's RRC Lite controller and ROS2 image and rebuilds the robot's low-level stack yourself, two-brain style (Pi 5 + Pico 2W). So most of the *mechanical* robot (chassis, wheels, motors, lidar, servos, battery) you **reuse**, and you **buy** the DIY brains, drivers, power-conditioning, and protection that the rebuild needs.
>
> Every "buy" line has a link, a quantity, an approximate price, and the **one-line reason it exists** — usually a voltage/current/connector fact from [device_contracts.md](./device_contracts.md). Prices are ballpark (USD, early 2026) and vary by vendor.
>
> Per-phase shopping lists live in each phase folder's `bom.md`. This file is the union of all of them.

---

## ✅ Already have — from the MentorPi M1 kit + your shelf

You do **not** buy these. Confirm them physically before ordering anything else (see ⚠️ verification notes).

| Item | Qty | Notes |
|---|---|---|
| Raspberry Pi 5 (16 GB) | 1 | The high-level brain. |
| Raspberry Pi Pico 2W | 1 | The real-time co-processor. Needs headers (solder if bare). |
| Mecanum chassis + motor mounts + 4× Mecanum wheels | 1 set | ⚠️ **Confirm the wheels have angled rollers (Mecanum).** All kinematics depend on it. |
| "310" metal-gear motors w/ Hall encoders (JGA27-310 / MD310Z20) | 4 | 7.4 V, ≤1.4 A stall, 1:20, JST-PH 2.0 6-pin. |
| LDROBOT STL-19P 360° lidar | 1 | ⚠️ Comes wired to the RRC Lite, **not** as a USB stick → you buy a CP2102 adapter. |
| LFD-01 servos + 2-DOF pan/tilt bracket | 2 + 1 | ⚠️ Confirm included. Else buy 2× MG90S (below). |
| Nuwa-HP60C USB depth camera | 1 | ⚠️ Optional to use. Confirm included; **don't buy one** if absent. |
| 2S LiPo 7.4 V 2200 mAh 10C (w/ built-in protection board) | 1 | The single system battery. Protection board cuts on overcharge / over-discharge / overcurrent / short. Output = keyed **SM-2P male** plug; charge via a separate **DC5.5×2.5 barrel** jack with the 8.4 V charger. (No XT60 — don't cut the factory plug; buy a mating SM-2P pigtail below.) |
| 64 GB microSD card | 1 | For Raspberry Pi OS. |
| 5 V/3 A USB power bank | 1 | **Demoted** — only as an optional tethered bench supply for early phases. Not the robot's supply. |
| Balance LiPo charger | 1 | Non-negotiable for LiPo safety. (If you don't have one, add ~$30.) |

> **The RRC Lite controller and the kit's ROS2 software are intentionally set aside** — not thrown away. Keep them; you can always return the robot to stock, and Phase 8's optional capstone bridges your stack to ROS2.

---

## 🛒 Buy — the DIY brain & drivers (core)

| Item | Qty | ~$ | Why (contract) | Link | Phase |
|---|---|---|---|---|---|
| Pololu **TB6612FNG** dual motor driver carrier (#713) | 2 | $10 | 1.2 A cont / **3.2 A peak** per channel → real headroom over the 310's 1.4 A stall; one motor/channel enables Mecanum. *Replaces the original DRV8833.* | https://www.pololu.com/product/713 | 1 |
| **BNO055** 9-axis IMU breakout (Adafruit #4646 or clone) | 1 | $20 | On-chip fusion → drift-free heading. Wired in **UART** mode to dodge the Pi I²C clock-stretch bug. | https://www.adafruit.com/product/4646 | 4 |
| **PCA9685** 16-ch PWM driver (Adafruit #815 or generic) | 1 | $6 | Hardware servo PWM over I²C, off the real-time loop. V+ from UBEC, addr 0x40. | https://www.adafruit.com/product/815 | 5 |
| **Raspberry Pi Camera Module 3** | 1 | $25 | Gimbal FPV + vision. | https://www.raspberrypi.com/products/camera-module-3/ | 5 |
| **Pi 5 CSI cable, 22-pin → 15-pin** (Adafruit #5820, 300 mm) | 1 | $5 | ⚠️ The Pi 5 CSI port is 22-pin; a generic 15-to-15 cable **will not fit**. | https://www.adafruit.com/product/5820 | 5 |
| **CP2102 USB-UART adapter (3.3 V)** | 1 | $7 | Clean `/dev/ttyUSB0` for the lidar at 230400 baud, 3.3 V TTL; keeps Bluetooth/console free. | https://www.waveshare.com/cp2102-usb-uart-board-type-a.htm | 6 |
| **MG90S** metal-gear servo (*only if kit lacks LFD-01*) | 2 | $6 | Robust pan/tilt. Don't use SG90. (Adafruit's old #1143 is now the digital **MG90D** — buy a genuine MG90S.) | https://www.amazon.com/s?k=MG90S+metal+gear+micro+servo | 5 |

---

## 🛒 Buy — power & protection (safety-critical)

> This whole section is the fix for two verified **UNSAFE** findings: a 5 V/3 A bank can't run a Pi 5 (it caps USB to 600 mA and browns out), and servos on the Pi rail reboot it. See [power_budget.md](./power_budget.md) and [safety.md](./safety.md).

| Item | Qty | ~$ | Why (contract) | Link | Phase |
|---|---|---|---|---|---|
| Pololu **D24V50F5** 5 V/5 A buck regulator (#2851) | 1 | $25 | Powers the Pi 5 properly from the LiPo (6–38 V in, 5 V/5 A out, reverse-protect, soft-start). | https://www.pololu.com/product/2851 | 3 |
| **Hobbywing UBEC-3A** (2–6S → 5 V/3 A) | 1 | $10 | Dedicated servo rail; isolates 1.5 A servo stall from the Pi. | https://www.hobbywingdirect.com/products/ubec-3a-2-6s-lipo-input | 5 |
| Pololu **#2815** MOSFET slide switch *(or rocker + #5381)* | 1 | $8 | Master disconnect **+ reverse-polarity protection** in one part. *Deferred to Phase 3* — Phase 1's keyed SM-2P already prevents reverse-mating, and bench bring-up disconnects by unplugging it. | https://www.pololu.com/product/2815 | 3 |
| **22 mm latching mushroom e-stop, 1×NC** | 1 | $8 | Instant physical motor kill; NC = fail-safe. In the motor branch only. *Added in Phase 3* (the first floor-driving phase) — wheels-in-the-air Phases 1–2 already have unplug / `stop` / STBY-low kills. | https://www.amazon.com/dp/B00W947PS0 | 3 |
| Inline **ATC/ATO blade fuse holders** (18 AWG) + blades **10 A / 7.5 A / 5 A** | 3 + asst | $10 | Coordinated fusing: main / motor branch / Pi branch. Use **time-delay (slow-blow)** blades on the main + motor branch (ride the motor startup/reversal inrush); fast-acting OK on the Pi branch. *Starts in Phase 3* — selectivity only buys you something once the Pi adds a second branch; Phase 1 relies on the LiPo's built-in protection board (one optional 7.5 A inline blade is fine but not required). | https://www.amazon.com/dp/B0DT4NCD5V | 3 |
| **INA219** current+voltage sensor (Adafruit #904) | 1 | $10 | Self-monitoring + low-voltage cutoff. **Set addr 0x41** (0x40 collides w/ PCA9685). | https://www.adafruit.com/product/904 | 4 |
| **2S balance-port low-voltage alarm/buzzer** | 1 | $5 | Dumb hardware backstop against over-discharge. Remove between sessions. | https://hobbyking.com/en_us/cell-checker-with-low-voltage-alarm-2s-8s.html | 1 |
| **SM-2P female pigtail** (mates the LiPo's factory output plug) | 1 | $4 | Breaks the battery's keyed SM-2P male output out to red (+) / black (−) leads. Keyed → can't mate reversed; **don't cut the factory plug.** Replaces the old XT60 assumption. | https://www.amazon.com/Connector-VANDESAIL-Female-Adapter-Electrical/dp/B076HLQ4FX | 1 |
| **WAGO 221-413** lever-nut splice, 3-conductor | 2 | $6 | No-solder rail split: one joins LiPo + → both TB6612 VM, the other joins LiPo − → both GND. ~20 A / 600 V rated. All three ports are joined (no in/out). | https://www.amazon.com/s?k=WAGO+221-413 | 1 |
| **Screw Terminal Block: 6-Pin, 0.1″ pitch, side entry** (Pololu #2495) | 2 | $5 | One 6-wide block lands each driver's six power-edge pins (AO1/AO2/BO2/BO1/VMOT/GND, positions 3–8) in one solder op; the top GND/VCC pins sit outside it and take a male header for the Pico logic jumpers. 150 V / 6 A, 26–18 AWG. Side-entry, short pins — soldered to the board, not a breadboard. One per TB6612, two drivers. | https://www.pololu.com/product/2495 | 1 |
| Bulk electrolytics **470–1000 µF, ≥16 V, low-ESR/105 °C** | ~4 | $8 | Absorb motor/servo start & stall transients (di/dt) so rails don't sag. (No Adafruit SKU meets ≥16 V at this capacitance — source from DigiKey/Mouser, e.g. Nichicon UVZ-series 470 µF/25 V/105 °C.) | https://www.digikey.com/en/products/filter/aluminum-electrolytic-capacitors/58 | 1 |
| **0.1 µF ceramic caps** | 12 | $3 | Brush-noise suppression at each motor body. | https://www.adafruit.com/product/753 | 1 |

---

## 🛒 Buy — wiring, connectors & mechanical (reliability)

> A Mecanum robot vibrates. Loose Dupont and breadboarded motor power are the #1 cause of "it worked yesterday." This section buys reliability.

| Item | Qty | ~$ | Why | Link | Phase |
|---|---|---|---|---|---|
| **Pi Pico screw-terminal carrier board** | 1 | $13 | Move motor/encoder interconnect off the breadboard onto retained terminals. **Never run motor power through a breadboard.** | https://czh-labs.com/products/screw-terminal-block-breakout-module-board-for-raspberry-pi-pico | 2 |
| **JST-PH 2.0 6-pin pigtail leads** (mate the motors' factory connector) | 4 | $6 | Keyed motor+encoder connection; don't cut the factory plug. | https://www.amazon.com/s?k=jst+ph+2.0+6+pin+pigtail | 1 |
| **Ferrule kit + ratcheting ferrule crimp tool** (IWISS HSC8-class, ~23–10 AWG) | 1 | $25 | Solid, low-resistance ferrule terminations on the screw-terminal carrier (bare stranded wire works loose; solder cold-flows under a clamp). *A ferrule crimper does not crimp JST/Dupont pins — those use the pre-made pigtails above.* | https://www.amazon.com/s?k=wire+ferrule+crimping+tool+kit+ratcheting | 1 |
| **Silicone stranded wire**, 18 AWG (motor/power) + 22 AWG (signal) | 1 set | $12 | 18 AWG carries the ~5.6 A all-stall motor current safely. | https://www.amazon.com/s?k=silicone+wire+18+awg+22+awg+kit | 1 |
| **Dupont jumper wires** — M/M (#758) + M/F (#826) + F/F (#266) | 1 set | $8 | Short signal jumps only (I²C/UART), never motor power. Adafruit #758 is **Male/Male only**; add the M/F (#826) and F/F (#266) packs to match your headers. | https://www.adafruit.com/product/758 | 1 |
| **Full-size breadboard** | 1 | $5 | Phase-1/2 *signal* bring-up only. | https://www.adafruit.com/product/239 | 1 |
| **M2.5 nylon standoff kit** | 1 | $10 | Pi 5 / STL-19P / breakouts are M2.5 (not M3); nylon = non-conductive + keeps ferrous mass off the magnetometer. | https://www.cytron.io/p-nylon-m2p5-pcb-standoff-kit-for-raspberry-pi-200pcs | 1 |
| **Heat-shrink tubing** assortment + **wire labels** | 1 | $8 | Motor leads, battery leads, FL/RL/FR/RR labeling. | https://www.amazon.com/s?k=heat+shrink+tubing+assortment | 1 |
| Cable-tie mounts + zip ties + **3M VHB foam tape** | 1 set | $10 | Strain relief + vibration damping. **No hot glue** for structural mounting. | https://www.amazon.com/s?k=adhesive+cable+tie+mounts+zip+ties+3M+VHB+foam+tape | 5 |
| USB-A → micro-USB cable, short (~20 cm), **data-capable** | 1 | $4 | Pi 5 → Pico link (power + serial). | https://www.amazon.com/s?k=usb+a+to+micro+usb+short+data+cable | 3 |

---

## 🛒 Buy — instrumentation & learning tools

> Strongly recommended. These turn invisible signals (PWM, quadrature, I²C/UART, current) into things you can *see, screenshot, and put in a writeup.*

| Item | Qty | ~$ | Why | Link | Phase |
|---|---|---|---|---|---|
| **8-ch 24 MHz USB logic analyzer** (sigrok/PulseView) | 1 | $13 | See PWM duty, quadrature A-leads-B, UART framing, I²C clock-stretch — the exact bug class in this build. | https://www.amazon.com/dp/B0BQ7S16H7 | 1 |
| **Digital multimeter** (continuity + DC volts) | 1 | $25 | Every wire gets a beep test before power-on. If you have one, skip. | https://www.amazon.com/s?k=digital+multimeter | 1 |
| Pocket scope (FNIRSI DSO152) | 1 | $25 | *Nice-to-have, not required.* See analog waveforms. | https://www.amazon.com/s?k=fnirsi+dso152 | optional |

---

## 🛒 Optional — deeper-learning add-ons

| Item | Qty | ~$ | Why | Link | Phase |
|---|---|---|---|---|---|
| Adafruit **ICM-20948** 9-DOF IMU (#4554) | 1 | $15 | "Write your own fusion" track — no clock-stretch issue, shares I²C-1. | https://www.adafruit.com/product/4554 | 4 (opt) |
| STEMMA-QT/Qwiic **5-port** hub (Adafruit #5625) | 1 | $5 | Keyed, reliable I²C fan-out for PCA9685 + INA219. (Passive hub — fine here since the two addresses differ.) | https://www.adafruit.com/product/5625 | 4–5 (opt) |

---

## Cost summary

| Bucket | Approx. |
|---|---|
| Core DIY brain & drivers | ~$75 (less ~$6 if reusing kit LFD-01 servos) |
| Power & protection | ~$101 (XT60 dropped for the LiPo's native SM-2P; +SM-2P pigtail, WAGO splices, two #2495 6-pin screw-terminal blocks) |
| Wiring, connectors, mechanical | ~$110 (one-time tools dominate: crimper, wire, standoffs) |
| Instrumentation | ~$40 (logic analyzer + multimeter) |
| **Subtotal (new parts)** | **~$326** |
| Optional add-ons | +$20 |

Most of the spend is **one-time tooling and safety** (crimper, logic analyzer, buck, reverse-protection, fuses) that you keep for every future project. The robot-specific electronics are ~$75–120. The original plan's ~$120–180 estimate omitted the power-tree and protection that make a LiPo-powered autonomous robot safe — that gap is the difference between "it ran once" and "it runs reliably and didn't catch fire."

---

## ⚠️ Verify on YOUR kit before ordering
1. **Wheels are Mecanum** (angled rollers). If plain wheels → see [engineering_decisions.md](./engineering_decisions.md) for the differential fallback.
2. **Motors are the JGA27 "310"** (27 mm can, 3 mm D-shaped shaft, JST-PH 6-pin). Confirms wheel/coupler fit and the encoder pinout.
3. **LFD-01 servos + bracket included?** If yes, skip the MG90S line.
4. **Nuwa depth camera included?** If yes it's a free optional sensor; if no, ignore it entirely.
5. **Measure, don't trust, `COUNTS_PER_REV`** in Phase 2 (calculated 1040, but verify).
