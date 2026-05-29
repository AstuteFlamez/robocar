# Phase 4 — Bill of Materials

Parts **introduced or first-used in Phase 4** only. This is a slice of the master list — see [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md) for the full union of every phase, with the cost summary and the "verify on YOUR kit" notes.

Both new sensors are tiny loads (BNO055 ~12 mA, INA219 chip ~few mA) on the Pi's clean 3.3 V logic rail — no power-budget impact. The engineering payoff is all in *transport choice* and *address hygiene*, not amps.

## Buy / already-have

| Item | Qty | ~$ | Why (electrical reason) | Link |
|---|---|---|---|---|
| **BNO055** 9-axis IMU breakout (Adafruit #4646 or clone) | 1 | $20 | **buy** — on-chip ARM fusion → drift-free absolute heading; wired in **UART mode (PS1=HIGH)** at 115200 to dodge the Pi's I²C clock-stretch bug that corrupts orientation bytes. VIN = 3.3 V, ~12 mA. | https://www.adafruit.com/product/4646 |
| **INA219** current+voltage sensor (Adafruit #904) | 1 | $10 | **buy** — high-side shunt lets the Pi read pack V & I over I²C-1 for a power dashboard + graceful low-voltage park/halt. **Set addr 0x41** (0x40 collides with the Phase-5 PCA9685). ~3.2 A shunt ceiling (monitor, not stall sensor). | https://www.adafruit.com/product/904 |
| **M2.5 nylon standoff kit** | (from kit) | $10 | **already have (Phase 1)** — non-conductive, keeps ferrous mass off the magnetometer; mounts the IMU ≥ 5 cm from motors/LiPo. | https://www.cytron.io/p-nylon-m2p5-pcb-standoff-kit-for-raspberry-pi-200pcs |
| **Dupont jumper assortment** (M-F) | (from kit) | $8 | **already have (Phase 1)** — short 3.3 V signal jumps for UART (4 wires) + I²C (4 wires). Never motor power. | https://www.adafruit.com/product/758 |

## Optional — deeper-learning add-ons (tagged Phase 4)

| Item | Qty | ~$ | Why (electrical reason) | Link |
|---|---|---|---|---|
| Adafruit **ICM-20948** 9-DOF IMU (#4554) | 1 | $15 | **optional** — "write your own fusion" track: no clock-stretch issue, shares I²C-1. Validate your complementary/Kalman filter against the BNO055's fused output. | https://www.adafruit.com/product/4554 |
| STEMMA-QT / Qwiic breakout hub | 1 | $5 | **optional** — keyed, reliable I²C fan-out for INA219 (+ PCA9685 in Ph5), fewer Dupont failures from vibration. | https://www.adafruit.com/product/4209 |

> The logic analyzer + multimeter from Phase 1's instrumentation list are reused here (UART framing, I²C address scan, INA219 sanity-check) — not re-bought.

**Full list:** [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).
