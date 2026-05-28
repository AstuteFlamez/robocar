# Phase 6 — Lidar setup & visualization

## Goal
The STL-19P lidar is wired up, spinning, and streaming scan data to the Pi 5. The web UI gains a "lidar view" — a top-down polar plot updating in real time. No SLAM yet; just verifying the lidar works end-to-end and you can see what it sees.

## Difficulty
Medium · 1 weekend

## Prerequisites
- Phase 5 complete (camera and pan/tilt working)

## New parts used this phase
- LDROBOT STL-19P (already have)
- Mounting hardware to put the lidar on top of the bot (3D-printed plate, standoffs, or a flat plywood disc)

## Two wiring paths for the STL-19P

The STL-19P module has a small ZH1.5T-4P 4-pin connector with these signals:

| Pin | Signal | Notes                              |
|-----|--------|-------------------------------------|
| 1   | TX     | 3.3V UART output, 230400 baud      |
| 2   | PWM    | input — controls motor speed       |
| 3   | GND    | ground                              |
| 4   | 5V     | power, ~300 mA                     |

You have two choices:

### Option A — STL-19P with the official USB-UART adapter (D300/D500 kit)
If your purchase included the small USB adapter board, just plug a USB-A cable from the adapter into the Pi 5. The Pi sees the lidar as `/dev/ttyUSB0`. The adapter handles power, PWM speed control, and UART-to-USB internally. **Easier path — pick this if you have the option.**

### Option B — Bare STL-19P, wire directly to Pi 5 UART
Wire the 4 pins to the Pi 5's GPIO header. The Pi 5 sees the lidar on `/dev/ttyAMA0` (or another UART). You'll have to drive the PWM pin yourself or tie it to a fixed level for default speed.

Both paths produce the same data stream. Software is identical except for which device file you open.

## Schematic — Option A (USB adapter)

```
                ┌──────────────────┐
                │  USB power bank  │
                └────────┬─────────┘
                         │ USB-C
                         ▼
       ┌────────────────────────────────────┐
       │           Raspberry Pi 5           │
       │                                    │
       │  USB ports        I2C-1            │
       └───┬─────┬──────────────┬───────────┘
           │     │              │
           │     │              ▼
           │     │       (IMU + PCA9685 unchanged)
           │     │
           │     └─── micro-USB ──► Pico 2W
           │                        (unchanged)
           ▼
   ┌──────────────────┐
   │  STL-19P with    │  ── 4-pin cable ── attached
   │  USB adapter     │     to lidar module on top
   │  (/dev/ttyUSB0)  │     of the bot
   └──────────────────┘
```

## Schematic — Option B (direct UART)

```
                ┌──────────────────────────────────┐
                │         Raspberry Pi 5           │
                │                                  │
                │   GPIO14 = UART0 TX  (hdr pin 8) │
                │   GPIO15 = UART0 RX  (hdr pin 10)│
                │   5V                (hdr pin 4)  │
                │   GND               (hdr pin 6)  │
                └─┬──────────┬──────────┬──────────┘
                  │          │          │
                 5V         GND     GPIO15 RX
                  │          │          │
                  ▼          ▼          ▼
       ┌──────────────────────────────────┐
       │       STL-19P bare module        │
       │   (pin 4)   (pin 3)   (pin 1)    │
       │     5V      GND        TX        │
       │                                  │
       │     PWM (pin 2) — see note below │
       └──────────────────────────────────┘
```

For PWM (pin 2): if you don't want speed control, tie it to 3.3V with a 10 kΩ pullup; the lidar runs at default speed (~10 Hz scan). If you want to control speed from software, connect it to a hardware-PWM-capable Pi GPIO (e.g., GPIO12 / header pin 32) and feed a 30 kHz PWM with duty ~40% for 10 Hz scan rate.

## Wire-by-wire connection list — new wires this phase

Pick ONE of the two options below depending on what you have.

### Option A — STL-19P with the USB adapter board (one cable)

- [ ] `Pi 5 USB-A port` (any free one) ──► `STL-19P USB adapter USB connector` — provides 5V to lidar and creates `/dev/ttyUSB0` on the Pi
- [ ] `STL-19P USB adapter` 4-pin output ──► `STL-19P module` 4-pin input — cable usually comes pre-attached

That's it for Option A. The adapter handles 5V, PWM, and UART internally.

### Option B — Bare STL-19P, direct UART wiring (four wires)

- [ ] `Pi 5 5V` (header pin 4) ──► `STL-19P pin 4` (5V) — power supply, ~300 mA
- [ ] `Pi 5 GND` (header pin 39) ──► `STL-19P pin 3` (GND) — ground
- [ ] `STL-19P pin 1` (TX, 3.3V UART out) ──► `Pi 5 GPIO15 / RXD` (header pin 10) — lidar talks, Pi listens
- [ ] `STL-19P pin 2` (PWM in) ──► `Pi 5 3V3` (header pin 17) **via 10 kΩ resistor** — fixed default speed (~10 Hz)

  *— OR, if you want software speed control —*

- [ ] `STL-19P pin 2` (PWM in) ──► `Pi 5 GPIO12` (header pin 32) — drive with 30 kHz hardware PWM, ~40% duty for 10 Hz scan

(Note: STL-19P UART is one-way — lidar only sends, Pi only receives. The Pi's TX pin stays unused for the lidar.)

### Pi 5 UART config for Option B

By default the Pi 5's primary UART is wired to the Bluetooth chip. To free it up for the lidar:

In `/boot/firmware/config.txt`, add:
```
enable_uart=1
dtoverlay=disable-bt
```

Disable the serial console:
```bash
sudo systemctl disable serial-getty@ttyAMA0.service
```

Reboot. Lidar is now on `/dev/ttyAMA0` at 230400 baud.

## Mounting the lidar

The STL-19P spins. The whole top half rotates. Critical placement rules:

1. **Up high** — nothing should be at the lidar's beam height within ~5 cm in any direction, or it'll show as a permanent obstacle. The lidar beam height is roughly the middle of the spinning section (~15 mm from the base of the module).
2. **Clear 360°** — no antennas, USB cables, or chassis posts blocking. If the camera is taller than the lidar, the lidar will see the camera on every scan.
3. **Level** — tilt = distorted distance. Use a small bubble level.
4. **Forward marked** — the lidar has a 0° reference direction. Mark which way that is on the chassis; it determines the rotation of all map data later.

Standard mounting: a 3D-printed disc plate on M3 standoffs above the rest of the electronics, with the camera offset behind the lidar so it doesn't block scans.

## Software task list

### 1. Install a driver

Three options:

a) **ldlidar_stl_ros2** (if using ROS 2). Official LDROBOT package. Heavy but full-featured.
b) **ldlidar Python** — community Python parser. Lighter. Search GitHub for `ldrobot-lidar` or `pyldlidar`; the parser referenced in the search results above (Nannigalaxy/ldrobot-lidar) is one option.
c) **Roll your own parser** — the protocol is documented (DTOF packet format, CRC8 checksum). Useful if you want to learn what's happening but unnecessary for a working build.

Start with (b). You can rewrite later if needed.

### 2. Parse scans

The driver gives you angle/distance pairs. A complete scan is ~450 points covering 360°. Update rate is 10 Hz.

```python
for scan in lidar.scans():        # yields one full revolution at a time
    # scan is a list of (angle_deg, distance_mm, intensity)
    points = [(math.radians(a), d / 1000.0) for (a, d, _) in scan if d > 0]
    latest_scan = points          # share with the web UI
```

### 3. Web UI — polar plot

Add a `<canvas>` to the page. Every 100 ms, fetch `/lidar` (returns latest scan as JSON), clear the canvas, plot each point as a dot in polar coordinates (with the bot at the center, lidar 0° pointing up).

```javascript
async function drawLidar() {
  const data = await fetch('/lidar').then(r => r.json());
  ctx.clearRect(0, 0, W, H);
  // draw bot
  ctx.fillRect(W/2 - 5, H/2 - 5, 10, 10);
  // draw points
  const scale = 50;  // px per meter
  for (const [theta, r] of data.points) {
    const x = W/2 + Math.cos(theta - Math.PI/2) * r * scale;
    const y = H/2 + Math.sin(theta - Math.PI/2) * r * scale;
    ctx.fillRect(x, y, 2, 2);
  }
}
setInterval(drawLidar, 100);
```

### 4. Bot-frame orientation

Decide which direction on your chassis is the lidar's "0°". Convention: forward of bot = 0°, CCW positive. Whatever you pick, be consistent — it'll matter when SLAM aligns scans with bot pose.

## Verification checklist

- [ ] **Lidar shows up as serial device.** `ls /dev/serial/by-id/` for USB adapter, or `dmesg | grep tty` after enabling the GPIO UART. Note the device path.
- [ ] **Lidar is spinning.** Power on. The top half should start rotating at ~5–10 Hz within 2 seconds. Audible whirring.
- [ ] **Raw data is flowing.** `cat /dev/ttyXXX | xxd | head` should show streams of bytes. If nothing arrives: baud rate wrong, wires swapped, or PWM pin floating.
- [ ] **Driver opens and parses scans.** Run the driver test script. Should print "got scan with N points" repeatedly.
- [ ] **Point count is sane.** Each scan should have 400–500 points. Far fewer = something's wrong (probably reflective floor or wrong baud).
- [ ] **Polar plot in web UI.** Open the page, see dots forming an outline of the room.
- [ ] **Walls look like walls.** Stand by a wall — should see a straight line of dots ~1 m from the bot (or whatever distance).
- [ ] **Rotate the bot manually.** Dots should rotate accordingly in the polar plot (because the bot rotated, not the world).
- [ ] **No phantom obstacles right at the bot.** If you see dots at <10 cm in any direction, something on the chassis is in the lidar's beam.
- [ ] **Drive the bot, watch the plot.** Walls scroll past as you drive forward. Reassuring.

## Common pitfalls

- **Reflective surfaces (glass, mirrors).** Lidar may report extremely long distances or miss them entirely. Cover with paper for testing if needed.
- **Black/matte surfaces.** Less reflective → shorter effective range. The 12 m spec assumes white walls.
- **Sunlight outdoors.** STL-19P is anti-glare rated, but bright direct sun still causes dropouts. Mostly an indoor sensor.
- **Forgot to disable Bluetooth on the Pi 5.** UART is unusable if BT is using it. The `dtoverlay=disable-bt` line in config.txt is non-optional for Option B.
- **PWM floating in Option B.** Lidar spins erratically or won't start. Tie to 3V3 through a 10k resistor for default speed, or actually drive it from PWM-capable GPIO.
- **Bot vibration shaking the lidar.** Lidar mounted on flexy standoffs vibrates when the bot moves, smearing scan data. Use rigid mounting.
- **Bot pose not aligned with scan frame.** Phase 7 will catch this in SLAM, but worth noting now: if your bot's "+x is forward" and the lidar's "0° is forward-right", your SLAM map will be rotated. Document the rotation explicitly.

## What's next
Phase 7 takes this scan data and combines it with the pose from Phase 4 to build a 2D occupancy map of the room as the bot drives. That's SLAM — the hardest phase of the project, but you have all the pieces now.
