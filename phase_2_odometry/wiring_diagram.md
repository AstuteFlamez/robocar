# Phase 2 — Wiring Diagram (cumulative)

> This is the **full system state at the end of Phase 2.** It redraws the Phase 1 picture with one change: the four encoders are now *energized and read* — their Hall A/B channels feed the Pico's PIO pins, and Enc-VCC is powered from the Pico's 3V3(OUT) rail. The motor/encoder interconnect also moves **off the breadboard onto a soldered Pi Pico screw-terminal carrier board**, because a Mecanum bot's vibration will shake a Dupont jumper loose mid-drive and a breadboard spring (~1 A) was never allowed to carry motor power anyway. Every connection's *contract* (voltage, current, logic level, connector) lives in [`../_engineering/device_contracts.md`](../_engineering/device_contracts.md); the power-tree sizing is in [`../_engineering/power_budget.md`](../_engineering/power_budget.md). Tick each wire's box after you've placed it **and** beep-tested it.

---

## What changed since Phase 1

| | Phase 1 | Phase 2 |
|---|---|---|
| Encoder conductors (Hall-A, Hall-B, Enc-VCC, Enc-GND) | mated in the 6-pin plug but **parked** (taped off) | **routed and read** — A/B → Pico PIO pins, VCC → 3V3(OUT), GND → star |
| Encoder VCC source | n/a (unpowered) | **Pico 3V3(OUT), pin 36** — ★ **never** 5 V or 7.4 V |
| Motor/encoder interconnect | breadboard for signals; soldered/screw for motor power | **all on a soldered Pico screw-terminal carrier board** |
| PIO state machines used | 0 | **4 of 12** (one per encoder) |
| Everything on the Pi 5 / motor power tree | — | **unchanged** (Pi still just supplies USB to the Pico) |

The two rails are still completely separate and joined only at the common star ground. The encoders run on a *third* low-current rail — the Pico's 3V3(OUT) — but its return still lands on the same star.

---

## Block schematic — end of Phase 2

```
   ┌────────────────────┐
   │   Laptop (USB)     │  power + serial REPL (temporary; Pi takes over in Phase 3)
   └─────────┬──────────┘
             │ USB  (5 V on VBUS + serial)
             ▼
   ┌──────────────────────────────────────────────────────────────────────────────┐
   │                          Raspberry Pi Pico 2W                                  │
   │  VBUS◄─USB 5V                3V3(OUT) pin36 ──┐ logic + encoder supply (<300mA) │
   │                                               │                                │
   │  ── motor control (unchanged from Phase 1) ── │                                │
   │  GP2 GP3 GP4  GP5 GP6 GP7   GP8 GP9 GP10  GP11 GP12 GP13   GP14  GND            │
   │  IN1 IN2 PWM  IN1 IN2 PWM   IN1 IN2 PWM   IN1  IN2  PWM    STBY                 │
   │  └─FL chA─┘   └─RL chB─┘    └─FR chA─┘    └─RR chB─┘         │                   │
   │                                                                                │
   │  ── encoder PIO inputs (NEW this phase; A/B on ADJACENT GPIOs) ──               │
   │  GP16 GP17   GP18 GP19   GP20 GP21   GP26 GP27   ◄── Hall A/B ×4                 │
   │   A    B      A    B      A    B      A    B                                    │
   │  └─FL─┘      └─RL─┘      └─FR─┘      └─RR─┘  (GP26/27 are ADC pins — NOT 5V tol.) │
   └──┬───┬────────┬────┬──────────────────────────────────┬───┬─────────┬──────────┘
      │   │ (motor signals on breadboard OK)               │ A │ B  …     │ 3V3 + GND
      ▼   ▼                                                ▼   ▼          │  to encoders
  ┌─────────────────────────┐                  ┌─────────────────────────┐│
  │   TB6612FNG  Board A     │   STBY both ─────│   TB6612FNG  Board B     ││
  │   VCC=3.3V   VM=7.4V     │   to GP14 (HIGH) │   VCC=3.3V   VM=7.4V     ││
  │   chA: AO1 AO2  chB:BO1BO2                  │   chA: AO1 AO2  chB:BO1BO2││
  └────┬────┬──────┬───┬────┘                  └────┬────┬──────┬───┬────┘ │
       │M+  │M−    │M+ │M−                          │M+  │M−    │M+ │M−     │
   ╔═══╪════╪══════╪═══╪══════════════════════════════════════════════════╪═══════╗
   ║   │    │      │   │   Pi Pico SCREW-TERMINAL CARRIER BOARD (soldered) │       ║
   ║   │ The motor-power pair (M+/M−, 18 AWG) and the encoder conductors   │       ║
   ║   │ (A,B,VCC,GND, 22 AWG) all land on retained screw terminals here.  │       ║
   ╚═══╪════╪══════╪═══╪══════════════════════════════════════════════════╪═══════╝
       ▼    ▼      ▼   ▼                          ▼    ▼      ▼   ▼         │
   ┌──────────┐ ┌──────────┐                  ┌──────────┐ ┌──────────┐    │
   │ FL motor │ │ RL motor │                  │ FR motor │ │ RR motor │    │
   │ +0.1µF   │ │ +0.1µF   │                  │ +0.1µF   │ │ +0.1µF   │    │
   │ +ENCODER │ │ +ENCODER │                  │ +ENCODER │ │ +ENCODER │    │
   └────┬─────┘ └────┬─────┘                  └────┬─────┘ └────┬─────┘    │
        │ JST-PH 6P  │ JST-PH 6P                   │ JST-PH 6P  │ JST-PH 6P │
        │  Enc-VCC ◄──┴─────────────────────────────┴───────────┴──────────┘
        │  (all 4 encoder VCCs ← Pico 3V3 OUT, 3.3 V — ★ NOT 5V/7.4V)
        │  Enc-GND ──► star ground (own thin 22 AWG legs)
        │  Hall-A / Hall-B ──► Pico PIO base / base+1 (per wheel)

   ═════════ MOTOR POWER PATH (7.4 V) — unchanged from Phase 1 (still on the stand) ═════════
   ┌──────────────┐
   │ 2S LiPo 7.4V │  SM-2P male → SM-2P female pigtail → red(+) / black(−)
   │ 2200mAh 10C  │
   │ +protection  │  red(+) ─► ┌────────────────────┐ ──► Board A VM (7.4V)
   │  board       │            │  WAGO 221-413 (+)  │ ──► Board B VM (7.4V)
   │              │            └────────────────────┘   ╪ 470–1000µF bulk cap per rail
   │              │
   │              │  black(−) ─► ┌────────────────────┐ ──► Board A GND
   │              │              │  WAGO 221-413 (−)  │ ──► Board B GND
   │              │              └────────────────────┘   (this splice = the star node)
   └──────────────┘
        No fuse / master switch / e-stop yet — the LiPo's built-in protection board plus
        unplug-SM-2P / `stop` / STBY-low cover a single-branch, wheels-in-air build.
        The full protection tree (10 A main fuse, #2815 switch, branch fuses, NC e-stop)
        is introduced in Phase 3, when the Pi becomes a second branch.

   ───────────────── COMMON GROUND (single-point STAR at the WAGO − splice) ─────────────────
     separate legs to: Board A GND · Board B GND · Pico GND (via laptop USB this phase)
     · 4× Enc-GND (thin 22 AWG) · heavy 18 AWG for motor return · NO daisy-chaining
```

**Three rails, joined only at ground.** The **7.4 V motor rail** (LiPo → driver VM → motor M+/M−) carries amps. The **3.3 V logic rail** (Pico 3V3 → driver VCC) carries the driver-input references. The **3.3 V encoder rail** (Pico 3V3 → Enc-VCC) is a sub-100 mA tap off that same 3V3(OUT) pin. ★ The single reason the encoder runs at 3.3 V and not the motor's 7.4 V: a Hall output swings to *its own supply rail*, so a 5 V or 7.4 V encoder supply would slam the Pico's 3.3 V GPIO well past its limit — and RR's A/B land on **GP26/GP27, which are ADC pins that are *never* 5 V tolerant.** All three rails meet at exactly one place: the common star ground.

---

## Wire-by-wire connection list — NEW this phase

Tick each box after the wire is placed **and** beep-tested. Format: `A --> B — purpose`. Everything below lands on the **screw-terminal carrier board**, not the breadboard. The encoder A/B/VCC/GND conductors arrive inside each motor's factory **JST-PH 2.0 6-pin** plug (the same plug whose M+/M− you wired in Phase 1) — un-park them now.

### FL encoder (Board A · ch A) — JST-PH 6P → carrier → Pico

- [ ] `FL Enc Hall-A` --> `Pico GP16` (phys. pin 21) — FL quadrature A → PIO **base** pin
- [ ] `FL Enc Hall-B` --> `Pico GP17` (phys. pin 22) — FL quadrature B → PIO base+1 (must be adjacent) ⚠️ footgun #2
- [ ] `FL Enc-VCC` --> `Pico 3V3(OUT)` (phys. pin 36) — 3.3 V encoder supply ⚠️ **not** 5 V/7.4 V (footgun #1)
- [ ] `FL Enc-GND` --> `STAR ground` — own thin 22 AWG return leg

### RL encoder (Board A · ch B) — JST-PH 6P → carrier → Pico

- [ ] `RL Enc Hall-A` --> `Pico GP18` (phys. pin 24) — RL quadrature A → PIO base
- [ ] `RL Enc Hall-B` --> `Pico GP19` (phys. pin 25) — RL quadrature B → PIO base+1 (adjacent)
- [ ] `RL Enc-VCC` --> `Pico 3V3(OUT)` (phys. pin 36) — 3.3 V encoder supply
- [ ] `RL Enc-GND` --> `STAR ground` — own thin 22 AWG return leg

### FR encoder (Board B · ch A) — JST-PH 6P → carrier → Pico

- [ ] `FR Enc Hall-A` --> `Pico GP20` (phys. pin 26) — FR quadrature A → PIO base
- [ ] `FR Enc Hall-B` --> `Pico GP21` (phys. pin 27) — FR quadrature B → PIO base+1 (adjacent)
- [ ] `FR Enc-VCC` --> `Pico 3V3(OUT)` (phys. pin 36) — 3.3 V encoder supply
- [ ] `FR Enc-GND` --> `STAR ground` — own thin 22 AWG return leg

### RR encoder (Board B · ch B) — JST-PH 6P → carrier → Pico

- [ ] `RR Enc Hall-A` --> `Pico GP26` (phys. pin 31) — RR quadrature A → PIO base ⚠️ **ADC pin, never 5 V tolerant**
- [ ] `RR Enc Hall-B` --> `Pico GP27` (phys. pin 32) — RR quadrature B → PIO base+1 (adjacent) ⚠️ ADC pin
- [ ] `RR Enc-VCC` --> `Pico 3V3(OUT)` (phys. pin 36) — 3.3 V encoder supply (★ doubly important: A/B on ADC pins)
- [ ] `RR Enc-GND` --> `STAR ground` — own thin 22 AWG return leg

### Carrier-board migration — *moving the motor/encoder interconnect off the breadboard*

> This is a mechanical re-home, not new electrical nets. The Phase 1 motor-power pairs (M+/M−, 18 AWG) and the new encoder conductors (22 AWG) all terminate on the **soldered Pico screw-terminal carrier board** so vibration can't loosen them. The 4 motors' factory JST-PH 6-pin plugs mate to their pigtails as before.

- [ ] Mount the Pico onto the **screw-terminal carrier board** (solder the carrier's header / seat the Pico) — retained terminals, not breadboard springs
- [ ] Re-land `FL/RL/FR/RR motor M+/M−` (18 AWG) on the carrier's motor terminals — moved off any breadboard rail ⚠️ footgun #4
- [ ] Land all 4 encoders' `Hall-A / Hall-B / Enc-VCC / Enc-GND` on the carrier's signal terminals (per the four lists above)
- [ ] Bridge `Pico 3V3(OUT)` to a carrier rail feeding **all 4 Enc-VCC + both TB6612 VCC** — one 3.3 V net (load ≪ 300 mA) ⚠️ footgun #1
- [ ] Strain-relief the four JST-PH 6P pigtails to the chassis (zip-tie) so cable tug lands on the tie, not the screw terminal

### Encoder noise suppression — *only if counts jitter under power (see README pitfalls)*

- [ ] (optional, recommended) **`4.7 k pull-up`** from each `Hall-A → 3.3 V` and `Hall-B → 3.3 V` at the carrier (8 total) — a defined HIGH that's harmless if the output is push-pull, the pull-up if it's open-collector. **To 3.3 V only, never 5 V.** Also enable the Pico's internal pull-ups in firmware. (Encoder is datasheet-rated 3.3–5 V with built-in pull-up shaping, so this is belt-and-suspenders.)
- [ ] (optional) `100 nF ceramic` from `FL Hall-A ↔ GND` and `FL Hall-B ↔ GND` at the motor — tame brush noise on the A/B lines
- [ ] (optional) repeat for `RL / FR / RR` if their counts jitter when the motor rail is live
- [ ] Route the encoder 22 AWG signal pairs **away from** the 18 AWG motor-power pair (don't bundle them)

---

## Pin reference tables (this phase)

### Pico 2W encoder pins (canonical — A/B MUST be adjacent GPIOs)

| Wheel | Board · Ch | A (PIO base) | B | Phys. pin A | Phys. pin B | PIO block · SM |
|---|---|---|---|---|---|---|
| FL | A · A | GP16 | GP17 | 21 | 22 | block 0 · sm 0 |
| RL | A · B | GP18 | GP19 | 24 | 25 | block 0 · sm 1 |
| FR | B · A | GP20 | GP21 | 26 | 27 | block 0 · sm 2 |
| RR | B · B | GP26 | GP27 | 31 | 32 | block 1 · sm 0 |

- **Encoder VCC** (all 4) ← Pico **3V3(OUT)** (phys. pin 36). **Encoder GND** (all 4) → common star ground.
- 4 of the 12 PIO state machines used; 8 stay free. (PIO block/SM assignment matches the README firmware sketch — each encoder owns a unique `(pio, sm)`.) ⚠️ footgun #3.

### Motor JST-PH 2.0 6-pin connector — full conductor use as of Phase 2

| Conductor | Phase 1 | Phase 2 (now) | Lands on |
|---|---|---|---|
| M+ | → driver output | unchanged | carrier motor terminal (18 AWG) → TB6612 AO/BO |
| M− | → driver output | unchanged | carrier motor terminal (18 AWG) → TB6612 AO/BO |
| Enc-VCC | parked (tape off) | → **Pico 3V3(OUT)** (3.3 V) | carrier 3.3 V rail |
| Enc-GND | parked | → **star ground** | carrier GND → star |
| Hall-A | parked | → **Pico PIO base pin** | carrier signal terminal |
| Hall-B | parked | → **Pico PIO base+1 pin** | carrier signal terminal |

### Full Pico GPIO allocation as of Phase 2 (for context)

| GPIO | Use |
|---|---|
| GP2–GP13 | Motor control (IN1/IN2/PWM × 4 wheels) — Phase 1 |
| GP14 | TB6612 STBY (both boards), tied HIGH — Phase 1 |
| GP16/17, GP18/19, GP20/21, GP26/27 | **Encoder A/B** (FL, RL, FR, RR) — **NEW this phase** |
| GP0, GP1, GP15, GP22, GP28 | **Free** for later phases |
| 3V3(OUT) pin 36 | TB6612 ×2 VCC + 4× Enc-VCC (load ≪ 300 mA) |

> The motor-control pins (GP2–GP14) and the entire 7.4 V power tree are **unchanged from Phase 1** — see [`../phase_1_powertrain/wiring_diagram.md`](../phase_1_powertrain/wiring_diagram.md) for those wire-by-wire lists and the TB6612 pin meanings.

---

## ⚠️ Footguns that apply to THIS phase's wires

1. **Encoder VCC = 3.3 V, never 5 V or 7.4 V.** A Hall output swings to its *own* supply rail, so powering the encoder above 3.3 V drives the A/B lines past the Pico's GPIO input limit. Enc-VCC comes from **Pico 3V3(OUT) (pin 36)** only. Some encoders "work" at 5 V for a while — that's silent over-stress, not safety. *Contract: device_contracts.md — "Encoder supply: 3.3–5 V → use 3.3 V … Never feed the encoder 7.4 V."* **Verify with a multimeter:** 3.3 V across each Enc-VCC↔GND on USB power, motors unpowered, before you ever read a pin. The encoder is datasheet-rated **3.3–5 V** with built-in pull-up shaping, so 3.3 V is *in-spec* (not a guess) and the A/B HIGH is ~3.3 V — no level-shifter. Optionally fit a **4.7 k A/B pull-up to 3.3 V** (never 5 V), robust whether the output is open-collector or push-pull. Don't rely on RP2350 5 V-tolerance (stepping-dependent / withdrawn from the datasheet).
2. **A and B must be on adjacent GPIOs.** The PIO quadrature decoder reads **two consecutive GPIOs** from a base pin, so each wheel's A→base, B→base+1 (16/17, 18/19, 20/21, 26/27). Swap them onto non-adjacent pins and the decoder reads the wrong second channel. If a wheel counts the *wrong direction*, swap A/B **at the carrier terminal**, not in software — keeps the sign convention honest everywhere downstream. *README — "swap its A/B at the carrier terminals (don't fix it in code)."*
3. **PIO state-machine ID collision.** Each PIO block has only 4 state machines (sm 0–3). Give every encoder a unique `(pio, sm)` — the table above spreads FL/RL/FR across block 0 and puts RR on block 1. Reuse a pair and the second encoder silently clobbers the first. *README task 1.*
4. **Motor power still never on the breadboard — and now neither is the interconnect.** All M+/M−, VM, and LiPo wiring is soldered/screw-terminal 18 AWG on the **carrier board**. Breadboard springs (~1 A) can't carry the ~1.4 A/motor (~5.6 A all-stall), and Mecanum vibration walks Dupont jumpers loose. The breadboard survives only for transient signal bring-up. *power_budget.md — "motor power never touches a breadboard."*
5. **GP26/GP27 (RR's A/B) are ADC pins — never 5 V tolerant.** Even the "5 V-input-tolerant while 3V3 is powered" rule for GP0–25 does **not** apply to GP26–29. RR's channels land here, so an over-voltage encoder supply damages these pins outright. One more reason Enc-VCC = 3.3 V. *device_contracts.md — "ADC pins (GPIO26–29) are never 5 V tolerant."*
6. **Star ground, not a daisy-chain — the encoder GNDs too.** Each of the four Enc-GND conductors gets its own thin 22 AWG leg back to the one star node at LiPo−. Don't fold them into the heavy motor-return leg; 20 kHz motor current on a shared return corrupts the ground reference your quadrature edges are measured against (→ jitter / phantom counts). *power_budget.md — "Single-point STAR at LiPo (−) … No daisy-chaining."*
7. **Don't back-feed the Pico (carry-forward).** The 7.4 V motor rail must never reach VBUS/VSYS/3V3 — including via a mis-landed M+ wire on the carrier next to the 3.3 V rail. Motor power and Pico/encoder power are separate rails joined only at ground. *device_contracts.md — Pico footgun.*

> Devices in the canonical design **not yet present** (added in later phases): Raspberry Pi 5 + D24V50F5 buck and the Pi↔Pico serial protocol (Phase 3), BNO055 IMU + INA219 (Phase 4), Camera 3 + PCA9685 + UBEC + servos (Phase 5), STL-19P lidar + CP2102 (Phase 6). This phase only *energizes and reads* the encoder conductors that were already mated in Phase 1.
>
> **Power-protection parts still deferred** (like Phase 1): the **10 A main fuse, #2815 reverse-polarity master switch, and branch fuses** arrive in **Phase 3** (when the Pi adds a second branch and selectivity finally matters), and the **NC mushroom e-stop** with it (Phase 3 is also the first floor-driving phase). Phase 2 is still wheels-on-stand on the simplified SM-2P → WAGO motor path, protected by the LiPo's built-in board.
