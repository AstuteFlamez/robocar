# Phase 5 — Bill of Materials (parts introduced / first used this phase)

This is the **slice** of the master BOM that this phase adds. For the full project parts list, links, prices, and cost summary, see [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).

| Item | Qty | ~$ | Why (electrical reason) | Link | Have/Buy |
|---|---|---|---|---|---|
| Raspberry Pi Camera Module 3 | 1 | $25 | CSI camera — streams MJPEG via picamera2 and stays **off the USB bus** (which the Pico + lidar need); light enough to ride the gimbal. | https://www.raspberrypi.com/products/camera-module-3/ | **buy** |
| Pi 5 CSI cable, 22-pin → 15-pin (Adafruit #5820, 300 mm) | 1 | $5 | ⚠️ The Pi 5 CSI port is **22-pin 0.5 mm**; the camera is 15-pin. A generic 15-to-15 "longer CSI cable" **will not physically fit** a Pi 5. | https://www.adafruit.com/product/5820 | **buy** |
| PCA9685 16-ch PWM driver (Adafruit #815 or generic) | 1 | $6 | Dedicated 12-bit hardware PWM over I²C at **0x40** — jitter-free servo timing off the Pi's OS and the Pico's real-time loop. **VCC=3.3 V logic, V+ from UBEC.** | https://www.adafruit.com/product/815 | **buy** |
| Hobbywing UBEC-3A (2–6S → 5 V/3 A) | 1 | $10 | Dedicated **5 V servo rail**: covers 2× LFD-01 ~1.5 A stall + 3–4 A inrush, **isolating it from the Pi** so the Pi never browns out. | https://www.hobbywingdirect.com/products/ubec-3a-2-6s-lipo-input | **buy** |
| LFD-01 servos + 2-DOF pan/tilt bracket | 2 + 1 | — | Pan (CH0) / tilt (CH1) actuators. 4.8–6 V → run at 5 V; ≤60 mA run, **700 mA stall each**. Built-in anti-stall firmware. | (from kit) | **already have (kit)** |
| MG90S metal-gear servo (*only if kit lacks LFD-01*) | 2 | $6 | Robust pan/tilt fallback. **Never SG90** (too weak — chatters under the bracket load). | https://www.adafruit.com/product/1143 | buy *(only if needed)* |
| Bulk electrolytic 470–1000 µF, ≥16 V, low-ESR/105 °C | 1 | — | Across PCA9685 **V+↔GND** to absorb the servo inrush di/dt so the 5 V servo rail doesn't sag on a fast slew. | https://www.adafruit.com/product/4267 | already have (Phase 1 kit) |
| Cable-tie mounts + zip ties + 3M VHB foam tape | 1 set | $10 | Mount the gimbal/camera with strain relief + vibration damping. **No hot glue** (gets brittle near a warm Pi). | https://www.adafruit.com/product/4248 | **buy** |
| Nuwa-HP60C USB depth camera (*optional*) | 1 | — | Optional body-fixed RGB/depth sensor over USB UVC. Counts ~0.4 A against the 5 V USB budget. **Don't buy** if not in kit. | (from kit, optional) | already have (kit) *(opt)* |

**Notes**
- The **LiPo, D24V50F5 buck, Pico, TB6612 drivers, INA219, BNO055, protection/fuses** were all introduced in earlier phases and are reused unchanged — they are not re-listed here.
- The **bulk cap** is the same 470–1000 µF part bought in the Phase 1 power kit; one of those caps lands on the servo V+ rail this phase.
- Full master list with cost summary and verification notes: [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).
