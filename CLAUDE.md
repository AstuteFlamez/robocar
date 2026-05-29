# CLAUDE.md — working guide for agents improving this repo

This repo is a **documentation/build-plan project**, not a software codebase. It is an 8-phase course for an **ECE sophomore** who is rebuilding a commercial **Hiwonder MentorPi M1** robot (Mecanum chassis, Raspberry Pi 5) into a two-brain robot they fully understand — deliberately bypassing the kit's vendor controller (RRC Lite) and ROS2 image. There is no code to build/run; the deliverable is **correct, engaging, electrically-verified Markdown.**

If you're here to improve the docs, read this whole file first, then `_engineering/` — that folder is the source of truth.

---

## Golden rules (read before editing anything)

1. **`_engineering/` is authoritative. Change there first, then ripple outward.** Every voltage, current, logic level, pin number, part name, price, and purchase link in a phase folder must trace to `_engineering/`. If you change a fact, edit `_engineering/device_contracts.md` (and `products_to_buy.md` / `power_budget.md` as needed) **first**, then update every phase doc that references it. Never let a phase doc invent or contradict an `_engineering/` number.
2. **One canonical pin map.** It lives in `_engineering/device_contracts.md` → "Canonical pin assignments." Do not introduce a different pin anywhere. If you must change a pin, change it there and propagate.
3. **One canonical wheel order: `FL, RL, FR, RR`.** Used everywhere — the GPIO pin table, the four PID arrays, the `t` telemetry line, and the `inverse()`/`forward()` kinematics tuples. A mismatch here silently swaps two wheels (this was a real bug that was caught and fixed in Phase 3). If you touch kinematics, keep all four in this order.
4. **The robot is Mecanum (holonomic), never differential.** Use body twists `(vx, vy, ωz)`. Any `v_left/v_right` or single-steering-curvature reasoning is wrong (it may appear only inside a *pitfall warning* telling the reader not to do it).
5. **Verify electrical claims against primary datasheets.** This project's whole point is electrical correctness. Don't assert a current/voltage/connector fact you haven't confirmed. When in doubt, say "measure it" — the bench is the authority.
6. **Don't edit `_archive/original_plan/`.** It's the preserved first draft, kept for before/after comparison. It is intentionally superseded.

---

## Repository structure

```
README.md                  top-level guide
CLAUDE.md                  this file
_engineering/              SOURCE OF TRUTH — edit here first
  device_contracts.md        per-part V/I/logic/connector + canonical pin map + connection matrix
  products_to_buy.md         master BOM (have-vs-buy, links, prices, reason per part)
  power_budget.md            power tree + current budget + fuse/buck/UBEC sizing
  engineering_decisions.md   ADR-style rationale for every design choice (+ alternatives + learning payoff)
  safety.md                  LiPo rules, protection layers, power-up order, continuity ritual
  README.md                  index of the above
phase_1_powertrain/ … phase_8_autonomy/
  README.md                  goal, pin tables, software tasks, verification checklist, pitfalls,
                             a "Measurement Lab", and a "Phase Wrap-Up" (learned / résumé bullet / interview point)
  wiring_diagram.md          cumulative ASCII schematic + checkable wire-by-wire list + pin tables
  bom.md                     just this phase's parts, with links (a slice of products_to_buy.md)
_archive/original_plan/    the first-draft plan (DO NOT EDIT — superseded)
```

Phase order builds cumulatively: 1 Powertrain → 2 Odometry/PID → 3 Teleop+Mecanum → 4 IMU/pose → 5 Camera/pan-tilt → 6 Lidar → 7 SLAM → 8 Autonomy. Each phase's wiring diagram shows the full system state *at the end of that phase*.

---

## Verified key facts (don't re-derive these wrong)

These were confirmed against manufacturer datasheets + adversarial verification. Treat as ground truth (sources are cited in `_engineering/`):

- **Kit = Hiwonder MentorPi M1.** Mecanum chassis; ships with RRC Lite controller (STM32, bypassed), four "310" (JGA27-310/MD310Z20) encoder motors, STL-19P lidar, Nuwa-HP60C USB depth cam (or GC0308 mono), LFD-01 servos, 7.4 V 2200 mAh 10C LiPo.
- **310 motor:** 7.4 V, running ≤0.65 A, **stall ≤1.4 A**, 1:20 gearbox, ~450 RPM. Encoder = 13-line Hall → **COUNTS_PER_REV ≈ 1040** (13×4×20) — but it is **measured empirically in Phase 2**, never hardcoded. Connector = JST-PH 2.0 6-pin (motor + encoder combined).
- **Motor driver = TB6612FNG** (×2, one motor/channel), *not* DRV8833 — 3.2 A peak/channel gives headroom over the 1.4 A stall. STBY must be pulled HIGH.
- **Pi 5 needs 5 V/5 A.** A 5 V/3 A bank is UNSAFE (caps USB to 600 mA + browns out). Powered via a **Pololu D24V50F5 buck** from the LiPo + `usb_max_current_enable=1`.
- **Servos** get a **dedicated 5 V UBEC** (never the Pi rail). **BNO055 IMU runs over UART** (PS1 HIGH), not the Pi's hardware I²C (clock-stretch corruption).
- **I²C-1:** PCA9685 = **0x40**, INA219 = **0x41** (must differ). **Lidar** = STL-19P via a **3.3 V CP2102** USB-UART (PWM pin tied to GND for 10 Hz). **Camera Module 3** needs the **Pi 5 22-pin→15-pin CSI cable** (a generic 15-to-15 won't fit).
- **Power tree:** single LiPo → 10 A main fuse → reverse-polarity/master switch → branch fuses (7.5 A motor, 5 A Pi) → NC e-stop in the motor branch only → loads. Star ground at LiPo−. Full diagram in `power_budget.md`.

---

## Style & quality bar

- **Audience: ECE sophomore.** Assume Ohm's law, basic circuits, intro Python, a little controls. Explain everything else from the ground up (H-bridge, PWM, quadrature, PID, fusion, kinematics, SLAM).
- **Engaging prose + real technical depth.** Readable paragraphs that explain *why*, backed by actual numbers — not a dry pin-dump, not hand-waving.
- **Every phase keeps:** a goal, pin tables, a software task list with code skeletons, an ordered verification checklist, common pitfalls, a **Measurement Lab** (a named quantity + instrument + expected number), and a **Phase Wrap-Up** (What you learned / Résumé bullet / Interview talking point).
- **Markers:** `⚠️` = footgun, `★` = a load-bearing fact the design hangs on. Keep them consistent.
- **Conventions:** `FL/RL/FR/RR`; Pico pins `GP<n>` (physical `pin <m>`); Pi pins `GPIO<n>` BCM (physical `header pin <m>`); voltages nominal ("measure before connecting").

---

## How to make improvements (recommended workflow)

For substantial changes (the user values this approach):
1. **Research/verify first.** Confirm any new electrical claim against a primary datasheet before writing it.
2. **Update `_engineering/` first**, then propagate to every affected phase doc. Run a consistency check: `grep` for the changed pin/part/voltage across all phases.
3. **Adversarially review before delivering.** Re-read changed docs against `device_contracts.md` (pin map + connection matrix) and `power_budget.md`. Check: no V/I/logic/connector mismatch, no pin contradicting the canonical map, no leftover differential-drive where Mecanum is required, no broken/placeholder links, no I²C address collision, Measurement Lab + Phase Wrap-Up still present.
4. Spawn fresh-context subagents for breadth (per-subsystem brainstorms, per-phase reviews) when the change is large; the docs were originally produced this way.

Quick consistency greps that catch the common regressions:
```
grep -rn "v_left\|v_right\|left_mps"   phase_*/   # differential leak (OK only inside a "don't do this" warning)
grep -rn "DRV8833"                     phase_*/   # should read as "replaced by TB6612", never as the chosen part
grep -rn "0x40"                        phase_*/   # INA219 must be 0x41, PCA9685 is 0x40
grep -rn "power bank"                  phase_*/   # only as "retired/bench-only", never as the Pi's supply
grep -rn "560"                         phase_*/   # only as "don't trust 560 — measure ~1040"
```

---

## Open items only the human can close (flagged in the docs, don't silently resolve)
1. Confirm the wheels are **Mecanum** (angled rollers). If plain wheels → switch to differential kinematics.
2. Confirm the kit includes **LFD-01 servos + bracket** and the **Nuwa depth camera** (decides reuse-vs-buy / use-vs-ignore).
3. **Measure `COUNTS_PER_REV`** in Phase 2 (calculated ~1040; verify before trusting odometry).
