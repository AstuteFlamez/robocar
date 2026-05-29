# Phase 1 — Bill of Materials

> Parts **introduced or first used** in Phase 1 (the powertrain + the motor side of the power tree). This is a slice of the master list — see [`../_engineering/products_to_buy.md`](../_engineering/products_to_buy.md) for the full, union-of-all-phases BOM with cost summary. Prices are ballpark USD, early 2026. "Why" gives the *electrical* reason the part exists.

## Core: brain + motor drivers

| Item | Qty | ~$ | Why (electrical reason) | Link | Status |
|---|---|---|---|---|---|
| Raspberry Pi Pico 2W (RP2350) | 1 | — | Real-time co-processor; sources four 20 kHz PWM channels + direction bits. 3V3(OUT) (≤300 mA) feeds the driver logic. Powered from laptop USB this phase. | (have) | already have (kit/shelf) |
| Pololu **TB6612FNG** dual H-bridge (#713) | 2 | $10 ($20) | 1.2 A continuous / **3.2 A peak per channel** → real headroom over the 310's 1.4 A stall; one motor/channel = 4 independent wheels for Mecanum. Replaces the DRV8833 (whose ~1.2–1.3 A sustainable limit sits on the stall knee). | https://www.pololu.com/product/713 | **buy** |
| "310" metal-gear motors w/ Hall encoders (JGA27-310 / MD310Z20) | 4 | — | The drive wheels: 7.4 V, ≤0.65 A run / **≤1.4 A stall**, 1:20 gearbox. Motor leads used now; encoder conductors parked until Phase 2. | (have) | already have (kit) |
| **JST-PH 2.0 6-pin pigtail leads** | 4 | $6 | Mate the motors' *factory* keyed 6-pin plug (motor power + encoder). Keeps a reliable connector instead of 24 mis-wireable bare ends. | [https://www.amazon.com/s?k=jst+ph+2.0+6+pin+pigtail](https://www.amazon.com/10Pair-JST-PH-Connector-Female-Connection/dp/B09JP1YJWL/ref=sr_1_11?dib=eyJ2IjoiMSJ9.xe7Cfsp2Tp5VFOyq4HCqHYYdTXJfcipvDBcbflDbX2IM-XbStIY5MzUWqv1KuX2jIVrN1gnD8u5TOicz1X3mkCE06W0epu24KqWWgmw2WB64L-OAuNXmrVLDyonzJJO6RKEwS0wlXeXSr8rQLELfX6KLoXhkAA4x9gxqLwdeMze7HTIX4SahlAnmi2qSbC_g2mmD_t1reE-ZsC0_XE6wXMiSWM3P3AjUXBQ9kgLwEAE.RzozxCARmtMnuFo7wF3_YhGMZGv1rmV1bSKPey1E3zo&dib_tag=se&keywords=jst%2Bph%2B2.0%2B6%2Bpin%2Bpigtail&qid=1780095193&sr=8-11&th=1) | **buy** |

## Power & protection (motor side of the tree)

| Item | Qty | ~$ | Why (electrical reason) | Link | Status |
|---|---|---|---|---|---|
| 2S LiPo 7.4 V 2200 mAh 10C | 1 | — | The single energy source. 10C × 2.2 Ah ≈ 22 A capable — far above the ~5.6 A all-stall motor load. | (have) | already have (kit) |
| **XT60 connector pair** + pigtails | 1 set | $5 | Keyed, current-rated main battery connector (won't reverse-mate). | https://www.amazon.com/s?k=xt60+connector+pigtail | **buy** |
| Pololu **#2815** MOSFET slide switch | 1 | $8 | Master disconnect **+ reverse-polarity protection** in one P-FET part — no diode voltage drop/heat. | https://www.pololu.com/product/2815 | **buy** |
| Inline **ATC/ATO fuse holders** (18 AWG) + **10 A / 7.5 A** blades | 2 (+5 A later) | $10 | Coordinated fusing: 10 A main at the battery, 7.5 A on the motor branch (selectivity — branch < main). Fuses protect the *wires*. | https://www.amazon.com/dp/B0DT4NCD5V | **buy** |
| **22 mm latching mushroom e-stop, 1× NC** | 1 | $8 | Instant physical motor kill in series in the motor branch; **NC** = a broken wire also trips it (fail-safe). | https://www.amazon.com/dp/B00W947PS0 | **buy** |
| Bulk electrolytics **470–1000 µF, ≥16 V**, low-ESR/105 °C | ~2 (of 4) | $8 | Absorb the millisecond di/dt when motors start/stall so the 7.4 V VM rail doesn't sag. (No Adafruit SKU meets ≥16 V at this capacitance — source from DigiKey/Mouser.) | https://www.digikey.com/en/products/filter/aluminum-electrolytic-capacitors/58 | **buy** |
| **0.1 µF ceramic caps** | 4 (of 12) | $3 | Brush-noise suppression soldered across each motor body (prevents USB-link crashes later). | https://www.adafruit.com/product/753 | **buy** |
| **2S balance-port low-voltage alarm/buzzer** | 1 | $5 | Dumb hardware backstop against LiPo over-discharge; works even if firmware crashes. | https://hobbyking.com/en_us/cell-checker-with-low-voltage-alarm-2s-8s.html | **buy** |

## Wiring, tools & instrumentation

| Item | Qty | ~$ | Why (electrical reason) | Link | Status |
|---|---|---|---|---|---|
| **Silicone stranded wire**, 18 AWG (power) + 22 AWG (signal) | 1 set | $12 | 18 AWG safely carries the ~5.6 A all-stall motor current in short runs; 22 AWG for logic. | https://www.amazon.com/s?k=silicone+wire+18+awg+22+awg+kit | **buy** |
| **Ferrule kit + ratcheting ferrule crimp tool** (IWISS HSC8-class) | 1 | $25 | Solid, low-resistance terminations (bare stranded wire works loose; solder cold-flows under clamp). | https://www.amazon.com/s?k=wire+ferrule+crimping+tool+kit+ratcheting | **buy** |
| **Dupont jumper wires** (M/M #758; + M/F #826, F/F #266 as needed) | 1 | $8 | Short 3.3 V *signal* jumps only (Pico → driver inputs). Never motor power. #758 is Male/Male; add M/F or F/F packs to match your headers. | https://www.adafruit.com/product/758 | **buy** |
| **Full-size breadboard** | 1 | $5 | Phase-1/2 *signal* bring-up only. Motor power never touches it. | https://www.adafruit.com/product/239 | **buy** |
| **M2.5 nylon standoff kit** | 1 | $10 | Pi/Pico/breakouts are M2.5 (not M3); nylon = non-conductive + keeps ferrous mass off the (later) magnetometer. | https://www.cytron.io/p-nylon-m2p5-pcb-standoff-kit-for-raspberry-pi-200pcs | **buy** |
| **Heat-shrink tubing** + **wire labels** | 1 | $8 | Insulate the parked encoder conductors; label FL/RL/FR/RR motor + battery leads. | https://www.amazon.com/s?k=heat+shrink+tubing+assortment | **buy** |
| **8-ch 24 MHz USB logic analyzer** (sigrok/PulseView) | 1 | $13 | See the 20 kHz PWM duty change with command — the Measurement Lab for this phase. | https://www.amazon.com/dp/B0BQ7S16H7 | **buy** (recommended) |
| **Digital multimeter** (continuity + DC volts) | 1 | $25 | Every wire gets a beep test before power-on; measure VM = 7.4 V. Skip if you own one. | https://www.amazon.com/s?k=digital+multimeter | **buy** (if needed) |

> Quantities like "2 of 4" mean this phase uses part of a multi-pack you buy once (the other caps land in later phases). The **5 A fuse**, **USB-A→micro-USB cable**, **D24V50F5 buck**, **UBEC**, IMU, PCA9685, camera, and lidar adapter are introduced in their own later-phase BOMs.
