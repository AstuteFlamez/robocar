# Device Contracts

> **What this document is.** A *contract* is the promise each component makes about how it connects to the rest of the system: what voltage it wants, how much current it draws, what logic level its signals use, what physical connector it has, and what it is *allowed* to touch. If two parts' contracts don't match — a 5 V signal into a 3.3 V pin, a 1.5 A load on a 0.6 A rail, a JST plug that doesn't fit a header — the build fails, sometimes loudly (magic smoke) and sometimes silently (intermittent resets you chase for a week).
>
> This is the single source of truth for the whole project. Every per-phase BOM and wiring diagram pulls its numbers from here. If you change a part, change it here first, then ripple it outward.
>
> **Every number here was pulled from a manufacturer datasheet or product page and cross-checked.** Sources are listed per device. Nominal voltages are nominal — *measure with a multimeter before you connect anything.*

---

## How to read a contract

| Field | Meaning |
|---|---|
| **Supply (V_in)** | The voltage the device needs to be *powered*. |
| **I (run / peak)** | Typical running current, and worst-case peak or stall current. Peak is what sizes your wires, fuses, and regulators — not the average. |
| **Logic level** | The voltage of its *signal* pins (data, PWM, UART, I²C). A 3.3 V device and a 5 V device can't share signals without thought. |
| **Bus / addr** | Which communication bus it lives on, and its address if shared (I²C). |
| **Connector** | The physical plug/header. Two parts only connect if their connectors mate *and* their contracts agree. |
| **Talks to** | What it is wired to in this build. |

---

## The two brains

### Raspberry Pi 5 (16 GB) — *the high-level brain*
- **Role:** Linux SBC. Runs vision, SLAM, navigation, the web UI, WiFi. Talks to the Pico over USB.
- **Supply (V_in):** 5 V via USB-C. **Wants 5 V / 5 A (25 W).** On a supply that can't prove it does 5 A, the Pi **caps *all* USB ports to 600 mA combined** and throws undervoltage warnings. ← This single fact drives the entire power redesign. See [power_budget.md](./power_budget.md).
- **I (run / peak):** ~0.8 A typical active; **up to ~3.4 A** on the 5 V rail under heavy CPU+GPU+USB load (SLAM + camera + WiFi *is* heavy).
- **Logic level:** **3.3 V GPIO.** Not 5 V tolerant. Never drive a GPIO input above 3.3 V.
- **Undervoltage thresholds (fixed in the PMIC, can't change):** warning/throttle at **4.63 V**, protective shutdown ~4.25 V.
- **40-pin header pins we use:** 3.3 V = pins 1, 17 · 5 V = pins 2, 4 · GND = pins 6, 9, 14, 20, 25, 30, 34, 39 · I²C-1 SDA = pin 3 (GPIO2) · I²C-1 SCL = pin 5 (GPIO3) · UART = pin 8 (TXD/GPIO14), pin 10 (RXD/GPIO15).
- **Connector:** USB-C (power), USB-A ×4 (Pico, lidar adapter, optional depth cam), CSI/MIPI **22-pin 0.5 mm** (camera — *changed from the 15-pin connector on Pi 4*), 40-pin GPIO header.
- **Talks to:** Pico (USB), lidar via CP2102 (USB), PCA9685 + INA219 (I²C-1), BNO055 (UART), Camera Module 3 (CSI).
- **Powered by:** the 5 V/5 A buck (D24V50F5) — *not* a phone power bank. Set `usb_max_current_enable=1` in `/boot/firmware/config.txt`.
- **Source:** raspberrypi.com Pi 5 docs & 27 W PSU page.

### Raspberry Pi Pico 2W (RP2350A) — *the real-time co-processor*
- **Role:** Hard-real-time motor brain. 20 kHz PWM to the drivers, quadrature decode of 4 encoders via **PIO**, per-wheel PID, command-timeout watchdog. Appears to the Pi as a USB serial device.
- **Supply (V_in):** **5 V on VBUS from the Pi's USB** (this is how it's powered in this build). VSYS accepts 1.8–5.5 V. The onboard buck-boost makes the 3.3 V rail.
- **I (run / peak):** tens of mA for itself. **3V3(OUT) pin can source ≤ ~300 mA** for external logic — enough for both driver logic rails + 4 encoder VCCs, nothing more.
- **Logic level:** **3.3 V.** Treat all GPIO as 3.3 V-only and **level-shift** anything 5 V — Raspberry Pi *withdrew* the RP2350's conditional-5 V-input-tolerance language from the datasheet (stepping-dependent), so don't rely on it. ADC pins (GPIO26–29) are **never** 5 V tolerant.
- **PWM / PIO:** **24 PWM channels** (12 slices × 2 — 4 used for the motors, ample spare), **12 PIO state machines** (1 per encoder → 4 used, 8 free).
- **Connector:** micro-USB (power + data + flashing), 40-pin 0.1″ header. Key pins: VBUS (40), VSYS (39), 3V3_EN (37), 3V3 OUT (36), GND throughout.
- **Talks to:** Pi (USB serial), 2× TB6612FNG (PWM out + STBY + logic VCC), 4× encoders (PIO in + 3V3 + GND).
- **Status I/O (firmware-driven, diagnostics only — *not* a safety device):** a **heartbeat** on a status LED (slow blink = idle/link-OK, fast = receiving commands, very fast = a latched stall fault) and an optional **~$1 passive piezo** on a free GPIO for a *beep-before-spin* on the first valid command plus a chirp when a watchdog fires. The on-board Pico 2W LED is on the CYW43 (needs `cyw43_arch`); a discrete LED on a free GPIO (e.g. GP15/GP22) avoids that dependency. The real safety backstops stay the **e-stop** and the **balance-port LVA buzzer**.
- **⚠️ Footgun:** never back-feed the 7.4 V motor rail into VBUS/VSYS/3V3. Motor power and Pico power are separate rails joined **only at the common ground**.
- **Source:** Pico 2W datasheet, RP2350 datasheet.

---

## Pi↔Pico serial protocol contract

★ The two brains talk over **one USB-CDC serial link** using a deliberately **human-readable ASCII line protocol** (debuggable with `screen`/`cat`, visible on a logic analyzer). This is robocar's *own* protocol — it deliberately does **not** impersonate the vendor's binary CRC frames. This table is the single source of truth; every phase doc and the `firmware/` must match it.

**Framing:** one message per line, `\n`-terminated, space-separated ASCII. **Canonical wheel order `FL, RL, FR, RR`** in every multi-wheel line. **Sign convention:** x forward (+), y left (+), ωz CCW (+) — the same right-handed frame the kinematics, the IMU heading (Phase 4), and the SLAM map (Phase 7) all share.

| Dir | Line | Fields / units | Meaning |
|---|---|---|---|
| Pi→Pico | `v <vx> <vy> <wz>` | m/s, m/s, rad/s | Body-twist command → `mecanum_inverse()` → 4 PID setpoints; **pets the comms watchdog**. |
| Pi→Pico | `s` | — | Stop: twist (0, 0, 0). |
| Pi→Pico | `ping` | — | Link check. |
| Pico→Pi | `t <ms> <wFL> <wRL> <wFR> <wRR>` | ms + rad/s ×4 | Monotonic-ms timestamp, then measured per-wheel angular speed (~20 Hz). |
| Pico→Pi | `o <vx> <vy> <wz>` | m/s, m/s, rad/s | Forward-kinematics body twist (odometry, ~20 Hz). |
| Pico→Pi | `ok` / `err` | — | Per-command ack / parse failure. |
| Pico→Pi | `pong` | — | Reply to `ping`. |

**Watchdog semantics:** if no `v` (or `s`) arrives within **0.5 s**, the Pico zeroes all setpoints and coasts (runaway prevention). A *hung* Pico is caught separately by the hardware watchdog (reboot + USB re-enumerate) — see `safety.md`.

**Rates:** commands as needed (the Pi's teleop/autonomy loop); telemetry **~20 Hz** — one `t` + one `o` per cycle, never flooding the USB-CDC buffer. Implemented in [`../firmware/src/main.c`](../firmware/src/main.c) and mirrored by Phase 3's protocol section.

---

## Drivetrain

### Hiwonder "310" metal-gear motor + Hall encoder (×4) — *MD310Z20, JGA27-310 family*
- **Role:** The four Mecanum drive wheels. Closed-loop via encoder feedback.
- **Supply (V_in, motor):** **7.4 V** nominal (runs on the 2S LiPo rail). Family range ~4.2–8.4 V.
- **I (run / peak):** running **≤ 0.65 A**, **stall ≤ 1.4 A** @ 7.4 V. No-load ~0.05–0.1 A. *Size the driver/wire/fuse to the 1.4 A stall, not the 0.65 A average.*
- **Encoder supply:** **3.3–5 V (datasheet-rated — Hiwonder/Yahboom 310 spec) → use 3.3 V.** A Hall output swings to its *own* Vcc, so 3.3 V keeps the A/B lines inside the Pico's 3.3 V GPIO limit — and 3.3 V is *in-spec*, not a compromise. **Never feed the encoder 7.4 V**, and **don't run it at 5 V to copy the vendor:** the RP2350 isn't reliably 5 V-tolerant (stepping-dependent; the claim was withdrawn from the datasheet) and RR's A/B sit on GP26/27, which are *never* 5 V-tolerant.
- **Encoder output:** AB-phase quadrature, Hall, **13 lines/rev on the motor shaft**, with **built-in pull-up shaping** — an open-collector-style stage pulled to its own Vcc, so an MCU reads it directly. **Optionally fit a 4.7 kΩ pull-up from each Hall-A/Hall-B to 3.3 V (never 5 V) and enable the Pico's internal pull-ups + Schmitt inputs** — harmless if the output is push-pull, the defined HIGH if it's open-collector. *Scope a clean 0–3.3 V swing before trusting counts (the vendor characterized this encoder at 5 V).*
- **★ Counts per *output-shaft* revolution = 13 × 4 (quadrature) × 20 (gearbox) = 1040.** (Or 260 if your firmware counts single-channel rising edges only.) **This is a *calculation*, not a printed spec — you MUST measure it empirically** (Phase 2). Every distance/velocity/SLAM number downstream is scaled by it.
- **Gearbox / speed:** 1:20, output ~450 RPM no-load @ 7.4 V. Output shaft 3 mm D-shaped, eccentric.
- **Connector:** **JST-PH 2.0 mm, 6-pin** carrying *both* motor power and the encoder (M+, M−, Enc-VCC, Enc-GND, Hall-A, Hall-B). **Keep this factory connector** — buy mating pigtails, don't cut it.
- **Talks to:** motor leads → TB6612FNG channel outputs; encoder → Pico (3V3, GND, A→PIO pin, B→PIO pin).
- **Source:** hiwonder.com 310 motor page, Yahboom 310 motor doc.

### TB6612FNG dual motor driver (×2, Pololu #713) — *replaces the original DRV8833*
- **Role:** Turns the Pico's 3.3 V PWM logic into motor current. **One motor per channel → 2 boards drive 4 wheels independently** (required for Mecanum).
- **Supply:** **VM (motor) = 7.4 V** from the LiPo rail (range 2.5–13.5 V). **VCC (logic) = 3.3 V** from the Pico's 3V3.
- **I (per channel):** **1.2 A continuous, 3.2 A peak.** The 310's 1.4 A stall sits comfortably under the peak with real thermal headroom (this is *why* we moved off the DRV8833, whose ~1.2–1.3 A sustainable limit sits right at the stall knee).
- **Logic level:** 3.3 V-compatible inputs (IN1/IN2/PWM). Drive directly from the Pico.
- **★ STBY pin:** must be pulled **HIGH (→ 3.3 V)** or the outputs stay disabled. Easy to forget.
- **Connector:** 0.1″ headers (VM, VCC, GND, AIN1/2, BIN1/2, PWMA, PWMB, STBY) + motor output terminals (AO1/2, BO1/2).
- **Talks to:** Pico (6 logic pins/board: IN1, IN2, PWM per channel; + shared STBY, VCC, GND), motors (outputs), LiPo rail (VM), common ground.
- **Source:** Pololu #713, Toshiba TB6612FNG datasheet.

---

## Sensing

### BNO055 9-axis IMU (Adafruit #4646 or clone) — *UART mode, not I²C*
- **Role:** Absolute orientation. On-chip ARM core fuses accel+gyro+mag → ready-made heading/quaternion. Gives the bot its sense of "which way am I pointing."
- **Supply (V_in):** 3.3–5 V on the breakout (onboard regulator). **3.3 V from the Pi is clean.**
- **I:** ~12 mA. Trivial.
- **★ Interface = UART, NOT I²C.** The BNO055 **stretches the I²C clock during the ACK bit, and the Pi's hardware I²C cannot handle that** — you get random `OSError`s *and silently corrupted orientation bytes*. Adafruit explicitly recommends UART on a Pi. Set **PS1 = HIGH** to select UART, wire to a Pi UART at 115200, common ground.
- **Logic level:** 3.3 V (breakout level-shifts; safe with the Pi).
- **Connector:** STEMMA-QT (JST-SH 1.0 mm) + 0.1″ header (solder the header for UART; the QT connector is I²C-only).
- **Talks to:** Pi UART (TX↔RX, RX↔TX), 3.3 V, GND.
- **★ Magnetometer is poisoned by motors** — mount ≥ 5 cm from every motor can, off the LiPo, on **nylon** standoffs (no ferrous screws nearby).
- **Source:** Adafruit BNO055 guide, Bosch BNO055 datasheet, Bosch forum clock-stretch note.

### PCA9685 16-ch PWM driver (Adafruit #815 or generic) — *servo controller*
- **Role:** Hardware PWM for the pan/tilt servos, over I²C. Keeps jittery servo timing off the Pi's OS *and* off the Pico's real-time loop.
- **Supply:** **VCC (logic) = 3.3 V** (match the Pi so its I²C pull-ups reference 3.3 V). **V+ (servo power) = 5 V from the servo UBEC — *never* the Pi rail.**
- **I:** chip itself ~10 mA. Servo current flows through V+, not the chip.
- **Bus / addr:** I²C-1, **default 0x40.**
- **★ Do NOT cross-wire VCC and V+.** They are separate rails; bridging the 5 V servo rail to the 3.3 V logic pin kills the board.
- **Connector:** 6-pin I²C header (GND, OE, SCL, SDA, VCC, V+), screw terminal for V+, 16× 3-pin servo headers.
- **Talks to:** Pi (I²C-1: SDA/SCL, VCC=3.3 V, GND), servos (CH0=pan, CH1=tilt), UBEC (V+).
- **Source:** Adafruit #815 guide, NXP PCA9685 datasheet.

### Hiwonder LFD-01 servos (×2, from kit) — *pan/tilt; fallback MG90S*
- **Role:** Pan and tilt the camera gimbal.
- **Supply:** **4.8–6 V → run at 5 V** from the UBEC.
- **I (run / stall):** ≤ 60 mA running, **700 mA stall *each*.** Two servos → budget **~1.5 A** worst case (plus 3–4 A inrush transient). This is why they get their own UBEC + bulk cap.
- **Signal:** 50 Hz PWM, **500–2500 µs** pulse = 0–180°. Signal pin is 3.3/5 V tolerant.
- **Connector:** 3-pin servo (Signal / V+ / GND).
- **Talks to:** PCA9685 CH0/CH1 (signal), UBEC 5 V (V+), common ground.
- **Note:** LFD-01 has built-in anti-stall firmware. If your kit lacks them, buy **MG90S** (metal gear), *not* SG90.
- **Source:** hiwonder.com LFD-01 page.

### LDROBOT STL-19P 360° DTOF lidar (from kit) — *via CP2102 USB-UART adapter*
- **Role:** Spinning laser rangefinder → 360° point cloud for mapping/obstacle avoidance.
- **Supply (V_in):** **5 V, ~290 mA** (from the CP2102 adapter's USB, *not* the motor rail).
- **Data:** one-way UART, **230400 baud, 8N1, 3.3 V TTL** (TX only — the lidar never listens).
- **★ PWM pin (motor speed) must be tied to GND** for the default closed-loop 10 Hz spin. Leaving it floating = erratic/no spin.
- **Logic level:** **3.3 V TTL, NOT 5 V tolerant** on the signal lines → use a 3.3 V USB-UART adapter (CP2102 with its jumper set to 3.3 V).
- **Connector:** JST-ZH 1.5 mm 4-pin (Pin1 Tx, Pin2 PWM, Pin3 GND, Pin4 +5V).
- **Talks to:** CP2102 adapter → Pi USB → `/dev/ttyUSB0`.
- **★ Note:** in the kit the lidar is wired to the RRC Lite board you're bypassing, so it does **not** arrive as a USB stick — you supply the CP2102 adapter.
- **Source:** LDROBOT LD19/STL-19P datasheet.

### Raspberry Pi Camera Module 3 — *gimbal camera, CSI*
- **Role:** Live video for teleop FPV and future vision. Streams MJPEG via picamera2.
- **Supply / data:** powered over the CSI ribbon by the Pi. No separate power.
- **★ Cable:** the Pi 5 uses a **22-pin 0.5 mm** CSI connector; the camera is 15-pin 1 mm. You need a **Pi 5 22-to-15-pin FPC adapter cable** (e.g. Adafruit #5820, 300 mm). A generic "longer 15-to-15 CSI cable" **will not fit a Pi 5.**
- **Connector:** CSI/MIPI FFC.
- **Talks to:** Pi CSI port.
- **Weight:** light — fine on the LFD-01/MG90S gimbal. (The kit *depth* camera is too heavy/long to gimbal — mount it body-fixed if used.)
- **Source:** Raspberry Pi Camera 3 docs, Adafruit #5820.

### Nuwa-HP60C depth camera (kit, **optional**)
- **Role:** Optional structured-light depth sensor (RGB + depth over USB-C UVC). Highest learning payoff in sensing, but vendor SDK is ROS-centric.
- **Supply:** 5 V USB, < 0.4 A.
- **★ It's a USB device, not CSI** — it occupies a USB port and counts against the USB power budget. Mount **body-fixed forward** (too heavy to gimbal).
- **Use only if your kit includes it.** Do not buy one for this project.
- **Source:** Yahboom Nuwa-HP60C repo, hiwonder depth camera page.

---

## Power & protection

> Full sizing, the power-tree diagram, and the current budget live in [power_budget.md](./power_budget.md). Contracts only here.

### 2S LiPo, 7.4 V 2200 mAh 10C (from kit) — *the one and only energy source*
- **Output:** 7.4 V nominal (8.4 V full → 6.0 V empty). **10C × 2.2 Ah ≈ 22 A** continuous capability — far above our ~5–10 A worst case.
- **★ Never discharge below ~3.0 V/cell (6.0 V pack).** Balance-charge only. Store at storage charge in a LiPo bag.
- **★ Built-in protection board:** this pack ships with an integrated protection PCB (overcharge / over-discharge / overcurrent / short-circuit cut-off). It is a first line of defense, *not* a substitute for the downstream branch fuses once the system has multiple branches.
- **Connector:** keyed **SM-2P male** output plug (mate with an SM-2P female pigtail — *do not cut it*); the keying prevents reverse-mating. Charging is via a **separate DC5.5×2.5 female barrel jack** with the 8.4 V charger — never charge through the SM-2P output. (Earlier drafts assumed an XT60; the actual kit pack uses SM-2P.)
- **Feeds:** (1) motor rail → 2× TB6612FNG VM; (2) D24V50F5 buck → Pi; (3) UBEC → servos. Through fuse + reverse-protection + switch + e-stop as below — though **Phase 1's single-branch bench build adds none of those** (battery protection board + unplug/`stop`/STBY-low suffice); the coordinated fuses + #2815 switch come online in Phase 3, the e-stop in the first mobile phase.

### Pololu D24V50F5 — *5 V / 5 A buck for the Pi*
- **In:** 6–38 V (the 7.4 V pack is in range, holds 5 V even at 6.0 V empty). **Out: 5 V / 5 A.** ~90% efficient → draws ~3.5–3.9 A from the pack at full Pi load. Integrated reverse-voltage protection + soft-start.
- **Talks to:** LiPo (in, via 5 A branch fuse), Pi USB-C (out).
- **Source:** Pololu #2851.

### Hobbywing UBEC-3A — *5 V for servos*
- **In:** 2–6S LiPo. **Out: 5 V / 3 A** (covers 2× LFD-01 ~1.5 A stall + inrush, with a bulk cap). Includes reverse protection.
- **Talks to:** LiPo (in), PCA9685 V+ (out), common ground.
- **Source:** Hobbywing UBEC-3A.

### INA219 current/voltage sensor (Adafruit #904) — *self-monitoring*
- **Supply:** 3.3 V logic. **Bus / addr: I²C-1, set to 0x41** (NOT default 0x40 — that collides with the PCA9685).
- **Role:** Lets the Pi read pack voltage & current → live power dashboard + graceful low-voltage park/halt. ~3.2 A built-in shunt ceiling (a teaching point — see decisions doc).
- **Source:** Adafruit #904.

### Reverse-polarity + master switch, fuses, e-stop
- **Reverse-polarity / master:** Pololu **#2815** MOSFET slide switch (~16 A — the "Big MOSFET Slide Switch with Reverse Voltage Protection, **HP**"; switch + reverse protection in one) **or** a rocker + Pololu **#5381** LM74500 protector (12 A). The ~16 A #2815 sits in series on the pack feed; robocar's draw is ~5.4 A continuous with a brief ~10 A all-stall + SLAM transient it tolerates — so **#2815 alone already clears the 10 A main fuse with margin.** Pick the rocker + **#5381** only if you'd rather have a separate, panel-mount master switch. (The ~8 A part is the *MP* sibling, **#2814** — don't confuse the two.)
- **Fuses (coordinated):** **10 A main** (closest to LiPo +) · **7.5 A motor branch** · **5 A Pi-buck branch**. Inline ATC/ATO blade holders on 18 AWG. **Character: time-delay (slow-blow) on the main + motor branch** (rides the brief all-motor startup/reversal inrush — locked-rotor current + bulk-cap charge); **fast-acting OK on the Pi branch** (no inductive inrush).
- **E-stop:** 22 mm **latching mushroom, normally-closed**, in series in the **motor power branch only** (kills wheels, keeps the brains alive). NC = a broken wire also trips it (fail-safe).
- **Source:** Pololu #2815 / #5381; sizing in power_budget.md.

---

## End-to-end connection matrix

Every electrical interface in the finished robot, with the contract check. Read it as "A's *output* meets B's *input* — and they agree on V, I, level, and connector."

| From | Signal / rail | To | V | Logic | Connector | Contract check |
|---|---|---|---|---|---|---|
| LiPo + | 7.4 V | (Ph1) SM-2P pigtail → WAGO split → VM ×2 · (Ph3+) → fuse → revpol/switch → e-stop → rails | 7.4 V | power | SM-2P (keyed) | Ph1: battery protection board + keyed plug ✓ · Ph3+: 10 A main fuse < wire/pack rating ✓ |
| Motor rail | 7.4 V | TB6612 VM ×2 | 7.4 V | power | screw/header | 7.4 V in 2.5–13.5 V range ✓; 7.5 A branch fuse ✓ |
| LiPo branch | 7.4 V | D24V50F5 in | 7.4 V | power | header | 7.4 V in 6–38 V ✓; 5 A branch fuse ✓ |
| D24V50F5 out | 5 V/5 A | Pi USB-C | 5 V | power | USB-C | Meets Pi's 5 A want ✓; `usb_max_current_enable=1` |
| LiPo branch | 7.4 V | UBEC in | 7.4 V | power | header | 2–6S range ✓ |
| UBEC out | 5 V/3 A | PCA9685 **V+** | 5 V | power | screw | covers 1.5 A servo stall ✓; **not** the Pi rail ✓ |
| Pico 3V3(OUT) | 3.3 V | TB6612 VCC ×2 + 4× enc VCC | 3.3 V | power | jumper | total load ≪ 300 mA ✓ |
| Pico GPx (PWM×6/board) | PWM | TB6612 IN1/IN2/PWM | 3.3 V | 3.3 V | jumper | Pico 3.3 V drives TB6612 3.3 V-compat inputs ✓ |
| Pico GPx | high | TB6612 STBY ×2 | 3.3 V | 3.3 V | jumper | must be HIGH to enable ✓ |
| TB6612 AO/BO | motor PWM | 310 motor M+/M− | 7.4 V | power | JST-PH 6P | 1.4 A stall < 3.2 A peak/ch ✓ |
| 310 encoder A/B | quadrature | Pico PIO pins (adjacent pairs) | 3.3 V | 3.3 V | JST-PH 6P | enc VCC=3.3 V keeps A/B ≤ 3.3 V ✓ · optional 4.7 k pull-up A/B→3.3 V (never 5 V) ✓ |
| Pi USB-A | USB | Pico micro-USB | 5 V | USB | USB-A↔micro | powers + serial link ✓ |
| Pi USB-A | USB | CP2102 adapter | 5 V | USB | USB-A | → `/dev/ttyUSB0` |
| CP2102 (3.3 V) | UART RX | STL-19P Tx | 3.3 V | 3.3 V | JST-ZH 4P | lidar TX 3.3 V matches ✓; PWM pin→GND ✓ |
| Pi UART (pin 8/10) | UART | BNO055 (PS1=HIGH) | 3.3 V | 3.3 V | jumper | dodges I²C clock-stretch bug ✓ |
| Pi I²C-1 (pin 3/5) | I²C | PCA9685 0x40 + INA219 0x41 | 3.3 V | 3.3 V | jumper | distinct addresses ✓ |
| PCA9685 CH0/CH1 | PWM | LFD-01 pan/tilt signal | 3.3–5 V | logic | 3-pin servo | 50 Hz, 500–2500 µs ✓ |
| Pi CSI (22-pin) | MIPI | Camera Module 3 (15-pin) | — | — | 22→15 FPC #5820 | **adapter cable required** ✓ |
| All grounds | 0 V | Star point at LiPo − | 0 V | — | bus/tie | single-point star, no daisy-chain ✓ |

> If any cell in the right column ever reads ✗ after a change, **stop and fix the contract before wiring.** That column is the whole point of this document.

---

## Canonical pin assignments

> **This is the authoritative pin map for the whole build.** Every phase wiring diagram uses *these exact pins.* If you must change one, change it here first. TB6612FNG is driven in standard mode: **IN1/IN2 set direction, PWM sets speed** (one motor per channel).

### Pico 2W — motor control (2× TB6612FNG)

| Wheel | Board.Channel | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | Board A · ch A | GP2 | GP3 | GP4 |
| RL | Board A · ch B | GP5 | GP6 | GP7 |
| FR | Board B · ch A | GP8 | GP9 | GP10 |
| RR | Board B · ch B | GP11 | GP12 | GP13 |
| **STBY** (both boards, tied together) | — | GP14 → must be **HIGH** | | |

### Pico 2W — encoders (PIO quadrature; A/B must be *adjacent* GPIOs)

| Wheel | A (base pin) | B |
|---|---|---|
| FL | GP16 | GP17 |
| RL | GP18 | GP19 |
| FR | GP20 | GP21 |
| RR | GP26 | GP27 |

- **Encoder VCC** (all 4) ← Pico **3V3(OUT)** · **Encoder GND** (all 4) → common ground. (3.3 V keeps the A/B outputs within the Pico's GPIO limit.) *Optionally add a 4.7 kΩ pull-up from each Hall-A/Hall-B to 3.3 V (never 5 V) + enable the Pico internal pull-ups — robust whether the output is open-collector or push-pull; verify clean 0–3.3 V edges on a scope before trusting counts.*
- Each encoder gets its **own PIO state machine** (4 used of 12). A/B on adjacent pins because the PIO decoder reads two consecutive GPIOs from a base pin.
- Free Pico GPIO for later: GP0, GP1, GP15, GP22, GP28.
- **Pico power:** VBUS ← Pi USB 5 V. **3V3(OUT)** feeds both TB6612 VCC rails + the 4 encoder VCCs (load ≪ 300 mA). Never feed 7.4 V into the Pico.

### MCU resource budget (RP2350) — every on-board duty maps to a finite resource, with headroom

| Duty | Resource | Used | RP2350 has | Note |
|---|---|---|---|---|
| 4× motor PWM (20 kHz) | PWM channels | 4 (GP4/7/10/13) | 24 ch / 12 slices | ⚠️ confirm GP4/7/10/13 land on **distinct slice+channel** pairs — two outputs on one slice are forced to equal duty (driving one wheel tugs another). Verify on the bench. |
| 4× quadrature decode | PIO state machines | 4 (pio0 sm0–2, pio1 sm0) | 12 SMs (3 blocks × 4) | 8 free. A/B must be adjacent GPIOs. |
| 8× motor dir (IN1/IN2) + STBY | GPIO | 9 | — | plain digital out |
| Pi link | USB-CDC | 1 | native USB | the protocol contract above |
| 3.3 V external loads | 3V3(OUT) | 2 driver rails + 4 enc VCC | ≤ ~300 mA | well under budget |
| (optional) current shunt | ADC | 0 used | 3 ch (GP26/27 spent on RR enc!) | only **GP28** free — see stall-detection ADR |
| Free for later | GPIO | — | GP0, GP1, GP15, GP22, GP28 | status LED / piezo / shunt |

### Connector pinout — verify on YOUR cable (color codes vary by batch)

The contracts above are the V/I/logic source of truth; this is the **physical-mating** layer. Meter each cable before first power-on (the factory JST-PH plug is *kept*, never cut):

| Connector | Pins | Verify (meter it — don't trust color) |
|---|---|---|
| Motor JST-PH 2.0 **6-pin** | M+, M−, Enc-VCC, Enc-GND, Hall-A, Hall-B | Typical: Red=M+, White=M−, Blue=Enc-VCC, Black=Enc-GND, Yellow=Hall-A, Green=Hall-B — **confirm by continuity**, batches differ. Enc-VCC → 3.3 V only. |
| Servo 3-pin | V+, GND, signal | V+ from the **UBEC**, never the Pi rail; signal from PCA9685. |
| LiPo **SM-2P** (pack output) | + / − | Keyed male plug — mate the SM-2P female pigtail; can't mate reversed. Confirm red=+ with a meter before plugging in. **Don't cut it.** |
| LiPo **DC5.5×2.5** (pack charge port) | + / − | Charging only, via the 8.4 V charger. Never draw robot power from here; never charge through the SM-2P output. |
| Pi↔Pico | USB | stable `/dev/serial/by-id/…`, not `ttyACM*` (enumeration order varies). |

### Raspberry Pi 5 — buses & power (40-pin header)

| Function | Pin(s) | Device |
|---|---|---|
| I²C-1 SDA (GPIO2) | 3 | PCA9685 (0x40), INA219 (0x41) |
| I²C-1 SCL (GPIO3) | 5 | PCA9685 (0x40), INA219 (0x41) |
| UART0 TXD (GPIO14) | 8 | → BNO055 RX |
| UART0 RXD (GPIO15) | 10 | ← BNO055 TX |
| 3.3 V | 1, 17 | BNO055 VIN, PCA9685 VCC, INA219 VCC |
| GND | 6, 9, 14, 20, 25, 30, 34, 39 | common ground (star) |
| USB-A ×4 | — | Pico, CP2102→lidar, (opt) depth cam |
| USB-C | — | ← D24V50F5 5 V/5 A buck |
| CSI (22-pin) | — | Camera Module 3 via #5820 cable |

> **PCA9685 V+ and the servos do NOT touch the Pi header** — V+ comes from the UBEC. The Pi's 5 V pins (2, 4) are reserved/unused in this build to keep servo transients off the Pi rail.

---

## Sources (primary)
- Raspberry Pi 5 & 27 W PSU, Pico 2W / RP2350 datasheets — raspberrypi.com / datasheets.raspberrypi.com
- Hiwonder 310 motor, LFD-01, MentorPi M1, RRC Lite — hiwonder.com; Yahboom 310 motor doc
- TB6612FNG — Pololu #713, Toshiba datasheet · DRV8833 (compared) — TI datasheet, Pololu #2130
- BNO055 — Adafruit #4646 guide, Bosch datasheet & forum · PCA9685 — Adafruit #815, NXP datasheet
- STL-19P / LD19 — LDROBOT datasheet · INA219 — Adafruit #904
- D24V50F5 — Pololu #2851 · UBEC-3A — Hobbywing · Camera 3 cable — Adafruit #5820
