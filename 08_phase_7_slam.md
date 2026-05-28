# Phase 7 — SLAM mapping

## Goal
Drive the bot around the room and watch a 2D occupancy map build itself in the web UI in real time. At the end of the session, save the map to disk. Pose drift is corrected by lidar scan matching against the growing map, so the bot stays localized even on carpet/slip.

## Difficulty
Hard · 2 weekends

## Prerequisites
- Phase 6 complete (live lidar scans flowing)
- Phase 4 complete (pose tracking with IMU + odometry)

## New parts
None. All software.

## What SLAM is, briefly
SLAM = Simultaneous Localization And Mapping. Two problems entangled:

1. **Mapping:** given pose, plot the lidar scan into a global map
2. **Localization:** given a map, figure out pose from the current lidar scan

You can't do either independently in a new environment — you need the map to localize, and you need pose to map. SLAM solves both at once by alternating: roughly localize from odometry, refine localization by matching the current scan to the map-so-far, then add the scan to the map at the refined pose.

For a 2D lidar on a wheeled robot indoors, this is a well-solved problem with mature open-source libraries. **Don't write SLAM from scratch.** Use one of:

- **BreezySLAM** — pure Python, easy to integrate, decent quality
- **slam_toolbox** — ROS 2 native, production-grade, much heavier setup
- **Cartographer** — Google's, very good, even heavier
- **PythonRobotics** — has FastSLAM and EKF SLAM tutorials

Recommendation for this build: **BreezySLAM first.** Get a working map. If quality isn't enough, graduate to slam_toolbox.

## Schematic — no hardware changes

All the wiring is already in place from previous phases. This phase is entirely software running on the Pi 5.

```
   Lidar scans  ─────►  ┐
                          │
   Pose (x,y,θ)  ─────►  ├──► SLAM ──► occupancy map ──► web UI
   (from Phase 4)         │
                          │
   Wheel odom  ─────────►  ┘
```

## Software task list

### 1. Install BreezySLAM
```bash
git clone https://github.com/simondlevy/BreezySLAM
cd BreezySLAM
sudo python3 setup.py install
```

The Python wrapper is a thin layer over a C implementation, so it's fast enough for real-time use.

### 2. Tune for the STL-19P

BreezySLAM expects a `Laser` configuration object that describes the sensor:

```python
from breezyslam.sensors import Laser

stl19p = Laser(
    scan_size=450,           # points per full scan
    scan_rate_hz=10,         # 10 Hz scan rate
    detection_angle_degrees=360,
    distance_no_detection_mm=12000,
    detection_margin=0,
    offset_mm=0,
)
```

These numbers come from the STL-19P datasheet. Tweak `scan_size` to match what your driver actually delivers per revolution (count the points in one scan first, then set this).

### 3. Wire it up

```python
from breezyslam.algorithms import RMHC_SLAM

MAP_SIZE_PIXELS = 800
MAP_SIZE_METERS = 16     # 16 m square area, plenty for a house
slam = RMHC_SLAM(stl19p, MAP_SIZE_PIXELS, MAP_SIZE_METERS)

while True:
    scan = lidar.get_latest_scan()         # list of mm distances
    odom = (dt_ms, dxy_mm, dtheta_deg)     # from your Phase 4 pose code

    slam.update(scan, odom)

    x_mm, y_mm, theta_deg = slam.getpos()  # SLAM-corrected pose

    map_bytes = bytearray(MAP_SIZE_PIXELS * MAP_SIZE_PIXELS)
    slam.getmap(map_bytes)
```

`map_bytes` is a grayscale occupancy grid: 0 = obstacle, 127 = unknown, 255 = free.

### 4. Render the map in the web UI

Add another canvas. Every ~500 ms, fetch `/map` (returns the occupancy bytes as a PNG, encoded with PIL), draw it. Overlay the bot's current SLAM pose as a triangle pointing in the heading direction.

```python
from PIL import Image
from io import BytesIO

@app.route('/map')
def get_map():
    img = Image.frombytes('L', (MAP_SIZE_PIXELS, MAP_SIZE_PIXELS), bytes(map_bytes))
    buf = BytesIO()
    img.save(buf, format='PNG')
    buf.seek(0)
    return send_file(buf, mimetype='image/png')
```

### 5. Save and load maps

```python
# Save:
with open('home_map.pgm', 'wb') as f:
    f.write(b'P5\n')
    f.write(f'{MAP_SIZE_PIXELS} {MAP_SIZE_PIXELS}\n255\n'.encode())
    f.write(bytes(map_bytes))

# Load:
# Convert back to bytearray of MAP_SIZE_PIXELS * MAP_SIZE_PIXELS bytes
```

PGM is a dead-simple grayscale format that's perfect for occupancy maps.

### 6. Mapping procedure

1. Place the bot at a known spot — call this `(0, 0, 0)`
2. Drive *slowly* — 0.15 m/s linear, 0.3 rad/s angular max
3. Cover the whole room. Revisit places to let SLAM close loops.
4. Avoid abrupt accelerations (they hurt scan matching)
5. Save the map when done

Driving slowly is critical. Fast motion = motion blur on scans (since a single scan takes 100 ms) = bad scan matching = drift. Most beginner SLAM failures are "I drove it like a remote control car."

### 7. (Optional) Pose validation overlay

In the web UI, overlay the raw odometry pose (from Phase 4) AND the SLAM-corrected pose on the map. Watching them diverge over time is hugely instructive. Without scan matching, odometry drifts. With SLAM, the divergence stays bounded.

## Verification checklist

- [ ] **SLAM library installed without errors.** Import works in Python.
- [ ] **First scan produces map output.** The map starts as gray (unknown); within 1–2 seconds, the area immediately around the bot turns dark/light (obstacles/free space).
- [ ] **Walls appear correctly on the map.** Stand the bot near a known wall — that wall should appear as a dark line in the map at the right distance and angle.
- [ ] **Driving forward extends the map.** Drive 2 meters forward. The visited area on the map grows by ~2 meters.
- [ ] **Rotating in place builds a circle of map data.** Slow 360° rotation should fill in the area around the bot in all directions.
- [ ] **SLAM pose tracks reality.** Place the bot at (0,0,0), drive it to a measured point (1 m forward, then 1 m right, then turn 180°). SLAM pose should match within ~10 cm and ~5°.
- [ ] **Loop closure works.** Drive a closed loop in one direction, end where you started. SLAM should snap pose back to (0,0,0) instead of accumulating drift around the loop.
- [ ] **Map saves and reloads.** Save mid-session, restart everything, load the map, drive a bit more — new scans should integrate into the existing map.
- [ ] **Multiple rooms.** Map a hallway and two rooms. Doorways should be visible as gaps in walls.

## Common pitfalls

- **Driving too fast.** Biggest single cause of bad maps. Cap velocity in the firmware while mapping.
- **Bad pose feed to SLAM.** If odometry says "moved 10 cm" but reality is "moved 50 cm", scan matching can't recover. Verify Phase 2 / Phase 4 are accurate before blaming SLAM.
- **Reflective walls.** Mirrors, glass cabinets, and shiny floors confuse the lidar and warp the map locally. Tape paper over mirrors during initial mapping.
- **Dynamic obstacles (people, pets).** They get baked into the map as half-obstacles. Map when the room is still.
- **Inconsistent heading reference.** If the lidar's 0° doesn't match the bot's forward direction (per the convention you set in Phase 6), the entire map is rotated relative to bot pose, and SLAM fails to converge. Verify the rotation is right.
- **Map size too small.** If MAP_SIZE_METERS is smaller than the actual area, the bot will eventually drive off the map and SLAM will lose lock. Set generously; 16 m is fine for most apartments.
- **Forgetting the lidar is on the bot.** The lidar's mounting offset relative to the bot's wheel base center matters for sub-cm accuracy. For typical hobby precision (~5 cm), you can ignore it; for better, set `offset_mm` in the Laser config.
- **CPU saturation.** SLAM + camera streaming + web server competes for CPU. If frame rate drops or scan updates lag, drop camera resolution or skip frames during mapping.

## What's next
Phase 8 — the final phase. The bot can now localize itself and there's a map. Phase 8 lets you click any point on the map and the bot plans a path there using the map for obstacle avoidance, then drives there autonomously. That's full autonomy.
