# Phase 7 — SLAM Mapping

> **The payoff phase.** You drive the bot around your apartment and a 2-D floor plan of the room *draws itself* on your laptop screen, in real time, with a little triangle showing where the bot thinks it is. At the end you press "save" and you have a map file you can reload tomorrow. No new hardware — every sensor and wire is already in place. This phase is pure software, and it's where every previous phase pays off at once: the motor control (Phase 1), the *measured* encoder constant (Phase 2), the holonomic odometry (Phase 4), and the lidar stream (Phase 6) all feed one algorithm — **SLAM** — that turns them into a map.

## Goal

Run **SLAM** (Simultaneous Localization And Mapping) on the Pi 5. Feed it (a) the 360° lidar scans from Phase 6 and (b) your holonomic odometry from Phase 4, and watch a 2-D **occupancy grid** of the room build itself in the web UI. Overlay the SLAM-corrected pose as a triangle. Save and reload the map as a PGM file. Then prove the algorithm earns its keep with an **odometry-vs-SLAM divergence plot** — raw dead-reckoning drifting away while SLAM stays locked to ground truth.

## Difficulty & time

**Hard · ~2 weekends.** The code is short (SLAM is a library call), but getting a *clean* map is a tuning-and-discipline exercise: feed it good odometry, drive slowly, and debug the handful of frame/convention bugs that rotate or smear your map.

## Prerequisites

- **Phase 6 complete** — lidar spinning, scans parsing to ~450 points/rev at 10 Hz, polar plot live in the web UI, and the lidar's 0° reference direction marked on the chassis.
- **Phase 4 complete** — holonomic pose `(x, y, θ)` from fused BNO055 heading + Mecanum forward-kinematics odometry, streaming in `/telemetry`.
- **Phases 2 & 4 must be *solid*.** This is not optional advice. SLAM's scan matcher corrects *small* odometry errors; it cannot rescue garbage. If your measured `COUNTS_PER_REV` is wrong or your strafe odometry is broken, the map will be too. **Garbage odometry in → garbage map out.** Re-run the Phase 2 distance test and the Phase 4 pure-`vx`/pure-`vy`/pure-`ωz` checks before you blame SLAM.

## New parts this phase

**None.** All software, all running on the Pi 5. See [bom.md](./bom.md) — it exists only to confirm "nothing to buy" and point at the master list.

## Where the build is right now (cumulative system state)

By the end of Phase 6 you have a complete, sensing, drivable robot. In prose:

A single **2S LiPo (7.4 V)** feeds everything through a fused, reverse-protected, e-stopped power tree. The **motor rail** drives two **TB6612FNG** dual H-bridges (one motor per channel → four independent Mecanum wheels), commanded by a **Pico 2W** that runs 20 kHz PWM, decodes four quadrature encoders in **PIO**, and closes a per-wheel PID loop. A **D24V50F5 buck** makes a clean 5 V/5 A rail for the **Pi 5**, which is the high-level brain. The Pi talks to the Pico over USB serial (`v vx vy wz` body-twist commands down, wheel telemetry up), reads the **BNO055** IMU over **UART** for drift-free heading, drives the camera gimbal through a **PCA9685** over I²C, monitors pack health with an **INA219** at 0x41, streams the **Camera Module 3** over CSI, and reads the **STL-19P lidar** over a **CP2102 USB-UART adapter** at `/dev/ttyUSB0`. A Flask web UI already shows live telemetry, a heading compass, the camera feed, and a polar lidar plot.

**This phase adds zero wires.** It adds one new piece of *software* on the Pi — a SLAM node — that consumes two streams you already produce (lidar scans + odometry pose) and emits two new things: a live **occupancy-grid map** and a **corrected pose**.

```
        ┌──────────────────────── Raspberry Pi 5 (Linux, high-level) ────────────────────────┐
        │                                                                                     │
  lidar │   /dev/ttyUSB0          ┌───────────────┐                                           │
  scans ├────────────────────────►│  scan buffer  │──┐                                        │
 (10Hz) │   (Phase 6 parser)      └───────────────┘  │                                        │
        │                                            ▼                                        │
        │   ┌──────────────────┐   (dt, dxy, dθ)  ┌────────────────────┐   map bytes  ┌────────────┐
   USB  │   │ Phase-4 odometry │─────────────────►│   SLAM (BreezySLAM │─────────────►│  web UI:   │
 serial ├──►│ (vx,vy,ωz)->pose │   odom delta     │   RMHC_SLAM)       │   corr. pose │  /map PNG  │
  from  │   └──────────────────┘                  └────────────────────┘─────────────►│  + pose    │
  Pico  │            ▲                                      │                          │  triangle  │
        │            │ BNO055 heading (UART)                ▼ getpos() / getmap()      └────────────┘
        │   ┌──────────────────┐               ┌────────────────────┐                            │
        │   │ BNO055 IMU (UART)│               │  PGM save / load    │                            │
        │   └──────────────────┘               └────────────────────┘                            │
        └─────────────────────────────────────────────────────────────────────────────────────┘
```

## What SLAM actually is (from the ground up)

You already have two things that *almost* let you make a map, and each is broken in a way the other fixes.

**1. You have a map-maker that needs to know where it is.** Plotting a lidar scan into a global picture is easy *if* you know the bot's pose: each scan point is a `(angle, distance)` pair in the bot's frame; rotate it by the heading θ and translate it by `(x, y)`, and you know the world coordinate of whatever the laser hit. Do that for every point of every scan and you've drawn the room. This is the **mapping** half.

**2. You have a pose estimator that drifts.** Your Phase-4 odometry says "the wheels turned this much, the IMU says I'm pointing this way, therefore I'm here." But wheels slip on carpet, the gearbox has backlash, and tiny per-step errors *accumulate* — after driving a loop you might be 40 cm and 15° off from reality even though every individual step looked fine. This is **dead reckoning**, and unbounded drift is its fatal flaw. This is the **localization** problem.

SLAM is the observation that these two problems *solve each other*. **S**imultaneous **L**ocalization **A**nd **M**apping: you can't perfectly map without knowing your pose, and you can't perfectly know your pose without a map — so do both at once, bootstrapping each from the other:

1. **Predict** — use odometry to guess where you moved since the last scan (small step, low error).
2. **Correct (scan matching)** — take the new lidar scan and *slide and rotate it* until it best overlaps the map you've built so far. The shift that makes it line up is the correction to your odometry guess. This is the magic step: the walls in the new scan have to match the walls already on the map, and that geometric constraint pins down your true pose far better than the wheels alone.
3. **Update the map** — plot the corrected scan into the map at the corrected pose.

Repeat at 10 Hz. The map sharpens and the pose stays locked.

### Scan matching, intuitively

Imagine printing the current scan on tracing paper and the map-so-far on the desk. You shuffle the tracing paper around — left/right, up/down, rotate a few degrees — until the printed walls land on the desk's walls. The amount you shuffled is the pose correction. BreezySLAM's matcher (RMHC = Random-Mutation Hill-Climbing) does exactly this numerically: it randomly perturbs the candidate pose, keeps perturbations that make the scan overlap the map better, and converges on the best fit. Because it *starts* from the odometry guess, it only has to search a small neighborhood — which is *why good odometry matters* and why you drive slowly (more on that below).

### Loop closure

The single most satisfying SLAM moment. Drive a big loop around the apartment and come back to your start. Pure dead reckoning would show you ending ~40 cm away from where you began (drift accumulated all the way around). But when SLAM's new scans suddenly match a *much older* part of the map — the corner you started at — the matcher realizes "I've been here before" and snaps the whole pose (and often the whole loop) back into consistency. **Loop closure** is how SLAM keeps error from growing without bound: every time you revisit, the map re-anchors you.

### Occupancy grids

The map itself is an **occupancy grid**: the room chopped into a square lattice of cells (here `MAP_SIZE_PIXELS × MAP_SIZE_PIXELS` cells over `MAP_SIZE_METERS`, so each cell is `MAP_SIZE_METERS / MAP_SIZE_PIXELS` metres wide). Each cell holds one byte = the belief that the cell is occupied:

- **0 (black)** = occupied (a wall/obstacle was seen there)
- **255 (white)** = free (the laser passed *through* that cell to reach something farther)
- **127 (gray)** = unknown (never observed)

Every scan point both *darkens* the cell it hit (something's there) and *whitens* every cell along the ray from the bot to that hit (the beam traveled through empty space — this is **ray-casting**). Over many scans these votes accumulate into crisp walls and clean free space. A grayscale PNG of this byte array *is* your floor plan.

### Why drive SLOW (the single most important operating rule)

A full lidar scan is **one revolution at 10 Hz = 100 ms** to collect. The lidar samples its ~450 points *sequentially* as the rotor spins. If the bot is moving fast during that 100 ms, the early points and late points were taken from *different bot positions* — the scan is **motion-blurred / skewed**, like a panorama photo taken while running. A smeared scan won't cleanly match the map, the corrector either fails or applies a wrong shift, and the map degrades.

Concrete numbers: at the **0.15 m/s** mapping cap, in one 100 ms scan the bot moves **1.5 cm** — negligible against ~5 cm map resolution. At a "remote-control-car" 1 m/s it moves **10 cm per scan** — two map cells of smear, every revolution. *Most beginner SLAM failures are "I drove it like a video game."* Cap velocity in firmware while mapping. (This is also why we keep the lidar rigidly mounted and level from Phase 6 — a wobbling lidar smears scans even when the bot is slow.)

## Mecanum note: strafing is real odometry, not noise

This is a Mecanum chassis. Most SLAM tutorials assume a differential/Ackermann robot that can only go forwards and turn — for them, sideways motion is *impossible*, so any lateral term is treated as noise. **That assumption is wrong for your robot and will corrupt your map if you copy it blindly.** When you command pure `vy`, your bot genuinely **strafes sideways** while pointing the same direction, and the lidar genuinely sees the world translate laterally. That lateral motion is *true* odometry the matcher must know about.

The clean way to feed SLAM is therefore **not** as a velocity model but as a **per-step pose delta**: between two scans you supply `(dt, dxy, dθ)` — the time elapsed, the distance the bot's *center* moved, and how much its heading changed. You compute `dxy` from the full holonomic displacement, including the strafe component:

```
dxy = sqrt(dx_world**2 + dy_world**2)      # magnitude of the 2-D translation, strafe included
```

BreezySLAM's odometry interface is direction-agnostic distance + rotation, so as long as you give it the *true magnitude* of motion (forward **and** lateral), it handles a holonomic robot fine. The bug to avoid is computing `dxy` from forward wheel velocity only (the differential habit) — that throws away every strafe and SLAM will fight your odometry on every sideways move. Build `dxy` from your Phase-4 world-frame `(dx, dy)`, which already includes `vy`.

## Pin tables (reference — nothing new this phase)

No pins change. These are the buses the SLAM node depends on, reproduced from the [canonical map](../_engineering/device_contracts.md#canonical-pin-assignments) so you can sanity-check the data path without flipping documents.

### Pi 5 — sensor buses feeding SLAM

| Function | Pin / port | Device | Role in SLAM |
|---|---|---|---|
| USB-A | any free port | CP2102 → STL-19P | lidar scans @ `/dev/ttyUSB0`, 230400 baud |
| USB-A | any free port | Pico 2W (micro-USB) | wheel telemetry → odometry |
| UART0 TXD (GPIO14) | pin 8 | → BNO055 RX | heading for odometry |
| UART0 RXD (GPIO15) | pin 10 | ← BNO055 TX | heading for odometry |
| 3.3 V | pin 1 / 17 | BNO055 VIN | IMU power |
| GND | pin 6 (+ star) | common ground | reference |

### STL-19P → BreezySLAM `Laser` parameters

| Laser param | Value | Source |
|---|---|---|
| `scan_size` | **measure it** (≈450) | count points in one real revolution (Phase 6) |
| `scan_rate_hz` | 10 | STL-19P closed-loop default spin (PWM pin → GND) |
| `detection_angle_degrees` | 360 | full-circle DTOF |
| `distance_no_detection_mm` | 12000 | STL-19P 12 m max range ([device_contracts](../_engineering/device_contracts.md)) |
| `offset_mm` | 0 (or lidar-to-center distance) | lidar mount offset from wheelbase center |

## Software task list

> Everything runs on the Pi 5, alongside the Phase-6 web server. Keep the SLAM update on its own thread/loop so a slow map render never stalls the lidar reader.

### 1. Install BreezySLAM

We use **BreezySLAM** first: it's a thin Python wrapper over a fast C core, real-time on a Pi 5, and trivial to integrate — exactly right for getting a working map before reaching for heavier ROS-native stacks (slam_toolbox, Cartographer) you'd graduate to only if quality demands it.

```bash
cd ~
git clone https://github.com/simondlevy/BreezySLAM
cd BreezySLAM/python
sudo python3 setup.py install
python3 -c "from breezyslam.algorithms import RMHC_SLAM; print('BreezySLAM OK')"
```

### 2. Describe the sensor (Laser config) — use YOUR measured scan_size

```python
from breezyslam.sensors import Laser

# scan_size MUST match what your Phase-6 driver actually delivers per revolution.
# Count it first:  len(scan_points)  averaged over a few revolutions.
STL19P_SCAN_SIZE = 450          # <-- replace with your measured number

stl19p = Laser(
    scan_size=STL19P_SCAN_SIZE,
    scan_rate_hz=10,            # STL-19P spins at 10 Hz (PWM pin tied to GND)
    detection_angle_degrees=360,
    distance_no_detection_mm=12000,   # 12 m max range, per device_contracts.md
    detection_margin=0,
    offset_mm=0,                # set to lidar-center offset if you want sub-cm accuracy
)
```

### 3. The SLAM loop — odometry as a (dt, dxy, dθ) delta

```python
import math
from breezyslam.algorithms import RMHC_SLAM

MAP_SIZE_PIXELS = 800
MAP_SIZE_METERS = 16            # 16 m square — plenty for a house; bot must never drive off it
slam = RMHC_SLAM(stl19p, MAP_SIZE_PIXELS, MAP_SIZE_METERS)

map_bytes = bytearray(MAP_SIZE_PIXELS * MAP_SIZE_PIXELS)
prev_x = prev_y = prev_th = None
prev_t = None

def angle_wrap(deg):                       # keep heading deltas in [-180, 180]
    return ((deg + 180) % 360) - 180

while mapping:
    scan = lidar.get_latest_scan()          # list of mm distances, length == scan_size
    pose = odom.get_pose()                  # Phase-4 holonomic pose: x_m, y_m, theta_deg
    t = time.monotonic()

    if prev_t is None:
        prev_x, prev_y, prev_th, prev_t = pose.x, pose.y, pose.theta_deg, t
        slam.update(scan)                   # first call: no odometry, just seed the map
        continue

    dt_ms  = (t - prev_t) * 1000.0
    dx     = pose.x - prev_x                # world-frame displacement (includes strafe!)
    dy     = pose.y - prev_y
    dxy_mm = math.hypot(dx, dy) * 1000.0    # MAGNITUDE of 2-D motion: forward + lateral
    dth    = angle_wrap(pose.theta_deg - prev_th)

    slam.update(scan, (dt_ms, dxy_mm, dth)) # predict from odom, correct by scan matching

    x_mm, y_mm, theta_deg = slam.getpos()   # SLAM-CORRECTED pose
    slam.getmap(map_bytes)                  # fill the occupancy grid (0=wall,127=unknown,255=free)

    prev_x, prev_y, prev_th, prev_t = pose.x, pose.y, pose.theta_deg, t
```

> ⚠️ **Sign of `dxy`.** `slam.update` takes *distance moved* — always a non-negative magnitude — plus a separate heading change. Driving backwards is still positive `dxy`; the heading/orientation tells SLAM the direction. Don't sign `dxy` by forward/back, or you'll confuse the matcher.

### 4. Render the map in the web UI

Add a second canvas next to the Phase-6 lidar plot. Every ~500 ms the browser fetches `/map` (a PNG of the occupancy bytes) and draws it, then overlays the SLAM pose as a heading triangle.

```python
from PIL import Image
from io import BytesIO
from flask import send_file

@app.route('/map')
def get_map():
    img = Image.frombytes('L', (MAP_SIZE_PIXELS, MAP_SIZE_PIXELS), bytes(map_bytes))
    buf = BytesIO()
    img.save(buf, format='PNG')
    buf.seek(0)
    return send_file(buf, mimetype='image/png')

@app.route('/slampose')
def slam_pose():
    x_mm, y_mm, theta_deg = slam.getpos()
    # convert mm -> map pixel for the overlay
    px = x_mm / 1000.0 / MAP_SIZE_METERS * MAP_SIZE_PIXELS
    py = y_mm / 1000.0 / MAP_SIZE_METERS * MAP_SIZE_PIXELS
    return {"px": px, "py": py, "theta_deg": theta_deg}
```

### 5. Save & load maps (PGM)

PGM (`P5`) is the dead-simplest grayscale image format and what most map tools expect.

```python
def save_pgm(path, buf, n):
    with open(path, 'wb') as f:
        f.write(b'P5\n')
        f.write(f'{n} {n}\n255\n'.encode())
        f.write(bytes(buf))

def load_pgm(path, n):
    with open(path, 'rb') as f:
        assert f.readline().strip() == b'P5'
        w, h = map(int, f.readline().split())
        assert int(f.readline()) == 255 and w == h == n
        return bytearray(f.read(n * n))

# Reload into a fresh SLAM instance to keep mapping where you left off:
# slam = RMHC_SLAM(stl19p, MAP_SIZE_PIXELS, MAP_SIZE_METERS, map_quality=..., ...)
# slam.setmap(load_pgm('home_map.pgm', MAP_SIZE_PIXELS))
```

### 6. Cap mapping speed in firmware

Add a "mapping mode" flag the Pi sends to the Pico that clamps the body twist to **≤ 0.15 m/s** linear and **≤ 0.3 rad/s** angular. This is the cheapest insurance against motion-blurred scans — enforce it on the real-time brain so a fat-fingered teleop command can't smear your map.

### 7. The mapping procedure (do it this way)

1. Place the bot at a known spot; call it `(0, 0, 0°)`.
2. Drive **slowly** — ≤ 0.15 m/s linear, ≤ 0.3 rad/s angular.
3. Cover the whole space; **revisit** places to give SLAM loop closures.
4. No abrupt accelerations (they jerk the lidar and skew scans).
5. Save the map (`home_map.pgm`) when done.

### 8. Odometry-vs-SLAM divergence plot (the proof scan matching works)

Log two pose streams to CSV during a drive and overlay them: **raw Phase-4 dead reckoning** (no correction, just integrating odometry) and the **SLAM-corrected** pose. Plot both as `x-y` trajectories on the same axes.

```python
# during the run, each step:
csv.writerow([t, odom.x, odom.y, odom.theta_deg,        # raw dead reckoning
                 x_mm/1000, y_mm/1000, theta_deg])       # SLAM-corrected
# afterwards, with matplotlib:
#   plot(odom_x, odom_y)  -> drifts/curls away over the run
#   plot(slam_x, slam_y)  -> stays tight to ground truth
```

Drive a closed loop and the two will visibly separate: dead reckoning spirals off; the SLAM trace closes the loop. That single figure is the most persuasive thing in your whole writeup — it *shows*, not tells, what scan matching buys you.

## Verification checklist

Ordered; each step assumes the previous passed.

- [ ] **BreezySLAM imports.** `python3 -c "from breezyslam.algorithms import RMHC_SLAM"` runs with no error.
- [ ] **`scan_size` matches reality.** Print `len(scan)` from the live lidar; it equals the `scan_size` you put in the `Laser` config (≈450). A mismatch silently breaks scan matching.
- [ ] **First update produces map output.** `slam.getmap(map_bytes)` fills the array; the map starts all gray (127), and within 1–2 s the cells around the bot turn black/white (walls/free).
- [ ] **A known wall appears in the right place.** Park the bot a measured distance from a wall; that wall shows as a dark line at the correct distance and angle on `/map`.
- [ ] **Driving forward extends the map.** Slowly drive 2 m forward; the mapped corridor grows by ~2 m and the pose triangle advances ~2 m.
- [ ] **Strafing extends the map sideways (Mecanum check).** Command pure `vy`; the bot slides sideways and the map/pose move *laterally*, not forward. If a strafe shows up as forward motion or as nothing, your `dxy` is dropping the lateral term — fix the odometry feed.
- [ ] **Rotating in place fills a full circle.** A slow 360° spin fills the area around the bot in all directions with no smear/double walls.
- [ ] **SLAM pose tracks ground truth (Measurement Lab).** From `(0,0,0)`, drive a measured path (e.g. 1 m forward → 1 m right → turn 180°); tape-measure where the bot actually ends and compare to `slam.getpos()`. Target **< ~10 cm position** and **< ~5° heading** error.
- [ ] **Loop closure snaps the pose back.** Drive a closed loop back to start; SLAM pose returns near `(0,0,0)` instead of accumulating drift around the loop.
- [ ] **Divergence plot shows the gap.** The CSV overlay shows raw dead reckoning drifting away while the SLAM trace closes the loop.
- [ ] **Map saves and reloads.** Save mid-session, restart everything, `setmap()` the PGM, drive a bit — new scans integrate into the existing map.
- [ ] **Multiple rooms / doorways.** Map a hallway and two rooms; doorways appear as gaps in walls.

## Common pitfalls

- **Driving too fast.** The #1 cause of bad maps. A scan takes 100 ms; at 1 m/s that's 10 cm of smear per revolution. Enforce the 0.15 m/s cap *in firmware* (Task 6).
- **Garbage odometry in → garbage map out.** If odometry says "10 cm" and reality is "50 cm", the matcher's search window doesn't even contain the truth and it can't recover. **Re-verify Phase 2 (`COUNTS_PER_REV`) and Phase 4 (pure-`vx`/`vy`/`ωz`) before debugging SLAM.** It is almost always upstream.
- **Dropping the strafe term.** Computing `dxy` from forward velocity only (a differential-drive habit) makes SLAM fight your odometry every time you strafe. Build `dxy` from the full world-frame `(dx, dy)` — see the Mecanum note.
- **Lidar 0° ≠ bot forward.** If the lidar's 0° reference (marked in Phase 6) doesn't match the bot's forward axis, the whole scan is rotated relative to the pose and SLAM never converges — or builds a map rotated off your heading. Apply the constant angular offset you documented in Phase 6, or align the mount.
- **`scan_size` mismatch.** The `Laser.scan_size` must equal the actual points/scan your driver yields. Count it; don't trust the datasheet's nominal.
- **Map too small.** If `MAP_SIZE_METERS` is smaller than the space, the bot eventually drives off the grid and SLAM loses lock. 16 m covers most apartments; size generously.
- **Reflective / glass walls.** Mirrors, glass cabinets, shiny floors return bad ranges and warp the map locally. Tape paper over mirrors during initial mapping.
- **Dynamic obstacles.** People and pets get half-baked into the map as smears. Map when the room is still.
- **Lidar mount offset ignored.** For ~5 cm accuracy you can leave `offset_mm=0`; for sub-cm, measure the lidar's distance from the wheelbase center and set it.
- **CPU saturation.** SLAM + camera MJPEG + Flask compete for cores. If scan updates lag, drop camera resolution or skip camera frames while mapping. Keep the SLAM update off the web-request thread.
- **Heading wraparound.** Subtracting two headings across the 0/360° (or ±180°) seam gives a huge bogus `dθ`. Always wrap: `((a - b + 180) % 360) - 180`.

## 🔬 MEASUREMENT LAB — SLAM pose vs ground truth, + the divergence plot

> **Quantity 1:** end-of-path pose error of the SLAM estimate.
> **Quantity 2:** the visual divergence between raw dead reckoning and SLAM over a loop.

**Setup.** Tape an origin mark on the floor and align the bot's forward axis to it; this is `(0, 0, 0°)`. Tape-measure a known path — a clean one is **1.0 m forward → 1.0 m right (strafe) → rotate 180° in place** (this deliberately exercises Mecanum strafe). Drive it slowly while logging both pose streams to CSV (Task 8).

**Instruments.** A tape measure + the floor tape for ground truth; the CSV log + matplotlib for the plot; `slam.getpos()` for the estimate.

**Expected numbers.**
- SLAM end pose vs the tape-measured end pose: **< ~10 cm position error, < ~5° heading error.** (If you're far outside this, suspect odometry or the lidar-forward offset, not SLAM.)
- Divergence plot: the **raw dead-reckoning** trajectory visibly curls away from ground truth over the run; the **SLAM-corrected** trajectory stays tight and closes the loop. The growing gap between the two curves *is* the value of scan matching, drawn.

Screenshot the map, the pose-error numbers, and the divergence plot — all three are portfolio gold.

## PHASE WRAP-UP

**What you learned.** SLAM as the resolution of two mutually-dependent problems (you need a map to localize and a pose to map, so do both at once); scan matching as the geometric correction that beats dead reckoning; loop closure as the mechanism that bounds drift; occupancy grids and ray-casting as the map representation; and the operational reality that a 100 ms scan demands slow, smooth driving. You also learned the system-level truth this whole build was arranged to teach: SLAM is only as good as the odometry beneath it, and a Mecanum robot's *strafe is real odometry* — sideways motion is signal, not noise.

**Résumé bullet (copy-ready).**
> Implemented real-time 2-D SLAM on a Raspberry Pi 5 mobile robot, fusing 360° DTOF lidar (10 Hz) with holonomic Mecanum wheel-odometry + IMU heading to build live occupancy-grid maps; validated localization to <10 cm / <5° against a tape-measured ground-truth path and demonstrated drift correction via an odometry-vs-SLAM divergence analysis.

**Interview talking point.**
> "I can explain why dead reckoning alone fails and how scan matching fixes it. My bot's wheel odometry drifts — slip on carpet, gearbox backlash — and the error is unbounded; over a loop it ended ~40 cm off. SLAM corrects that by sliding each new lidar scan against the map until the walls line up, and the shift that aligns them is the pose correction. I proved it with an overlay plot: raw odometry spirals away, the SLAM trace closes the loop. The Mecanum-specific subtlety I'm proud of catching is that my robot can strafe, so lateral motion is *true* odometry — I feed SLAM the full 2-D displacement magnitude, not just forward velocity, which is the bug most differential-drive tutorials would have led me into."

## What's next

**Phase 8 — autonomy.** You now have a map *and* a pose that stays locked to it. Phase 8 lets you click any point on that map in the web UI; the bot plans a collision-free path using the occupancy grid and drives there autonomously with the Mecanum kinematics from Phase 3 — full closed-loop navigation, the capstone of the build.
