# Phase 4 — IMU + pose tracking

## Goal
Add the BNO055 IMU. The Pi 5 reads orientation (heading) and combines it with wheel odometry to track the bot's pose — `(x, y, θ)` in a world frame — in real time. The web UI shows the current heading as a compass.

## Difficulty
Medium · 1 weekend

## Prerequisites
- Phase 3 complete (teleop works, telemetry flowing)

## New parts used this phase
- BNO055 IMU breakout board

## Why BNO055 specifically
The BNO055 is a 9-axis IMU (accel + gyro + magnetometer) with a *built-in sensor fusion ARM chip*. It outputs already-fused orientation as Euler angles or quaternions, with the Kalman/Madgwick math handled inside. Reading it is just an I2C transaction; you don't need to implement filter math yourself. Other IMUs (MPU-6050, ICM-20948) are cheaper but you write the fusion code yourself. For this build, BNO055 saves you a week.

## Schematic — current state of the build

```
                ┌──────────────────┐
                │  USB power bank  │  5V / 3A+
                └────────┬─────────┘
                         │ USB-C
                         ▼
       ┌────────────────────────────────┐
       │       Raspberry Pi 5           │
       │                                │
       │       USB         I2C-1        │
       └────────┬─────────────┬─────────┘
                │             │ SDA = GPIO2 (header pin 3)
                │             │ SCL = GPIO3 (header pin 5)
                │             │ 3V3 (header pin 1)
                │             │ GND (header pin 6)
                ▼             │
       ┌──────────────────┐   │
       │     Pico 2W      │   │
       └──┬───────────────┘   │
       (motor/encoder wiring  │
        unchanged from        │
        previous phases)      │
                              ▼
                     ┌──────────────────┐
                     │  BNO055 IMU      │
                     │  I2C addr 0x28   │
                     │                  │
                     │  VIN ◄── 3V3     │  (board has on-board regulator,
                     │  GND ◄── GND     │   but using 3V3 keeps logic clean)
                     │  SDA ◄── GPIO2   │
                     │  SCL ◄── GPIO3   │
                     └──────────────────┘

   (motor/encoder/battery wiring unchanged from Phase 2)
```

## New pin assignments — Pi 5 → BNO055

| Pi 5 pin           | BNO055 pin | Function       |
|--------------------|------------|----------------|
| 3V3 (header pin 1) | VIN        | logic power    |
| GND (header pin 6) | GND        | ground         |
| GPIO2 (header pin 3) | SDA      | I2C data       |
| GPIO3 (header pin 5) | SCL      | I2C clock      |

Some BNO055 breakouts have an `ADR` jumper that switches between 0x28 and 0x29. Leave it at 0x28 (default).

## Wire-by-wire connection list — new wires this phase

Just four wires to the IMU. All to the Pi 5's GPIO header.

### Pi 5 → BNO055 IMU

- [ ] `Pi 5 3V3` (header pin 1) ──► `BNO055 VIN` — logic supply (do NOT use 5V on bare chips)
- [ ] `Pi 5 GND` (header pin 6) ──► `BNO055 GND` — ground
- [ ] `Pi 5 GPIO2 / SDA1` (header pin 3) ──► `BNO055 SDA` — I2C data
- [ ] `Pi 5 GPIO3 / SCL1` (header pin 5) ──► `BNO055 SCL` — I2C clock

## Pi 5 I2C — one-time setup

Enable I2C if you haven't:
```bash
sudo raspi-config nonint do_i2c 0
```
Reboot. Then verify:
```bash
i2cdetect -y 1
```
You should see `28` in the grid. If not, check wiring (SDA/SCL not swapped) and 3V3 (not 5V).

## Software task list

### 1. Install the library
```bash
pip install adafruit-circuitpython-bno055
```

### 2. Initialize and read

```python
import board, busio, adafruit_bno055
i2c = busio.I2C(board.SCL, board.SDA)
imu = adafruit_bno055.BNO055_I2C(i2c)

heading, roll, pitch = imu.euler  # degrees
qx, qy, qz, qw = imu.quaternion
```

The BNO055 takes ~7 seconds after power-on to fully calibrate. During calibration, `imu.calibration_status` returns a tuple of (sys, gyro, accel, mag) calibration levels 0–3. Aim for at least 2/3 on each before trusting headings.

### 3. Calibration procedure (one time per power cycle)

For magnetometer: rotate the bot through a figure-8 pattern in the air.  
For gyro: leave it stationary for a few seconds.  
For accel: tilt it to several orientations (flat, on each side, upside down briefly).

The chip auto-calibrates in the background — you just need to give it enough motion variety. Once `calibration_status` shows good values, save them to disk and reload on startup:

```python
data = imu.calibration_data  # 22 bytes
# save to /home/pi/robocar/imu_cal.bin

# On next boot, after a brief stillness:
imu.mode = adafruit_bno055.CONFIG_MODE
imu.calibration_data = saved_data
imu.mode = adafruit_bno055.NDOF_MODE
```

### 4. Pose tracking from odometry + IMU heading

Standard differential-drive dead reckoning, but use IMU heading instead of integrated gyro from wheel speeds (much less drift).

State: `x, y, θ`. At each timestep:
```python
# v_left and v_right come from Pico telemetry
v = (v_left + v_right) / 2.0           # linear m/s
theta = math.radians(imu.euler[0])     # heading from IMU (handle wraparound)

dx = v * math.cos(theta) * dt
dy = v * math.sin(theta) * dt

x += dx
y += dy
# theta is set absolutely from the IMU — not integrated
```

This is hybrid odometry: wheel encoders give you forward velocity (reliable), IMU gives you heading (drift-free thanks to magnetometer). The combination is the simplest pose estimator that actually works on a hobby robot.

### 5. Web UI additions

Add a small compass widget. SVG circle with a triangle that rotates to match `θ`. Updates from `/telemetry` (which now also returns pose).

Updated `/telemetry` response:
```json
{
  "pose": {"x": 1.23, "y": -0.45, "theta_deg": 87.3},
  "wheels": {"fl": 0.32, "rl": 0.31, "fr": 0.32, "rr": 0.31},
  "imu_cal": [3, 3, 3, 3]
}
```

### 6. Mount the IMU properly

The BNO055 needs to be:
- **Flat and level** on the chassis (its accelerometer/gyro reference assumes that)
- **Away from motors and magnets** — the magnetometer is sensitive. At least 5 cm from the motor cans and away from any large iron screws.
- **Securely fastened** — vibration creates noise in accel readings

Use plastic standoffs, not metal. Avoid mounting directly above a motor.

## Verification checklist

- [ ] **`i2cdetect -y 1` shows 0x28.** If not, fix wiring before anything else.
- [ ] **`imu.euler` returns sane values.** Heading should change as you rotate the bot. Pitch/roll should change when you tilt it.
- [ ] **Calibration reaches 3/3/3/3.** Do the figure-8 dance. If mag won't go above 1, you're near something magnetic — move away from the laptop, then retry.
- [ ] **Calibration save/load works.** Save calibration, power cycle, load it, confirm `calibration_status` reports good values without manual recalibration.
- [ ] **Heading matches reality.** Point the bot's "forward" axis north (use phone compass for ground truth). `imu.euler[0]` should read ~0° (or whatever your conventions are — verify and document).
- [ ] **Heading is stable while still.** Place bot on the floor, don't touch it for 60 seconds. Heading shouldn't drift more than ~1°.
- [ ] **Pose tracking works for a straight line.** Drive forward 1 meter (measured with a tape measure). `x` (or `y`, depending on orientation) should be within 5% of 1.0.
- [ ] **Pose tracking works for a turn.** Spin in place 360° (visually). `theta` should return to within ~5° of where it started.
- [ ] **Compass widget rotates in web UI.** Bot heading and compass match as you rotate the bot.

## Common pitfalls

- **5V on VIN with a non-regulated breakout.** Some breakouts have a regulator (Adafruit's does), others don't. If yours has just bare BNO055 pins, you *must* use 3V3, not 5V. When in doubt: 3V3 is always safe for the chip itself.
- **Magnetometer near motors.** If heading goes nuts the moment you start driving, the motor magnetic field is bleeding into the magnetometer. Move the IMU further away or shield with a small grounded metal plate (counterintuitive but works for AC fields).
- **Calibration not persisted.** Without saving/loading calibration, every power-cycle requires the figure-8 dance. Annoying.
- **Heading wraparound bugs.** `imu.euler[0]` returns 0–360 (or sometimes -180 to 180 depending on firmware). When you subtract two headings, handle the wraparound: `delta = ((a - b + 180) % 360) - 180`.
- **Integrating heading from gyro instead of using fused output.** Drifts ~1°/sec. Use `imu.euler` or `imu.quaternion` — those are the fused outputs.
- **I2C clock stretching.** The BNO055 sometimes stretches the I2C clock and clones may not handle it well. If you see weird `OSError` on reads, slow the I2C bus down (`dtparam=i2c_arm_baudrate=100000` in `/boot/firmware/config.txt`).
- **Pose drift on bumpy floor.** Wheel odometry assumes no slip. On carpet or uneven floors, position drifts. Lidar correction in Phase 7 fixes this.

## What's next
Phase 5 adds the Pi Camera v3 on a pan/tilt mechanism, streaming video into the web UI. After this phase, you can see what the bot sees, look around without moving the bot, and start setting up the visual side of autonomy.
