# Phase 8 — Bill of materials

> **Nothing to buy this phase.** Phase 8 is **pure software** — the autonomous-navigation stack runs entirely on the Raspberry Pi 5 and reuses every part you already installed in Phases 1–7. No new components, no new wires, no new BOM lines.

## Parts this phase depends on (all already owned — confirm, don't buy)

The navigation stack doesn't *add* hardware, but it *leans on* these parts harder than any earlier phase, because the robot now moves under its own control. Each row is the electrical reason that part has to be working for autonomy to work.

| Item | Qty | ~$ | Why (electrical reason it matters for autonomy) | Link | Status |
|---|---|---|---|---|---|
| Raspberry Pi 5 (16 GB) | 1 | — | Runs A*, Pure Pursuit, the safety filter, the state machine, and the web UI — all the new code lives here. Needs the full 5 V/5 A rail under SLAM + camera + planner load. | (kit / on hand) | **already have (kit)** |
| Raspberry Pi Pico 2W | 1 | — | Receives `v vx vy wz`, runs per-wheel PID, and enforces the 0.5 s comms watchdog — your last-resort software stop if the nav loop wedges. | (kit / on hand) | **already have (kit)** |
| 2× TB6612FNG driver (Pololu #713) | 2 | $10 | Turn the four wheel-speed commands into motor current; **STBY (GP14) must stay HIGH** or autonomy commands silently do nothing. | https://www.pololu.com/product/713 | already bought (Phase 1) |
| 4× "310" motor + Hall encoder (JGA27-310) | 4 | — | The encoders feed odometry → SLAM pose → the planner; a loose JST-PH plug corrupts the whole navigation chain. Run encoders at **3.3 V**, never 7.4 V. | (kit / on hand) | **already have (kit)** |
| BNO055 9-axis IMU (Adafruit #4646) | 1 | $20 | Supplies the drift-free heading that the holonomic controller's `ωz` and A*'s pose both rely on. Must be on **UART**, not I²C. | https://www.adafruit.com/product/4646 | already bought (Phase 4) |
| LDROBOT STL-19P lidar (via CP2102) | 1 | — | Feeds the SLAM map A* plans on **and** the live scan the safety bubble vetoes motion from. PWM pin → GND for steady 10 Hz spin. | (kit / on hand) + CP2102: https://www.waveshare.com/cp2102-usb-uart-board-type-a.htm | **already have (kit)** + CP2102 (Phase 6) |
| 22 mm latching NC mushroom e-stop | 1 | $8 | Your fastest stop during an autonomous run — faster than the web STOP button or the Pico watchdog. In the motor branch only. | https://www.amazon.com/dp/B00W947PS0 | already bought (Phase 3) |
| USB-A → micro-USB data cable (Pi↔Pico) | 1 | $4 | Carries every `v vx vy wz` command; must stay strain-relieved so an autonomous lunge can't unseat it. | https://www.amazon.com/s?k=usb+a+to+micro+usb+short+data+cable | already bought (Phase 3) |

**New purchases this phase: $0.**

## Optional capstone (still no required purchase)

The optional **ROS 2 bridge** (publish `/odom` and `/scan`, subscribe `/cmd_vel` → your Mecanum kinematics) is **software only** — it installs `rclpy`/ROS 2 packages on the Pi you already own. No hardware to buy.

---

> This is a slice of the master BOM. For the complete parts list with every link, price, and the full power-and-protection tree, see **`_engineering/products_to_buy.md`**.
