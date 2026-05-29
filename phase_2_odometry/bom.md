# Phase 2 — Bill of Materials

> Parts **introduced or first used** in Phase 2 (odometry & closed-loop wheel speed). This is a slice of the master list — see [`../_engineering/products_to_buy.md`](../_engineering/products_to_buy.md) for the full, union-of-all-phases BOM with the cost summary. Prices are ballpark USD, early 2026. "Why" gives the *electrical* reason the part exists.
>
> Phase 2 is almost free: the wiring is light because you're routing conductors that **already exist** inside the motors' factory 6-pin plug. The only *new purchase* is one carrier board to get the motor/encoder interconnect off the breadboard and onto retained screw terminals.

## New purchase this phase

| Item | Qty | ~$ | Why (electrical reason) | Link | Status |
|---|---|---|---|---|---|
| **Pi Pico screw-terminal carrier board** | 1 | $13 | Moves the motor-power pairs (M+/M−, 18 AWG) and the four newly-energized encoder conductors (Hall-A/B, Enc-VCC, Enc-GND, 22 AWG) off the breadboard onto **retained screw terminals**. A Mecanum bot vibrates — a Dupont jumper or breadboard spring (~1 A) walks loose mid-drive, and motor power was never allowed on a breadboard anyway (~5.6 A all-stall). | https://czh-labs.com/products/screw-terminal-block-breakout-module-board-for-raspberry-pi-pico | **buy** |

## Already have — used (not bought) this phase

| Item | Qty | Why it matters this phase | Status |
|---|---|---|---|
| "310" metal-gear motors **with Hall encoders** (JGA27-310 / MD310Z20) | 4 | The encoder is *built into the motor* — 13 lines/motor-shaft rev, AB-phase quadrature, ~1040 counts/output-rev (★ measure it, don't trust it). Its Enc-VCC runs at **3.3 V** (not the motor's 7.4 V) so the A/B Hall outputs stay inside the Pico's GPIO limit — datasheet-rated 3.3–5 V with built-in pull-up shaping, so 3.3 V is in-spec. | already have (kit) |
| **JST-PH 2.0 6-pin pigtail leads** | 4 | The encoder conductors (Hall-A, Hall-B, Enc-VCC, Enc-GND) ride in the **same factory 6-pin plug** as the motor's M+/M−. You bought these pigtails in Phase 1 and mated them then — this phase just un-parks the four encoder wires. **Don't cut the factory connector.** | already bought (Phase 1) |
| Raspberry Pi Pico 2W (RP2350) | 1 | Decodes all four quadrature encoders in **PIO hardware** (4 of 12 state machines), runs the per-wheel PID. **3V3(OUT) (pin 36, ≤300 mA)** now also feeds the 4 encoder VCCs on top of the two TB6612 logic rails (total ≪ 300 mA). | already have (kit/shelf) |
| 8-ch USB logic analyzer (sigrok/PulseView) | 1 | Lab 2: confirm **A leads B** forward and the lead/lag flips on reverse — *seeing* quadrature on a real capture. | already bought (Phase 1) |
| Digital multimeter (continuity + DC volts) | 1 | The continuity ritual: beep-test each Enc-VCC to **3V3(OUT)** (never the 7.4 V rail) and confirm **3.3 V** across each Enc-VCC↔GND before reading any pin. | already bought (Phase 1) |
| 0.1 µF ceramic caps (+ spare 100 nF) | as needed | Brush-noise suppression. If counts jitter when the motor rail is live, add 100 nF from each A/B line to GND at the motor (README pitfalls). | already bought (Phase 1, 12-pack) |
| Heat-shrink + wire labels, 22 AWG signal wire, ferrule/crimp kit | — | Clean, labeled, low-resistance terminations for the encoder conductors landing on the carrier. | already bought (Phase 1) |

> No software is on this list — the PIO quadrature decoder, PID, and telemetry are firmware you write/adopt on the Pico (see the README's software task list). They cost nothing but a weekend.

---

## ⚠️ Buy-and-verify notes

- **Carrier board fit:** confirm it's sized for the **Pico 2W** form factor (40-pin, same footprint as the original Pico) and breaks out the GPIOs you need this phase — **GP16/17, GP18/19, GP20/21, GP26/27** (encoder A/B), **3V3(OUT)**, and **GND** — plus the GP2–GP14 motor-control pins from Phase 1. Solder the Pico to it (or seat it, if socketed) so the terminals are *retained*.
- **3.3 V, not 5 V or 7.4 V, for Enc-VCC.** This is the one number that protects the Pico's GPIO (and especially RR's A/B on the **ADC pins GP26/27, which are *never* 5 V tolerant**). Source it from the carrier's 3V3(OUT) rail. See [`../_engineering/device_contracts.md`](../_engineering/device_contracts.md).
- **Measure `COUNTS_PER_REV`.** Calculated 1040 (13 × 4 × 20), but the bench is the authority — every downstream distance/velocity/SLAM scale rides on it (README Lab 1).
- **Optional A/B pull-ups:** 8× **4.7 kΩ** ¼ W resistors to pull each Hall-A/Hall-B to **3.3 V** (never 5 V). Belt-and-suspenders for a defined HIGH — harmless if the output is push-pull, required if it's open-collector. ~$1 (or from the Phase-1 resistor assortment). Verify clean 0–3.3 V edges on a scope (README checklist).

---

**Full master BOM, all phases, with the cost summary and verify-before-ordering checklist:** [`../_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).
