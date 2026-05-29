# Phase 8 — Autonomous navigation

> **The payoff phase.** You click a point on the live SLAM map. The robot thinks for a moment, draws a green path across the room, and then *drives itself there* — slowing as it arrives, stopping within a hand's width of the goal, and freezing if your foot slides into its way. No joystick. No tether. This is the moment the eight-phase project becomes a *robot* instead of a very sophisticated remote-control car.

---

## Goal

Add a **navigation stack** on top of everything you have already built. Concretely:

1. **Click-to-navigate.** In the web UI, click any free cell on the map. The Pi plans a path from the robot's current SLAM pose to that goal.
2. **Drive there autonomously.** A controller turns that path into body-velocity commands `(vx, vy, ωz)`, which become four wheel speeds through your **Phase-3 Mecanum inverse kinematics**, sent to the Pico over the serial link you built in Phase 3.
3. **Arrive cleanly.** Stop within **~10 cm** of the goal, decelerating on approach instead of slamming to a halt.
4. **Don't hit anything new.** A lidar **"safety bubble"** vetoes forward motion if something appears within **~30 cm** in the direction of travel — your foot, the cat, a chair that wasn't there when you mapped.
5. **Behave predictably.** All of it runs inside an explicit **state machine** (`IDLE → PLANNING → DRIVING → WAITING → …`) with a **big red STOP button that overrides everything**.

Everything in this phase is **software running on the Pi 5.** No new hardware, no new wires, no new BOM lines. The robot's body is finished; this phase finishes its *mind*.

## Difficulty / time
**Hard · ~2 weekends.** The individual algorithms are each a day of work; the hard part is making three control layers, a state machine, and a UI cooperate without fighting each other. Budget the second weekend for tuning (lookahead distance, cruise speed, bubble radius) on the real floor.

## Prerequisites
- **Phase 7 complete** — SLAM produces a usable occupancy map of your space, and `slam.getpos()` returns a pose you trust to ~10 cm / ~5° (you verified this in the Phase 7 checklist).
- **Phase 6 complete** — live 360° lidar scans flowing at 10 Hz on `/dev/ttyUSB0`.
- **Phase 4 complete** — fused pose `(x, y, θ)` from BNO055 heading + Mecanum wheel odometry.
- **Phase 3 complete** — the Pi↔Pico serial bridge and the **Mecanum inverse-kinematics function** `body_twist_to_wheels(vx, vy, ωz)` that you wrote and verified by commanding pure `vx`, pure `vy`, and pure `ωz` one at a time. **This phase calls that function unchanged** — if strafing was wrong in Phase 3, it is wrong here too.
- **Phase 1 complete** — the power tree, e-stop, and the Pico-side comms watchdog + stall timeout are all live. Autonomy means the robot moves *without your hand on the joystick*, so every one of those hardware/firmware safety layers matters more now than ever.

## New parts this phase
**None.** This is a pure-software phase. (See `bom.md` — it confirms zero new purchases and points you back to the parts you already own.)

---

## Where the build stands now (cumulative system state, in prose)

By the time you reach Phase 8, the robot is electrically and mechanically complete. Walk the signal path once more, because the navigation stack sits on *top* of all of it and every layer's quirks can surface as a navigation bug.

At the bottom is the **2S LiPo (7.4 V nominal, 8.4 V full → 6.0 V empty)**. Its current passes through a **10 A main fuse**, a **reverse-polarity + master switch** (Pololu #2815), and then splits into three branches. The **motor branch** (7.5 A fuse, then the **normally-closed mushroom e-stop**) feeds the **VM pins of two TB6612FNG drivers** at 7.4 V. The **Pi branch** (5 A fuse) feeds a **Pololu D24V50F5 buck** that makes a clean **5 V / 5 A** rail into the Pi's USB-C. The **servo branch** feeds a **Hobbywing UBEC** making **5 V / 3 A** for the PCA9685's V+. Every ground returns to a single **star point at the LiPo negative** — no daisy chains.

The **Raspberry Pi 5** is the high-level brain. Over a single USB-A→micro-USB cable it powers and talks to the **Pico 2W**. The Pico is the hard-real-time co-processor: it runs **20 kHz PWM** to the two TB6612 drivers (one motor per channel, four channels for four wheels), decodes all **four quadrature encoders in hardware via PIO**, closes a **per-wheel PID loop**, and enforces a **command-timeout watchdog** (coast if no command for >0.5 s) and a **stall timeout**. The driver logic rails (TB6612 VCC) and all four encoder VCCs are fed from the Pico's **3V3(OUT)** — the encoders run at **3.3 V**, never 7.4 V, so their A/B outputs stay inside the Pico's GPIO limit. The shared **STBY line (GP14) is pulled HIGH** or the outputs stay dead.

On the Pi's own buses: the **BNO055 IMU** is on **UART** (not I²C — that dodges the Pi's clock-stretch bug), giving an absolute, drift-free heading. The **PCA9685** (I²C 0x40) drives the pan/tilt servos; the **INA219** (I²C 0x41) reports pack voltage and current. The **STL-19P lidar** streams 360° scans through a CP2102 USB-UART adapter at 230400 baud. The **Camera Module 3** rides the gimbal over the CSI ribbon.

The software you have already written: **Phase 2** closed-loop wheel-speed control on the Pico; **Phase 3** the serial protocol and the **Mecanum inverse kinematics** (body twist → four wheel speeds) plus a teleop web UI; **Phase 4** sensor-fused pose tracking; **Phase 5** the camera/gimbal stream; **Phase 6** the lidar driver; **Phase 7** SLAM that maps the room and corrects pose drift. Phase 8 adds exactly one new software subsystem — the navigation stack — and wires it into the existing web UI and the existing serial link.

---

## Pin tables (reference — unchanged this phase)

No pins change in Phase 8. This table is here so you never have to flip back; every number is from the **canonical pin map** in `_engineering/device_contracts.md`.

### Pico 2W — motors (2× TB6612FNG, standard mode: IN1/IN2 = direction, PWM = speed)

| Wheel | Board · channel | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | Board A · ch A | GP2 | GP3 | GP4 |
| RL | Board A · ch B | GP5 | GP6 | GP7 |
| FR | Board B · ch A | GP8 | GP9 | GP10 |
| RR | Board B · ch B | GP11 | GP12 | GP13 |
| **STBY** (both boards) | — | **GP14 → must be HIGH** | | |

### Pico 2W — encoders (PIO quadrature; A/B on *adjacent* GPIOs)

| Wheel | A (base) | B |
|---|---|---|
| FL | GP16 | GP17 |
| RL | GP18 | GP19 |
| FR | GP20 | GP21 |
| RR | GP26 | GP27 |

Encoder VCC (all 4) ← Pico **3V3(OUT)**; encoder GND → common star ground.

### Raspberry Pi 5 — buses (40-pin header)

| Function | Pin(s) | Device |
|---|---|---|
| I²C-1 SDA / SCL | 3 / 5 | PCA9685 (0x40), INA219 (0x41) |
| UART0 TXD / RXD | 8 / 10 | → BNO055 RX / ← BNO055 TX |
| 3.3 V / GND | 1, 17 / 6, 9, 14, 20, 25, 30, 34, 39 | logic + star ground |
| USB-A ×4 | — | Pico, CP2102→lidar, (opt) depth cam |
| USB-C | — | ← D24V50F5 5 V/5 A buck |
| CSI (22-pin) | — | Camera Module 3 via #5820 cable |

---

## Concepts: the planning hierarchy from the ground up

A common beginner instinct is to write one big function: "given the goal, drive there." That function becomes a tangle of special cases and never works. Real robots split navigation into a **hierarchy of three layers**, each running at a different rate and answering a different question. Understanding *why* the split exists is the single most important idea in this phase.

```
                          THE PLANNING HIERARCHY
   ┌───────────────────────────────────────────────────────────────┐
   │ GLOBAL PLANNER   "What's the overall route?"                    │
   │   input : start pose, goal pose, the whole static map          │
   │   output: a list of waypoints                                  │
   │   rate  : once per goal (slow, ~50–500 ms is fine)            │
   │   algo  : A* on the inflated occupancy grid                    │
   └───────────────────────────────┬───────────────────────────────┘
                                    │ waypoint list
   ┌────────────────────────────────▼──────────────────────────────┐
   │ LOCAL PLANNER    "Given the route, how do I move right now?"    │
   │   input : the path + current SLAM pose                         │
   │   output: a body twist (vx, vy, ωz)                            │
   │   rate  : 10–20 Hz (fast — this is the steering loop)          │
   │   algo  : Pure Pursuit, adapted for a HOLONOMIC base           │
   └───────────────────────────────┬───────────────────────────────┘
                                    │ (vx, vy, ωz)
   ┌────────────────────────────────▼──────────────────────────────┐
   │ REACTIVE SAFETY  "Will this command hit something right now?"   │
   │   input : the proposed twist + the latest lidar scan          │
   │   output: the twist, possibly clamped to zero forward         │
   │   rate  : scan rate, 10 Hz                                     │
   │   algo  : a 30 cm "safety bubble" in the travel direction     │
   └───────────────────────────────┬───────────────────────────────┘
                                    │ safe (vx, vy, ωz)
                          Phase-3 inverse kinematics
                                    │ (FL, RL, FR, RR) wheel speeds
                                    ▼
                              Pico over USB
```

**Why three layers and not one?** Because they live at different *timescales* and have different *knowledge*. The global planner is slow but sees everything (the whole map); it would be wasteful and pointless to re-run A* on an 800×800 grid at 20 Hz. The local planner is fast and myopic; it only knows the path and where the robot is right now, and it runs often enough to *feel* responsive. The reactive layer is faster still and trusts *nothing* but the live lidar — it's the reflex that catches the obstacle the map never knew about. Each layer's output is the next layer's input, and each can do its job even before the next one exists. This separation of timescales is a recurring pattern in robotics and controls — the same instinct that put hard-real-time on the Pico and soft-real-time on the Pi.

### Layer 1 — the global planner: A*

A* (pronounced "A-star") is the classic shortest-path search on a grid. Think of your map as a grid of cells, each either **free** or **obstacle**. You want the cheapest sequence of free cells from start to goal. A* explores cells in order of a score `f = g + h`:

- **g** = the actual cost to reach this cell from the start (distance travelled so far).
- **h** = a *heuristic* estimate of the cost from this cell to the goal — for us, straight-line (Euclidean) distance.

The genius of A* is that `h` makes it *greedy toward the goal* while `g` keeps it honest about the path so far. As long as `h` never *overestimates* the true remaining cost (Euclidean distance can't — a straight line is the shortest possible route), A* is guaranteed to find the optimal path, and it does so far faster than blind search because it doesn't waste time exploring cells pointing away from the goal. It's Dijkstra's algorithm with a sense of direction.

**The obstacle-inflation trick.** A* treats the robot as a *point*. Your robot is not a point — it's roughly a **24 cm-diameter disc** (radius ~12 cm). If you plan a point-path that grazes a wall, the real robot's *body* clips that wall. The fix, borrowed from configuration-space planning, is to **inflate every obstacle by the robot's radius** before planning. Grow each wall outward by ~12 cm; now a point-path through the inflated free space corresponds to a *body*-path that clears every real wall by the robot's radius. One `scipy.ndimage` call does it. (The radius ~12 cm is the standard figure for a hobby chassis this size; **measure your actual half-width and confirm it.**)

### Layer 2 — the local planner: Pure Pursuit, made holonomic

A* gives you a *geometric* path — a polyline of cells. It does **not** tell you what velocity to command moment-to-moment; it has no notion of the robot's heading, speed, or that the robot can't teleport between cells. That's the local planner's job: turn "here is the line" into "here is how to move *right now* to stay on the line."

**Pure Pursuit** is the most intuitive path-follower ever invented, and the geometry is genuinely beautiful. Picture a point on the path a fixed **lookahead distance `L`** ahead of the robot (typically 30–50 cm for these motors). The robot's job is simply: *"steer toward that point."* As the robot moves, the lookahead point slides forward along the path, and the robot chases it like a carrot on a stick. Make `L` too small and the robot snakes (oscillates) around the path; too large and it cuts corners. It's one tunable number, and you'll feel it on the floor.

Here's where **your Mecanum chassis changes everything**, and it's the conceptual heart of this phase.

#### Holonomic control: the advantage you paid for

A **differential-drive** robot (the kind the *original* plan assumed) is **non-holonomic**: it can only drive forward/back and rotate. It **cannot move sideways.** To correct a lateral error — say it has drifted 10 cm to the right of the path — a differential robot must *turn*, drive, and turn back. Classic Pure Pursuit for differential drive is built entirely around this limitation: it converts the lookahead geometry into a single steering curvature `κ = 2·y_local / L²`, then into `(v, ω)`, and *the only way it can move toward the lookahead point's lateral offset `y_local` is by spinning the whole body.*

Your **Mecanum** robot is **holonomic**: it can command independent `vx` (forward), `vy` (sideways/strafe), and `ωz` (yaw) *simultaneously.* That's the entire point of the angled rollers — and it makes the local planner dramatically more capable:

- **Correct lateral error directly.** If the robot is 10 cm right of the path, just command `vy` to slide left. No turn-drive-turn dance. The error closes in a straight line.
- **Translate while holding a fixed heading.** You can drive the body from A to B *without ever rotating* — keep the camera (and the lidar's 0°) pointed wherever you like while the robot strafes diagonally to the goal. A differential robot physically cannot do this.
- **Decouple steering from heading.** `ωz` becomes a free choice, not a slave to the path. You might hold heading fixed, or slowly rotate to face the direction of travel, or aim the camera at a target — all independent of *getting there*.

So the holonomic Pure Pursuit is actually *simpler* than the differential version. Find the lookahead point, express it in the robot's body frame as `(x_local, y_local)`, and command a velocity vector that *points at it*:

```
vx = K · x_local        # forward component toward the lookahead point
vy = K · y_local        # SIDEWAYS component — impossible on a diff-drive bot
ωz = Kθ · (θ_desired − θ)   # OPTIONAL, independent heading control
```

where `K` scales the vector to your cruise speed. The differential robot was forced to fold `y_local` into a turn; you get to spend it directly as strafe. **This is the single most important "why Mecanum" demonstration in the whole project** — call it out in your writeup.

### Layer 3 — reactive safety: the lidar bubble

The map is a *snapshot* — it knows the walls, not your foot. The reactive layer trusts only the **live lidar scan**. Before any twist reaches the kinematics, check it: if the robot is trying to move in some direction and the lidar reports a return within **~30 cm inside a cone around that travel direction**, **veto the forward part of the motion** (allow it to rotate or back away). This is the robot's reflex — it runs at scan rate (10 Hz) and overrides the local planner. For a Mecanum bot, "direction of travel" is the *vector* `(vx, vy)`, not just "forward," so the cone follows wherever the body is actually heading.

This is the simplest member of a family of reactive methods (the fancy cousin is the **Dynamic Window Approach**, which simulates many candidate velocities a second into the future and scores them). The bubble is "good enough to not hit the cat," which is exactly the bar for this phase.

### The state machine, and recovery behaviors

Without an explicit **state machine**, autonomy code rots into a pile of booleans (`is_driving`, `was_blocked`, `goal_set`…) that contradict each other, and the robot does insane things — driving past a goal it reached, replanning forever at an unreachable goal, ignoring the STOP button because it was "mid-command." A state machine makes the legal behaviors *explicit and exhaustive*:

```
            goal click            path found           arrived
   IDLE ───────────────► PLANNING ───────────► DRIVING ─────────► IDLE
    ▲                        │                    │  ▲
    │ STOP / unreachable     │ no path            │  │ clear & timeout
    └────────────────────────┘          blocked   ▼  │
                                        DRIVING → WAITING ──┐
                                                    │       │ still blocked
                                                    └───────► PLANNING (replan)

   STOP button: any state ─────────────────────────────────► IDLE (motors halted)
```

**Recovery behaviors** are what a *robust* robot does when it's stuck — and their absence is the difference between a demo and a toy. If the robot can't plan, is blocked, or thinks it's lost, it should *do something*: back up 20 cm, rotate slowly to get a fresh lidar view, then retry — rather than sitting there forever. Build at least one (back-up-and-retry) into the `WAITING → PLANNING` edge.

---

## Software task list

> All Pi-side, in your existing Flask app from Phase 3/7. Build the layers **bottom-up** (you can test each before the next exists): kinematics call → safety bubble → Pure Pursuit → A* → state machine → UI.

### 0. Reuse, don't reinvent: the Phase-3 kinematics

This phase's output is always a body twist `(vx, vy, ωz)`. The conversion to four wheel speeds is the **inverse-kinematics function you already wrote and verified in Phase 3** — do not rewrite it here. For reference, the standard Mecanum inverse-kinematics map (with `L` = half the wheelbase front-to-back, `W` = half the track left-to-right, `r` = wheel radius) is:

```python
def body_twist_to_wheels(vx, vy, wz):
    """(vx, vy, wz) body twist  ->  (FL, RL, FR, RR) wheel angular speeds.
    THIS IS YOUR PHASE-3 FUNCTION. Shown only so the path is self-contained."""
    k = (L + W)              # the lumped geometry term
    fl = (vx - vy - k*wz) / r
    rl = (vx + vy - k*wz) / r
    fr = (vx + vy + k*wz) / r
    rr = (vx - vy + k*wz) / r
    return fl, rl, fr, rr
# Serial: pico.send(f"v {vx:.3f} {vy:.3f} {wz:.3f}\n")  -- the Phase-3 protocol
```

> Note the new protocol is `v vx vy wz` (three numbers), **not** the original plan's `v left right`. Differential `(left, right)` is the wrong model for this chassis — it can't express strafe.

### 1. Lock down the coordinate convention (do this first — half this phase's bugs are unit confusion)

- World frame: **x forward, y left, θ counter-clockwise positive** (ROS REP-103).
- Map pixels: origin bottom-left, x right, y up.
- One conversion function each way, written *once*, with a unit test:

```python
def meters_to_px(mx, my):
    px = int(MAP_SIZE_PX/2 + mx * (MAP_SIZE_PX / MAP_SIZE_M))
    py = int(MAP_SIZE_PX/2 - my * (MAP_SIZE_PX / MAP_SIZE_M))  # y flips: image down vs world up
    return px, py
def px_to_meters(px, py):
    mx = (px - MAP_SIZE_PX/2) * (MAP_SIZE_M / MAP_SIZE_PX)
    my = (MAP_SIZE_PX/2 - py) * (MAP_SIZE_M / MAP_SIZE_PX)
    return mx, my
```

`MAP_SIZE_PX` and `MAP_SIZE_M` are the Phase-7 SLAM map dimensions (e.g. 800 px / 16 m).

### 2. Global planner — A* on the inflated grid

```python
import numpy as np, heapq
from scipy.ndimage import binary_dilation

BOT_RADIUS_M = 0.12                                   # ~12 cm — MEASURE your half-width
BOT_RADIUS_PX = int(BOT_RADIUS_M * MAP_SIZE_PX / MAP_SIZE_M)

def inflate(occ):
    """occ: bool array, True = obstacle. Grow obstacles by the bot radius."""
    return binary_dilation(occ, iterations=BOT_RADIUS_PX)

def a_star(grid, start, goal):
    """grid: 2D bool, True=blocked (already inflated). start/goal: (px,py). 8-connected."""
    if grid[goal[1], goal[0]] or grid[start[1], start[0]]:
        return None                                   # start or goal inside an obstacle
    H, W = grid.shape
    def h(a, b): return ((a[0]-b[0])**2 + (a[1]-b[1])**2) ** 0.5   # Euclidean heuristic
    open_set = [(h(start, goal), 0.0, start)]
    came, gscore = {}, {start: 0.0}
    nbrs = [(-1,0),(1,0),(0,-1),(0,1),(-1,-1),(-1,1),(1,-1),(1,1)]
    while open_set:
        _, g, cur = heapq.heappop(open_set)
        if cur == goal:
            path = [cur]
            while cur in came: cur = came[cur]; path.append(cur)
            return path[::-1]
        for dx, dy in nbrs:
            nx, ny = cur[0]+dx, cur[1]+dy
            if not (0 <= nx < W and 0 <= ny < H) or grid[ny, nx]: continue
            step = 1.0 if dx == 0 or dy == 0 else 1.41421
            ng = g + step
            if ng < gscore.get((nx,ny), 1e18):
                gscore[(nx,ny)] = ng; came[(nx,ny)] = cur
                heapq.heappush(open_set, (ng + h((nx,ny), goal), ng, (nx,ny)))
    return None                                       # no path exists
```

Treat SLAM's grayscale map (0=obstacle, 127=unknown, 255=free) as: obstacle **or unknown** = blocked for planning (you can't safely plan into territory you've never seen). If pure-Python A* ever takes >2 s on your map, downsample the grid 2× for planning only.

### 3. Local planner — holonomic Pure Pursuit

```python
import math
LOOKAHEAD = 0.4          # m — start 0.3-0.5, tune on the floor
CRUISE = 0.20            # m/s cruise speed
def holonomic_pursuit(path_m, pose, hold_heading=None):
    """path_m: list of (x,y) in meters. pose: (x,y,theta). Returns (vx, vy, wz)."""
    x, y, th = pose
    tgt = find_lookahead_point(path_m, x, y, LOOKAHEAD)   # walk along path ~L ahead
    dx, dy = tgt[0] - x, tgt[1] - y
    # rotate the goal vector into the BODY frame
    x_loc =  dx*math.cos(th) + dy*math.sin(th)
    y_loc = -dx*math.sin(th) + dy*math.cos(th)
    norm = math.hypot(x_loc, y_loc) or 1e-9
    vx = CRUISE * x_loc / norm        # forward component
    vy = CRUISE * y_loc / norm        # STRAFE component -- the holonomic advantage
    # heading is INDEPENDENT: hold a fixed heading, or leave it at 0 (pure translation)
    wz = 0.0 if hold_heading is None else 0.8 * wrap_angle(hold_heading - th)
    return vx, vy, wz
```

`find_lookahead_point` = find the path point nearest the robot, then march forward along the polyline until you're ~`L` away. `wrap_angle` keeps the heading error in `[-π, π]`.

### 4. Goal arrival with deceleration

```python
def near_goal(pose, goal_m):
    return math.hypot(goal_m[0]-pose[0], goal_m[1]-pose[1]) < 0.10   # 10 cm tolerance
def approach_scale(pose, goal_m):
    d = math.hypot(goal_m[0]-pose[0], goal_m[1]-pose[1])
    return min(1.0, d / 0.5)        # ramp speed down inside the last 0.5 m
```

Multiply the twist by `approach_scale` so the robot eases in instead of overshooting and bouncing back.

### 5. Reactive safety — the lidar bubble (Mecanum-aware)

```python
BUBBLE_M = 0.30          # 30 cm safety radius
CONE_DEG = 40            # +/- half-cone around the travel direction
def safety_filter(vx, vy, wz, scan):
    """scan: list of (angle_rad, dist_m) in the BODY frame. Veto motion INTO obstacles."""
    speed = math.hypot(vx, vy)
    if speed < 1e-3:
        return vx, vy, wz                  # only translation is dangerous; spin is fine
    travel = math.atan2(vy, vx)            # direction the BODY is actually heading
    for ang, dist in scan:
        if dist <= 0: continue
        if abs(wrap_angle(ang - travel)) < math.radians(CONE_DEG) and dist < BUBBLE_M:
            return 0.0, 0.0, wz            # kill translation, allow rotation/recovery
    return vx, vy, wz
```

The cone follows `atan2(vy, vx)` — so a strafing Mecanum bot is protected on the side it's moving toward, not just dead ahead.

### 6. The state machine + recovery

```python
import time, enum
class S(enum.Enum): IDLE=0; PLANNING=1; DRIVING=2; WAITING=3; RECOVER=4
class Nav:
    def __init__(self, pico, slam, lidar): self.state=S.IDLE; self.goal=None; self.path=None; ...
    def set_goal(self, gx, gy): self.goal=(gx,gy); self.state=S.PLANNING
    def stop(self):                                   # the STOP button calls THIS
        self.goal=None; self.path=None; self.state=S.IDLE; self.pico.stop()
    def tick(self):                                   # call at ~15 Hz
        pose = self.slam.getpos(); scan = self.lidar.latest()
        if self.state == S.PLANNING:
            self.path = plan(self.slam.map(), pose, self.goal)   # A* (steps 2)
            self.state = S.DRIVING if self.path else S.IDLE      # refuse unreachable
            self.block_t = None
        elif self.state == S.DRIVING:
            if near_goal(pose, self.goal): self.pico.stop(); self.state=S.IDLE; return
            vx,vy,wz = holonomic_pursuit(self.path, pose)
            s = approach_scale(pose, self.goal); vx,vy,wz = vx*s, vy*s, wz*s
            sx,sy,sw = safety_filter(vx,vy,wz, scan)
            if sx==0 and sy==0:                        # bubble vetoed forward motion
                self.pico.stop(); self.block_t=time.time(); self.state=S.WAITING
            else:
                self.pico.send_twist(sx,sy,sw)         # -> Phase-3 inverse kinematics
        elif self.state == S.WAITING:
            if path_clear_ahead(scan): self.state=S.DRIVING
            elif time.time()-self.block_t > 1.0: self.state=S.RECOVER
        elif self.state == S.RECOVER:
            back_up_20cm_then(self); self.state=S.PLANNING    # recovery behavior, then replan
```

### 7. Web UI — click to navigate, path overlay, STOP

```javascript
mapCanvas.addEventListener('click', (e) => {                 // click = set goal
  fetch('/goal', {method:'POST', headers:{'Content-Type':'application/json'},
                  body: JSON.stringify({px: e.offsetX, py: e.offsetY})});
});
document.getElementById('stop').onclick = () =>              // big red button
  fetch('/stop', {method:'POST'});                          // overrides everything
```

Server side: `px_to_meters()` → `nav.set_goal(...)`. Draw the A* path as a **green polyline** over the map canvas, and the live SLAM pose as a triangle. Refresh the map at 1–2 Hz so the operator can *see* what the robot is doing. Refuse a goal in an inflated-obstacle cell with a clear UI message.

### 8. (Optional capstone) A thin ROS 2 bridge — *legitimately claim ROS 2*

You deliberately bypassed the kit's ROS 2 image to learn the stack from scratch. A small **bridge node** lets you re-enter the ROS 2 ecosystem *honestly* — as a translation layer over a stack you built, not a black box you ran:

```python
# ros2_bridge.py  (rclpy)  -- pyserial on one side, ROS 2 topics on the other
#   SUBSCRIBE  /cmd_vel (geometry_msgs/Twist) -> body_twist_to_wheels() -> Pico serial
#   PUBLISH    /odom    (nav_msgs/Odometry)   <- your Phase-4 fused pose
#   PUBLISH    /scan    (sensor_msgs/LaserScan)<- your Phase-6 lidar driver
```

Now `rviz2`, `teleop_twist_keyboard`, and even Nav2 can talk to *your* robot. This is the single highest-leverage resume line in the project — see the wrap-up.

---

## Verification checklist

Do these **in order**, each assumes the previous passed, **wheels on a stand for the first few**, then on the floor in a cleared area with your hand near the e-stop.

- [ ] **Coordinate round-trip.** `px_to_meters(meters_to_px(mx,my))` returns `(mx,my)` to <1 px. Click a known landmark on the map; the printed meters match a tape-measure reading.
- [ ] **Kinematics still good (Phase-3 regression).** Command pure `vx`, pure `vy`, pure `ωz` through the nav path's serial call. Robot drives straight, strafes straight sideways, and spins in place. If strafe is wrong, fix Phase 3 before continuing.
- [ ] **A* finds a path.** With the saved map loaded, plan from current pose to a free cell across the room. A green polyline appears that hugs no wall (inflation working).
- [ ] **A* refuses the impossible.** Click inside a wall → refused with a message. Click in gray/unknown space → refused. Click across a wall with no opening → "no path."
- [ ] **Safety bubble vetoes (static).** On a stand, place an object 20 cm ahead and command forward. `safety_filter` returns `vx=vy=0`. Move the object to the side while commanding strafe — it vetoes on *that* side too (Mecanum-aware).
- [ ] **STOP latency.** While driving, hit the web STOP button. Wheels halt promptly; state → IDLE; goal cleared. (Then confirm the *physical* e-stop still kills motors independently — defense in depth.)
- [ ] **Drive to a near goal.** Click a point ~1 m away. Robot plans, drives, decelerates, stops **within ~10 cm** (measure with tape).
- [ ] **Lateral-error correction (the Mecanum demo).** Start the robot ~15 cm off the path line. It should slide back onto the line via `vy` *without spinning* — capture this; it's your holonomic proof.
- [ ] **Dynamic obstacle stop.** Walk in front while it drives. It stops within ~30 cm of you (measure the gap). Step aside → it resumes after the wait.
- [ ] **Replan around a new obstacle.** Slide a chair into the path mid-drive. After the wait + recovery, it replans and routes around.
- [ ] **Long run.** Click a goal 5+ m away through a doorway. It arrives within ~10 cm, path tracked to ~20 cm everywhere, no frame bumps.
- [ ] **Manual-move relocalize.** Pick the robot up mid-drive, set it elsewhere, hit "relocalize" (or drive briefly). Next plan uses the corrected pose.
- [ ] **(Capstone)** `ros2 topic echo /odom` and `/scan` show live data; `teleop_twist_keyboard` on `/cmd_vel` drives the robot.

---

## Common pitfalls

- **Differential thinking creeps back in.** The biggest conceptual trap: writing `v_left/v_right` or forcing the robot to turn to fix lateral error. You have a holonomic base — *use `vy`*. If you catch yourself computing a single steering curvature, stop.
- **Goal in an obstacle/inflated cell.** Always check `inflated[gy, gx]` before planning and refuse with a UI message, or A* silently returns `None` and the robot does nothing while you wonder why.
- **A* too slow in pure Python.** An 800×800 grid is ~640k cells. C-backed numpy ops are <100 ms; naïve pure-Python can crawl. If planning >2 s, downsample the grid 2× for planning only.
- **Lookahead fights the geometry.** Too-small `L` → the robot snakes/oscillates around the path; too-large `L` → it cuts corners. Tune on the floor; start 0.4 m.
- **No deceleration near the goal.** Without `approach_scale`, the robot zooms past the goal, then has to come back — and may oscillate around it forever. Ramp speed down inside 0.5 m.
- **Local loop too slow.** The Pure Pursuit + safety loop must run at ≥10 Hz to feel responsive while the Pi also does SLAM + camera. Profile; move expensive work off the hot path; throttle the camera during navigation.
- **Replan oscillation.** The robot flip-flops between two routes around an obstacle. Add hysteresis: once committed to a path, don't replan for ≥2 s unless the safety bubble fires.
- **Map/pose disagree after a manual move.** SLAM doesn't know you picked the robot up; its pose is wrong until it re-sees features. Add an explicit "relocalize" action or drive briefly before trusting the pose.
- **Recovery behaviors absent.** A stuck robot that just *sits there* is a failed robot. Build back-up-and-retry into `WAITING → RECOVER → PLANNING`.
- **STOP that doesn't actually stop.** The STOP button must be reachable from *every* state and must call `pico.stop()` immediately, not "after the current command finishes." Test it from each state.
- **Forgetting the watchdog is your friend.** Even if the nav code wedges, the Pico's 0.5 s comms watchdog coasts the motors. Don't rely on it as primary control, but know it's there — and know the e-stop is faster than any of it.
- **Lidar angle convention drift.** If the lidar's 0° doesn't match the body's forward (the Phase-6 convention), the safety cone points the wrong way and the bot "ignores" obstacles dead ahead. Re-verify the rotation.

---

## ⚡ MEASUREMENT LAB — *navigate, then measure the autonomy*

> Autonomy claims are only believable with numbers. Pick a cleared run and measure three things; put them in your writeup.

| Quantity | Instrument | Procedure | Expected |
|---|---|---|---|
| **Arrival tolerance** | tape measure | Mark a goal on the floor with tape; click the matching map cell; let it drive and stop; measure goal-to-wheelbase-center distance. Repeat 5×. | **≤ ~10 cm** (mean), small spread |
| **Obstacle-stop distance** | tape measure | While driving forward, slide a flat box into its path; measure the gap from the front of the robot to the box when it halts. Repeat 5×. | **~30 cm** (≈ `BUBBLE_M`), it stops *before* contact every time |
| **STOP latency** | stopwatch / phone slow-mo + logged timestamps | Log a timestamp when the STOP endpoint is hit and when `pico.stop()` is acknowledged; cross-check with slow-mo video of the wheels stopping. | **< ~0.3 s** software path; the *physical* e-stop is effectively instant |

Bonus measurement (the holonomic showcase): start the robot a known lateral offset off the path and log `(t, cross-track error)`. Plot it — a clean exponential decay *with the heading held constant* is the unambiguous signature of holonomic correction, and a great figure.

---

## 🏁 PHASE WRAP-UP

**What you learned.**
You built a complete three-layer navigation stack and learned *why* navigation is layered at all: a slow, global, all-knowing planner (A* on an inflated occupancy grid); a fast, myopic local controller (Pure Pursuit); and a faster-still reflexive safety layer (the lidar bubble) — a separation of timescales that mirrors the Pi/Pico split from the very first phase. You learned A* (Dijkstra with a heuristic), configuration-space obstacle inflation, and Pure Pursuit geometry. Most importantly you learned what **holonomy** actually buys you: because your Mecanum base commands `(vx, vy, ωz)` independently, your local planner corrects lateral error *directly* with strafe and can translate while holding a fixed heading — things a differential robot physically cannot do. You learned to express all of this as an explicit state machine with recovery behaviors, and to fold the new subsystem cleanly into the existing serial protocol and web UI. The optional ROS 2 bridge taught you that ROS 2 is, at heart, a typed message-passing translation layer over exactly the kind of stack you just built by hand.

**Résumé bullet (copy-ready).**
> Implemented a full autonomous-navigation stack on a self-built Raspberry Pi 5 + RP2350 (Pico 2W) Mecanum robot: A* global planning on a SLAM-derived, robot-radius-inflated occupancy grid; a **holonomic** Pure-Pursuit local controller commanding body twists `(vx, vy, ωz)` mapped to four wheel speeds via Mecanum inverse kinematics; and a lidar "safety-bubble" reactive layer — all coordinated by a state machine with recovery behaviors and a click-to-navigate web UI. Achieved ~10 cm arrival tolerance with reliable dynamic-obstacle stopping at ~30 cm. *(Optional: exposed a ROS 2 bridge publishing `/odom` and `/scan` and subscribing `/cmd_vel`.)*

**Interview talking point.**
> "Walk me through how your robot navigates" → describe the three-layer hierarchy and *why* it's layered (different timescales, different knowledge — the same insight that put hard-real-time on a microcontroller). Then the killer detail: "Because it's a Mecanum base, my local planner is holonomic — when the robot drifts off the path it just *strafes* back onto the line instead of doing the turn-drive-turn dance a differential robot needs, and it can translate while holding a fixed heading. I derived the inverse-kinematics matrix that turns a body twist into four wheel speeds, and I can show you a cross-track-error-vs-time plot that decays cleanly *with the heading held constant* — that's the holonomic signature." That one answer demonstrates planning, control, linear algebra, and the judgment to know *why* the hardware matters.

---

## You did it — and where to go next

You now have a 4-wheel-drive Mecanum robot that **drives itself**: click a point, it plans, navigates, avoids new obstacles, and arrives — on a **two-brain architecture you understand end-to-end**, built from first principles on top of a power tree, motor control, odometry, sensor fusion, lidar, and SLAM that are all *yours*. Reasonable next directions: camera object detection (navigate to a found object), voice goals (Whisper → goal), named locations ("go to the kitchen"), low-battery dock-finding using the INA219, or migrating the high-level stack fully into ROS 2 via the bridge. But honestly — a working autonomous robot is a milestone worth pausing at.
