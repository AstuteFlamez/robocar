# Safety

> Read this **once, fully, before you connect a battery** — then keep it open. None of this is bureaucratic box-ticking: every item here exists because skipping it has burned, shocked, or destroyed someone's hardware. A 2S LiPo can dump **22 A** into a short. That's enough to melt wire insulation, boil a cell, and start a fire in seconds. Respect it and it's perfectly safe; get casual and it bites.
>
> This complements [power_budget.md](./power_budget.md) (the *why* and the numbers) — this is the *do/don't*.

---

## The three LiPo rules that are never optional
1. **Charge only with a balance charger, never unattended, on a non-flammable surface.** A LiPo fire is a chemical fire — water won't stop it.
2. **Never let any cell drop below ~3.0 V (6.0 V pack).** Over-discharge damages cells and makes the *next* charge dangerous. This is why we add an INA219 software cutoff **and** a balance-port buzzer — two independent layers.
3. **Store at storage charge (~3.8 V/cell) in a LiPo-safe bag** if you won't use it for a week. Inspect for puffing; a puffed pack is retired, not charged.

---

## Layers of protection in this build (defense in depth)
No single thing keeps you safe — these stack, each catching what the others miss:

| Layer | Part | Catches |
|---|---|---|
| **Reverse-polarity** | P-FET high-side switch (#2815) | Battery plugged in backwards (the most common fatal mistake). |
| **Master disconnect** | same switch / rocker | One physical OFF for the whole robot. |
| **Main fuse (10 A)** | blade fuse at battery + | A dead short anywhere downstream → opens before wires overheat. |
| **Branch fuses (7.5 A / 5 A)** | blade fuses per branch | A fault on one branch can't take down the others (selectivity). |
| **E-stop (NC, motor branch)** | latching mushroom | Instant wheel kill; brains stay alive. Broken wire also trips it. |
| **Comms watchdog (sw)** | Pico firmware | Coasts motors if Pi comms drop >0.5 s (runaway prevention). |
| **Hardware watchdog** | Pico (RP2350) | Reboots + re-enumerates a *hung* Pico — the comms watchdog can't, since it assumes the main loop is still running. Two different failures, two layers. |
| **Stall timeout** | Pico firmware | Cuts PWM if a wheel is commanded but not moving (protects driver). |
| **Low-voltage cutoff** | INA219 + Pi | Graceful park/halt before the pack is over-discharged. |
| **LVA buzzer** | balance-port alarm | Dumb backstop if the Pi or its code fails. |
| **Driver protection** | TB6612FNG | Built-in thermal + overcurrent shutdown. |

---

## Power-up / power-down order (every single time)
**Up:** e-stop out → confirm rails with multimeter (buck 5 V, UBEC 5 V) → connect Pi/logic → *then* arm the motor branch (switch on, e-stop released). Logic settles before motor power, so the drivers never glitch their outputs on a cold start.

**Down:** e-stop in (or switch off) to kill motors first → then shut down the Pi cleanly (`sudo shutdown now`) → then disconnect the battery. Never yank the battery on a running Pi — you risk SD-card corruption.

---

## The continuity ritual (before first power-on, and after any rewire)
Battery **disconnected**, switch **off**. Multimeter on the beep (continuity) setting:
- [ ] Beep between every ground and the star point at LiPo (−). **No** ground daisy-chains.
- [ ] **No** beep between any V+ rail and any ground (that would be a short — find it before powering).
- [ ] Encoder VCC reads to **3.3 V** (Pico 3V3), never to the 7.4 V motor rail.
- [ ] TB6612 **STBY** on **GP14**, driven HIGH by firmware at init (not hard-tied to 3.3 V — so firmware can also kill all motors); PCA9685 **VCC ≠ V+** (not cross-wired).
- [ ] Motor power path is **soldered / screw-terminal 18 AWG** — never through a breadboard.
- [ ] Fuses seated and correct (10 / 7.5 / 5 A). Reverse-protection oriented right.

A 10-minute beep test before power-on prevents the 10-hour debugging session (or the smoke) after.

---

## First powered bring-up — on a current-limited supply
Before the LiPo ever touches a freshly-wired board, do the **first power-up on a current-limited bench supply** set to ~0.1–0.5 A (borrow a lab supply if you don't own one — not a required purchase). Bring the rails up slowly and watch the current draw: a wiring slip then **trips the limit** instead of dumping the pack's ~22 A into a short before the 10 A fuse can even blow (the fuse protects *wiring*, not a sub-10 A part-level short). Confirm no rail-to-ground short and correct rail voltages, **then** switch to the LiPo. This is the cheapest possible insurance for "first light" on the hand-wired carrier.

---

## Logic-level safety (the 3.3 V world)
- The **Pi 5 and Pico are 3.3 V**. Don't drive their GPIO inputs above 3.3 V. The lidar TX (3.3 V) and the 310 encoder outputs (3.3 V, because we power the encoder at 3.3 V) are safe by design — *that's why the encoder VCC must be 3.3 V, not 5 V.*
- Anything 5 V that must talk to a 3.3 V pin gets level-shifted. Don't rely on the Pico's conditional 5 V tolerance.

---

## Mechanical & operating safety
- **Bench-test wheels in the air**, on a stand, until you trust the code. A Mecanum bot can lunge sideways unexpectedly.
- Keep fingers, wires, and the lidar's 360° beam plane **clear of the spinning wheels and the lidar rotor.**
- Strain-relieve the Pi↔Pico USB "spinal cord" — if it wiggles loose mid-drive you lose the whole drivetrain link.
- The Pi 5 and motor drivers get warm; don't bury them in foam or hot glue. Airflow matters.
- 905 nm lidar laser is **Class 1 (eye-safe)** — fine, but don't stare into optics out of habit.

---

## Board-level risk register

One consolidated view of the controller-board failure modes scattered across the per-phase pitfalls — each with the guardrail that catches it and the phase that installs it. (Deferred items carry a *revisit-if* trigger.)

| ID | Failure mode | Why it bites | Guardrail | Installed |
|---|---|---|---|---|
| R1 | Battery reversed | Instantly kills drivers/buck/Pi | Keyed XT60 + P-FET (#2815/#5381) reverse protection | Phase 1 |
| R2 | Encoder fed >3.3 V | A/B exceed GPIO limit; GP26/27 (RR) are ADC pins, *never* 5 V-tolerant → damage | Enc-VCC = 3.3 V (in-spec); meter before power | Phase 2 |
| R3 | Encoder A/B float (open-collector, no pull-up) | PIO miscounts → silent odometry corruption | 4.7 k pull-up A/B→3.3 V + scope clean 0–3.3 V edges | Phase 2 |
| R4 | STBY not HIGH | TB6612 outputs disabled → "motor won't move" | GP14 driven HIGH by firmware at init | Phase 1 |
| R5 | PWM-slice aliasing | Two motor PWMs on one slice → coupled duty (one wheel tugs another) | Verify GP4/7/10/13 on distinct slice+channel | Phase 1 |
| R6 | Wheel-order / sign swap | `FL,RL,FR,RR` mismatch → strafe becomes spin; odometry wrong | One canonical order everywhere; pure-vx/vy/ωz lab | Phase 3 |
| R7 | Motor stall / jam | Sustained stall heats driver + pack | Software stall-timeout (+ optional shunt) | Phase 2 |
| R8 | Comms loss (Pi stops) | Last command latches → runaway | Comms watchdog: coast after 0.5 s | Phase 2/3 |
| R9 | Pico firmware hang | Control loop frozen at last duty | Hardware watchdog: reboot + re-enumerate | Phase 2 |
| R10 | Motor startup/reversal inrush | Brief ~5.6 A+ surge nuisance-trips a fast fuse | Time-delay (slow-blow) motor + main fuses | Phase 1 |
| R11 | Servo brownout onto a shared rail | Servo inrush sags the rail → Pi/Pico reboot | Dedicated 5 V UBEC; common ground only at star | Phase 5 |
| R12 | I²C address collision | PCA9685 and INA219 both answer | Distinct addresses (0x40 vs 0x41) | Phase 3/5 |

**Deferred — revisit-if:**
- **INA219 ~3.2 A ceiling** clips on stall (bus voltage stays accurate, so the low-voltage cutoff still works) → *revisit with INA226 + external shunt if seeing the full ~12 A transient becomes a goal.*
- **Wheels confirmed Mecanum** (45° rollers) — the whole kinematics matrix assumes it → *human must confirm before trusting odometry/strafe* (Phase 3).
- **`COUNTS_PER_REV`** calculated ~1040 → *measure empirically in Phase 2; never ship the hardcoded value.*

---

## If something goes wrong
- **Smoke / burning smell:** hit the e-stop and disconnect the battery *immediately*. Don't investigate a live short.
- **Undervoltage warning (`vcgencmd get_throttled` ≠ `0x0`):** your supply is sagging — check the buck, cable, and load before continuing.
- **A wheel jams / bot runs away:** e-stop. The software watchdog and stall timeout are backups, not your first line — your hand is.
- **Puffed/hot battery:** stop, disconnect, move it to a non-flammable area, retire it.

When in doubt, the safe state is **battery disconnected.** It costs you 30 seconds and zero dollars.
