# Phase 3 — Wiring Diagram (cumulative, end of phase)

> This is the **full system state at the end of Phase 3.** Phases 1–2 ran the motor + encoder wiring off a deliberately minimal bench power path (LiPo **SM-2P** → female pigtail → **WAGO** splits → both drivers' VM/GND). This phase is where the robot grows up: the Pi 5 joins as a **second power branch**, and with more than one branch to protect — and the bot now driving on the floor under teleop — the **full protection tree gets built for the first time**: the **10 A main fuse**, the **#2815 reverse-polarity master switch**, the **7.5 A motor-branch fuse**, the **5 A Pi-branch fuse**, and the **NC e-stop**, alongside the D24V50F5 buck and the single Pi↔Pico USB bridge. The diagram below shows *everything* so you can wire and verify the whole bot against one picture. Every connection traces to a contract in [`_engineering/device_contracts.md`](../_engineering/device_contracts.md); power sizing is in [`_engineering/power_budget.md`](../_engineering/power_budget.md).
>
> **NEW this phase is marked `◀── NEW`.**

---

## Cumulative block schematic

```
                          2S LiPo 7.4V (8.4V full → 6.0V empty)
                          ◀NEW URGENEX 2×1800mAh 35C (no PCB) — swapped in this phase
                                       │  Deans T-plug (keyed) → female pigtail
                                       ▼
                              ┌──────────────────┐
                              │ 10A MAIN FUSE ◀NEW│   (closest to battery +)
                              └────────┬─────────┘
                                       ▼
                       ┌────────────────────────────────┐
                       │ Reverse-polarity + MASTER switch│  Pololu #2815 (P-FET high-side) ◀── NEW
                       └───────────────┬────────────────┘
                                       │  protected 7.4V bus
            ┌──────────────────────────┼───────────────────────────┐
            ▼                          ▼                            ▼
     ┌─────────────┐          ┌─────────────────┐         (servo branch — Phase 5,
     │7.5A fuse◀NEW│          │ 5A fuse  ◀── NEW │          UBEC, not yet present)
     │ MOTOR branch│          │ Pi-buck branch   │
     └──────┬──────┘          └────────┬─────────┘
            ▼                          ▼
     ┌──────────────┐         ┌──────────────────┐
     │E-STOP(NC)◀NEW│         │ D24V50F5  ◀── NEW │
     │ in series    │         │ buck 5V / 5A      │
     └──────┬───────┘         └────────┬─────────┘
            │ 7.4V motor rail          │ 5V / 5A   (USB-C)
            ▼                          ▼
   ┌──────────────────────┐   ┌──────────────────────────────────────┐
   │  TB6612 #A VM   TB6612│   │          Raspberry Pi 5              │
   │  TB6612 #B VM   (7.4V)│   │  Flask web server · WiFi            │
   │  + 470–1000µF bulk    │   │  /dev/serial/by-id/<pico>           │
   │  + 0.1µF at each motor│   │  usb_max_current_enable=1           │
   └─────┬──────────┬──────┘   └───────────────┬──────────────────────┘
         │ AO/BO    │ AO/BO                     │ USB-A port ◀── NEW
   ┌─────▼───┐ ┌────▼────┐                      │ (USB-A → micro-USB cable)
   │ TB6612#A│ │ TB6612#B│                      │  5V→VBUS  +  serial bridge
   │ chA: FL │ │ chA: FR │                      ▼
   │ chB: RL │ │ chB: RR │             ┌──────────────────┐
   └──┬───┬──┘ └──┬───┬──┘             │     Pico 2W      │  20kHz PWM · PIO
      │   │       │   │                │  PID · watchdog  │  Mecanum inverse/fwd
    [FL][RL]    [FR][RR]   motors      │  parses v vx vy wz│
      │   │       │   │                └──┬────────────┬──┘
      │A,B│A,B    │A,B│A,B   encoders     │ GP2–GP14   │ GP16–GP27
      └───┴───────┴───┴─── A/B quadrature │ (motor/STBY)│ (4× enc A/B)
                  │                       │             │
                  └── enc VCC ◄── Pico 3V3(OUT) (3.3V!) ─┘
                      enc GND ──► common-ground star
   TB6612 VCC (logic, both boards) ◄── Pico 3V3(OUT)
   TB6612 STBY (both boards)       ◄── Pico GP14  (HIGH = enabled)

   ───────────────────────── COMMON GROUND (single-point STAR @ LiPo −) ─────────────────────────
   Separate legs to: TB6612#A, TB6612#B, buck, Pico, Pi. No daisy-chains.
   Pi↔Pico grounds are also tied through the USB bridge cable. Motor-return leg = heavy 18 AWG.
```

**What changed from the end of Phase 2:** three things. (1) The **battery is swapped** — the kit's SM-2P pack (fine for the Ph1–2 motors-only ~2.6 A) gives way to the higher-current **URGENEX pack on a Deans T-plug**, because the full system now draws ~5.4 A cruise / ~10 A worst, over the SM-2P's ~3 A. (2) The **full protection tree** is inserted between the battery's T-plug and the motor rail for the first time — **10 A main fuse → #2815 reverse-polarity master switch → 7.5 A motor-branch fuse → NC e-stop → both TB6612 VM** — replacing Phases 1–2's bare pigtail → WAGO path. (Use **time-delay/slow-blow** blades on the main + motor branch to ride the motor startup/reversal inrush.) (3) The **Pi 5 and its branch** are new: a **5 A Pi-branch fuse → D24V50F5 buck → Pi USB‑C**, plus the **Pi↔Pico USB bridge**. The motors, encoders, drivers, STBY, and the Pico's 3V3-fed logic are otherwise unchanged from Phases 1–2. The 5 V/3 A power bank is **removed** from the robot (bench-only now). *Why now and not Phase 1? Selectivity (branch fuses smaller than the main) only does anything once there's a second branch, and a fail-safe e-stop only earns its keep once the wheels can actually run away — both become true exactly here.*

---

## Wire-by-wire — NEW connections this phase

Tick each box after a multimeter beep test (continuity) and, for the power path, after metering voltage **before** connecting the load. Endpoints and pins are from [`_engineering/device_contracts.md`](../_engineering/device_contracts.md).

### Motor protection tree — NEW this phase (the Phase 1/2 bench path grows into the full tree)

> In Phases 1–2 the kit LiPo's SM-2P pigtail fed the WAGO splits *directly*. This phase **swaps in the URGENEX pack** (Deans T-plug) and inserts the protection chain between its T-plug pigtail and the motor rail. Build it dry, meter every junction, and plug in the T-plug **last**. Use **time-delay (slow-blow)** blades on the main + motor fuses to ride the motor startup/reversal inrush.

- [ ] `LiPo T-plug pigtail (+)` ──► `10 A MAIN fuse` (in) — main protection, closest to the battery + (replaces the bare Ph1–2 pigtail→WAGO link)
- [ ] `10 A MAIN fuse` (out) ──► `#2815 master switch` (in) — master ON/OFF **+** reverse-polarity protection
- [ ] `#2815 switch` (out = protected 7.4 V bus) ──► `7.5 A MOTOR-branch fuse` (in) — selectivity: smaller than the 10 A main
- [ ] `7.5 A MOTOR fuse` (out) ──► `E-STOP NC` (in) — fail-safe motor kill, in series, **motor branch only**
- [ ] `E-STOP NC` (out) ──► `WAGO 221-413 (+)` ──► both `TB6612 VM` — the WAGO still fans the (now protected) motor rail to both boards
- [ ] `LiPo T-plug pigtail (−)` ──► `WAGO 221-413 (−)` ──► both `TB6612 GND` + star node — unchanged from Phase 1

### Pi 5 power branch (LiPo → buck → Pi)

- [ ] `Protected 7.4 V bus` (after master switch) ──► `5 A blade fuse (Pi branch)` input — selectivity: smaller than the 10 A main, separate from the 7.5 A motor branch.
- [ ] `5 A fuse` output ──► `D24V50F5 VIN` — 7.4 V is inside the buck's 6–38 V input range.
- [ ] `LiPo − / common-ground star` ──► `D24V50F5 GND` — its own leg to the star, not daisy-chained.
- [ ] `D24V50F5 VOUT (5 V)` ──► `Pi 5 USB‑C power input` — meter **5.0–5.1 V here BEFORE plugging in the Pi**. Meets the Pi's 5 V/5 A want.
- [ ] `D24V50F5 GND` ──► `Pi 5 GND` (via the USB‑C cable's ground) — completes the Pi power return to the star.

### Pi 5 ↔ Pico 2W bridge (one cable: power + serial)

- [ ] `Pi 5 USB‑A` (any of the 4 ports) ──► `Pico 2W micro‑USB` — supplies 5 V to **Pico VBUS** *and* creates the serial device `/dev/serial/by-id/usb-MicroPython_...`. Must be a **data-capable** cable.

> That's the phase's wiring: the motor protection tree inserted ahead of the motor rail, plus the Pi branch and the Pi↔Pico bridge (6 conductors across 2 cables). The motors, drivers, encoders, and Pico logic stay exactly as Phases 1–2 left them.

### Config (software, but part of "wiring up" the power)

- [ ] `/boot/firmware/config.txt` contains `usb_max_current_enable=1` — without it the Pi caps USB to 600 mA even on the 5 A buck. Reboot after editing.

---

## Pin / rail reference for the new connections

| From | Rail / signal | To | V | Connector | Contract check |
|---|---|---|---|---|---|
| Protected 7.4 V bus | 7.4 V | 5 A fuse → D24V50F5 VIN | 7.4 V | blade holder / header | 7.4 V in 6–38 V ✓; 5 A branch fuse < 10 A main ✓ |
| D24V50F5 VOUT | 5 V / 5 A | Pi 5 USB‑C | 5 V | USB‑C | meets Pi 5 A want ✓; `usb_max_current_enable=1` ✓ |
| Pi 5 USB‑A | 5 V + USB serial | Pico micro‑USB (VBUS) | 5 V | USB‑A ↔ micro | powers Pico + `/dev/ttyACM*` ✓; data cable required ✓ |
| All new GND | 0 V | common-ground star @ LiPo − | 0 V | bus/tie | single-point star, no daisy-chain ✓ |

---

## ⚠️ Footguns that apply to THIS phase's wires

- **⚠️ Meter the buck output BEFORE connecting the Pi.** A miswired/over-trimmed D24V50F5 putting >5.1 V (or battery voltage) on the Pi's 5 V is instantly fatal. Expected reading: **5.0–5.1 V** with no load. This is the single most damaging mistake available this phase.
- **⚠️ Never back-feed 7.4 V into the Pico.** The Pico is powered *only* from the Pi's 5 V over the USB bridge (→ VBUS). The 7.4 V motor rail and the 5 V Pi rail join **only at the common-ground star** — never on a V+ line.
- **⚠️ Charge-only USB cable.** A power-but-no-data micro-USB cable powers the Pico but it never enumerates as serial → no `/dev/serial/by-id/` entry. Use a known data cable.
- **⚠️ Branch-fuse selectivity.** The Pi branch gets its own **5 A** fuse (smaller than the 10 A main, separate from the 7.5 A motor branch). Don't tap the Pi off the motor branch — a motor short would then also drop the Pi.
- **⚠️ Star ground, not a chain.** Run the buck's and Pi's grounds as their own legs back to the LiPo-negative star. Daisy-chaining the now-three converters' grounds lets 20 kHz motor-return current corrupt the logic reference your 3.3 V signals ride on.
- **⚠️ `usb_max_current_enable=1` is mandatory.** Even with a real 5 A buck, omitting it caps USB to 600 mA and the Pi will drop the Pico (and later the lidar).

### Carried-over footguns from earlier phases (still true — verify they're intact)

- **⚠️ TB6612 STBY must be HIGH** (GP14). STBY low = both drivers disabled; the bot won't move no matter how perfect your kinematics are.
- **⚠️ Encoder VCC = 3.3 V (Pico 3V3 OUT), never 7.4 V.** 3.3 V keeps the A/B outputs inside the Pico's GPIO limit.
- **⚠️ TB6612 VCC (logic, 3.3 V) ≠ VM (motor, 7.4 V).** Don't cross them.
- **⚠️ E-stop is in the motor branch only** — it kills the wheels and leaves the Pi/Pico (now both on their own rails) alive to log the fault.

---

## Strain relief (mechanical, but it's a wiring failure if you skip it)

The Pi↔Pico USB bridge is the robot's **spinal cord** — if it wiggles loose mid-drive you lose the entire drivetrain link in one instant. Strap it down with a cable-tie mount + VHB foam tape (no hot glue — it gets brittle near a warm Pi and releases under vibration). Same for the buck's 5 V lead to the Pi. See [`_engineering/safety.md`](../_engineering/safety.md).
