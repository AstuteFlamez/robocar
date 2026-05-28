# Parts List

Order everything before starting. Trying to source parts mid-build will stall you for days.

## Already have

| Item                      | Qty | Notes                                            |
|---------------------------|-----|--------------------------------------------------|
| Raspberry Pi 5            | 1   | 4 GB or 8 GB both fine                           |
| Pi Pico 2W                | 1   | With pre-soldered or solderable headers          |
| JGA27-310R-74 DC motor    | 4   | 7.4V, 450 RPM, with quadrature encoder           |
| 2S LiPo battery           | 1   | 7.4V nominal, 2000 mAh+ recommended              |
| USB power bank            | 1   | 5V, 3A+ output, USB-C preferred                  |
| LDROBOT STL-19P lidar     | 1   | 360° DTOF, 12m range                             |

## Need to buy — electronics

| Item                         | Qty | ~Price | Notes / suggested vendor                       |
|------------------------------|-----|--------|------------------------------------------------|
| DRV8833 dual H-bridge module | 2   | $5 ea  | Pololu, Adafruit #3297, SparkFun, or generic   |
| BNO055 IMU breakout          | 1   | $20    | Adafruit #4646 is the reference; clones work   |
| Pi Camera v3                 | 1   | $25    | Official, or v2 if v3 unavailable              |
| Pi Camera CSI cable, longer  | 1   | $5     | The default cable is short — get a 30–50cm one |
| Pan/tilt bracket             | 1   | $8     | Generic "SG90 pan tilt bracket", any vendor    |
| SG90 micro servo             | 2   | $3 ea  | Often included with pan/tilt bracket           |
| PCA9685 16-ch PWM driver     | 1   | $6     | Adafruit #815 or generic                       |
| USB-A to micro-USB cable     | 1   | $3     | For Pi 5 → Pico 2W; short (~20cm) is neater    |
| USB-A to USB-B (or whatever) | 1   | —      | Only if STL-19P came with USB adapter board    |

## Need to buy — chassis & mechanical

| Item                       | Qty | ~Price | Notes                                           |
|----------------------------|-----|--------|-------------------------------------------------|
| 4WD chassis kit            | 1   | $25–50 | Acrylic plates + motor mounts + wheels. Make sure motor mounts fit JGA27 (27mm motor, 4mm shaft) — measure before ordering |
| 4mm wheels (if not in kit) | 4   | $10    | 65mm diameter rubber wheels are standard        |
| M3 hardware kit            | 1   | $8     | Screws, nuts, standoffs of various lengths      |
| LiPo battery strap         | 1   | $3     | Velcro is fine, just secure the battery         |
| LiPo bag                   | 1   | $5     | For safe storage and charging                   |

## Need to buy — wiring & prototyping

| Item                       | Qty | ~Price | Notes                                           |
|----------------------------|-----|--------|-------------------------------------------------|
| Dupont jumper kit          | 1   | $8     | Mix of M-M, M-F, F-F, ~20cm                     |
| Breadboard, full-size      | 1   | $5     | For prototyping; we'll move to perfboard later  |
| Breadboard, half-size      | 1   | $3     | Optional, useful for the I2C bus hub            |
| Stranded silicone wire     | 1   | $10    | 22 AWG for signals, 18 AWG for motor power      |
| Heat-shrink tubing assortment | 1| $8     | For motor wires, battery leads                  |
| 0.1 µF ceramic capacitor   | 12  | $3     | Solder across motor terminals (noise suppression) |
| JST connectors (XT60, XT30, or similar) | 2 | $5 | Whatever your LiPo uses; one set for battery cable |
| Power switch (rocker, 10A) | 1   | $3     | Wired between LiPo and rest of system           |
| Inline fuse (5A automotive)| 1   | $3     | Between LiPo and switch — critical safety item  |

## Recommended tools

| Item                       | Notes                                              |
|----------------------------|----------------------------------------------------|
| Soldering iron (60W+)      | Adjustable temp, ~$30 (TS100 / Pinecil / Hakko)    |
| Solder, 60/40 or lead-free | Thin (0.5–0.8 mm) for board work                   |
| Wire cutters & strippers   | Cheap is fine                                      |
| Multimeter                 | Cheap one is fine. Continuity + voltage = enough   |
| Helping-hands or PCB holder| Optional but life-changing                         |
| Hot glue gun               | For securing wires & camera                        |
| LiPo charger (balance)     | $20–50. Don't skip this — non-balance chargers can damage cells |

## Software (free)

| Item                        | Notes                                                     |
|-----------------------------|-----------------------------------------------------------|
| Raspberry Pi OS (64-bit)    | Bookworm or later. Use Pi Imager                          |
| Thonny IDE                  | Easiest way to flash MicroPython on the Pico              |
| MicroPython for Pico 2W     | Latest .uf2 from micropython.org                          |
| (Optional) ROS 2 Jazzy      | If you want to use ROS for SLAM/nav. Heavy install.       |
| Python packages             | flask, opencv, picamera2, adafruit-circuitpython-bno055, adafruit-circuitpython-pca9685, numpy, pyserial — installed per-phase |

## Estimated total cost

Roughly **$120–180** in new parts on top of what you already have, depending on chassis and tool choices. The chassis is the biggest variable.

## A note on the LiPo

A 2S LiPo at 7.4V has real fire potential if shorted or punctured. Three non-negotiable habits:

1. Always charge with a balance charger
2. Never leave it charging unattended  
3. Store in a LiPo bag at ~50% charge if not using for >a week

The inline fuse on the parts list above is your friend — it'll blow before the wires light up if anything goes wrong.
