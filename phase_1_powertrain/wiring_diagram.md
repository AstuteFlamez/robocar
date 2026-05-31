# Phase 1 — Wiring Diagram (cumulative)

> This is the **full system state at the end of Phase 1.** Later phases redraw this same picture with new blocks bolted on, so it always shows the whole robot. Every connection's *contract* (voltage, current, logic level, connector) lives in [`../_engineering/device_contracts.md`](../_engineering/device_contracts.md); the power-tree sizing is in [`../_engineering/power_budget.md`](../_engineering/power_budget.md). Tick each wire's box after you've placed it **and** beep-tested it.

---

## Block schematic — end of Phase 1

```
   ┌────────────────────┐
   │   Laptop (USB)     │  power + serial REPL (temporary; Pi takes over in Phase 3)
   └─────────┬──────────┘
             │ USB  (5 V on VBUS + serial)
             ▼
   ┌────────────────────────────────────────────────────────────┐
   │                    Raspberry Pi Pico 2W                      │
   │  VBUS◄─USB 5V          3V3(OUT)──┐ (logic supply, <300 mA)   │
   │                                  │                           │
   │  GP2  GP3  GP4   GP5  GP6  GP7    │   GP8 GP9 GP10  GP11 GP12 GP13   GP14  GND│
   │  IN1  IN2  PWM   IN1  IN2  PWM    │   IN1 IN2 PWM   IN1  IN2  PWM    STBY     │
   │  └──FL ch A──┘   └──RL ch B──┘    │   └──FR ch A─┘  └──RR ch B──┘     │     │ │
   └───────┬───────────────┬──────────┼──────────┬──────────────┬────────┼─────┼─┘
           │  (3.3V signals on breadboard — SIGNALS ONLY)        │        │     │
           │ VCC=3.3V ◄─────────────────┤                         │ VCC=3.3V◄───┤  STBY tied to
           ▼                            │                         ▼        │     │  GP14 (HIGH)
   ┌─────────────────────────┐          │              ┌─────────────────────────┐ │
   │   TB6612FNG  Board A     │          │              │   TB6612FNG  Board B     │ │
   │   VCC=3.3V   VM=7.4V     │          │              │   VCC=3.3V   VM=7.4V     │ │
   │   STBY◄──────────────────┼──────────┼──────────────┼──────STBY (both to GP14)─┘
   │   chA: AO1 AO2  chB: BO1 BO2         │              │   chA: AO1 AO2  chB: BO1 BO2
   └────┬─────┬───────┬────┬──┘          │              └────┬─────┬───────┬────┬──┘
        │ M+  │ M−    │M+  │M−            │                   │M+   │M−     │M+  │M−
        ▼     ▼       ▼    ▼              │                   ▼     ▼       ▼    ▼
     ┌────────────┐ ┌────────────┐        │                ┌────────────┐ ┌────────────┐
     │  FL motor  │ │  RL motor  │        │                │  FR motor  │ │  RR motor  │
     │ opt 0.1µF* │ │ opt 0.1µF* │        │                │ opt 0.1µF* │ │ opt 0.1µF* │
     │ (enc PARKED)│ │(enc PARKED)│       │                │(enc PARKED)│ │(enc PARKED)│
     └────────────┘ └────────────┘        │                └────────────┘ └────────────┘
        ▲ via JST-PH 2.0 6-pin            │                   ▲ via JST-PH 2.0 6-pin
        │ (motor leads live;              │                   │ (encoder conductors mated
        │  encoder conductors mated       │                   │  but NOT wired/read yet)
        │  but parked until Phase 2)      │                   │
                                          │
   ════════════════ MOTOR POWER PATH (7.4 V) — Phase 1 bench build ════════════════
                                          │
   ┌──────────────┐                       │
   │ 2S LiPo 7.4V │  SM-2P male output     │
   │ 2200mAh 10C  │──►( SM-2P female ──► red(+) / black(−) )
   │ +protection  │       pigtail          │
   │  board       │                        │
   │ (charge via  │   red(+) ─► ┌────────────────────┐ ──► Board A VM (7.4V)
   │  DC5.5×2.5   │             │  WAGO 221-413 (+)  │ ──► Board B VM (7.4V)
   │  barrel jack)│             └────────────────────┘
   │              │
   │              │   black(−) ─► ┌────────────────────┐ ──► Board A GND
   │              │               │  WAGO 221-413 (−)  │ ──► Board B GND
   │              │               └────────────────────┘
   │              │
   │              │   (each board: power lands on SOLDERED 2.54 mm screw terminals)
   │              │   ╪ 470–1000µF bulk cap across VM↔GND (one per board/rail)
   └──────────────┘
        No fuse / master switch / e-stop in the bench path — the LiPo's built-in
        protection board covers short+overcurrent; kills = unplug SM-2P / `stop` / STBY-low.
        (Coordinated fuses + #2815 switch arrive Phase 3; NC e-stop in the first mobile phase.)

   ───────────────────────── COMMON GROUND (single-point STAR at LiPo −) ─────────────
     star node = the WAGO (−) splice; separate legs to: Board A GND · Board B GND ·
     Pico GND (via laptop USB this phase) · heavy 18 AWG motor return · NO daisy-chaining
```

> **\* Fit a 0.1 µF cap across each driver's motor outputs** — inspect each motor's rear PCB first; it *may* carry a factory cap, but there's no evidence the 310s do, so default to fitting. The motors are connectorized (factory JST-PH 6-pin), no bare tabs, so the cap goes **at the driver outputs**, not the motor. See "Motor noise suppression" below.

**Two rails, joined only at ground.** The **3.3 V logic rail** (Pico 3V3 → driver VCC) and the **7.4 V motor rail** (LiPo → driver VM) are completely separate. They meet at *exactly one place*: the common star ground (the WAGO − splice). Never let the 7.4 V rail back-feed into the Pico's 3.3 V / VBUS / VSYS.

---

## Wire-by-wire connection list — NEW this phase

Tick each box after the wire is placed **and** beep-tested. Format: `A --> B — purpose`.

### Pico 2W → TB6612 Board A (FL = ch A, RL = ch B) — *signals, breadboard OK*

- [ ] `Pico GP2` --> `Board A AIN1` — FL direction bit 1 (per device_contracts.md canonical map)
- [ ] `Pico GP3` --> `Board A AIN2` — FL direction bit 2
- [ ] `Pico GP4` --> `Board A PWMA` — FL speed (20 kHz PWM)
- [ ] `Pico GP5` --> `Board A BIN1` — RL direction bit 1
- [ ] `Pico GP6` --> `Board A BIN2` — RL direction bit 2
- [ ] `Pico GP7` --> `Board A PWMB` — RL speed (20 kHz PWM)

### Pico 2W → TB6612 Board B (FR = ch A, RR = ch B) — *signals, breadboard OK*

- [ ] `Pico GP8` --> `Board B AIN1` — FR direction bit 1
- [ ] `Pico GP9` --> `Board B AIN2` — FR direction bit 2
- [ ] `Pico GP10` --> `Board B PWMA` — FR speed (20 kHz PWM)
- [ ] `Pico GP11` --> `Board B BIN1` — RR direction bit 1
- [ ] `Pico GP12` --> `Board B BIN2` — RR direction bit 2
- [ ] `Pico GP13` --> `Board B PWMB` — RR speed (20 kHz PWM)

### Shared enable + logic supply — *signals, breadboard OK*

- [ ] `Pico GP14` --> `Board A STBY` — driver master enable (must be HIGH) ⚠️ see footgun #1
- [ ] `Pico GP14` --> `Board B STBY` — same GP14 net drives both boards' STBY
- [ ] `Pico 3V3(OUT)` (pin 36) --> `Board A VCC` — 3.3 V logic supply ⚠️ VCC ≠ VM (footgun #2)
- [ ] `Pico 3V3(OUT)` (pin 36) --> `Board B VCC` — 3.3 V logic supply
- [ ] `Pico GND` --> `Board A GND` — logic-side ground to star
- [ ] `Pico GND` --> `Board B GND` — logic-side ground to star

### Driver outputs → motors — *7.4 V power, soldered/screw-terminal 18 AWG, NEVER breadboard*

> Each motor's M+/M− are two of the six conductors in its factory **JST-PH 2.0 6-pin** plug. Mate the pigtail; bring the two **motor leads** to the driver outputs; **park** (leave unconnected) the four encoder conductors (Enc-VCC, Enc-GND, Hall-A, Hall-B) until Phase 2. Do **not** cut the connector or leave bare dangling ends — tape/heat-shrink the parked conductors.

- [ ] `Board A AO1` --> `FL motor M+` — FL motor lead (via JST-PH 6P)
- [ ] `Board A AO2` --> `FL motor M−` — FL motor lead
- [ ] `Board A BO1` --> `RL motor M+` — RL motor lead
- [ ] `Board A BO2` --> `RL motor M−` — RL motor lead
- [ ] `Board B AO1` --> `FR motor M+` — FR motor lead
- [ ] `Board B AO2` --> `FR motor M−` — FR motor lead
- [ ] `Board B BO1` --> `RR motor M+` — RR motor lead
- [ ] `Board B BO2` --> `RR motor M−` — RR motor lead

### Motor power path — *7.4 V, soldered screw-terminal 18 AWG, NEVER breadboard*

> Phase 1 bench build: SM-2P → WAGO splits → screw terminals on each driver. **No external fuse, master switch, or e-stop** (the LiPo's built-in protection board covers short/overcurrent; coordinated fuses + #2815 switch land in Phase 3, the NC e-stop in the first mobile phase). Each WAGO 221-413 joins *all three* inserted wires — there is no "in" vs "out".

- [ ] `LiPo SM-2P male` --> `SM-2P female pigtail` — mate the battery's keyed output plug (don't cut it); breaks out to red (+) / black (−)
- [ ] `Pigtail red (+)` --> `WAGO #1 port 1` — battery + into the (+) splice
- [ ] `WAGO #1 port 2` --> `Board A VM` (screw terminal) — 7.4 V motor supply, Board A
- [ ] `WAGO #1 port 3` --> `Board B VM` (screw terminal) — 7.4 V motor supply, Board B (parallel)
- [ ] `Pigtail black (−)` --> `WAGO #2 port 1` — battery − into the (−) splice (this is the star node)
- [ ] `WAGO #2 port 2` --> `Board A GND` (screw terminal) — power-side ground, Board A
- [ ] `WAGO #2 port 3` --> `Board B GND` (screw terminal) — power-side ground, Board B
- [ ] `470–1000 µF bulk cap (+)` --> `VM`; `bulk cap (−)` --> `GND` — **one cap per driver board (×2)**, absorbs start/stall di/dt across the motor rail
- [ ] *(optional)* `7.5 A inline blade fuse` in the red (+) lead between pigtail and WAGO #1 — belt-and-suspenders only; not required (battery protection board already covers it)

### Motor noise suppression — *optional; check for a factory cap first, else add at the driver*

> ⚠️ The 310 motors are **connectorized** (factory JST-PH 6-pin) — there are **no bare motor tabs** to solder a cap across, and we don't cut the factory plug. **Inspect each motor's rear PCB**: it *might* carry a factory noise cap, but there's **no manufacturer evidence the 310s do**, so **default to fitting one** — add a **0.1 µF ceramic across each channel's two driver outputs** (no polarity, no motor surgery). Cheap insurance against brush noise dropping the USB serial link mid-drive or jittering Phase-2 encoder counts. (Closer-to-motor alternative: solder it across M+/M− at the pigtail.)

- [ ] `0.1 µF ceramic` across `Board A AO1 ↔ AO2` — FL brush-noise suppression, at the driver
- [ ] `0.1 µF ceramic` across `Board A BO1 ↔ BO2` — RL
- [ ] `0.1 µF ceramic` across `Board B AO1 ↔ AO2` — FR
- [ ] `0.1 µF ceramic` across `Board B BO1 ↔ BO2` — RR

### Common ground — *single-point STAR at the WAGO (−) splice, separate leg each, NO daisy-chain*

- [ ] `Pigtail black (−)` --> `WAGO #2` — the WAGO (−) splice *is* the single star node (at the battery negative)
- [ ] `WAGO #2` --> `Board A GND` (power side) — own leg, heavy 18 AWG (motor return)
- [ ] `WAGO #2` --> `Board B GND` (power side) — own leg, heavy 18 AWG
- [ ] `Pico GND` --> `WAGO #2` (or a board GND screw terminal) — own thin 22 AWG leg (logic reference); reaches via laptop USB ground this phase

---

## Pin reference tables (this phase)

### Pico 2W motor-control pins (canonical)

| Wheel | Board · Ch | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | A · A | GP2 | GP3 | GP4 |
| RL | A · B | GP5 | GP6 | GP7 |
| FR | B · A | GP8 | GP9 | GP10 |
| RR | B · B | GP11 | GP12 | GP13 |
| STBY (both) | — | GP14 (HIGH) | | |

### TB6612FNG pin meaning (per board)

| Pin | Connect to | Rail / level |
|---|---|---|
| VM | LiPo motor branch (7.4 V) | **7.4 V power** |
| VCC | Pico 3V3(OUT) | **3.3 V logic** |
| GND | star ground | 0 V |
| STBY | Pico GP14 | 3.3 V, **must be HIGH** |
| AIN1/AIN2, PWMA | Pico (per table) | 3.3 V signals |
| BIN1/BIN2, PWMB | Pico (per table) | 3.3 V signals |
| AO1/AO2 | channel-A motor M+/M− | 7.4 V motor output |
| BO1/BO2 | channel-B motor M+/M− | 7.4 V motor output |

### Motor JST-PH 2.0 6-pin connector (conductor use this phase)

| Conductor | This phase | Phase 2 |
|---|---|---|
| M+ | → driver output | (unchanged) |
| M− | → driver output | (unchanged) |
| Enc-VCC | **parked** (tape off) | → Pico 3V3 (**3.3 V, not 7.4 V**) |
| Enc-GND | **parked** | → star ground |
| Hall-A | **parked** | → Pico PIO base pin |
| Hall-B | **parked** | → Pico PIO base+1 pin |

---

## ⚠️ Footguns that apply to THIS phase's wires

1. **STBY must be HIGH.** Both TB6612 STBY pins tie to `Pico GP14`, driven high in firmware (`enable_drivers(True)`). Left low, every motor stays dead no matter how perfect the rest of the wiring is. *Contract: device_contracts.md — "★ STBY pin must be pulled HIGH or the outputs stay disabled."*
2. **VCC ≠ VM.** TB6612 **VCC = 3.3 V logic** (Pico 3V3); **VM = 7.4 V motor** (LiPo). Cross-wiring 7.4 V into VCC destroys the logic side. Two different pins, two different rails — only ground is shared. *Contract: device_contracts.md — TB6612 "VCC (logic) = 3.3 V … VM (motor) = 7.4 V."*
3. **Encoder VCC = 3.3 V, never 7.4 V — and parked this phase.** You mate the encoder conductors in the 6-pin plug but do **not** power or read them yet. When you do (Phase 2), Enc-VCC comes from Pico 3V3 so the Hall A/B outputs stay ≤ 3.3 V for the Pico's GPIO. *Contract: device_contracts.md — "Never feed the encoder 7.4 V."*
4. **A WAGO has no "in" or "out" — and a half-seated wire browns out under load.** All three ports of a 221-413 are electrically joined; "in vs out" is just which wire comes from the battery. Strip ~11 mm, lift the lever, insert the bare copper *fully* (no stray strands), close, and tug-test each wire. Don't cut the LiPo's keyed SM-2P plug — mate the female pigtail to it (the key prevents reverse-mating, which is why Phase 1 *defers* a separate reverse-protection part). ⚠️ **Keying only protects the battery mate — the WAGO and screw-terminal wiring downstream is unkeyed**, so meter each driver's VM↔GND for correct polarity (+7.4 V, red = +) before connecting; a reversed lead at a WAGO destroys the TB6612 and there's no #2815 to catch it yet. *Brief: Phase 1 power path — SM-2P → WAGO splits → screw terminals.* (The NC e-stop and coordinated fuses this footgun used to cover are deferred — see the closing note.)
5. **Motor power never on the breadboard.** All VM, LiPo, and motor-output wires are soldered/screw-terminal 18 AWG. Breadboard springs (~1 A) can't carry ~1.4 A/motor (~5.6 A all-stall). Breadboard is for the 3.3 V signal jumpers only. *power_budget.md — "Breadboard spring contacts are ~1 A — motor power never touches a breadboard."*
6. **Star ground, not a daisy-chain.** Every ground gets its own leg back to one node at LiPo−. Don't chain LiPo− → BoardA → BoardB → Pico; the 20 kHz motor-return current would corrupt the logic ground reference. *power_budget.md — "Single-point STAR at LiPo (−) … No daisy-chaining."*
7. **Don't back-feed the Pico.** The 7.4 V motor rail must never reach VBUS/VSYS/3V3. The Pico is powered from laptop USB this phase; motor power and Pico power are separate rails joined only at ground. *Contract: device_contracts.md — Pico "⚠️ Footgun: never back-feed the 7.4 V motor rail into VBUS/VSYS/3V3."*

> Devices in the canonical design **not yet present** (added in later phases): Raspberry Pi 5 + D24V50F5 buck (Phase 3), BNO055 IMU + INA219 (Phase 4), Camera 3 + PCA9685 + UBEC + servos (Phase 5), STL-19P lidar + CP2102 (Phase 6). The encoder wiring of the already-mated 6-pin plugs is energized in Phase 2.
>
> **Power-protection parts deferred out of Phase 1** (explained in the README's Concepts, built when they pay off): the **coordinated fuse set (10 A main / 7.5 A motor / 5 A Pi)** and the **#2815 reverse-polarity master switch** arrive in **Phase 3**, when the Pi adds a second branch; the **22 mm NC mushroom e-stop** arrives in the first **mobile** phase. Phase 1's bench path relies on the LiPo's built-in protection board plus the unplug / `stop` / STBY-low kills.
