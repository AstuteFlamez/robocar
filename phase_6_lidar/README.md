# Phase 6 — Lidar setup & visualization

## Goal

By the end of this phase your robot can **see the shape of the room**. The
LDROBOT **STL-19P** spinning lidar comes up cleanly on the Pi as
`/dev/ttyUSB0`, its rotor whirs at a steady ~10 Hz, and a live **360° polar
plot** appears in the web UI you've been growing since Phase 3 — a ring of dots
that traces the walls, doorways, and your own legs as you walk around the bot.
No mapping yet (that's SLAM, Phase 7). This phase is about getting the raw
sensor working *correctly* and *visibly*, and understanding exactly what comes
out of it byte-for-byte.

A quick vocabulary primer, because the whole phase rests on these:

- **Lidar** = *Light Detection And Ranging*. You fire a laser at a surface and
  measure how long the reflection takes to come back. Multiply that time by the
  speed of light and you get distance. The STL-19P spins a single laser+detector
  around a vertical axis, so over one revolution you get a *fan* of distances —
  a 2D slice of the room at the height of the laser.
- **DTOF** = *Direct Time-of-Flight*. The STL-19P literally times the photon
  round-trip with a fast timer (as opposed to "indirect"/phase-based sensors
  that infer distance from a modulated beam's phase shift). DTOF is robust to
  ambient light and gives honest absolute distances — that's why it's the
  workhorse for indoor mapping.
- **UART** = *Universal Asynchronous Receiver/Transmitter*. A two-wire (TX/RX)
  serial link with no shared clock — both ends just agree on a **baud rate**
  (bits per second) and a framing format. The STL-19P talks UART at **230400
  baud, 8N1** (8 data bits, No parity, 1 stop bit) and it's **one-way**: the
  lidar only ever *transmits*. It never listens, so we only wire its TX.

## Difficulty / Time

**Medium · 1 weekend.** The wiring is four little JST-ZH pins and a USB plug —
trivial. The thinking is in the *logic-level matching* (one wrong jumper toasts
the receiver over time), the *PWM-pin-to-GND* detail (get it wrong and the rotor
won't spin), and getting the parser + web canvas to render a clean ring. The
optional roll-your-own packet parser turns this into a deep weekend.

## Prerequisites

- **Phase 5 complete** — camera streaming and pan/tilt servos working, the web
  UI (Flask) is live on `http://<pi-ip>:5000`, and the I²C bus carries the
  PCA9685 (0x40) and INA219 (0x41). We add a new panel to that same UI.
- The Pi is powered from the **D24V50F5 5 V/5 A buck** (Phase 3), not a phone
  bank — the lidar's ~290 mA draw is part of why the 600 mA USB cap had to go.
  See [power_budget.md](../_engineering/power_budget.md).
- You have the **CP2102 USB-UART adapter** (the one new buy this phase) and a
  multimeter to *verify the adapter's logic jumper is set to 3.3 V before you
  plug anything into the lidar.*

## New parts this phase

| Part | One-line reason |
|---|---|
| **LDROBOT STL-19P** (already have, kit) | The 360° DTOF lidar — your room-scanner. In the kit it was wired to the RRC Lite, so it does **not** arrive as a USB stick. |
| **CP2102 USB-UART adapter, 3.3 V** (buy, ~$7) | Bridges the lidar's 3.3 V UART to a clean `/dev/ttyUSB0` and supplies its 5 V — *with the jumper on 3.3 V to protect the receiver.* |
| **JST-ZH 1.5 mm 4-pin pigtail** (often ships with the lidar/adapter) | Mates the STL-19P's tiny connector; lets you land Pin1/2/3/4 on the CP2102 + a GND jumper. |
| **Rigid lidar mount** (3D-printed disc / standoffs, from shelf) | Holds the lidar level and high so the chassis doesn't sit in its beam plane. |

Full master list: [_engineering/products_to_buy.md](../_engineering/products_to_buy.md).

## Cumulative system state (where the build is now)

Picture the robot as it stands entering Phase 6. It's a real two-brain machine:

The **2S LiPo (7.4 V)** feeds a fused, reverse-protected, master-switched bus
(Phase 1). Three converters hang off it: the **D24V50F5** buck makes a clean
**5 V/5 A** rail for the **Pi 5**, the two **TB6612FNG** drivers take **7.4 V**
straight onto their VM pins to push the four **310 motors**, and a **UBEC**
makes an isolated **5 V** rail for the servos. An e-stop sits in series with the
*motor* branch only, so hitting it coasts the wheels while the brains stay alive.

The **Pico 2W** (Phase 1–2) is the real-time floor of the system: it generates
20 kHz PWM into the TB6612s, decodes all four quadrature encoders in hardware
via **PIO**, and closes a **per-wheel PID** loop. Its `3V3(OUT)` pin powers the
driver logic and the encoder VCCs (encoders at **3.3 V**, never 7.4 V, so their
A/B outputs stay inside the Pico's GPIO limit). It speaks to the Pi over a
single USB cable carrying both power and serial.

On the Pi side (Phases 3–5), a Flask **web UI** drives the bot over WiFi using
**Mecanum kinematics** — you command a body twist `(vx, vy, ωz)` and the Pi maps
it to four independent wheel speeds (this is a *holonomic* chassis: it can
strafe sideways, not just turn like a car). The **BNO055 IMU** streams
absolute heading over **UART** (deliberately *not* I²C — the BNO055 stretches
the I²C clock and the Pi's controller mishandles it). The **I²C-1** bus carries
the **PCA9685** servo driver (0x40, pan/tilt camera gimbal) and the **INA219**
power monitor (0x41). A **Camera Module 3** on the gimbal streams MJPEG to the UI.

**Phase 6 adds one thing: a USB port now hosts the CP2102 → STL-19P lidar at
`/dev/ttyUSB0`.** Nothing else changes. The lidar lives entirely on its own USB
link; it does not touch the GPIO header, the I²C bus, or the motor rail.

## Why the CP2102, and why the 3.3 V jumper *matters*

The original plan assumed "Option A: the lidar shows up as a USB stick, just
plug it in." On *this* kit it doesn't — the STL-19P was wired into the RRC Lite
controller you're bypassing, so it arrives as a bare module with a 4-pin
JST-ZH 1.5 mm pigtail and *no* USB bridge. You supply one. (See the decision in
[engineering_decisions.md](../_engineering/engineering_decisions.md).)

Two bridge options exist:

1. **CP2102 USB-UART adapter (the path we take).** A ~$7 board with a Silicon
   Labs CP2102 chip that turns UART into USB. The Pi enumerates it as
   `/dev/ttyUSB0` with zero config — Bluetooth and the serial console stay
   untouched. It also provides the lidar's 5 V from the USB bus. **Crucially it
   has a logic-level jumper: set it to 3.3 V, not 5 V.**

2. **Direct Pi GPIO UART (advanced alternative).** Wire the lidar TX straight to
   the Pi's RXD (GPIO15, header pin 10). This works but you must *disable
   Bluetooth* (`dtoverlay=disable-bt`) and the serial console, because the Pi's
   primary PL011 UART is shared with the BT chip by default. More fragile, and
   it ties up GPIO you may want later. We document it in `wiring_diagram.md` as a
   fallback, but the CP2102 is the recommended bring-up path.

**The 3.3 V jumper is the load-bearing detail.** The STL-19P's TX pin swings
**0–3.3 V** and the datasheet says the signal lines are **3.3 V TTL and NOT
5 V tolerant.** The CP2102's logic level sets the voltage of its own RX input
threshold *and* (on most boards) the level it would drive at — but more
importantly, matching the adapter to 3.3 V guarantees you never present a 5 V
edge to a 3.3 V-only pin. Drive a 3.3 V receiver with 5 V logic and you forward-
bias its input protection diodes; it might survive a demo and then die a slow
death from cumulative stress. **Set the jumper to 3.3 V and confirm with a
multimeter (VCCIO pin reads ~3.3 V) before the lidar ever sees the adapter.**
This is the same 3.3 V-world discipline from [safety.md](../_engineering/safety.md)
that keeps the encoder VCC at 3.3 V.

> ⚠️ Note: the CP2102's 5 V output to the lidar comes from the **USB bus**
> (Pin4 +5V), while the *logic/UART* side runs at **3.3 V**. Those are two
> different rails on the same little board — same spirit as "PCA9685 VCC ≠ V+."

## Pin tables

### STL-19P JST-ZH 1.5 mm 4-pin connector → CP2102 adapter

This is the only new wiring in the phase. From
[device_contracts.md](../_engineering/device_contracts.md):

| Lidar pin | Signal | Direction | → Goes to | Why |
|---|---|---|---|---|
| **Pin 1** | TX (3.3 V UART out, 230400 8N1) | lidar → adapter | CP2102 **RXD** | The lidar talks; the Pi (via CP2102) listens. One-way link. |
| **Pin 2** | PWM (motor-speed input) | into lidar | **GND** (tie low) | Pulled to GND selects the default **closed-loop ~10 Hz** spin. Floating = erratic / no spin. |
| **Pin 3** | GND | — | CP2102 **GND** | Signal + power return. |
| **Pin 4** | +5 V (~290 mA) | into lidar | CP2102 **5V** (from USB bus) | Powers the laser + motor. *Not* from the motor rail. |

> The CP2102's TXD pin is **unused** — the lidar never receives. Leave it
> disconnected.

### Pi 5 — buses & power (full state, unchanged this phase)

For reference; Phase 6 adds nothing to this header. From
[device_contracts.md](../_engineering/device_contracts.md):

| Function | Pin(s) | Device |
|---|---|---|
| I²C-1 SDA (GPIO2) | 3 | PCA9685 (0x40), INA219 (0x41) |
| I²C-1 SCL (GPIO3) | 5 | PCA9685 (0x40), INA219 (0x41) |
| UART0 TXD (GPIO14) | 8 | → BNO055 RX |
| UART0 RXD (GPIO15) | 10 | ← BNO055 TX |
| 3.3 V | 1, 17 | BNO055 VIN, PCA9685 VCC, INA219 VCC |
| GND | 6, 9, 14, 20, 25, 30, 34, 39 | common ground (star) |
| USB-A ×4 | — | Pico, **CP2102 → lidar (new this phase)**, (opt) depth cam |
| USB-C | — | ← D24V50F5 5 V/5 A buck |
| CSI (22-pin) | — | Camera Module 3 via #5820 cable |

## Scan rate vs. point density — the one number that shapes everything

The STL-19P fires its laser at a fixed *sampling* rate and spins at a fixed
*rotation* rate. Those two numbers divide to give you **points per revolution**:

```
points_per_scan  ≈  sample_rate (Hz)  /  rotation_rate (rev/s)
```

The STL-19P samples roughly **4500 points/second** and spins at **~10 Hz**, so
each full 360° scan carries about **450–500 points** — one every ~0.8°. That's
the number you'll *measure* in the lab below, and it's a real design knob: if you
ever drove the PWM pin to spin *faster*, you'd trade angular resolution for a
fresher map (fewer points per scan, but more scans per second). For SLAM at
walking pace, the default 10 Hz / ~450 points is the sweet spot — dense enough to
see a doorway, fresh enough that the room hasn't moved much between scans. This
is exactly the kind of sampling-vs-update-rate tradeoff you'll see again in
control loops and cameras.

## Software task list

We're growing the same Flask app from Phases 3–5. The plan: open the serial
port, run a background thread that parses scans into `(angle, distance)` lists,
expose the latest scan as JSON, and draw it on a `<canvas>`.

### 1. Identify the serial device

```bash
# Before plugging the lidar in, list what's there:
ls /dev/ttyUSB* 2>/dev/null
# Plug in the CP2102, then again — a new node appears (usually /dev/ttyUSB0):
ls /dev/ttyUSB*
dmesg | grep -i cp210x          # confirms the CP2102 driver bound
# Stable name that survives reboots / re-plugging:
ls -l /dev/serial/by-id/
```

Prefer the `/dev/serial/by-id/...` path in your code — `/dev/ttyUSB0` can
renumber if you add another USB-serial device.

### 2. Sanity-check raw bytes (before any driver)

```bash
# Configure the port and dump hex. You should see a torrent of bytes,
# with the STL-19P packet header 0x54 appearing regularly.
stty -F /dev/ttyUSB0 230400 raw
timeout 1 cat /dev/ttyUSB0 | xxd | head
```

If `xxd` shows nothing: PWM pin floating (rotor not spinning), TX↔RX swapped,
wrong baud, or the 3.3 V jumper / power is off. If you see bytes but no `54`
recurring, baud is likely wrong.

### 3. Install / use a community driver (the fast path)

The pragmatic route is the community **LDROBOT** Python driver (search GitHub
for `ldrobot` / `pyldlidar`). It already implements the STL-19P packet format
and CRC, and hands you whole scans:

```python
# Conceptual API — exact names vary by driver. The shape is what matters:
import math
from ldlidar import LD19           # community driver class

lidar = LD19(port="/dev/ttyUSB0", baudrate=230400)
lidar.start()

latest_scan = []                   # shared with the web UI (guard with a lock)

for scan in lidar.scans():         # each yield = one full revolution
    # scan: list of (angle_deg, distance_mm, intensity)
    pts = [(math.radians(a), d / 1000.0)      # → (radians, meters)
           for (a, d, _) in scan if 0 < d < 12000]   # drop 0 / out-of-range
    latest_scan = pts
```

`d > 0` drops "no return" points; the `< 12000` clamp drops the occasional wild
reading beyond the sensor's ~12 m spec.

### 4. Serve the latest scan as JSON

```python
import threading
from flask import jsonify

scan_lock = threading.Lock()
shared_scan = []

def lidar_thread():
    global shared_scan
    for scan in lidar.scans():
        pts = [(math.radians(a), d/1000.0) for (a,d,_) in scan if 0 < d < 12000]
        with scan_lock:
            shared_scan = pts

threading.Thread(target=lidar_thread, daemon=True).start()

@app.route("/lidar")
def lidar_json():
    with scan_lock:
        return jsonify(points=shared_scan)      # [[theta_rad, r_m], ...]
```

### 5. Web UI — the polar `<canvas>`

Add a canvas next to the camera feed. Poll `/lidar` ~10× per second, clear, and
plot each point. Put the bot at center with the lidar's 0° pointing **up**.

```html
<canvas id="lidar" width="400" height="400" style="background:#111"></canvas>
```

```javascript
const cv = document.getElementById("lidar");
const ctx = cv.getContext("2d");
const W = cv.width, H = cv.height, SCALE = 50;   // pixels per meter

async function drawLidar() {
  const data = await fetch("/lidar").then(r => r.json());
  ctx.clearRect(0, 0, W, H);
  // range rings every 1 m
  ctx.strokeStyle = "#333";
  for (let r = 1; r <= 4; r++) {
    ctx.beginPath(); ctx.arc(W/2, H/2, r*SCALE, 0, 2*Math.PI); ctx.stroke();
  }
  // the bot
  ctx.fillStyle = "#0f0";
  ctx.fillRect(W/2 - 4, H/2 - 4, 8, 8);
  // scan points — rotate so 0° (forward) points UP on screen
  ctx.fillStyle = "#0ff";
  for (const [theta, r] of data.points) {
    const x = W/2 + Math.cos(theta - Math.PI/2) * r * SCALE;
    const y = H/2 + Math.sin(theta - Math.PI/2) * r * SCALE;
    ctx.fillRect(x, y, 2, 2);
  }
}
setInterval(drawLidar, 100);     // 10 Hz, matches the scan rate
```

### 6. Define the bot-frame 0° (you'll thank yourself in Phase 7)

The lidar has a physical 0° reference printed on the housing. Decide *now* how it
maps to the robot body frame from Phase 3's kinematics. **Convention for this
build: bot forward = +x = 0°, counter-clockwise positive** (matches the Mecanum
`(vx, vy, ωz)` frame). If the lidar's 0° doesn't point straight forward, record
the constant angular offset — SLAM in Phase 7 needs to rotate every scan into the
body frame, and a silent 30° offset there produces a map that's rotated 30° and
hours of confusion.

### 7. (Optional learning module) Roll your own STL-19P packet parser with CRC8

High résumé payoff, zero added cost. Capture a few seconds of raw bytes
(`timeout 3 cat /dev/ttyUSB0 > scan.bin`) and parse it **offline** against that
file — no spinning hardware needed while you debug the bit-twiddling. The
STL-19P packet (47 bytes, fixed):

```
byte  0      : 0x54                    header
byte  1      : 0x2C                    VerLen (0x2C = 12 points/packet)
bytes 2..3   : speed (deg/s, LE u16)   ← your spin-rate readout
bytes 4..5   : start angle (0.01°, LE) 
bytes 6..41  : 12 × { distance (LE u16, mm), intensity (u8) }   = 36 bytes
bytes 42..43 : end angle (0.01°, LE)
bytes 44..45 : timestamp (LE u16)
byte  46     : CRC8                     over bytes 0..45
```

```python
def parse_packet(buf):                 # buf = 47 bytes
    assert buf[0] == 0x54 and buf[1] == 0x2C
    if crc8(buf[:46]) != buf[46]:
        return None                    # corrupt — drop it
    speed = buf[2] | (buf[3] << 8)             # deg/s  → /360 = Hz
    a0    = (buf[4] | (buf[5] << 8)) / 100.0   # start angle, deg
    a1    = (buf[42] | (buf[43] << 8)) / 100.0 # end angle, deg
    n = 12
    span = (a1 - a0) % 360
    pts = []
    for i in range(n):
        off = 6 + i*3
        dist = buf[off] | (buf[off+1] << 8)    # mm
        inten = buf[off+2]
        ang = (a0 + span * i/(n-1)) % 360       # linear-interp the 12 angles
        pts.append((ang, dist, inten))
    return speed, pts
```

You supply `crc8()` from the LDROBOT-published lookup table. Validate by
comparing your parsed angles/distances against the community driver on the *same*
captured `scan.bin`. When they match byte-for-byte, you understand the sensor.

## Verification checklist

Do these **in order** — each assumes the previous passed.

- [ ] **Jumper checked FIRST.** With the lidar *unplugged*, set the CP2102 jumper
      to **3.3 V** and confirm with a multimeter that the adapter's VCCIO/3V3 pin
      reads ~3.3 V (not 5 V). Only then connect the lidar.
- [ ] **PWM pin tied to GND.** Confirm Lidar Pin2 is jumpered to GND (continuity
      beep to the adapter GND). Floating here = no/erratic spin.
- [ ] **Serial device appears.** Plug the CP2102 into the Pi. `ls /dev/ttyUSB*`
      shows a new node; `dmesg | grep -i cp210x` confirms the driver bound. Note
      the `/dev/serial/by-id/` path.
- [ ] **Rotor spins steadily.** Within ~2 s of power you hear a soft, *constant*
      whir. Not pulsing, not silent. The top section is turning.
- [ ] **Raw bytes flow.** `stty -F /dev/ttyUSB0 230400 raw` then
      `timeout 1 cat /dev/ttyUSB0 | xxd | head` shows a stream with `0x54`
      recurring (the packet header).
- [ ] **Driver parses scans.** Run the driver test; it prints "scan: N points"
      repeatedly without CRC/timeout errors.
- [ ] **Point count is sane (~450–500).** Far fewer means wrong baud, a
      reflective floor, or a stalling rotor. This is the Measurement Lab number.
- [ ] **Polar plot renders.** Open the web UI; a ring of cyan dots traces the
      room outline, refreshing smoothly at ~10 Hz alongside the camera feed.
- [ ] **Walls look like walls.** Stand the bot ~1 m from a flat wall — you should
      see a *straight* line of dots at the right radius on the correct side.
- [ ] **Rotating the bot rotates the plot.** Turn the bot by hand; the dot
      pattern rotates with it (because the bot moved, not the world).
- [ ] **No phantom obstacle at the center.** No dots inside ~10 cm. If there are,
      something on the chassis (camera, cable, post) sits in the beam plane.
- [ ] **Drive + scan together.** Drive slowly; walls scroll past in the plot
      while the camera streams. Both run without stutter, and
      `vcgencmd get_throttled` still reads `0x0` (the lidar's 290 mA didn't sag
      the rail).

## Common pitfalls

- **5 V jumper on the CP2102.** The headline footgun. The lidar's TX is 3.3 V and
  not 5 V tolerant; a 5 V adapter can stress/kill the receiver path over time.
  Set 3.3 V and verify before connecting. *Do this first, every time.*
- **PWM pin (Pin2) floating.** The single most common "lidar won't spin"
  cause. It must be tied to **GND** for the default closed-loop 10 Hz. Don't
  leave it dangling, and don't tie it to 3.3 V/5 V unless you're deliberately
  feeding a speed-control PWM.
- **TX/RX swapped.** Lidar **Pin1 (TX) → adapter RXD.** If you wire TX→TX you
  get a dead-silent port even though the rotor spins. Remember it's one-way:
  the adapter's TXD stays unconnected.
- **Powering the lidar from the motor rail.** Pin4 wants **5 V** from the
  adapter's USB, ~290 mA. Never feed it 7.4 V from the LiPo motor bus.
- **`/dev/ttyUSB0` renumbered.** Add another USB-serial gadget and your lidar may
  become `ttyUSB1`. Use the `/dev/serial/by-id/` path in code.
- **Reflective / glass / mirror surfaces.** DTOF reads them as absurdly far or
  drops them entirely — you'll see gaps or stray points. Tape paper over a mirror
  while bench-testing.
- **Black/matte surfaces.** Reflect less light → shorter effective range. The
  ~12 m spec assumes white walls; dark furniture will read short.
- **Direct sunlight.** The STL-19P is anti-glare rated but bright sun still
  causes dropouts. It's an indoor sensor.
- **Flexy mount.** A lidar on wobbly standoffs smears scans when the bot moves.
  Mount it **rigid and level** (bubble-level it).
- **Beam plane blocked by your own robot.** If the camera/gimbal or a USB cable
  is taller than the lidar at its radius, every scan shows a phantom obstacle.
  Mount the lidar **highest**, with a clear 360°.
- **Forgot to define 0°.** A silent angular offset between the lidar's 0° and the
  bot's forward axis produces a rotated SLAM map in Phase 7. Document the offset
  now.

## Measurement Lab

> **Make the invisible visible.** Three measurements turn "I think the lidar
> works" into "I measured it."

1. **Points per scan.** *Instrument:* your parser, counting the length of one
   yielded scan list (or summing 12 per packet across one revolution).
   *Expected:* **~450–500 points** per 360° (≈ one every 0.8°). This is the
   sampling-vs-rotation ratio made real.
2. **Spin rate.** *Instrument:* read the `speed` field (deg/s) from the packet
   header and divide by 360, *or* time N scans with a stopwatch in code.
   *Expected:* **~10 Hz** (≈3600 deg/s), steady — a wandering value means the
   PWM-to-GND tie or power is flaky.
3. **Decode the UART on a logic analyzer.** *Instrument:* the $13 8-ch logic
   analyzer + PulseView, probing the lidar TX line (Pin1) against GND.
   *Expected:* PulseView's UART decoder set to **230400 baud, 8N1** cleanly
   resolves bytes, and you'll spot the recurring **0x54** header — the exact same
   byte your software locks onto. Confirm the idle line sits at **3.3 V**, not
   5 V, proving the logic-level match.

## Phase wrap-up

**What you learned.** How a spinning DTOF lidar turns photon time-of-flight into
a 2D point cloud; how a one-way UART at 230400 8N1 carries that stream; *why*
logic-level matching (3.3 V, not 5 V) protects a receiver over its lifetime, not
just in a demo; the sampling-rate-vs-rotation-rate tradeoff that sets angular
resolution; how to bring up a USB-UART bridge cleanly (and why that beats
fighting the Pi's Bluetooth-shared GPIO UART); and — if you did the optional
module — how to parse a real binary sensor protocol with a CRC8 integrity check,
validated offline against a captured byte stream.

**Résumé bullet (copy-ready).**
> Integrated a 360° DTOF lidar (LDROBOT STL-19P) over a 230400-baud UART via a
> 3.3 V USB-UART bridge, parsing ~450 points/scan at 10 Hz and rendering a live
> polar point cloud in a Flask web UI; wrote a from-scratch packet parser with
> CRC8 validation verified offline against captured sensor data.

**Interview talking point.** "The lidar's TX is 3.3 V and explicitly *not* 5 V
tolerant, so I matched the USB-UART adapter to 3.3 V — driving a 3.3 V receiver
with 5 V logic forward-biases its protection diodes and degrades it over time,
which is the kind of bug that passes the demo and fails in the field. I also tied
the rotor-speed PWM pin to ground for the default closed-loop 10 Hz spin, and I
can explain the sampling-rate-over-rotation-rate math that gives ~450 points per
scan — and why I'd trade points-per-scan for scans-per-second if I needed a
fresher map."

## What's next

**Phase 7 — SLAM.** You now have two ingredients SLAM needs: a *pose* (from
Phase 4's IMU + Phase 2's wheel odometry) and a *scan* (from this phase). SLAM
stitches successive scans together, using the pose to guess where each one
belongs, to build a single consistent 2D occupancy map of the room — and
corrects the pose drift in the process. The 0° convention you defined here is
what lets each scan land in the map at the right rotation.
