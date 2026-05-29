# Phase 7 — Bill of materials

> **No new parts — all software.** Phase 7 (SLAM mapping) runs entirely on the
> Raspberry Pi 5 you already own and reuses every sensor, wire, and rail from
> Phases 1–6. No new components, no new wires, no new BOM lines. **New purchases
> this phase: $0.**

## Software dependencies (the only "shopping list" this phase)

SLAM is a *library call*. Instead of parts, what you "acquire" this phase is a
handful of Python packages, all free and all running on the Pi 5. Install them once
on the Pi:

| Dependency | Install | Why it exists this phase |
|---|---|---|
| **BreezySLAM** | `git clone https://github.com/simondlevy/BreezySLAM` → `cd BreezySLAM/python` → `sudo python3 setup.py install` | The SLAM engine itself: a thin Python wrapper over a fast C core. Provides `RMHC_SLAM` (Random-Mutation Hill-Climbing scan matcher) and the `Laser` sensor model. Real-time on a Pi 5 and trivial to integrate — the right first stop before heavier ROS-native stacks. |
| **numpy** | `pip3 install numpy` (usually already present) | Vectorized math for the occupancy-grid byte array, the odometry deltas, and the divergence-plot logging. BreezySLAM leans on it for the map buffer. |
| **Pillow (PIL)** | `pip3 install pillow` | Turns the raw occupancy-grid bytes into a grayscale PNG (`Image.frombytes('L', …)`) that the web UI fetches at `/map`. |
| Flask | already installed in Phase 6 | Serves the new `/map` PNG and `/slampose` endpoints next to the existing telemetry/camera/lidar routes. Not a new install — just new routes. |
| matplotlib *(host-side, optional)* | `pip3 install matplotlib` | Only for the offline odometry-vs-SLAM divergence plot (Task 8). Runs on your laptop against the logged CSV, not on the robot. |

> ★ **Verify the install before writing any loop code:**
> `python3 -c "from breezyslam.algorithms import RMHC_SLAM; print('BreezySLAM OK')"`
> If that import fails, nothing downstream will work — fix it first.

## Hardware this phase depends on (all already owned — confirm, don't buy)

SLAM doesn't *add* hardware, but it *consumes* the streams these parts produce. Each
row is the reason that part has to be working for a clean map. Confirm them; buy
nothing.

| Item | Qty | ~$ | Why it matters for SLAM | Status |
|---|---|---|---|---|
| Raspberry Pi 5 (16 GB) | 1 | — | The SLAM node, the web server, and the camera MJPEG stream all share its cores. Needs the full 5 V/5 A buck rail under load — `vcgencmd get_throttled` should stay `0x0`. | already have (kit) |
| LDROBOT STL-19P lidar (via CP2102) | 1 | — | Supplies the 360° scans SLAM matches against the map (~450 pts/rev @ 10 Hz, `/dev/ttyUSB0`). PWM pin → GND for steady 10 Hz spin. | already have (kit) + CP2102 (Phase 6) |
| Raspberry Pi Pico 2W | 1 | — | Streams wheel telemetry → holonomic odometry, the *other* SLAM input. Also enforces the **≤ 0.15 m/s** mapping-speed cap that prevents motion-blurred scans (Task 6). | already have (kit) |
| 4× "310" motor + Hall encoder (JGA27-310) | 4 | — | The encoders feed odometry; a loose JST-PH 6-pin plug or a wrong `COUNTS_PER_REV` corrupts the map. Encoders run at **3.3 V**, never 7.4 V. | already have (kit) |
| BNO055 9-axis IMU (Adafruit #4646) | 1 | $20 | Supplies the drift-free heading that becomes the `dθ` term in the `(dt, dxy, dθ)` SLAM odometry delta. Must be on **UART**, not I²C. | already bought (Phase 4) |

---

> This is a slice of the master BOM. For the complete parts list with every link,
> price, and the full power-and-protection tree, see
> **[`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md)**.
