# Power Budget & Power Tree

> **Why this document exists.** Power is where hobby robots quietly die. Two independent adversarial checks on the *original* plan came back **UNSAFE**:
> 1. A **5 V/3 A USB power bank cannot run a Pi 5.** Without a real 5 V/5 A supply the Pi caps *all* USB ports to **600 mA combined** — and the lidar (290 mA) + depth cam (400 mA) alone blow past that — *and* the whole-system worst case (~4.6 A) exceeds the 3 A the bank can give, so the rail sags below the 4.63 V undervoltage line and the Pi throttles, drops USB devices, and reboots.
> 2. **Servos on the Pi's 5 V header brown out the Pi.** Two LFD-01 servos pull ~1.5 A at stall with 3–4 A inrush; stacked on the Pi's own load through an unfused header pin, that sag reboots the Pi mid-SLAM.
>
> The fix is a proper **single-battery power tree**: one 2S LiPo, conditioned into clean rails by dedicated converters, each branch fused, with reverse-polarity protection and an e-stop. This is also the most *educational* part of the electrical design — it's where an ECE student actually learns DC-DC conversion, current budgeting, and protection coordination.

---

## The power tree

```
                          2S LiPo 7.4V (8.4V full → 6.0V empty)
                          2200 mAh · 10C ≈ 22 A capable
                          built-in protection board (short/OC/OV/UV cut-off)
                                   │  SM-2P (keyed) → female pigtail
                                   ▼
                          ┌─────────────────┐
                          │  10 A MAIN FUSE │   (closest to battery +)
                          └────────┬────────┘
                                   ▼
                    ┌──────────────────────────────┐
                    │  Reverse-polarity + MASTER     │  Pololu #2815 (or rocker + #5381)
                    │  switch (P-FET high-side)      │
                    └───────────────┬────────────────┘
                                    │  protected 7.4 V bus
          ┌─────────────────────────┼─────────────────────────────┐
          ▼                         ▼                               ▼
   ┌─────────────┐         ┌─────────────────┐            ┌──────────────────┐
   │ 7.5 A fuse  │         │   5 A fuse      │            │  (servo branch)  │
   │ MOTOR branch│         │   Pi branch     │            │   UBEC input     │
   └──────┬──────┘         └────────┬────────┘            └────────┬─────────┘
          ▼                         ▼                              ▼
   ┌──────────────┐        ┌────────────────┐            ┌──────────────────┐
   │  E-STOP (NC) │        │  D24V50F5      │            │  Hobbywing UBEC  │
   │  in series   │        │  buck 5V/5A    │            │  5V / 3A         │
   └──────┬───────┘        └───────┬────────┘            └────────┬─────────┘
          ▼                        ▼  USB-C                       ▼  V+
   ┌──────────────┐        ┌────────────────┐            ┌──────────────────┐
   │ 2× TB6612 VM │        │  Raspberry Pi 5 │           │  PCA9685 V+      │
   │  (7.4 V)     │        │  (5 V / 5 A)    │           │  → 2× servos     │
   │  → 4 motors  │        │  +usb_max_      │           │  (5 V)           │
   │              │        │   current=1     │           └──────────────────┘
   └──────────────┘        └───────┬────────┘
   ▲ bulk 470–1000µF               │ USB-A ports
   │ + 0.1µF at each motor         ├── Pico 2W (5 V, USB)  ── 3V3 OUT → driver logic + encoders
                                   ├── CP2102 → STL-19P lidar (5 V from adapter USB)
                                   └── (optional) Nuwa depth cam (5 V USB)

   ───────────────────────── COMMON GROUND ─────────────────────────
   Single-point STAR at LiPo (−): separate legs to each driver, buck,
   UBEC, Pico/Pi. No daisy-chaining. Heavy 18 AWG for motor return.
```

**Read the tree as a hierarchy of promises.** The LiPo promises ≤22 A. The main fuse promises to open before the *wiring* overheats. Reverse protection promises a backwards battery does nothing. Each branch fuse promises a short on *its* branch won't take down the others. Each converter promises a clean, regulated rail regardless of what the battery sags to. The e-stop promises the wheels stop the instant you hit it, while the brains stay alive to tell you why.

> **This is the *complete* tree, built up over several phases — not what you wire on day one.** **Phase 1** builds only the leftmost branch, and a deliberately minimal version of it: LiPo **SM-2P** output → female pigtail → a **WAGO 221-413** lever-nut splitting + and − → both **TB6612 VM/GND** on soldered screw terminals. There is **no main fuse, reverse-polarity switch, branch fuse, or e-stop yet** — with a single branch, a self-protecting battery (built-in protection board), and the wheels in the air on a stand, those parts are belt-and-suspenders. They earn their place as the system grows: the **main fuse, #2815 reverse-polarity master switch, and branch fuses** come online in **Phase 3**, when the D24V50F5 buck makes the Pi a second branch and selectivity finally means something; the **NC e-stop** is added in the first **mobile** phase, when a runaway robot becomes possible. The reasoning for each lives in `phase_1_powertrain/README.md` → "Concepts."

> ⚠️ **Connector ampacity — the SM-2P is a Phase-1-only current path.** The kit LiPo's factory **SM-2P** output is rated only **~3 A per contact** (JST SM series). That covers the Phase-1 *motor-branch* cruise (~2.6 A, wheels in the air) but is already exceeded by a four-motor stall (~5.6 A) and is **far** below the **~10 A whole-robot worst case** (motors + Pi-buck + servo-UBEC) once Phase 3 adds the second and third branches. The power wiring is also 18 AWG — outside the SM contact's 22 AWG max. **Before Phase 3, replace the battery output with an XT30 (~30 A) / XT60 (~60 A), or terminate the SM-2P immediately into a fused distribution block and pull the Pi/servo branches from that block** — so the ~3 A SM contacts never carry whole-system current. ("Don't cut the factory plug" is a Phase-1 convenience, not a permanent constraint.)

---

## Current budget

Worst case = "driving hard while a wheel jams and the Pi is doing SLAM with the camera streaming." That's the number every rail is sized to. Averages are for runtime estimates only.

### On the 5 V Pi rail (out of the D24V50F5)

| Load | Typical | Peak / worst |
|---|---|---|
| Pi 5 board (CPU+GPU+WiFi, SLAM) | 0.8 A | **3.4 A** |
| Pico 2W (via USB) | 0.05 A | 0.1 A |
| STL-19P lidar (via CP2102 USB) | 0.29 A | ~0.4 A (spin-up) |
| Depth cam (optional, USB) | 0.3 A | 0.4 A |
| Camera Module 3 (CSI) | 0.25 A | 0.4 A |
| **5 V rail total** | **~1.7 A** | **~4.7 A** |

→ The **D24V50F5 (5 A)** covers the ~4.7 A peak with a little margin. `usb_max_current_enable=1` tells the Pi to allow the higher USB draw since the rail can actually supply it. *If you ever add more USB devices, move the lidar + depth cam onto a **powered USB hub** fed from this same 5 V rail so the Pi's own ports stay light.*

### On the 7.4 V motor rail (LiPo → TB6612 VM)

| State | Current (4 motors) |
|---|---|
| Cruising (≤0.65 A each) | ~2.6 A |
| All four stalled (≤1.4 A each) | **~5.6 A** (brief — fix it or the stall-timeout cuts it) |

→ **7.5 A motor-branch fuse** rides above the 5.6 A all-stall transient without nuisance-tripping. Each motor's 1.4 A stall is well under the **TB6612's 3.2 A/channel peak**. Use a **time-delay (slow-blow)** blade here: when all four motors start or reverse together they briefly draw their locked-rotor (≈ stall) current at once — ~5.6 A and up for a few ms, plus the bulk-cap charge spike — and that transient must ride under the fuse's i²t, not trip it. A fast-acting blade on this branch is a likely cause of mid-drive nuisance shutoffs. *(The vendor RRC Lite ships with no battery fuses at all, so this is robocar's own protection layer — size it for the inrush, not just the 5.6 A average.)*

### On the 5 V servo rail (UBEC → PCA9685 V+)

| State | Current (2 servos) |
|---|---|
| Holding / slewing | ~0.1–0.5 A |
| Both stalled | **~1.5 A** (+ 3–4 A inrush spike, absorbed by the bulk cap) |

→ **UBEC-3A (3 A)** + a 470–1000 µF bulk cap on the rail handles it.

### Draw back at the battery

Converters aren't free — they trade voltage for current at ~90% efficiency. The Pi's 5 V × ~4.7 A ≈ 23.5 W pulls **~3.5 A from the 7.4 V pack** (23.5 W ÷ 0.9 ÷ 7.4 V). Add motors (~2.6 A cruise) and servos (~0.3 A average):

| Branch | Pack-side current (cruise) | Pack-side (worst) |
|---|---|---|
| Pi (via buck) | ~2.5 A | ~3.5 A |
| Motors | ~2.6 A | ~5.6 A |
| Servos (via UBEC) | ~0.3 A | ~1.2 A |
| **Total from pack** | **~5.4 A** | **~10 A** |

→ The **10 A main fuse** sits above the ~10 A worst case and below the wiring/pack limits. The 22 A-capable pack is never the bottleneck — *the fuse protects the wires, not the battery.* This is the **selectivity** idea: branch fuses (7.5 A, 5 A) are smaller than the main (10 A), so a fault on one branch trips *that* branch's fuse first, not the main — the rest of the robot stays up.

### Runtime estimate
2200 mAh ÷ ~5.4 A cruise ≈ **0.4 h ≈ 24 min** of active driving + compute. Mapping (slow driving) stretches it; aggressive driving shortens it. Plan sessions around ~20 min and keep a charged spare.

---

## Sizing logic (so you can re-derive it, not just copy it)

- **Buck for the Pi:** must exceed the Pi's 5 V peak (~4.7 A incl. USB). 5 A is the minimum sane choice; the D24V50F5 is exactly 5 A with reverse protection and soft-start. A 6 A unit would add margin if budget allows.
- **UBEC for servos:** must exceed 2× servo stall (~1.5 A) + inrush. 3 A with a bulk cap is comfortable.
- **Main fuse:** just above total worst-case pack draw (~10 A), below the 18 AWG wire ampacity and pack rating. Round to a standard blade value (10 A).
- **Branch fuses:** just above each branch's worst case (motor 5.6 A → 7.5 A; Pi 3.5 A → 5 A), and each **smaller than the main** for selectivity.
- **Fuse character (fast vs slow-blow):** use **time-delay (slow-blow)** blades on the **motor branch and the main** so the brief simultaneous motor startup/reversal inrush (locked-rotor current + bulk-cap charge, a few ms) rides under the i²t instead of nuisance-tripping; a **fast-acting** blade is fine (and slightly safer) on the **Pi-buck branch**, which sees no inductive inrush. Standard ATC/ATO automotive blades come in both characters — specify slow-blow where it matters.
- **Wire gauge:** motor/main power = **18 AWG** (good for ~10 A in short runs); signals = **22 AWG**. Breadboard spring contacts are ~1 A — **motor power never touches a breadboard.**
- **Connector ampacity:** size connectors to the *worst-case branch current*, like wires and fuses — not the average. The LiPo's factory **SM-2P (~3 A/contact)** is adequate for the Phase-1 motor branch only; the whole-robot ~10 A path needs an **XT30/XT60 or a fused distribution block** (see the ⚠️ note under the power tree).
- **Bulk capacitance:** ~470–1000 µF where big loads switch (motor VM, servo V+) to absorb the millisecond di/dt that fuses and average budgets ignore; 0.1 µF ceramics at each motor body for brush noise.

---

## Common-ground star

Every ground returns to **one node at the LiPo negative terminal** — a separate leg from each converter, each driver, and the Pi/Pico. *Do not* daisy-chain (LiPo− → driver1 → driver2 → Pico): once you have three converters, daisy-chained grounds form loops, and motor return current (amps, switching at 20 kHz) corrupts the logic ground reference your 3.3 V signals are measured against. Keep the motor-return leg short and heavy; give logic its own thin leg to the star. The original plan's "common ground is pitfall #1" instinct was right — this just makes it a star instead of a chain.

---

## Pre-power-on power checklist
- [ ] Battery **disconnected**, switch **off**, e-stop **out (pressed)**. Multimeter on continuity.
- [ ] No continuity between any V+ rail and ground anywhere.
- [ ] Reverse-protection oriented correctly; fuse values seated (10/7.5/5 A).
- [ ] Buck output reads **5.0–5.1 V** *before* connecting the Pi.
- [ ] UBEC output reads **5.0 V** *before* connecting servos.
- [ ] All grounds beep-continuous to the star point; logic grounds tied.
- [ ] `usb_max_current_enable=1` in `/boot/firmware/config.txt`.
- [ ] After boot: `vcgencmd get_throttled` returns `0x0` (no undervoltage). Re-check under load.
