# Phase 4 — IMU + Pose Tracking

## Goal

Give the robot a sense of **where it is and which way it's pointing**, and show it live in the web UI.

By the end of this phase the Pi 5 will fuse two streams of information — the wheel speeds you already measure (Phase 2) and a **drift-free heading** from a 9-axis IMU — into a running estimate of the bot's **pose**: `(x, y, θ)` in a fixed *world frame*. A compass widget in the browser will spin to match the bot's real heading, and an `(x, y)` readout will track as you drive. You'll also add a tiny power sensor (INA219) so the Pi can watch its own battery and **park itself before the LiPo is over-discharged.**

This is the moment the robot stops being a remote-control toy and starts being a thing that *knows its own state* — the foundation everything in Phases 7–8 (SLAM, navigation) is built on.

## Difficulty / Time

**Medium · 1 weekend.** Two new sensors, both low-current and low-wire-count. The hardware is two short harnesses; the *real* work is the pose math (holonomic dead-reckoning) and getting the UART transport right.

## Prerequisites

- **Phase 3 complete:** the Pi ↔ Pico USB serial bridge works, telemetry (`t <ms> <fl> <rl> <fr> <rr>`) streams at 20 Hz, and you can drive the bot from the web UI using **Mecanum body-twist commands** (`v vx vy wz`).
- **Phase 2 complete:** the Pico holds commanded wheel velocities with per-wheel PID, and you have an **empirically measured** `COUNTS_PER_REV` (calculated 1040 — *measured* is what you use).
- The Pi 5 runs Raspberry Pi OS (Bookworm 64-bit), is on your WiFi, SSH enabled.
- The power tree from Phase 1/3 is live: 2S LiPo → D24V50F5 buck → Pi 5 (with `usb_max_current_enable=1`).

## New parts this phase

- **BNO055 9-axis IMU** (Adafruit #4646 or clone) — on-chip sensor fusion gives a ready-made, drift-free heading over **UART**.
- **INA219 current/voltage sensor** (Adafruit #904) — lets the Pi read pack voltage and current over **I²C-1 at address 0x41** for a power dashboard and a graceful low-voltage halt.
- **M2.5 nylon standoffs** (from the Phase 1 kit) — to mount the IMU **≥ 5 cm from motors and the LiPo**, on non-ferrous hardware.

See `bom.md` for links/prices; the master list is `_engineering/products_to_buy.md`.

---

## Cumulative system state (in prose)

Here's the whole robot as it stands the moment you finish this phase, told as a chain of cause and effect.

A single **2S LiPo (7.4 V nominal, 8.4 V full → 6.0 V empty)** is the only energy source. Its positive lead passes through a **10 A main fuse**, then a **MOSFET reverse-polarity + master switch**, producing a *protected 7.4 V bus* that fans out three ways. The **motor branch** (7.5 A fuse → normally-closed e-stop) feeds the **VM pins of two TB6612FNG drivers**. The **Pi branch** (5 A fuse) feeds a **Pololu D24V50F5 buck** that makes a clean **5 V / 5 A** rail for the Pi 5 over USB-C. (The servo branch — a UBEC for the PCA9685 — arrives in Phase 5; it isn't wired yet.)

The **Pi 5** is the high-level brain: WiFi, the Flask web server, and now pose estimation. It powers and talks to the **Pico 2W** over a single USB-A → micro-USB cable (5 V on VBUS + a `/dev/ttyACM*` serial link). The **Pico** is the hard-real-time co-processor: it runs 20 kHz PWM into the two TB6612 boards, decodes four quadrature encoders in **PIO** state machines, runs **per-wheel PID**, and enforces a command-timeout watchdog. Each TB6612 drives **one motor per channel** (four channels → four wheels), with **STBY pulled HIGH** to enable the outputs and **VCC = 3.3 V** from the Pico's `3V3(OUT)` — the *same* 3.3 V rail also powers all four **encoder VCCs** (never the 7.4 V motor rail — that would push the A/B outputs past the Pico's 3.3 V GPIO limit).

The drive geometry is **Mecanum, not differential**: four wheels with 45° rollers let the body translate forward/back (`vx`), strafe sideways (`vy`), *and* rotate (`wz`) independently — a **holonomic** platform. The web joystick sends a body twist `v vx vy wz`; the Pico's **inverse kinematics** turns that into four wheel-speed setpoints (Phase 3). Going the other way, the four *measured* wheel speeds run through the **forward kinematics** to recover the body twist — and that's the new ingredient this phase feeds into pose tracking.

**New this phase:** a **BNO055 IMU** hangs off the Pi's **hardware UART** (pins 8/10) — *not* I²C — to give an absolute heading, and an **INA219** joins the Pi's **I²C-1** bus at **0x41** to report pack voltage and current. The Pi fuses wheel odometry (for *how far*) with the IMU (for *which way*) into `(x, y, θ)`, and the browser shows a live compass.

---

## Pin tables (from the canonical map)

### Pi 5 — new this phase (40-pin header)

| Function | Pi 5 pin | GPIO | Device | Notes |
|---|---|---|---|---|
| UART0 **TXD** | 8 | GPIO14 | → BNO055 **RX** | Pi *transmits*; crosses to IMU receive |
| UART0 **RXD** | 10 | GPIO15 | ← BNO055 **TX** | Pi *receives*; crosses from IMU transmit |
| I²C-1 **SDA** | 3 | GPIO2 | INA219 SDA (also PCA9685 in Ph5) | shared bus |
| I²C-1 **SCL** | 5 | GPIO3 | INA219 SCL (also PCA9685 in Ph5) | shared bus |
| **3.3 V** | 1 (or 17) | — | BNO055 VIN + INA219 VCC | logic supply, *not 5 V* |
| **GND** | 6 / 9 / 14 / 20 / 25 / 30 / 34 / 39 | — | BNO055 GND + INA219 GND | common ground (star) |

### BNO055 — interface select

| BNO055 pin | Connects to | Why |
|---|---|---|
| PS1 | **3.3 V (HIGH)** | Selects **UART** mode (PS1 LOW = I²C — *do not use on a Pi*) |
| VIN | Pi 3.3 V (pin 1/17) | onboard regulator; 3.3 V is clean |
| GND | Pi GND | common ground |
| TX / SDA-pin | Pi RXD (pin 10) | IMU transmit → Pi receive |
| RX / SCL-pin | Pi TXD (pin 8) | Pi transmit → IMU receive |

> On many breakouts the UART TX/RX share the silkscreen labels **SDA (=TX)** and **SCL (=RX)** because those pins are reused between modes. Read your board's table; in UART mode SDA carries data *out* of the IMU.

### INA219 — power sensor

| INA219 pin | Connects to | Why |
|---|---|---|
| VCC | Pi 3.3 V (pin 1/17) | logic supply (so I²C pull-ups reference 3.3 V) |
| GND | Pi GND | common ground |
| SDA | Pi SDA (pin 3) | I²C-1 data |
| SCL | Pi SCL (pin 5) | I²C-1 clock |
| Vin+ / Vin− | across the sensed line (see wiring_diagram.md) | high-side shunt |
| **A0/A1 address jumpers** | **set to 0x41** | 0x40 collides with the PCA9685 added in Phase 5 |

The full Pico pin map (motors GP2–GP13, STBY GP14, encoders GP16/17, GP18/19, GP20/21, GP26/27) is **unchanged** this phase — see Phase 1/2. No Pico wires change.

---

## Concept primer (ground-up)

You can skip this if it's review, but each idea here is load-bearing for the code below.

### Why fuse sensors at all?

Three sensors inside the IMU each measure orientation, and **each is wrong in a different, complementary way**:

- A **gyroscope** measures *angular rate* (°/s). Integrate it and you get angle — but any tiny constant offset (bias) integrates into an angle that grows without bound. A cheap gyro **drifts ~1°/s**; in a minute your heading is off by a minute of marching. Gyros are *smooth and fast* but *drift over time*.
- An **accelerometer** measures the gravity vector (plus motion). Gravity points "down" always, so it gives an absolute, **drift-free** tilt reference — but it's **noisy** and it can't tell gravity from the bot accelerating. Accels are *absolute but jittery*.
- A **magnetometer** measures Earth's magnetic field → an absolute compass heading, drift-free — but it's **wrecked by nearby ferrous metal and motor currents**.

**Sensor fusion** is the art of combining them so the result has the *strengths* of each: the gyro's smoothness short-term, the accel/mag's absolute reference long-term. The simplest version is a **complementary filter** (the optional lab below): `angle = α·(angle + gyro·dt) + (1−α)·(accel_angle)` — trust the gyro at high frequency, the accel at low frequency, blend with one constant `α`. The fancy version is a Kalman filter. Either way, the *point* is: no single sensor is trustworthy alone.

### The easy path: on-chip fusion

The **BNO055** has a small ARM Cortex-M0 *inside the package* running Bosch's fusion algorithm. You don't write the filter — you read **already-fused** Euler angles or a quaternion over a serial transaction. That's why we picked it: it gets you a working robot fast, and (bonus) it gives you a *ground-truth reference* on the same chip to validate your own filter against in the optional lab. The trade is that the magnetometer is still physically poisoned by motors, which is why mounting matters.

### UART vs I²C transport (why this isn't on I²C)

Both UART and I²C are ways to move bytes between chips. **I²C** is a two-wire bus (SDA + SCL) where a controller clocks every bit and devices share addresses — great for hanging many sensors on one bus. **UART** is point-to-point (TX↔RX), no shared clock, each side runs at an agreed baud (we use **115200**).

We deliberately put the BNO055 on **UART, not the Pi's hardware I²C**, because of a real, documented bug: the BNO055 **stretches the I²C clock during the ACK bit** (it holds SCL low to say "wait, I'm not ready"), and the **Raspberry Pi's hardware I²C controller mishandles clock stretching** — you get random `OSError`s *and, worse, silently corrupted orientation bytes.* A corrupted heading that *looks* valid is more dangerous on an autonomous robot than a clean crash. Adafruit explicitly recommends UART on a Pi. So we set **PS1 = HIGH** to select UART and wire TX/RX to the Pi's UART pins. As a bonus this frees the I²C bus for the INA219 (and the Phase-5 PCA9685).

### I²C address collisions

Every device on an I²C bus needs a **unique 7-bit address**. The PCA9685 servo driver (Phase 5) defaults to **0x40**; the INA219 *also* defaults to 0x40. Two devices answering the same address corrupts every transaction. So we **move the INA219 to 0x41** with its address-select jumpers now, before the PCA9685 ever shows up — fixing a collision before it can bite.

### Holonomic dead-reckoning (the pose math)

**Dead-reckoning** means estimating where you are by integrating how you've moved — no external reference. On a Mecanum bot the recipe is:

1. From the four measured wheel speeds, run the **forward Mecanum kinematics** to get the **body twist** `(vx, vy, wz)` — velocity *in the robot's own frame* (vx = forward, vy = left-strafe). This is the inverse of the matrix the Pico already uses to turn a twist into wheel speeds.
2. The body frame is rotated relative to the world by the heading **θ**. To advance the world-frame position, **rotate the body velocity into the world frame** with a 2-D rotation:

   ```
   world_vx = vx·cos(θ) − vy·sin(θ)
   world_vy = vx·sin(θ) + vy·cos(θ)
   ```

   Note the `−vy·sin` / `+vy·cos` terms — *this is the part a differential model has no concept of.* A diff-drive bot can't strafe, so it never needs them; a Mecanum bot does, and dropping them silently corrupts odometry the instant the bot moves sideways.
3. Integrate: `x += world_vx·dt`, `y += world_vy·dt`.
4. **Set θ absolutely from the IMU** — do *not* integrate `wz` from the wheels (that's just a worse gyro that also drifts and slips). Wheels tell you *how far*; the IMU tells you *which way*. That split is the whole trick.

---

## Software task list

> All paths assume the `~/robocar` project from Phase 3. New files: `imu.py`, `pose.py`, `power.py`; edits to `webserver.py` and the front-end.

### 0. One-time Pi setup — free the UART, enable I²C

The Pi's primary UART (`/dev/ttyAMA0` / `/dev/serial0`) is, by default, used for the Linux serial **console** and partly shared with Bluetooth. We need it free for the IMU.

```bash
sudo raspi-config nonint do_i2c 0          # enable I2C-1
sudo raspi-config nonint do_serial_hw 0    # enable the serial *hardware*
sudo raspi-config nonint do_serial_cons 1  # DISABLE the serial *login console*
```

Then, to give the BNO055 the *full* PL011 UART (more reliable than the mini-UART), add to `/boot/firmware/config.txt`:

```ini
enable_uart=1
dtoverlay=disable-bt        # frees PL011 from Bluetooth → /dev/ttyAMA0 on pins 8/10
```

Reboot. Confirm the device exists and the console is gone:

```bash
ls -l /dev/serial0          # should symlink to ttyAMA0
sudo systemctl status serial-getty@ttyAMA0   # should be inactive/dead
```

### 1. IMU driver over UART — `imu.py`

The BNO055 speaks a small register protocol over UART; Adafruit's CircuitPython driver wraps it. Install and wrap it in a thread-safe reader so the web loop never blocks on a serial read.

```bash
pip install adafruit-circuitpython-bno055 pyserial adafruit-blinka
```

```python
# imu.py
import threading, time, board, busio
import adafruit_bno055
import serial

class IMU:
    """BNO055 over UART. Exposes a drift-free heading in radians."""
    def __init__(self, port="/dev/serial0", baud=115200):
        uart = serial.Serial(port, baudrate=baud, timeout=0.1)
        self.sensor = adafruit_bno055.BNO055_UART(uart)
        self._theta = 0.0          # radians, world heading
        self._cal = (0, 0, 0, 0)   # sys, gyro, accel, mag (0..3)
        self._lock = threading.Lock()
        self._run = True
        threading.Thread(target=self._loop, daemon=True).start()

    def _loop(self):
        import math
        while self._run:
            try:
                euler = self.sensor.euler           # (heading, roll, pitch) deg
                if euler and euler[0] is not None:
                    with self._lock:
                        self._theta = math.radians(euler[0])
                        self._cal = self.sensor.calibration_status
            except Exception:
                time.sleep(0.05)                    # transient UART hiccup; retry
            time.sleep(0.02)                        # ~50 Hz

    @property
    def theta(self):
        with self._lock:
            return self._theta

    @property
    def calibration(self):
        with self._lock:
            return self._cal
```

### 2. Calibration: persist it once, reload every boot

The BNO055 auto-calibrates in the background as it sees motion. `calibration_status` returns `(sys, gyro, accel, mag)`, each **0–3**. Aim for **≥2/3 on each** (mag is the stubborn one). Then save the 22-byte blob and reload it on the next boot so you skip the figure-8 dance every time.

```python
# Run once interactively after a good figure-8 + tilt routine:
blob = imu.sensor.offsets_... # see driver API; persist the calibration bytes
with open("/home/pi/robocar/imu_cal.bin", "wb") as f:
    f.write(bytes(blob))

# On boot, after a brief stillness: enter CONFIG mode, write the blob, return to NDOF.
```

**Calibration moves (per power cycle if not reloaded):** gyro → hold still a few seconds; accel → tilt to several faces (flat, each side, briefly upside-down); mag → slow **figure-8** in the air, *away from the laptop and motors.*

### 3. Pose estimator — `pose.py`

This is the heart of the phase. Forward Mecanum kinematics → body twist → rotate by IMU θ → integrate.

```python
# pose.py
import math

# Mecanum geometry (use YOUR measured numbers)
R  = 0.0325          # wheel radius [m] (65 mm dia) — measure it
LX = 0.085           # half wheelbase, front↔rear [m] — measure it
LY = 0.095           # half track,   left↔right  [m] — measure it
K  = LX + LY

def forward_kinematics(w_fl, w_rl, w_fr, w_rr):
    """Four wheel angular speeds [rad/s] -> body twist (vx, vy, wz).
    Inverse of the matrix the Pico uses to turn a twist into wheel speeds.
    Sign convention: vx = forward, vy = +left, wz = +CCW. VERIFY on YOUR bot."""
    vx =  (w_fl + w_rl + w_fr + w_rr) * R / 4.0
    vy = (-w_fl + w_rl + w_fr - w_rr) * R / 4.0
    wz = (-w_fl - w_rl + w_fr + w_rr) * R / (4.0 * K)
    return vx, vy, wz

def wrap(a):
    """Wrap an angle to (-pi, pi]."""
    return (a + math.pi) % (2 * math.pi) - math.pi

class Pose:
    def __init__(self):
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0          # set absolutely from IMU each step
        self._t0 = None

    def update(self, wheels_rad_s, imu_theta, t):
        if self._t0 is None:
            self._t0 = t
            self.theta = imu_theta
            return
        dt = t - self._t0
        self._t0 = t
        if dt <= 0:
            return
        vx, vy, _wz = forward_kinematics(*wheels_rad_s)
        self.theta = imu_theta                       # absolute, not integrated
        c, s = math.cos(self.theta), math.sin(self.theta)
        world_vx = vx * c - vy * s                   # rotate body->world
        world_vy = vx * s + vy * c
        self.x += world_vx * dt
        self.y += world_vy * dt
```

> Telemetry gives wheel speeds in **m/s** (from Phase 2); convert to rad/s with `omega = v / R` before calling `forward_kinematics`, or rewrite FK in m/s. Keep one convention and write it down. **Verify the FK signs empirically** (Measurement Lab) — command pure `vx`, pure `vy`, pure `wz` and confirm the recovered twist matches.

### 4. Power monitor — `power.py` (INA219 at 0x41)

```bash
pip install adafruit-circuitpython-ina219
```

```python
# power.py
import board, busio, adafruit_ina219

class Power:
    def __init__(self, addr=0x41):
        i2c = busio.I2C(board.SCL, board.SDA)
        self.ina = adafruit_ina219.INA219(i2c, addr=addr)   # 0x41, NOT 0x40
        self.low_v_warn = 6.4    # ~3.2 V/cell — start parking
        self.low_v_halt = 6.0    # ~3.0 V/cell — hard floor

    def read(self):
        return {
            "bus_v": self.ina.bus_voltage,        # pack volts
            "current_a": self.ina.current / 1000  # mA -> A
        }
```

**Graceful low-voltage behavior:** in the web/control loop, when `bus_v < low_v_warn` raise a UI banner and refuse new motion commands; when `bus_v < low_v_halt`, send `s` (stop) to the Pico and shut the drive loop down. This is the *software* layer of the over-discharge defense-in-depth — the balance-port buzzer is the dumb backstop. (Note the INA219's ~3.2 A shunt ceiling clips on a 5.6 A all-stall transient; it's a *monitor*, not a stall sensor.)

### 5. Web UI — pose readout + compass

Extend `/telemetry` to carry pose and power, and add an SVG compass that rotates to `theta_deg`.

```json
{
  "pose":  {"x": 1.23, "y": -0.45, "theta_deg": 87.3},
  "wheels":{"fl": 0.32, "rl": 0.31, "fr": 0.32, "rr": 0.31},
  "power": {"bus_v": 7.71, "current_a": 2.34},
  "imu_cal":[3, 3, 3, 3]
}
```

Compass: an SVG `<g transform="rotate(θ)">` containing a needle, redrawn on each `/telemetry` poll. Add an `x / y / θ` text line and a battery bar driven by `bus_v` (map 6.0–8.4 V to 0–100%).

### 6. Mount the IMU (mechanical, but it *is* the spec)

- **Flat and level** on the chassis — the fusion assumes its X/Y plane is horizontal.
- **≥ 5 cm from every motor can and off the LiPo**, on **nylon** standoffs, with **no ferrous screws** nearby. The magnetometer reads Earth's field at ~0.5 gauss; a motor's stray field nearby swamps it.
- **Securely fastened** — vibration adds accel noise.
- Note and document the board's **forward axis** relative to the bot's forward (`vx`) axis; you'll add a constant heading offset if they differ.

### 7. (Optional learning module) Roll your own complementary filter

Read the BNO055's **raw** accel + gyro (not the fused output), implement a one-line complementary filter, and validate it against the chip's fused Euler angles as ground truth. **Do it on pitch/roll, not heading** — pitch/roll lean on the accelerometer (gravity), which the motors *don't* corrupt, whereas heading leans on the magnetometer, which they do.

```python
# complementary filter on pitch (repeat for roll)
alpha = 0.98
pitch = alpha * (pitch + gyro_y * dt) + (1 - alpha) * accel_pitch
# compare `pitch` against sensor.euler[2] (fused) -> plot both vs time
```

Plot your filtered pitch over the BNO055's fused pitch while you tilt the bot by hand. They should track within a degree or two — proof you understand fusion, not just that you called a library.

---

## Verification checklist

Each step assumes the previous one passed.

- [ ] **UART is free.** `ls -l /dev/serial0` points at `ttyAMA0`; the serial *console* (`serial-getty@ttyAMA0`) is **inactive**. Bluetooth disabled (`dtoverlay=disable-bt`).
- [ ] **IMU answers on UART.** `python -c "import imu; m=imu.IMU(); import time; time.sleep(1); print(m.theta)"` prints a number (no `OSError` storm). If silent: TX/RX are crossed wrong — swap them. Confirm **PS1 = HIGH** (UART selected).
- [ ] **`i2cdetect -y 1` shows `41`** (and *nothing* at `40` yet — PCA9685 isn't wired). If you see `40`, your address jumpers are wrong; fix before Phase 5.
- [ ] **IMU heading tracks reality.** Rotate the bot by hand; `theta` changes smoothly and proportionally. Tilt it; pitch/roll change. Point "forward" north (phone compass as ground truth) and note the offset.
- [ ] **Heading is stable while still.** Set the bot down, don't touch it 60 s. `theta` drifts < ~1° (proof the fused output isn't a raw gyro).
- [ ] **Calibration reaches ≥ 2/3 on all four.** Do gyro-still + accel-tilt + mag figure-8. If `mag` won't climb above 1, you're near metal/motors — move away and retry.
- [ ] **Calibration persists.** Save the blob, power-cycle, reload it, confirm `calibration_status` is good *without* re-dancing.
- [ ] **INA219 reads sane power.** Pack voltage matches a multimeter across the LiPo within ~0.1 V; current rises when motors spin.
- [ ] **Straight-line pose.** Drive forward a **measured 1.0 m** (tape measure). The world-frame displacement is within **5 %** of 1.0 m. (Measurement Lab.)
- [ ] **Pure strafe pose.** Command pure `vy` for ~0.5 m sideways; pose moves *sideways* in the world frame, not forward. (This is the Mecanum-only test a diff model fails.)
- [ ] **Spin-and-return heading.** Rotate in place ~360° and stop facing the start. `theta` returns within **~5°** of its starting value (wraparound handled). (Measurement Lab.)
- [ ] **Compass + battery widgets live.** The browser compass matches the bot's heading as you rotate it; the battery bar tracks `bus_v`.
- [ ] **Graceful low-voltage.** Temporarily raise `low_v_warn` above the current pack voltage; confirm the UI banners and the bot refuses motion / stops.

---

## Common pitfalls

- **BNO055 wired to I²C instead of UART.** The whole point of this phase. If you get random `OSError`s *or* a heading that occasionally jumps to garbage, you're on I²C and hitting the clock-stretch bug. Set **PS1 = HIGH**, move to the UART pins (8/10).
- **PS1 left LOW (or floating).** Defaults to I²C. Tie PS1 to **3.3 V** explicitly.
- **TX/RX not crossed.** UART is `TX→RX` and `RX→TX`. Pi TXD (pin 8) → IMU RX; Pi RXD (pin 10) → IMU TX. Straight-through = silence.
- **Serial console still grabbing the UART.** If `imu.py` opens `/dev/serial0` but reads garbage or "device busy," the login console is still attached. Re-run the `do_serial_cons 1` step.
- **VIN on 5 V on a bare-chip clone.** Some clones don't have the regulator. **3.3 V is always safe for the chip.** Use pin 1/17.
- **INA219 left at 0x40.** Works *now*, then silently collides with the PCA9685 in Phase 5. Set **0x41** today.
- **Integrating the gyro/`wz` for heading.** Don't. Use the BNO055's **fused** `euler[0]` and set θ absolutely. The wheels' `wz` drifts *and* slips.
- **Heading wraparound bugs.** `euler[0]` is 0–360° (or −180…180 on some firmware). When you difference two headings, wrap: `delta = ((a - b + 180) % 360) - 180`. Same idea as the `wrap()` helper in `pose.py`.
- **Dropping the strafe terms.** If you copy a diff-drive `x += v·cos(θ)·dt; y += v·sin(θ)·dt` you've thrown away `vy`. Strafing then teleports your pose. Use the full rotation matrix.
- **Magnetometer goes insane when motors run.** Stray motor field bleeding into the mag. Move the IMU further away / higher; verify the ≥5 cm + nylon rule. (Pitch/roll, which don't use the mag, stay clean — that's why the optional filter lab uses them.)
- **Pose drifts on carpet / bumpy floor.** Dead-reckoning assumes no wheel slip. Slip → position error accumulates. This is *expected*; lidar SLAM corrects it in Phase 7. Heading stays good because it's from the IMU, not the wheels.
- **Wrong `COUNTS_PER_REV` carried over from Phase 2.** Every distance scales by it. If your 1 m drive reads 1.9 m, you likely used 560 instead of your measured ~1040.

---

## MEASUREMENT LAB

> Turn invisible state into a number you can screenshot.

**A. Address scan — confirm 0x41, prove no collision.**
- *Instrument:* `i2cdetect -y 1` on the Pi.
- *Expected:* the cell at row 40, column 1 shows **`41`**; **nothing at `40`** (the PCA9685 isn't on the bus yet). This is the one-command proof that you dodged the address clash before Phase 5.

**B. Straight-line pose error — the dead-reckoning accuracy number.**
- *Instrument:* a tape measure on a hard, flat floor.
- *Method:* zero the pose, drive forward a **measured 1.000 m**, read the world-frame displacement `√(x²+y²)`.
- *Expected:* within **5 %** (≤ 0.05 m error). Bigger → wrong `COUNTS_PER_REV` or wheel-radius `R`.

**C. Spin-and-return heading — the drift number.**
- *Instrument:* a floor mark for the starting facing; the IMU itself.
- *Method:* note `theta`, rotate in place ~360°, stop facing the mark, read `theta`.
- *Expected:* returns within **~5°** of the start, with correct wraparound. This is the headline result of the phase: a heading that comes home.

**D. Current vs time — the power dashboard plot.**
- *Instrument:* INA219 → log `(t, bus_v, current_a)` to CSV; plot.
- *Expected:* baseline ~1–2 A idle, spikes as motors accelerate (cruise ~2.6 A across four motors at the pack); voltage sags under load and recovers. A textbook current trace you can put in a writeup. (Note: the current curve **flat-tops at the INA219's ~3.2 A shunt ceiling** on hard stalls — the *voltage* curve is the trustworthy one, and it's what drives the low-voltage cutoff.)

---

## PHASE WRAP-UP

**What you learned.**
You learned *why* you fuse sensors (a gyro drifts, an accelerometer is noisy, a magnetometer hates motors — each wrong in a complementary way) and the easy industrial answer (on-chip fusion in the BNO055). You learned that "it's just a sensor on a bus" hides transport-level reality: the Pi's I²C controller can't survive the BNO055's clock-stretching, so you chose **UART** instead — and you learned to spot and pre-empt an **I²C address collision** (INA219 → 0x41). And you built **holonomic dead-reckoning**: forward Mecanum kinematics to a body twist, rotated into the world frame by an *absolute* IMU heading, with wheels supplying distance and the IMU supplying direction — including the strafe terms a differential model can't express.

**Résumé bullet (copy-ready).**
> Implemented real-time 3-DOF pose estimation for a holonomic Mecanum robot, fusing PIO-decoded wheel odometry with a BNO055 IMU (UART transport to bypass a Raspberry Pi I²C clock-stretching defect); achieved <5% dead-reckoning error over 1 m and <5° heading drift over a 360° rotation, with a live web compass and INA219-based low-voltage safe-park.

**Interview talking point.**
> "I deliberately put the IMU on UART instead of I²C. The BNO055 stretches the I²C clock during the ACK bit, and the Pi's hardware I²C mishandles that — you get not just random errors but *silently corrupted* heading bytes, which is the worst failure mode on something that drives itself. Moving to UART fixed it and, as a bonus, freed the I²C bus, where I then had to dodge an address collision between the INA219 and the servo driver by setting the INA219 to 0x41. For the pose itself, I used the wheels only for distance and the IMU only for heading — integrating the gyro or wheel `wz` just gives you a worse, drifting heading. And because it's Mecanum, the body velocity has a sideways component, so I rotate the *full* body twist into the world frame, not just the forward term."

---

## What's next

**Phase 5** adds the Raspberry Pi Camera Module 3 on a pan/tilt gimbal driven by the **PCA9685** (which is *why* you set the INA219 to 0x41 here) on a dedicated **UBEC** servo rail — so you can see what the bot sees and look around without moving the body.
