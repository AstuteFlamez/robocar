# Phase 8 — Autonomous navigation

## Goal
Click any point on the map in the web UI. The bot plans a path from its current SLAM pose to the goal, drives along that path autonomously, and stops when it arrives. If a new obstacle appears during driving (your foot, the cat), the bot replans and goes around. This is the full autonomy stack you set out to build.

## Difficulty
Hard · 2 weekends

## Prerequisites
- Phase 7 complete (SLAM produces a usable map of your space, current pose is accurate)

## New parts
None. All software.

## The three layers

Autonomous navigation is conventionally split into three layers, top to bottom:

1. **Global planner** — given current pose, goal pose, and the static map, find a path (sequence of waypoints) that avoids known obstacles. Runs once per goal (or when major replanning is needed). Algorithm: A* on the occupancy grid.

2. **Local planner / controller** — given the current path and current pose, compute velocity commands to follow the path. Runs at 10–20 Hz. Algorithm: Pure Pursuit for differential drive (simple and works well).

3. **Reactive obstacle avoidance** — given the latest lidar scan and the planned velocity, modify or veto the command if there's a close obstacle that wasn't on the map. Runs at scan rate (10 Hz). Algorithm: Dynamic Window Approach (DWA), or simpler — a "safety bubble" that stops the bot if anything is too close in the direction of travel.

You don't have to implement all three perfectly at once. Build them in order; each is useful even before the next exists.

## Schematic — no hardware changes

```
                 Web UI click ──► (goal_x, goal_y)
                                       │
                                       ▼
                                Global planner (A*)
                                       │
                                       │ list of waypoints
                                       ▼
   SLAM pose ──────────────► Local planner (Pure Pursuit)
                                       │
                                       │ (v, ω) command
                                       ▼
   Lidar scan ──────────► Reactive safety check
                                       │
                                       │ possibly clamped (v, ω)
                                       ▼
                          Differential drive math
                                       │
                                       │ (v_left, v_right)
                                       ▼
                                  Pico over USB
```

## Software task list

### 1. Coordinate system clarity

Pick once and stick to it:
- World frame: x forward, y left, θ counter-clockwise positive (ROS REP-103 convention)
- Map pixels: origin at bottom-left, x right, y up
- Conversion: `pixel = (map_size/2) + meter * (map_size_pixels / map_size_meters)`

Document this somewhere visible. Half the bugs in this phase are unit confusion.

### 2. Global planner — A* on the occupancy grid

```python
def a_star(occupancy_map, start_px, goal_px):
    """
    occupancy_map: 2D numpy array, 0=free, 1=obstacle
    Returns list of (x, y) pixel waypoints
    """
    # Standard A* with 8-connectivity
    # Heuristic: Euclidean distance
    # Cost to enter a cell: 1 + obstacle_penalty if close to obstacle
```

Inflate obstacles before planning by the bot's radius (about 12 cm for typical hobby chassis). Otherwise the planner picks paths that brush against walls and the controller can't track them.

```python
from scipy.ndimage import grey_dilation
inflated = grey_dilation(occupancy_map, size=BOT_RADIUS_PX * 2)
```

A* implementations are everywhere; `python-pathfinding`, `networkx`, or write your own in ~60 lines.

### 3. Local planner — Pure Pursuit

The geometry: given a path of waypoints and the bot's current pose, find the point on the path that's a fixed "lookahead distance" L ahead of the bot. Draw a circle arc from the bot through that point. Drive at that arc's curvature.

```python
def pure_pursuit(path, pose, lookahead=0.4):
    x, y, theta = pose
    # find point on path closest to (x, y), then walk forward by L
    target = find_lookahead_point(path, x, y, lookahead)
    # transform target into bot frame
    dx = target.x - x
    dy = target.y - y
    local_x = dx * cos(theta) + dy * sin(theta)
    local_y = -dx * sin(theta) + dy * cos(theta)
    # curvature = 2*y / L^2
    curvature = 2 * local_y / (lookahead ** 2)
    v = 0.2                                 # m/s — pick a cruise speed
    omega = curvature * v
    return v, omega
```

Convert `(v, ω)` to differential drive:
```python
v_left  = v - omega * WHEEL_BASE / 2
v_right = v + omega * WHEEL_BASE / 2
```

`WHEEL_BASE` is the distance between left and right wheels (measure on your chassis).

Lookahead tuning: too small = oscillates around the path; too large = cuts corners. Start at 30–50 cm for these motors.

### 4. Goal arrival

Stop when within tolerance of the goal:
```python
dist_to_goal = math.hypot(goal_x - x, goal_y - y)
if dist_to_goal < 0.10:   # 10 cm tolerance
    stop()
    goal_reached = True
```

### 5. Reactive safety — the simple version

Before sending `(v, ω)` to the Pico, do a sanity check on the latest lidar scan:

```python
def safety_check(v, omega, scan):
    if v <= 0:
        return v, omega       # only checking forward motion
    # Check the cone in front of the bot
    for angle, distance in scan:
        if abs(angle) < radians(30):     # ±30° cone
            if distance < 0.30:           # 30 cm safety bubble
                return 0.0, omega         # stop forward, allow rotation
    return v, omega
```

Good enough for "don't drive into a thing that appeared". For more nuance later: DWA samples a window of possible velocities, simulates 1 second forward, and picks the best velocity that doesn't collide.

### 6. Replanning

If the safety check triggers (bot stops because of obstacle), wait 1 second. If still blocked, re-run the global planner from current pose — the new obstacle gets baked into the map first, so A* will route around it.

You can mark these "transient obstacles" separately from the SLAM map so they decay after being clear for a while.

### 7. Web UI — click to navigate

Add click handler on the map canvas:
```javascript
mapCanvas.addEventListener('click', (e) => {
  const px = e.offsetX, py = e.offsetY;
  fetch('/goal', {method: 'POST', body: JSON.stringify({px, py})});
});
```

Server side converts pixel to meters, sets it as goal, starts the planning + driving loop. Show the planned path on the map as a green polyline. Show the current trajectory as the bot moves.

Add a big red "STOP" button. It should override everything.

### 8. State machine

Roll the navigation into a clean state machine:

```
IDLE ──goal──► PLANNING ──path──► DRIVING ──blocked──► WAITING ──timeout──► PLANNING
                                       │                                        │
                                       └──arrived──► IDLE                        └──clear──► DRIVING
```

Without an explicit state machine, the bot will do weird things like keep driving past a goal it already reached, or replan repeatedly while the goal is unreachable.

## Verification checklist

- [ ] **Click a point 1 m away on the map. Bot drives there and stops.** Tolerance ~10 cm.
- [ ] **Click a point on the other side of a wall.** Planner refuses or routes around.
- [ ] **Click a point in unknown space.** Planner refuses (cannot plan into unmapped territory).
- [ ] **Walk in front of the bot while it's driving.** It stops within ~30 cm of you.
- [ ] **Step out of the way.** Bot resumes after waiting.
- [ ] **Move a chair into the path mid-drive.** Bot replans and goes around.
- [ ] **STOP button works.** Pressing it stops the bot immediately, cancels the goal.
- [ ] **Long drive (5+ meters).** Bot reaches goal with reasonable straightness — path matches the planned line within ~20 cm everywhere.
- [ ] **Goal in a different room.** Bot navigates through doorway without bumping the frame.
- [ ] **Replan from new start.** Move the bot manually mid-drive; the next planning cycle uses the new pose.

## Common pitfalls

- **Goal in obstacle cell.** Always check: if `inflated[goal_y, goal_x]` is occupied, refuse the goal with a clear error in the UI.
- **A* timeout.** On a 800×800 map, naïve A* with 8-connectivity is ~640k cells, fine for under 100 ms with C-backed numpy ops, slow with pure Python. If planning takes >2 seconds, use a coarser map (downsample 2x) for planning.
- **Lookahead distance fights wheel base.** If L is smaller than half the wheel base, the bot oscillates. Increase L.
- **Velocity command updates too slowly.** Local planner needs to run at 10+ Hz to feel responsive. If your Pi 5 is doing SLAM + camera + planner, profile and move expensive things off the hot path.
- **No deceleration near goal.** Bot zooms past the goal at full speed, then has to come back. Scale velocity by `min(1.0, dist_to_goal / 0.5)` so it slows down approaching.
- **Stuck in oscillation.** Bot can't decide between two paths around an obstacle. Add hysteresis: once committed to a path, don't replan for at least 2 seconds unless safety triggers.
- **Map and pose disagree after a manual move.** If you pick up the bot and put it somewhere, SLAM doesn't know — its pose estimate is wrong until it sees enough distinctive features. Either let SLAM relocalize (drive around briefly) or implement an explicit "I have moved the bot, please relocalize" button.
- **Recovery behaviors absent.** When the bot is stuck (can't plan, blocked, lost), it should do *something* — back up 20 cm, rotate slowly, try again. Without this, the bot just sits there.
- **Web UI map stale.** If the displayed map doesn't update during driving, the user can't tell what's happening. Update the displayed map at 1–2 Hz minimum.

## You did it
By the end of this phase you have:
- A 4WD robot that drives itself
- A web interface to teleop, watch video, see a live map, and click to navigate
- A two-processor architecture you understand end-to-end
- A foundation for whatever you want to add next

Reasonable next directions if you want to keep going:
- **Object detection** with the camera (YOLO or similar) — find specific objects in the room and navigate to them
- **Voice control** — Whisper for speech to text, send goals from spoken commands
- **Multi-room mapping** with named locations ("go to kitchen")
- **Battery monitoring + dock-finding** — bot returns to a charging station when low
- **Cloud telemetry / remote control** — drive from outside your LAN
- **ROS 2 migration** — rewrite the higher levels in ROS 2 for compatibility with the wider ecosystem

But honestly: a working autonomous bot is a milestone worth pausing at.
