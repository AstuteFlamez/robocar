# Phase 4 — Wiring Diagram (cumulative)

This is the **full system at the end of Phase 4**. Everything from Phases 1–3 stays exactly as it was; the *new* wires this phase are two short signal harnesses to the Pi's 40-pin header: the **BNO055 IMU over UART** and the **INA219 power sensor over I²C-1**. No Pico wiring changes.

Every connection traces to a contract in [`_engineering/device_contracts.md`](../_engineering/device_contracts.md) and a pin in its **Canonical pin assignments** section.

---

## Cumulative block schematic (end of Phase 4)

```
                          2S LiPo 7.4 V (8.4 V full → 6.0 V empty)
                          2200 mAh · 10C ≈ 22 A capable
                                   │  XT60
                                   ▼
                          ┌─────────────────┐
                          │  10 A MAIN FUSE │   (closest to battery +)
                          └────────┬────────┘
                                   ▼
                    ┌──────────────────────────────┐
                    │ Reverse-polarity + MASTER sw  │  Pololu #2815
                    └───────────────┬───────────────┘
                                    │  protected 7.4 V bus
          ┌─────────────────────────┼─────────────────────────┐
          ▼                         ▼                          ▼
   ┌─────────────┐         ┌─────────────────┐        ┌──────────────────┐
   │ 7.5 A fuse  │         │   5 A fuse      │        │ (servo branch —  │
   │ MOTOR branch│         │   Pi branch     │        │  UBEC, Phase 5)  │
   └──────┬──────┘         └────────┬────────┘        └──────────────────┘
          ▼                         ▼
   ┌──────────────┐        ┌────────────────┐
   │  E-STOP (NC) │        │  D24V50F5      │
   │  in series   │        │  buck 5V/5A    │
   └──────┬───────┘        └───────┬────────┘
          ▼  7.4 V (VM)            ▼  5 V / 5 A  (USB-C)
   ┌──────────────────────┐   ┌──────────────────────────────────────────────┐
   │  2× TB6612FNG        │   │            Raspberry Pi 5                      │
   │  VM = 7.4 V          │   │                                               │
   │  VCC = 3.3 V ◄───┐   │   │  USB-A ─┬─► Pico micro-USB (5V + serial)       │
   │  STBY = HIGH ◄───┤   │   │         └─► (CP2102 lidar — Phase 6)           │
   │  AO/BO ─► 4 motors│   │   │                                               │
   └──────┬────────────┘   │   │   40-pin header:                              │
          │ motor leads    │   │     pin 8  TXD (GP14) ─────┐                  │
          ▼                │   │     pin 10 RXD (GP15) ───┐ │                  │
      [FL][RL][FR][RR]     │   │     pin 3  SDA  (GP2) ──┐ │ │                 │
       Mecanum wheels      │   │     pin 5  SCL  (GP3) ─┐│ │ │                 │
       (45° rollers)       │   │     pin 1  3V3 ──────┐ ││ │ │                 │
          ▲ encoders       │   │     pin 6  GND ────┐ │ ││ │ │                 │
          │ A/B            │   └────────────────────┼─┼─┼┼─┼─┼─────────────────┘
   ┌──────┴───────────┐    │                        │ │ ││ │ │
   │     Pico 2W      │◄───┘ USB (5V + serial)      │ │ ││ │ └── BNO055 TX
   │  3V3(OUT) ──► TB6612 VCC ×2 + 4× enc VCC       │ │ ││ └──── BNO055 RX
   │  GP2–GP13 PWM/dir, GP14 STBY                   │ │ │└────── INA219 SDA / (PCA9685 Ph5)
   │  GP16/17,18/19,20/21,26/27 ◄ encoders (PIO)    │ │ └─────── INA219 SCL / (PCA9685 Ph5)
   └───────────────────────────────────────────────┘ │ └─────── 3V3 → BNO055 VIN + INA219 VCC
                                                      └───────── GND → BNO055 GND + INA219 GND
                          ┌──────────────────────────────────────────────┐
        NEW THIS PHASE →  │  BNO055 9-axis IMU      INA219 power sensor    │
                          │  PS1 = HIGH (UART)      addr = 0x41            │
                          │  VIN ◄ 3V3   GND ◄ GND  VCC ◄ 3V3  GND ◄ GND   │
                          │  TX  ► RXD(10) RX ◄ TXD(8)  SDA ◄►SDA  SCL ◄►SCL│
                          │  mounted ≥5 cm from      Vin+/Vin− across the   │
                          │  motors+LiPo, nylon      sensed power line       │
                          └──────────────────────────────────────────────┘

   ───────────────────────── COMMON GROUND (single-point STAR at LiPo −) ─────────
   Separate legs to: each TB6612, the buck, the Pico, the Pi. BNO055 GND and
   INA219 GND ride the Pi's GND leg. No daisy-chaining; motor return is heavy 18 AWG.
```

---

## What changed vs Phase 3

| | Phase 3 (end) | Phase 4 (end) |
|---|---|---|
| Pi UART (pins 8/10) | unused | **BNO055** IMU @ 115200 (UART mode) |
| Pi I²C-1 (pins 3/5) | unused | **INA219** @ **0x41** (PCA9685 joins in Ph5) |
| Pi 3.3 V (pin 1/17) | unused | BNO055 VIN + INA219 VCC |
| Heading source | none (wheels only) | absolute, from IMU |
| Pose `(x,y,θ)` | none | live, holonomic dead-reckoning |
| Pico wiring | motors+encoders | **unchanged** |

---

## Wire-by-wire connection list — NEW wires this phase

> Two short harnesses, all to the Pi 5's 40-pin header. Use 22 AWG signal wire / Dupont; these are logic-level, low-current. Contracts: [BNO055](../_engineering/device_contracts.md#bno055-9-axis-imu-adafruit-4646-or-clone--uart-mode-not-ic) (UART mode), [INA219](../_engineering/device_contracts.md#ina219-currentvoltage-sensor-adafruit-904--self-monitoring) (0x41).

### A. BNO055 IMU — UART (NOT I²C)

- [ ] `BNO055 PS1` ──► `Pi 5 3.3 V` (header pin 1) — **selects UART mode (PS1 = HIGH).** ⚠️ Floating/LOW = I²C = the clock-stretch bug.
- [ ] `BNO055 VIN` ──► `Pi 5 3.3 V` (header pin 1 or 17) — logic supply. ⚠️ **3.3 V, never 5 V** on bare-chip clones.
- [ ] `BNO055 GND` ──► `Pi 5 GND` (header pin 6 or 9) — common ground (star).
- [ ] `BNO055 TX` (silk may read **SDA**) ──► `Pi 5 RXD / GPIO15` (header pin 10) — IMU transmit → Pi receive.
- [ ] `BNO055 RX` (silk may read **SCL**) ──► `Pi 5 TXD / GPIO14` (header pin 8) — Pi transmit → IMU receive. ⚠️ **TX/RX cross over** — straight-through = silence.

### B. INA219 power sensor — I²C-1 @ 0x41

- [ ] `INA219 VCC` ──► `Pi 5 3.3 V` (header pin 1 or 17) — logic supply (I²C pull-ups reference 3.3 V).
- [ ] `INA219 GND` ──► `Pi 5 GND` (header pin 9 or 14) — common ground (star).
- [ ] `INA219 SDA` ──► `Pi 5 SDA1 / GPIO2` (header pin 3) — I²C data.
- [ ] `INA219 SCL` ──► `Pi 5 SCL1 / GPIO3` (header pin 5) — I²C clock.
- [ ] `INA219 A0/A1 address jumpers` ──► **set to 0x41** — ⚠️ NOT default 0x40 (collides with the Phase-5 PCA9685).

### C. INA219 power-path sense leads (high-side shunt)

The INA219 measures current through an internal 0.1 Ω shunt between **Vin+** and **Vin−**, and reads bus voltage at **Vin−**. Insert the shunt **in series on the high side of the rail you want to watch.** For this build, watch the **whole pack**: put it right on the **LiPo + line, just after the 10 A main fuse**, so you see *total* current (Pi + motors + servos) *and* the true pack voltage your low-voltage cutoff depends on.

- [ ] `LiPo + (just after the 10 A main fuse)` ──► `INA219 Vin+` — current enters the shunt.
- [ ] `INA219 Vin−` ──► `protected 7.4 V bus (on to the switch + branches)` — current leaves the shunt to the load. ⚠️ Do **not** break ground for the shunt; it goes in the **positive** line only.

> ⚠️ **Know the INA219's ceiling.** Its built-in 0.1 Ω shunt tops out around **±3.2 A**, so the ~5.6 A all-stall (and even the ~3.5 A Pi peak) will **clip** the *current* reading. The *bus-voltage* reading stays accurate through all of it — and that's what the low-voltage cutoff actually keys on. Treat the INA219 as a **monitor + voltage watchdog**, not a precision stall ammeter. To capture the full stall transient, use the optional INA226 + external shunt noted in [`products_to_buy.md`](../_engineering/products_to_buy.md).

---

## Pin tables (this phase)

### Pi 5 → BNO055 (UART)

| Pi 5 pin | Pi function | BNO055 pin | Direction |
|---|---|---|---|
| 1 (or 17) | 3.3 V | PS1 | → (tie HIGH = UART) |
| 1 (or 17) | 3.3 V | VIN | → power |
| 6 (or 9) | GND | GND | — |
| 8 | TXD / GPIO14 | RX (silk: SCL) | Pi → IMU |
| 10 | RXD / GPIO15 | TX (silk: SDA) | IMU → Pi |

### Pi 5 → INA219 (I²C-1)

| Pi 5 pin | Pi function | INA219 pin |
|---|---|---|
| 1 (or 17) | 3.3 V | VCC |
| 9 (or 14) | GND | GND |
| 3 | SDA1 / GPIO2 | SDA |
| 5 | SCL1 / GPIO3 | SCL |
| — | (jumpers) | A0/A1 → **0x41** |

> The Pi's **5 V pins (2, 4) stay unused** in this build — servo transients (Phase 5) are kept off the Pi rail. The PCA9685 will share the *same* I²C-1 pins (3/5) next phase at 0x40; that's why the INA219 takes 0x41 now.

---

## ⚠️ Footguns that apply to THIS phase's wires

- **BNO055 PS1 must be HIGH.** This is the difference between a clean UART heading and the I²C clock-stretch bug that corrupts orientation bytes. Tie PS1 to 3.3 V explicitly; don't leave it floating.
- **BNO055 is on UART, not I²C.** Do not connect the IMU's SDA/SCL to the Pi's I²C pins (3/5). In UART mode those same physical pins carry TX/RX and go to UART pins 8/10.
- **Cross TX↔RX.** Pi TXD (pin 8) → IMU RX; Pi RXD (pin 10) → IMU TX. The single most common UART mistake.
- **3.3 V, not 5 V, to VIN** (and PS1). A bare-chip BNO055 clone with no regulator dies on 5 V.
- **INA219 at 0x41, not 0x40.** Set the address jumpers now or you'll have a silent bus collision the moment the PCA9685 arrives in Phase 5.
- **INA219 shunt in the + line only**, never breaking ground. The common-ground star must stay intact.
- **Mount the IMU ≥ 5 cm from motors and the LiPo, on nylon standoffs, no ferrous screws** — otherwise the magnetometer (hence heading) goes haywire when motors run.
- **Free the Pi's UART first** (disable the serial login console + Bluetooth, `dtoverlay=disable-bt`). If the console still owns `/dev/serial0`, the IMU reads garbage even with perfect wiring.
- **Don't disturb the carried-over footguns:** encoder VCC stays at **3.3 V** (not 7.4 V); TB6612 **STBY stays HIGH**; common ground stays a **single-point star** at the LiPo negative. None of those wires change this phase — leave them alone.

---

## Continuity ritual before power-on (delta for this phase)

Battery disconnected, switch off (see [`_engineering/safety.md`](../_engineering/safety.md)):
- [ ] No beep between any 3.3 V pin and any GND (no short on the new harnesses).
- [ ] BNO055 PS1 reads continuous to 3.3 V (HIGH).
- [ ] INA219 VCC reads to 3.3 V (not 5 V), GND to the star.
- [ ] INA219 Vin+/Vin− are in series in the **positive** rail (ohm-meter shows the shunt, ~0.1 Ω, between them; *not* a dead short to ground).
- [ ] Pi TXD (8) → IMU RX and Pi RXD (10) → IMU TX (crossed, not straight).
