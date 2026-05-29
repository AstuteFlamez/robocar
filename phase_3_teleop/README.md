# Phase 3 — Pi↔Pico Bridge, Mecanum Kinematics & WiFi Teleop

> **The phase where the robot becomes a *robot*.** Up to now you've had a Pico tethered to a laptop, holding wheel velocities. This phase cuts the laptop loose. The Pi 5 climbs onto the chassis, comes alive on its own battery-derived 5 V rail, takes command of the Pico over a single USB cable, and serves a web page you open on your phone. And — the headline — you teach the software the one thing a Mecanum chassis is *for*: **strafing.** By the end you'll push a joystick sideways on your phone and watch the whole robot slide left without rotating. That's holonomic motion, and a tank-drive robot physically cannot do it.

---

## Goal

The **Pi 5 — now mounted on the bot and powered from the LiPo** — drives the Pico over USB serial, and you drive the bot from a phone browser on the same WiFi, **including pure sideways strafing**. Three things change at once:

1. **Power comes online.** A Pololu **D24V50F5** buck converts the 7.4 V LiPo down to **5 V / 5 A** and feeds the Pi's USB‑C. The 5 V/3 A power bank is retired as the robot supply (it stays as a bench-only tether — see *Why the power bank had to go* below).
2. **The bridge goes live.** One **USB‑A → micro‑USB** cable runs from a Pi USB‑A port to the Pico's micro‑USB. That single cable carries *both* 5 V power (to the Pico's VBUS) and the bidirectional serial link. The Pico stops being a laptop peripheral and becomes the Pi's real-time co-processor.
3. **The math becomes Mecanum.** We replace the differential `v left right` protocol with a true 3‑DOF holonomic protocol `v vx vy wz`, and implement a **kinematics module** with both an **inverse** map (body twist → four wheel speeds) and a **forward** map (four wheel speeds → body twist). The web UI gets a translate joystick (vx/vy) plus a separate rotate control (wz).

---

## Difficulty & time

**Medium.** ~1 weekend. Roughly: half a day on the power tree + buck (do it carefully — this is the one part that can damage a Pi), half a day on the serial bridge and the kinematics module, and half a day on the Flask UI and the strafe-validation lab.

---

## Prerequisites

- **Phase 1 complete:** all four wheels spin under Pico control through the two TB6612FNG drivers; the LiPo power tree (10 A main fuse, reverse-polarity/master switch, e-stop, motor-branch 7.5 A fuse) is built and the bot drives open-loop from the Pico REPL.
- **Phase 2 complete:** the Pico reads all four encoders via PIO quadrature decode, you've **measured `COUNTS_PER_REV` empirically** (calculated 1040, but trust the bench), and each wheel holds a commanded angular velocity with a per-wheel PID loop.
- **Pi 5 prepared off-bot:** flashed with Raspberry Pi OS (Bookworm, 64-bit), joined to your WiFi, SSH enabled, and you can `ssh` into it from your laptop. Do all the OS setup on the bench-only power bank *before* you mount it and switch to buck power.

---

## New parts this phase

| Part | One-line reason |
|---|---|
| Pololu **D24V50F5** 5 V/5 A buck regulator (#2851) | Converts the 7.4 V LiPo to a clean 5 V/5 A the Pi 5 actually needs (the power bank can't). |
| **5 A blade fuse** + inline ATC/ATO holder (Pi-buck branch) | Protects the Pi branch so a buck/Pi fault can't take down the motor branch (selectivity). |
| **USB‑A → micro‑USB cable**, short (~20 cm), **data-capable** | The Pi↔Pico "spinal cord": 5 V to the Pico's VBUS + the serial bridge in one cable. |

> Everything else (chassis, motors, encoders, TB6612 ×2, LiPo, fuses, switch, e-stop, Pi 5, Pico 2W) is already on the bot from Phases 1–2. Full master list: [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).

---

## Where we are now — the cumulative system in prose

After Phases 1 and 2, the **Pico 2W** is a competent little motor brain. It runs two **TB6612FNG** dual H-bridge drivers (one motor per channel, four channels for four wheels — exactly what Mecanum requires). It throws **20 kHz PWM** at them — above hearing, so the motors don't whine — with **IN1/IN2 setting direction and PWM setting speed** in TB6612 standard mode. Both drivers' **STBY** pins are tied high to GP14, because a TB6612 with STBY low is a brick: its outputs stay disabled and the motors do nothing. The drivers' **VM** comes from the 7.4 V LiPo motor rail (through the 7.5 A motor-branch fuse and the normally-closed e-stop); their **VCC logic rail and all four encoder VCCs** come from the Pico's **3V3(OUT)** pin, whose ~300 mA budget the whole logic load sits comfortably under. Each motor's **Hall encoder** feeds an A/B quadrature pair into the Pico's **PIO state machines** (one per wheel, four of twelve used), and a 50 Hz PID loop turns "0.3 m/s commanded" into "0.3 m/s on the floor" regardless of friction or battery sag.

What's been missing is a *commander*. Until now the laptop's USB REPL has played that role. **This phase installs the real commander: the Pi 5.** It rides on the chassis, draws its 5 V from the LiPo through the D24V50F5 buck, and speaks to the Pico over the USB bridge. The motor power rail (7.4 V) and the logic/compute rails (5 V Pi, 3.3 V Pico) remain **separate, joined only at the common-ground star point** at the LiPo negative — never back-feed 7.4 V into the Pico, and never bridge the buck's 5 V into the Pico's 3V3.

At the end of this phase the robot is **fully self-contained and drivable from a phone**, and it moves the way a Mecanum chassis is supposed to: forward/back, strafe left/right, and rotate — independently and simultaneously.

---

## Pin tables (from the canonical map)

Nothing is *rewired* this phase — the bridge is a USB cable and a power cable, not GPIO. These tables are the cumulative state you're commanding through. (Authoritative source: [`_engineering/device_contracts.md`](../_engineering/device_contracts.md) → *Canonical pin assignments*.)

### Pico 2W — motor control (2× TB6612FNG, standard mode: IN1/IN2 = direction, PWM = speed)

| Wheel | Board · channel | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | Board A · ch A | GP2 | GP3 | GP4 |
| RL | Board A · ch B | GP5 | GP6 | GP7 |
| FR | Board B · ch A | GP8 | GP9 | GP10 |
| RR | Board B · ch B | GP11 | GP12 | GP13 |
| **STBY** (both boards, tied together) | — | **GP14 → must be HIGH** | | |

### Pico 2W — encoders (PIO quadrature; A/B on *adjacent* GPIOs)

| Wheel | A (base pin) | B |
|---|---|---|
| FL | GP16 | GP17 |
| RL | GP18 | GP19 |
| FR | GP20 | GP21 |
| RR | GP26 | GP27 |

- **Encoder VCC** (all 4) ← Pico **3V3(OUT)** — **3.3 V, not 7.4 V**, so the A/B outputs stay inside the Pico's 3.3 V GPIO limit.
- **Encoder GND** (all 4) → common-ground star.
- **Pico power:** VBUS ← **Pi USB 5 V** (this phase's new cable). 3V3(OUT) feeds both TB6612 VCC rails + the four encoder VCCs.

### Pi 5 — power & the bridge (new this phase)

| Function | Connector | Source / device |
|---|---|---|
| Pi 5 main power | USB‑C | ← **D24V50F5 buck, 5 V/5 A** (was: power bank) |
| Pico bridge | USB‑A (any of 4) → Pico micro‑USB | 5 V to Pico VBUS **+** serial link → `/dev/ttyACM*` |

> The Pi's 40-pin I²C/UART/CSI buses don't come online until Phases 4–6. The Pi's own 5 V header pins (2, 4) stay **unused** in this build — servo and sensor rails never touch the Pi rail.

---

## The big idea this phase: where computation lives, and how the two brains talk

A recurring theme of this whole project is **deciding where each computation should run.** A Linux box (the Pi 5) is wonderful at "soft real-time, compute-heavy" work — vision, mapping, web serving, WiFi — but the OS scheduler can preempt your code for *milliseconds* at unpredictable times. That's fatal for a 20 kHz PWM edge or a 17 kHz aggregate stream of encoder transitions. A microcontroller (the Pico) is the opposite: it has no scheduler stealing cycles, so it is **deterministic**, but it has no business running SLAM. So you split the work along that seam, and the two halves talk over a **serial link**.

**Why text-line serial and not JSON/binary/msgpack?** Because a line-delimited ASCII protocol is debuggable with `cat`, `screen`, or `print()`, readable on a logic analyzer at a glance, and trivial to parse on both ends. JSON and binary framing are real tools, but they're optimizations you reach for only after you've shipped one boring, working version. We send human-readable lines at human-readable rates (commands ≤ ~20 Hz, telemetry 20 Hz) where every byte is visible.

### Serial protocol — Pi 5 ↔ Pico 2W (CHANGED for Mecanum)

The Phase 2 protocol was differential: `v left right`. **That is wrong for a Mecanum chassis** — it can only ever express "drive + turn," never "strafe." We replace it with a **3‑DOF body-twist** protocol.

**Pi → Pico (commands), one line each, `\n`-terminated:**
```
v <vx> <vy> <wz>\n     # body twist: vx fwd (m/s), vy left (m/s), wz CCW (rad/s)
s\n                    # stop  (equivalent to: v 0 0 0)
ping\n                 # liveness check
```

**Pico → Pi (telemetry @ 20 Hz):**
```
t <ms> <fl> <rl> <fr> <rr>\n   # monotonic ms, then the 4 measured wheel ang. speeds (rad/s)
o <vx> <vy> <wz>\n             # FORWARD-kinematics body twist computed from the 4 wheels (odometry)
pong\n                         # reply to ping
ok\n                          # ack of last command  (err\n on parse failure)
```

**Sign conventions (lock these down once, everywhere):** robot frame, **x forward, y to the robot's left, z up, ωz positive counter-clockwise** (the standard right-handed robotics convention). Pick it now and the kinematics signs, the IMU heading in Phase 4, and the SLAM map in Phase 7 all agree.

> ★ **Canonical wheel order — `FL, RL, FR, RR` — is used *everywhere*:** the GPIO pin table, the four PID setpoint/measured arrays, the `t` telemetry line above, and the `inverse()`/`forward()` tuples below. If any one array is in a different order, a pure-`vy` strafe silently turns into a drive-or-spin and odometry is wrong. One order, no exceptions.

### The fail-safe watchdog (keep it — it's a safety layer)

The Pico expects a fresh `v` command at least **every 0.5 s**. If none arrives — Pi crashed, WiFi dropped, USB wiggled loose — the Pico **forces the body twist to zero and coasts the wheels.** This is one of several stacked safety layers (see [`_engineering/safety.md`](../_engineering/safety.md)): the *default* state is "stopped," so any failure decays to safe. It pairs with the hardware e-stop in the motor branch — software watchdog for comms loss, your hand on the mushroom button for everything else.

---

## Mecanum kinematics — from geometry to a 4×3 matrix

This is the conceptual heart of the phase, so we'll build it from the ground up.

### What makes a wheel "Mecanum," and why it can strafe

A normal wheel exerts force only *along its rolling direction* and resists sideways motion (that's why a car can't slide sideways — it's **non-holonomic**, it can't instantaneously move in every direction). A **Mecanum wheel** is a hub studded with passive rollers mounted at **45°** to the wheel's axle. When the wheel turns, it pushes the ground along its rolling direction *as usual*, but the angled rollers freely let the contact point slip along the 45° line. The net ground-force from one wheel therefore points at **45°**.

Mount four of them so the roller angles form an **"X" pattern** when viewed from above (the FL and RR rollers along one diagonal, FR and RL along the other), and now you have four 45° force vectors you can mix. Spin them in the right combination and the sideways components reinforce while the forward components cancel → the whole robot **strafes**. Spin another combination and you get pure rotation, or pure forward, or any blend. A Mecanum robot is **holonomic**: it can command vx, vy, and ωz independently. That's the entire point of the chassis, and modeling it as left/right differential throws it away.

### Inverse kinematics (the one your driving code uses)

We want: given a desired **body twist** (vx, vy, ωz), what angular speed must each of the four wheels spin? Two geometric facts do all the work:

- **Wheel radius `r`** converts a wheel's angular speed ω (rad/s) to the linear speed at its rim, `v = ωr`.
- **Lever arm `L = lx + ly`**, the sum of the half-length (axle-to-axle front/back, ÷2) and half-width (left/right track, ÷2). This is how a body rotation ωz turns into a tangential speed demand at each corner wheel.

For the standard X-pattern Mecanum layout with the x-forward / y-left / ωz-CCW convention, the inverse map is:

```
ω_FL = (1/r) · ( vx − vy − L·ωz )
ω_RL = (1/r) · ( vx + vy − L·ωz )
ω_FR = (1/r) · ( vx + vy + L·ωz )
ω_RR = (1/r) · ( vx − vy + L·ωz )
```

Read the **sign matrix** (rows in canonical FL, RL, FR, RR order) and the physics jumps out:

| | vx (forward) | vy (left) | ωz (CCW) |
|---|---|---|---|
| **FL** | + | − | − |
| **RL** | + | + | − |
| **FR** | + | + | + |
| **RR** | + | − | + |

- **vx column is all `+`** → pure forward spins all four wheels the same way. (Sanity-checks instantly.)
- **vy column is `−,+,+,−`** → that's the strafe: diagonally-opposed wheels cooperate, lateral components add, forward components cancel (the X pattern).
- **ωz column is `−,−,+,+`** (the left pair one way, the right pair the other) → pure spin-in-place, same as a tank turn.

> ⚠️ **Those exact signs depend on which physical wheel is FL/FR/RL/RR *and* on which roller diagonal each one carries.** If your kit's wheels are arranged differently, a pure-vy command will rotate or drive instead of strafe. That's exactly what the Measurement Lab below catches — and the fix is to permute/flip rows here, **never** to hack it in the UI.

### Forward kinematics (odometry — for telemetry and Phase 4+)

Invert the relationship to recover the body twist from the four *measured* wheel speeds. Because there are 4 wheels and only 3 DOF, it's a least-squares-style average (the redundant 4th wheel is what lets you later detect slip):

```
vx = (r/4) · ( ω_FL + ω_RL + ω_FR + ω_RR )
vy = (r/4) · ( −ω_FL + ω_RL + ω_FR − ω_RR )
ωz = (r/4) · ( −ω_FL − ω_RL + ω_FR + ω_RR ) / L
```

The Pico computes this every cycle from real encoder data and ships it as the `o vx vy wz` telemetry line. Integrate that twist over time (Phase 4, fused with the IMU) and you get the bot's (x, y, heading) — the foundation for SLAM.

---

## Why the power bank had to go (the realized power budget)

The original plan powered the Pi from a **5 V/3 A USB power bank.** Two hard facts kill that for a Pi 5 (full derivation in [`_engineering/power_budget.md`](../_engineering/power_budget.md)):

1. **The Pi 5 wants to see 5 V / 5 A.** On any supply that can't *prove* it delivers 5 A, the Pi's PMIC **caps all USB ports to 600 mA combined** and throws undervoltage warnings. Just the lidar (290 mA) plus a depth cam (400 mA) already blow past 600 mA — and we haven't added the Pico's draw.
2. **3 A isn't enough current anyway.** The 5 V rail's realistic worst case (Pi under SLAM + camera + USB) is **~4.7 A.** A 3 A bank sags below the Pi's **4.63 V** undervoltage line, the Pi throttles, drops USB devices, and reboots — mid-drive.

The fix is the **D24V50F5: 6–38 V in (7.4 V is in range, and it still holds 5 V even when the pack sags to 6.0 V empty), 5 V / 5 A out, ~90 % efficient,** with built-in reverse-voltage protection and soft-start. At full Pi load it pulls **~3.5 A from the 7.4 V pack** (23.5 W ÷ 0.9 ÷ 7.4 V — converters trade voltage for current, they aren't free). Setting **`usb_max_current_enable=1`** in `/boot/firmware/config.txt` tells the Pi "the rail really can supply the current, stop capping the ports." We add a **5 A fuse on the Pi branch** so a Pi/buck fault opens *that* branch without touching the 7.5 A motor branch or the 10 A main — that's **selectivity** (branch fuses smaller than the main).

The retired power bank isn't trash — it's a fine **tethered bench supply** for early OS setup before the buck is wired.

---

## Software task list

> Repo layout suggestion: Pico firmware in `pico/` (`main.py`, `motor.py`, `encoder.py`, `pid.py`, `kinematics.py`); Pi code in `pi/` (`pico_link.py`, `kinematics.py`, `webserver.py`, `static/index.html`). The kinematics constants (`r`, `L`) live in one place each side and must agree.

> **↪ Buildable C kinematics.** The inverse/forward maps below are realized as compilable Pico-SDK C in [`../firmware/src/mecanum_kinematics.c`](../firmware/src/mecanum_kinematics.c) — numerically verified **sign-for-sign identical** to the Python `inverse()`/`forward()` here. The `inverse()` sign matrix was additionally cross-validated against the working vendor controller's matrix; `forward()` has no vendor counterpart (the vendor firmware carries no odometry), so it is round-trip-verified against robocar's own `inverse()` instead. ⚠️ In the C, `L = lx + ly` uses **half** dimensions; the vendor's lumped lever is `2·(lx+ly)`, so copying its raw number would *double* the rotation gain. Measure `R` and `L` in the lab. See [`../firmware/README.md`](../firmware/README.md).

### A. Pico firmware — add kinematics + new protocol

1. **`kinematics.py` (Pico):** pure functions, no hardware.
   ```python
   import math
   WHEEL_RADIUS = 0.0485   # m  — MEASURE your wheel; Mecanum wheels are often ~97 mm OD
   L = 0.150               # m  — lx + ly (half-wheelbase + half-track). MEASURE your chassis.

   def inverse(vx, vy, wz):
       """Body twist (m/s, m/s, rad/s) -> 4 wheel angular speeds (rad/s), canonical order [FL, RL, FR, RR]."""
       r = WHEEL_RADIUS
       wfl = (vx - vy - L*wz) / r
       wrl = (vx + vy - L*wz) / r
       wfr = (vx + vy + L*wz) / r
       wrr = (vx - vy + L*wz) / r
       return wfl, wrl, wfr, wrr   # FL, RL, FR, RR — same order as the PID arrays and the t-line

   def forward(wfl, wrl, wfr, wrr):
       """4 measured wheel angular speeds (rad/s), order [FL, RL, FR, RR] -> body twist (vx, vy, wz)."""
       r = WHEEL_RADIUS
       vx = (r/4.0) * ( wfl + wrl + wfr + wrr)
       vy = (r/4.0) * (-wfl + wrl + wfr - wrr)
       wz = (r/4.0) * (-wfl - wrl + wfr + wrr) / L
       return vx, vy, wz
   ```

2. **Command parser (replace differential):** on `v vx vy wz`, run `inverse()` to get four wheel setpoints, feed them to the four Phase-2 PID loops, and **reset the watchdog counter.** On `s`, set the twist to (0, 0, 0). On `ping`, reply `pong`.

3. **50 Hz control loop:** read encoders → wheel speeds → PID → PWM, and **decrement the watchdog; if it hits 0, force the twist to (0,0,0)** (coast). Non-blocking stdin so the loop never stalls waiting for a command:
   ```python
   import select, sys
   poll = select.poll()
   poll.register(sys.stdin, select.POLLIN)
   # inside the loop:
   if poll.poll(0):                  # 0 ms timeout = non-blocking
       line = sys.stdin.readline()
       if line:
           handle_command(line.strip())
   ```

4. **20 Hz telemetry:** each cycle, compute `forward(...)` from the measured wheel speeds and emit **one** `t ...` line and **one** `o ...` line. Keep it to one print per stream per cycle — flooding the USB-CDC buffer is what stalls the loop (see pitfalls).

### B. Pi 5 — serial bridge

5. **Find the stable device name** (don't hardcode `/dev/ttyACM0` — it renumbers across reboots):
   ```bash
   ls -l /dev/serial/by-id/
   # -> /dev/serial/by-id/usb-MicroPython_Board_in_FS_mode-if00   (use THIS path)
   ```

6. **`pico_link.py`:** a `pyserial` wrapper. Open the by-id path; run a **background reader thread** that continuously drains incoming lines into the latest-telemetry dict (so the Pico's output buffer never backs up). Expose:
   ```python
   class PicoLink:
       def __init__(self, device, baud=115200): ...
       def set_twist(self, vx, vy, wz): ...   # writes "v {vx} {vy} {wz}\n"
       def stop(self): ...                     # writes "s\n"
       def get_telemetry(self) -> dict: ...    # latest {wheels:[...], odom:{vx,vy,wz}, t_ms}
   ```

### C. Pi 5 — Flask web server + holonomic UI

7. **`webserver.py` (Flask):**
   - `GET /` → serves the holonomic control page.
   - `POST /cmd` → JSON `{"vx":0.3,"vy":0.0,"wz":0.0}` → `pico_link.set_twist(...)`.
   - `GET /telemetry` → latest telemetry as JSON (for an on-screen readout).
   - **Rate-limit the browser to ~20 Hz** commands (not one per touch event), and **send `v 0 0 0` on joystick release** so a stuck UI can't run the bot away.

8. **Holonomic UI** — *two* controls, because we have *three* DOF:
   - A **translate joystick** (nipplejs is a tiny drop-in): joystick **up → +vx**, **left → +vy** (match the sign convention!).
   - A **separate rotate control** (a horizontal slider or a second small joystick's x-axis) → **wz**.
   ```javascript
   // translate joystick (nipplejs): data.vector.y up = forward, data.vector.x right
   const vx =  data.vector.y * MAX_VX;     // up  -> +x (forward)
   const vy = -data.vector.x * MAX_VY;     // left -> +y (left)   (screen x grows right)
   // rotate slider in [-1,1]:
   const wz =  rotateSlider.value * MAX_WZ;  // left/CCW positive
   postCmd({vx, vy, wz});                    // throttled to ~20 Hz
   ```

9. **Serve it:** `python webserver.py` on port 5000; open `http://<pi-ip>:5000/` from your phone (find the IP with `hostname -I`, or use `http://<hostname>.local:5000/` via mDNS). Optional: a `systemd` unit so it starts on boot.

---

## Verification checklist

Do these **in order** — each assumes the previous passed. Wheels **in the air on a stand** until the very last step (a Mecanum bot can lunge *sideways*).

- [ ] **Buck output is correct *before* the Pi is connected.** LiPo armed, Pi USB‑C unplugged. Multimeter on the buck output: reads **5.0–5.1 V**. If not, stop — do not plug in the Pi.
- [ ] **Pi-branch fuse + reverse protection seated.** 5 A blade in the Pi branch; master switch gives reverse-polarity protection. Continuity ritual passed (no V+↔GND shorts).
- [ ] **Pi boots on buck power.** Disconnect the bench power bank; power the Pi from the buck's USB‑C only. It boots to login.
- [ ] **No undervoltage, idle.** `vcgencmd get_throttled` → **`0x0`**. Any other value means the rail is sagging.
- [ ] **No undervoltage, under load.** Run `stress-ng --cpu 4 &` (or start SLAM later), re-check `vcgencmd get_throttled` → still **`0x0`**. This is the proof the buck fixed the power problem.
- [ ] **Pico enumerates over the bridge.** `ls /dev/serial/by-id/` lists the Pico. If absent, your USB‑A→micro cable is **charge-only** — swap for a data cable.
- [ ] **Manual twist over serial.** `screen /dev/serial/by-id/<pico> 115200`, type `v 0.2 0 0` → all four wheels roll forward at the same speed. `v 0 0.2 0` → wheels assume the **strafe** pattern. `v 0 0 1.0` → spin in place. `s` → stop.
- [ ] **Telemetry streams at 20 Hz.** `t ...` and `o ...` lines arrive steadily; the `o` twist roughly matches what you commanded once steady.
- [ ] **Watchdog: stop on comms loss.** During a `v 0.2 0 0`, **yank the bridge cable** → wheels coast to a stop within ~0.5 s. Repeat by killing the Pi-side Python process — same result.
- [ ] **Web UI loads on phone.** `http://<pi-ip>:5000/` on a phone on the same WiFi shows the translate joystick + rotate control. (If it loads on the Pi but not the phone, it's the Pi firewall: `sudo ufw allow 5000`.)
- [ ] **Joystick drives the bot (in the air first).** Forward → forward; **sideways → strafe** (no rotation); rotate control → spin in place; release → stops.
- [ ] **Floor test.** On the ground, clear of feet and cables, drive a square *and a sideways slide*. Strafing on the floor is the moment the whole project pays off.

---

## Common pitfalls

- **Plugging the Pi into the buck before measuring it.** A miswired buck (or a swapped trim) can put battery voltage on the Pi's 5 V — instant death. *Always* meter the buck output first.
- **Forgetting `usb_max_current_enable=1`.** Even with a real 5 A buck, without this line the Pi may still cap USB to 600 mA and drop the Pico/lidar. Edit `/boot/firmware/config.txt`, reboot, re-check `get_throttled`.
- **`/dev/ttyACM0` keeps changing.** Use the stable `/dev/serial/by-id/...` path everywhere — the `ttyACM*` number is not stable across reboots or re-plugs.
- **Charge-only USB cable.** A surprising number of micro-USB cables carry power but no data lines; the Pico then powers up but never enumerates as serial. Test with a known data cable.
- **Telemetry print flooding stalls the control loop.** If the Pi isn't draining the serial fast enough, the Pico's USB-CDC buffer fills and `print()` blocks — your 50 Hz PID loop stutters. Keep telemetry to one line per stream per cycle, and run the Pi reader in its own thread.
- **Joystick "stuck off-center."** If a touch-release event is missed, the last twist keeps the bot moving. Always send `v 0 0 0` on release, and rely on the 0.5 s watchdog as the backstop.
- **Strafe goes diagonal or spins instead of sideways.** Almost always a **sign/row error in the inverse matrix** or a physically swapped wheel/roller-diagonal. Fix it in `kinematics.py` rows, never in the UI — the UI must stay a clean (vx, vy, wz) source.
- **Mixing up the differential code from Phase 2.** Delete the `v left right` path entirely. Leaving both protocols alive invites "why is strafe sometimes a turn" bugs.
- **Web UI works on the Pi, not the phone.** Pi firewall, or you bound Flask to `127.0.0.1`. Bind to `0.0.0.0` and `ufw allow 5000`.

---

## 🔬 Measurement Lab

> **Three measurements this phase: validate the kinematics signs, prove the power fix, and see the serial framing.**

1. **Validate the Mecanum sign matrix — one DOF at a time (the headline lab).**
   *Instrument:* your eyes + the bot on a stand, then on the floor.
   *Procedure & expected:* command **pure vx** (`v 0.2 0 0`) → bot drives **straight forward**, all four wheels same direction. Command **pure vy** (`v 0 0.2 0`) → bot **strafes left**, *no* forward motion and *no* rotation. Command **pure ωz** (`v 0 0 1.0`) → bot **spins in place CCW**, no translation. Each pure command must produce *exactly one* kind of motion. Any cross-coupling = a wrong sign/row in `inverse()` or a swapped wheel — this test is the empirical proof your kinematics matrix is correct.

2. **Prove the buck fixed the power problem.**
   *Instrument:* `vcgencmd get_throttled` on the Pi (and a multimeter on the buck output).
   *Expected:* **`0x0`** at idle *and* under CPU load (`stress-ng --cpu 4`), with the buck output holding **5.0–5.1 V**. A 5 V/3 A power bank fails this; the 5 A buck passes it. (`get_throttled` bit 0 = under-voltage now, bit 16 = under-voltage occurred since boot — you want all zeros.)

3. **Logic-analyzer the serial framing.**
   *Instrument:* the $13 8-channel USB logic analyzer (PulseView) on the Pi↔Pico data lines.
   *Expected:* clean **UART/CDC framing** — decode a `v 0.2 0 0\n` command and a `t ...\n` telemetry line, confirm 8N1 bytes, the `\n` (0x0A) terminator, and that telemetry lines arrive at ~20 Hz (50 ms apart). Seeing the actual bytes turns "it works" into "I measured the protocol."

---

## 🎓 Phase wrap-up

**What you learned.** You drew the architectural line between *deterministic* real-time control (Pico) and *compute-heavy* soft-real-time work (Pi), and connected them with a deliberately simple line-based serial protocol. You derived **Mecanum inverse and forward kinematics** from wheel geometry (radius `r`, lever arm `L = lx + ly`), implemented the 4×3 sign matrix, and *empirically validated the signs* by commanding one DOF at a time. You built a real **power tree**: a buck converter sized from the Pi's 5 V/5 A contract and a branch-fused, selectivity-coordinated distribution, and you proved it with `vcgencmd get_throttled`. And you kept a **fail-safe comms watchdog** so the robot coasts to a stop on any loss of command.

**Résumé bullet (copy-ready).**
> Built a two-processor mobile-robot control stack (Raspberry Pi 5 + RP2350 co-processor over USB serial); implemented 3-DOF **Mecanum inverse/forward kinematics** for true holonomic motion (forward, lateral strafe, and yaw), a phone-browser teleop UI, and a fail-safe command-timeout watchdog. Designed the LiPo→5 V/5 A buck power tree with coordinated fusing and verified zero supply throttling under load.

**Interview talking point.**
> *"Why is driving a Mecanum chassis as left/right differential wrong?"* — Because a Mecanum wheel's 45° rollers produce a force vector at 45°, and arranging four of them in an X pattern lets you independently command vx, vy, and ωz — the robot is **holonomic**. A differential model only has two DOF (drive + turn), so it can never express the lateral term; commanding "strafe" in a differential model is undefined, and worse, the *odometry* silently corrupts the instant the bot moves sideways because the model has no vy. The right model is a 4×3 inverse-kinematics matrix derived from `r` and `L = lx + ly`, which I validated by commanding pure vx, pure vy, and pure ωz one at a time and confirming each produced exactly one kind of motion.
