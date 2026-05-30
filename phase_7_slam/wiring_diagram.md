# Phase 7 — Wiring diagram (cumulative system, unchanged)

> **No hardware changes this phase — all software.** Phase 7 (SLAM mapping) adds
> **zero new wires, zero new pins, zero new rails.** Every sensor and bus the SLAM
> node consumes — the lidar on `/dev/ttyUSB0`, the Pico odometry over USB serial,
> the BNO055 heading over UART — was already wired and verified in Phases 1–6.
> This diagram is therefore the **complete system carried unchanged from the end of
> Phase 6**, reproduced here so you have one authoritative picture of the machine
> while you write the SLAM code, and so you can re-run the continuity ritual before
> a long mapping run. Every pin, voltage, and bus below traces to
> `_engineering/device_contracts.md` (canonical pin map + end-to-end connection
> matrix). **If you find yourself reaching for a wire stripper in Phase 7, stop —
> you've drifted out of scope. The only thing that changes this phase is code.**

---

## Full system block schematic (end of Phase 7 = end of Phase 6, unchanged)

```
                         ┌──────────────────────────────────────────────┐
                         │            2S LiPo  7.4 V  2200 mAh 10C        │
                         │         (8.4 V full → 6.0 V empty, ~22 A)      │
                         └───────────────────────┬──────────────────────┘
                                                 │ SM-2P (keyed) → pigtail
                                          ┌──────▼──────┐
                                          │ 10 A MAIN   │  (closest to LiPo +)
                                          │ FUSE        │
                                          └──────┬──────┘
                                       ┌─────────▼──────────┐
                                       │ Reverse-polarity + │  Pololu #2815
                                       │ MASTER switch      │  (P-FET high-side)
                                       └─────────┬──────────┘
                                  protected 7.4 V bus
          ┌─────────────────────────────┬────────┴────────────┬───────────────────────┐
          ▼                             ▼                      ▼
   ┌────────────┐               ┌──────────────┐       ┌──────────────┐
   │ 7.5 A fuse │               │  5 A fuse    │       │ UBEC input   │
   │ MOTOR br.  │               │  Pi branch   │       │ (servo br.)  │
   └─────┬──────┘               └──────┬───────┘       └──────┬───────┘
         ▼                             ▼                      ▼
   ┌────────────┐               ┌──────────────┐      ┌───────────────┐
   │ E-STOP (NC)│               │ D24V50F5     │      │ Hobbywing UBEC│
   │ in series  │               │ buck 5V / 5A │      │  5 V / 3 A    │
   └─────┬──────┘               └──────┬───────┘      └──────┬────────┘
         │ 7.4 V VM                     │ 5 V USB-C           │ 5 V  V+ (NOT VCC!)
         ▼                             ▼                      ▼
 ┌────────────────┐        ┌──────────────────────┐   ┌──────────────┐
 │ 2× TB6612FNG   │        │   Raspberry Pi 5     │   │  PCA9685     │ I²C 0x40
 │  VM = 7.4 V    │        │  (high-level brain)  │   │  V+ = 5 V    │
 │  VCC = 3.3 V ◄─┼──┐     │  usb_max_current=1   │   │  VCC = 3.3 V │◄─ Pi 3.3 V
 │  STBY ◄ GP14   │  │     │                      │   └──┬────────┬──┘
 │ A:FL/RL B:FR/RR│  │     │  CSI(22pin)──FPC#5820│      │ CH0    │ CH1
 └───┬────────────┘  │     │   └► Camera Module 3 │      ▼ pan    ▼ tilt
     │ AO/BO 7.4 V   │     │                      │   ┌──────────────┐
     ▼               │     │  I²C-1 (pin 3/5) ────┼──►│ INA219        │ 0x41
 ┌────────────┐      │     │  UART0 (pin 8/10)────┼──►│ BNO055 (UART) │ PS1=HIGH
 │ 4× 310 motor│     │     │                      │   └──────────────┘
 │  + encoder  │     │     │  USB-A ×4 ───────────┤
 │ FL RL FR RR │     │     │     ├─► Pico micro-USB (5 V VBUS + serial)
 └──────┬──────┘     │     │     ├─► CP2102 (3.3 V) ─► STL-19P lidar  (/dev/ttyUSB0)
        │ A/B 3.3 V  │     │     └─► (opt) Nuwa depth cam
        │ quadrature │     └──────────────────────┘
        ▼            │ 3V3(OUT) ≤300 mA
 ┌──────────────────────────────┐
 │           Pico 2W            │  real-time co-processor
 │  GP2..GP13  → TB6612 IN/PWM  │  20 kHz PWM, per-wheel PID
 │  GP14       → STBY (HIGH)    │  command-timeout watchdog
 │  GP16..27   ← 4× encoder A/B │  PIO quadrature decode (1 SM each)
 │  3V3(OUT)   → TB6612 VCC ×2  │
 │             + 4× encoder VCC │  ◄─ VBUS 5 V from Pi USB
 └──────────────────────────────┘

 ───────────────────────────── COMMON GROUND (STAR at LiPo −) ─────────────────────────
 Separate legs: motor return (heavy 18 AWG) · buck · UBEC · Pico/Pi logic. No chains.
 The CP2102's GND reaches the star through the Pi's USB ground.
```

Legend for the Pi USB-A ports: Pico micro-USB (power + serial), CP2102 → lidar,
and an optional depth cam / spare. **None of these three USB peripheries changed
in Phase 7** — SLAM just *reads* the streams they already produce.

---

## Software data flow added this phase (no wires — for orientation only)

Phase 7 adds **one new piece of software** on the Pi 5 — a SLAM node — that
consumes two streams you already produce and emits two new things (a live
occupancy-grid map and a corrected pose). All of this rides **existing** links:
the lidar's USB-serial (`/dev/ttyUSB0`), the Pico's USB-serial telemetry, and the
BNO055's UART. ★ Not one byte of it touches the GPIO header, the I²C bus, or any
power rail.

```
   STL-19P lidar  ──► /dev/ttyUSB0 ──► scan buffer ─┐   (360° scans, ~450 pts, 10 Hz)
   (Phase-6 parser, 230400 baud)                    │
                                                     ▼
   Pico odom (USB serial) ─┐                  ┌───────────────────────┐
   BNO055 heading (UART) ──┴─► holonomic ─────► (dt, dxy, dθ) ─► SLAM  │
                              odometry          per-step delta   (Breezy│
                              (vx,vy,ωz→pose)  (strafe included!) SLAM, │
                                                                 RMHC)  │
                                                  ┌──────────────┴──────┘
                                  getmap() ◄──────┤      getpos() ─► corrected pose
                                       │          │
                                       ▼          ▼
                              occupancy grid   PGM save / load
                              (0=wall, 127=unknown, 255=free)
                                       │          │
                                       └────┬─────┘
                                            ▼
                                   web UI  /map PNG + pose triangle
```

★ The odometry feed is a **per-step pose delta `(dt, dxy, dθ)`**, *not* a velocity
model — and `dxy` is the **magnitude of the full 2-D world-frame displacement,
strafe included** (`hypot(dx, dy)`). This is the Mecanum-specific subtlety: lateral
motion is *true* odometry, not noise. See the Phase-7 README's "Mecanum note."

---

## Wire-by-wire connection list — NEW wires this phase

**There are none.** Phase 7 introduces **zero new physical connections.** Instead
of a new wire checklist, confirm the *existing* harness (from Phases 1–6) is intact
before a mapping run — a flaky connection here doesn't smoke, it quietly produces a
bad map:

- [ ] CP2102 → lidar link seated; lidar enumerates as `/dev/ttyUSB0` and spins at
      10 Hz. *(Contract: CP2102 jumper @ 3.3 V, lidar TX 3.3 V TTL @ 230400 8N1,
      PWM pin → GND. device_contracts.md "CP2102 (3.3 V) → STL-19P Tx".)* The scan
      stream is half of what SLAM eats.
- [ ] Pi↔Pico USB cable seated and **strain-relieved** — it carries the wheel
      telemetry that becomes odometry, the *other* half of what SLAM eats.
      *(Contract: Pi USB-A → Pico micro-USB; 5 V VBUS + serial. device_contracts.md.)*
- [ ] All four motor/encoder JST-PH 6-pin connectors fully seated. *(A jiggled-loose
      encoder corrupts odometry → corrupts the SLAM pose → smears the map.
      device_contracts.md "310 encoder A/B → Pico PIO pins". Encoder VCC = 3.3 V,
      never 7.4 V.)*
- [ ] BNO055 UART link (pins 8/10) intact and PS1 = HIGH. *(Heading drift goes
      straight into your `dθ`. device_contracts.md "Pi UART → BNO055 (PS1=HIGH)".)*

> ★ **Garbage odometry in → garbage map out.** SLAM's scan matcher corrects *small*
> odometry errors; it cannot rescue a loose encoder or a wrong `COUNTS_PER_REV`. The
> "wires" that matter most this phase are the ones that were already there — verify
> them, don't add to them.

---

## ⚠️ Footguns inherited from earlier phases (still live — do NOT regress)

Phase 7 adds no wires, but it *leans on* the existing harness in a new way: every
flaw now surfaces as a warped or rotated map instead of obvious smoke. Re-check
these before mapping:

- **⚠️ Lidar PWM pin (Pin 2) → GND.** The STL-19P's default closed-loop **10 Hz**
  spin requires the PWM pin tied to GND; floating = erratic/no spin = stale or empty
  scans feeding SLAM. The whole "drive slow because a scan takes 100 ms" rule assumes
  a steady 10 Hz. (device_contracts.md: STL-19P.)
- **⚠️ CP2102 jumper on 3.3 V, never 5 V.** The lidar TX is **3.3 V TTL and NOT 5 V
  tolerant.** Verify with a multimeter. (This is the same 3.3 V-world rule that keeps
  encoder VCC at 3.3 V.)
- **⚠️ Lidar 0° ≠ bot forward.** Not a wire, but a frame bug that *acts* like one: if
  the lidar's 0° reference (marked in Phase 6) isn't aligned to the bot's forward
  axis, the scan is rotated relative to the pose and SLAM never converges — or builds
  a map rotated off your heading. Apply the angular offset you documented in Phase 6,
  or align the mount.
- **⚠️ Encoder VCC = 3.3 V (Pico 3V3 OUT), NEVER the 7.4 V motor rail.** Keeps the
  A/B outputs inside the Pico's GPIO limit. A drift to 7.4 V here doesn't just fry a
  pin — it corrupts odometry, which corrupts the SLAM pose. Continuity-check each
  encoder VCC to 3V3, **not** to VM.
- **⚠️ BNO055 on UART, not I²C** (PS1 = HIGH). The Pi's hardware I²C can't handle the
  BNO055's clock-stretching and silently corrupts orientation bytes — a corrupted
  heading feeds a corrupted `dθ` straight into SLAM.
- **⚠️ TB6612 STBY (GP14) must stay HIGH** or the motors are silently disabled — and
  a bot that won't move maps nothing.
- **⚠️ Star ground intact.** Sustained motor current on a daisy-chained ground
  modulates the logic reference and injects noise into encoder edges and UART
  framing. Beep every ground leg back to the single star point at LiPo −.

---

## Pin tables (reference — unchanged from the canonical map)

> Nothing changes this phase. These are reproduced from
> [device_contracts.md](../_engineering/device_contracts.md#canonical-pin-assignments)
> so you can sanity-check the data path feeding SLAM without flipping documents.

### Raspberry Pi 5 — sensor buses feeding SLAM (40-pin header + USB)

| Function | Pin / port | Device | Role in SLAM |
|---|---|---|---|
| USB-A | any free port | CP2102 → STL-19P | lidar scans @ `/dev/ttyUSB0`, 230400 8N1, ~450 pts/rev @ 10 Hz |
| USB-A | any free port | Pico 2W (micro-USB) | wheel telemetry → holonomic odometry |
| UART0 TXD / RXD | 8 / 10 | BNO055 (PS1=HIGH) | drift-free heading → odometry `dθ` |
| 3.3 V | 1, 17 | BNO055 VIN, PCA9685 VCC, INA219 VCC | logic power |
| GND | 6, 9, 14, 20, 25, 30, 34, 39 | star ground | reference |
| I²C-1 SDA / SCL | 3 / 5 | PCA9685 0x40, INA219 0x41 | (not used by SLAM; gimbal + power monitor) |
| USB-C | — | ← D24V50F5 5 V/5 A buck | Pi power |
| CSI (22-pin) | — | Camera Module 3 via FPC #5820 | (camera; competes for CPU with SLAM) |

### Pico 2W — motors & encoders (the odometry source)

| Wheel | Board · ch | IN1 | IN2 | PWM | Enc A (base) | Enc B |
|---|---|---|---|---|---|---|
| FL | A · A | GP2 | GP3 | GP4 | GP16 | GP17 |
| RL | A · B | GP5 | GP6 | GP7 | GP18 | GP19 |
| FR | B · A | GP8 | GP9 | GP10 | GP20 | GP21 |
| RR | B · B | GP11 | GP12 | GP13 | GP26 | GP27 |
| **STBY** (both boards) | — | **GP14 → HIGH** | | | | |

Encoder VCC ← Pico **3V3(OUT)** (≤ 300 mA total); encoder GND → star ground.

### STL-19P → BreezySLAM `Laser` parameters (data path, not wiring)

| Laser param | Value | Source |
|---|---|---|
| `scan_size` | **measure it** (≈450) | count points in one real revolution (Phase 6) |
| `scan_rate_hz` | 10 | STL-19P closed-loop default spin (PWM pin → GND) |
| `detection_angle_degrees` | 360 | full-circle DTOF |
| `distance_no_detection_mm` | 12000 | STL-19P 12 m max range (device_contracts.md) |
| `offset_mm` | 0 (or lidar-to-center distance) | lidar mount offset from wheelbase center |

> The Pi's 5 V header pins (2, 4) stay **unused** — servo power comes from the UBEC's
> V+, never the Pi rail, so servo transients can't sag the Pi mid-mapping.
