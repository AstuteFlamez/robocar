# Phase 3 — Wiring Diagram (cumulative, end of phase)

> This is the **full system state at the end of Phase 3.** Phases 1–2 built the motor/encoder/power tree under the Pico. This phase adds exactly **two new connections** — the Pi's buck-fed power and the single Pi↔Pico USB bridge — but the diagram below shows *everything* so you can wire and verify the whole bot against one picture. Every connection traces to a contract in [`_engineering/device_contracts.md`](../_engineering/device_contracts.md); power sizing is in [`_engineering/power_budget.md`](../_engineering/power_budget.md).
>
> **NEW this phase is marked `◀── NEW`.**

---

## Cumulative block schematic

```
                          2S LiPo 7.4V (8.4V full → 6.0V empty) · 10C ≈ 22A
                                       │  XT60
                                       ▼
                              ┌──────────────────┐
                              │   10A MAIN FUSE   │   (closest to battery +)
                              └────────┬─────────┘
                                       ▼
                       ┌────────────────────────────────┐
                       │ Reverse-polarity + MASTER switch│  Pololu #2815 (P-FET high-side)
                       └───────────────┬────────────────┘
                                       │  protected 7.4V bus
            ┌──────────────────────────┼───────────────────────────┐
            ▼                          ▼                            ▼
     ┌─────────────┐          ┌─────────────────┐         (servo branch — Phase 5,
     │ 7.5A fuse   │          │ 5A fuse  ◀── NEW │          UBEC, not yet present)
     │ MOTOR branch│          │ Pi-buck branch   │
     └──────┬──────┘          └────────┬─────────┘
            ▼                          ▼
     ┌──────────────┐         ┌──────────────────┐
     │ E-STOP (NC)  │         │ D24V50F5  ◀── NEW │
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

**What changed from the end of Phase 2:** the Pi 5 and its power path (Pi-branch 5 A fuse → D24V50F5 buck → Pi USB‑C) are new, and the Pi↔Pico USB bridge is new. The motor rail, encoders, drivers, STBY, and the Pico's 3V3-fed logic are all unchanged from Phases 1–2. The 5 V/3 A power bank is **removed** from the robot (bench-only now).

---

## Wire-by-wire — NEW connections this phase

Tick each box after a multimeter beep test (continuity) and, for the power path, after metering voltage **before** connecting the load. Endpoints and pins are from [`_engineering/device_contracts.md`](../_engineering/device_contracts.md).

### Pi 5 power branch (LiPo → buck → Pi)

- [ ] `Protected 7.4 V bus` (after master switch) ──► `5 A blade fuse (Pi branch)` input — selectivity: smaller than the 10 A main, separate from the 7.5 A motor branch.
- [ ] `5 A fuse` output ──► `D24V50F5 VIN` — 7.4 V is inside the buck's 6–38 V input range.
- [ ] `LiPo − / common-ground star` ──► `D24V50F5 GND` — its own leg to the star, not daisy-chained.
- [ ] `D24V50F5 VOUT (5 V)` ──► `Pi 5 USB‑C power input` — meter **5.0–5.1 V here BEFORE plugging in the Pi**. Meets the Pi's 5 V/5 A want.
- [ ] `D24V50F5 GND` ──► `Pi 5 GND` (via the USB‑C cable's ground) — completes the Pi power return to the star.

### Pi 5 ↔ Pico 2W bridge (one cable: power + serial)

- [ ] `Pi 5 USB‑A` (any of the 4 ports) ──► `Pico 2W micro‑USB` — supplies 5 V to **Pico VBUS** *and* creates the serial device `/dev/serial/by-id/usb-MicroPython_...`. Must be a **data-capable** cable.

> That's the whole phase's wiring: 6 conductors across 2 cables. Everything else stays exactly as Phases 1–2 left it.

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
