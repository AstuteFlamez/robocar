# Phase 2 — Odometry & Closed-Loop Wheel Speed

## Goal

In Phase 1 you told a wheel "spin at 50% duty cycle" and *hoped* it spun at the speed you wanted. Open-loop. Duty cycle is a request, not a promise: the same 50% PWM produces a different RPM on carpet vs. hardwood, on a full battery vs. a sagging one, going uphill vs. coasting. That's useless for a robot that needs to know *where it is*.

This phase closes the loop. The Pico learns to **measure** how fast each wheel is actually turning — by reading the quadrature encoders built into the 310 motors through its **PIO** hardware — and then to **hold** a commanded wheel speed using a per-wheel **PID** controller. When you finish, `set_wheel_velocity(FL, 4.0 rad/s)` produces 4.0 rad/s on the floor, and the Pico fights friction, battery droop, and your finger pushing on the wheel to keep it there.

This is the bedrock every later phase stands on. Odometry (Phase 3+), the Mecanum forward kinematics that estimate `(vx, vy, ωz)` (Phase 3), the pose the IMU corrects (Phase 4), and the map SLAM builds (Phase 7) are **all** scaled by one number you measure this phase: `COUNTS_PER_REV`. Get it wrong and every distance your robot ever reports is wrong by the same factor — silently.

## Difficulty & time

**Medium · 1 weekend.** The wiring is light (you're routing conductors that already exist). The depth is in the concepts (quadrature, PIO, PID) and in two real lab measurements you'll do with a logic analyzer and a CSV plot.

## Prerequisites

- **Phase 1 complete.** All four motors spin under Pico software control via the two TB6612FNG drivers; STBY is tied HIGH on GP14; the power tree (LiPo → fuse → reverse-polarity/master switch → e-stop → motor rail) is built and the Pi-buck and servo rails exist even if unused yet. You can command each wheel individually from the Thonny REPL.
- You can flash MicroPython and use the Thonny REPL on the Pico.
- A multimeter (continuity + DC volts) and the 8-channel USB logic analyzer from the Phase 1 BOM.
- A laptop with Python 3 + `matplotlib` for the step-response lab.

## New parts this phase

| Part | One-line reason |
|---|---|
| **Pi Pico screw-terminal carrier board** | Retains the motor/encoder interconnect on screwed-down terminals so a Mecanum bot's vibration can't shake a Dupont jumper loose mid-drive. Motor power *never* belongs in a breadboard. |
| **Encoder conductors** (already in the motors' JST-PH 6-pin harness — no purchase) | The A/B Hall channels + Enc-VCC + Enc-GND that were sitting unused in Phase 1's connector now get routed to the Pico's PIO pins. |

Everything electrical here you already bought; the only *new* purchase is the carrier board. See `bom.md`.

---

## Cumulative system state (in prose)

After Phase 1 you had a powertrain: a 2S LiPo feeding a protected 7.4 V motor rail (10 A main fuse → reverse-polarity/master switch → 7.5 A motor-branch fuse → normally-closed e-stop) into two **TB6612FNG** dual H-bridge drivers, each driving two of the four 310 Mecanum motors. The Pico 2W, powered over USB, drove the six logic lines per board (IN1/IN2/PWM × 2 channels) plus a shared **STBY** held HIGH on **GP14**, all referenced to a single-point star ground at the LiPo negative. The motors' factory **JST-PH 2.0 mm 6-pin** connectors mated to pigtails; the motor-power pair (M+, M−) ran on 18 AWG to the driver outputs, and the four encoder conductors in that same connector hung unused.

**This phase changes two things.** First, it *uses* those four encoder conductors per motor — Hall-A, Hall-B, Enc-VCC, Enc-GND — routing A and B to the Pico's PIO pins and powering the encoders from the Pico's **3V3(OUT)** rail (never the 7.4 V motor rail; never 5 V). Second, it *retires the breadboard* for anything load-bearing: the motor-power and encoder interconnect moves onto a **soldered Pico screw-terminal carrier board**, because a breadboard's spring contacts are rated ~1 A (the all-stall motor current is ~5.6 A) and they rattle loose under Mecanum vibration. Signal-only bring-up jumpers can stay on the breadboard until later phases.

Nothing on the Pi 5 changes this phase — the Pi is still just supplying USB power/serial to the Pico, and the I²C/UART sensor buses arrive in Phases 4–6. The Mecanum nature of the chassis is real but not yet exploited: you're still commanding each of the four wheels *independently*. Full Mecanum kinematics — mapping a body twist `(vx, vy, ωz)` to four wheel speeds and back — arrives in Phase 3. This phase's job is to make each wheel a trustworthy, calibrated velocity actuator so that kinematics has solid ground to stand on.

---

## Pin tables (from the canonical map)

### Pico 2W — encoders (PIO quadrature decode)

The PIO quadrature decoder reads **two consecutive GPIOs** starting from a base pin, so each motor's A and B **must** land on adjacent GPIOs. These are the canonical assignments — use exactly these:

| Wheel | A (PIO base pin) | B | Phys. pin A | Phys. pin B |
|---|---|---|---|---|
| FL | GP16 | GP17 | 21 | 22 |
| RL | GP18 | GP19 | 24 | 25 |
| FR | GP20 | GP21 | 26 | 27 |
| RR | GP26 | GP27 | 31 | 32 |

- **Encoder VCC** (all 4) ← Pico **3V3(OUT)** (phys. pin 36). **Encoder GND** (all 4) → common ground.
- Each encoder gets its **own PIO state machine** — 4 of the 12 available are used, 8 stay free.
- 3.3 V supply keeps the Hall A/B outputs at a 3.3 V swing, inside the Pico's GPIO limit. (ADC pins GP26–29 are *never* 5 V tolerant, which is one more reason the encoders run at 3.3 V — RR's channels land on GP26/27.)

### Pico 2W — motor control (unchanged from Phase 1, shown for context)

| Wheel | Board·Channel | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | A·A | GP2 | GP3 | GP4 |
| RL | A·B | GP5 | GP6 | GP7 |
| FR | B·A | GP8 | GP9 | GP10 |
| RR | B·B | GP11 | GP12 | GP13 |
| **STBY** (both boards) | — | **GP14 → HIGH** | | |

### Full Pico GPIO allocation as of Phase 2

| GPIO | Use |
|---|---|
| GP2–GP13 | Motor control (IN1/IN2/PWM × 4 wheels) |
| GP14 | TB6612 STBY (both boards), tied HIGH |
| GP16/17, GP18/19, GP20/21, GP26/27 | Encoder A/B (FL, RL, FR, RR) |
| GP0, GP1, GP15, GP22, GP28 | **Free** for later phases |

---

## Background — the concepts, from the ground up

### Quadrature encoding (A/B, 90° out of phase)

Each 310 motor has a magnetic (Hall-effect) encoder bolted to the *motor* shaft, behind the gearbox. As the shaft turns, two Hall sensors produce two square waves, **A** and **B**, offset by a quarter cycle — 90° out of phase. That phase offset is the whole trick. Watch the order in which edges arrive:

```
forward:  A ┐_┌──┐__┌──┐     B leads/lags by 90°, so at each A edge you
          B ──┐__┌──┐__┌     can read B to learn direction.

   A: __▔▔__▔▔__▔▔__▔▔
   B: ▔__▔▔__▔▔__▔▔__▔   ← shifted 1/4 period
```

If at a rising edge of A, B is low, the shaft is going one way; if B is high, the other way. Spin the motor backward and A and B swap who-leads-whom. So from two one-bit signals you recover both **how far** (count the edges) and **which direction** (their phase relationship). That's quadrature.

"Quadrature × 4" means you count **every edge of both channels** — rising A, falling A, rising B, falling B — giving 4 counts per full A/B cycle and the finest resolution the encoder can offer.

### `COUNTS_PER_REV` — and why you must MEASURE it, not trust 560

You will see "560" in old tutorials (a guess of 7 lines × 20 × 4). **It is wrong for this motor.** The 310's encoder ring has **13 lines per motor-shaft revolution**, the gearbox is **20:1**, and ×4 quadrature edges per line gives:

```
COUNTS_PER_REV = 13 lines × 4 (quadrature) × 20 (gearbox) = 1040 counts per OUTPUT-shaft revolution
```

But 1040 is a **calculation, not a printed datasheet number**, and the gearbox ratio and edge-counting convention can differ from the spec sheet. So the rule for this build is: **the bench is the authority.** You will mark a wheel, rotate it *exactly one output turn* by hand, and read the count delta. *That measured number* — whatever it is — is your `COUNTS_PER_REV`. Every velocity, every odometry distance, every SLAM map scale downstream is multiplied by this constant. A 2× error here (560 vs. 1040) makes your robot think it drove half as far as it did. Measure it. (If your firmware counts single-channel edges only, expect ~260; the ×4 quadrature path gives ~1040. Either way: measure.)

### Why PIO beats interrupts (the ~17 kHz problem)

The motor's no-load output speed is ~450 RPM = 7.5 rev/s. At 1040 counts/rev that's ~7,800 counts/s per wheel — call it ~31,000 edges/s across four wheels at full tilt, and on the order of **~17 kHz aggregate** at typical drive speeds. If you tried to count those edges with GPIO interrupts, every edge would yank the CPU into an interrupt handler — tens of thousands of times a second — *while* the same CPU is supposed to be running four PID loops and a serial link. Interrupts would dominate, jitter would creep into your PID timing, and at high speed you'd start *missing* edges (two edges arrive before the handler finishes the first) and silently undercount.

The RP2350's **PIO (Programmable I/O)** solves this. PIO is a set of tiny, deterministic state machines that run their own little programs independently of the CPU, clocked in lockstep with the system. A ~10-line PIO program watches the A/B pins and increments or decrements a counter on every edge **in hardware**, with zero CPU involvement. The CPU just reads the accumulated count whenever it wants (say, 50 times a second). The RP2350 has **12 PIO state machines**; we use **4** (one per encoder) and leave 8 free. This — offloading hard-real-time edge counting to dedicated hardware so the CPU is free for control — is the single best argument for having a real-time co-processor at all.

### PID, briefly (you'll go deep in the lab)

A **PID controller** turns an *error* (commanded velocity minus measured velocity) into a *correction* (a PWM duty). **P** (proportional) pushes harder the bigger the error. **I** (integral) accumulates leftover error over time to kill steady-state offset (so the wheel reaches *exactly* the setpoint, not just close). **D** (derivative) reacts to how fast the error is changing — useful for damping, but it amplifies encoder noise, so for these little motors we keep **kd = 0**. Seed gains for this build: **kp = 0.5, ki = 2.0, kd = 0.** You'll verify and tune them with a real plot, not by eyeballing a spinning wheel.

---

## Software task list

> Build firmware on the Pico. Keep your Phase 1 `Motor` class — the PID's output is just a signed speed in `[-1, +1]` you hand to `motor.set_speed()`.

### 1. PIO quadrature decoder (don't write the PIO program from scratch)

Use a known-good MicroPython PIO quadrature decoder (e.g. Peter Hinch's `encoder_rp2` or the official `rp2` examples). Wrap it so each encoder owns a unique `(pio_block, state_machine)` and a base pin:

```python
# encoders.py  — one PIO state machine per wheel
from quad_decoder import QuadEncoder   # the PIO-backed class you adopted

enc = {
    "FL": QuadEncoder(pio=0, sm=0, base_pin=16),   # A=GP16, B=GP17
    "RL": QuadEncoder(pio=0, sm=1, base_pin=18),   # A=GP18, B=GP19
    "FR": QuadEncoder(pio=0, sm=2, base_pin=20),   # A=GP20, B=GP21
    "RR": QuadEncoder(pio=1, sm=0, base_pin=26),   # A=GP26, B=GP27  (PIO block 1)
}

count = enc["FL"].read()   # signed 32-bit running count, ticks up or down
```

> ⚠️ **State-machine collisions:** each PIO block has only 4 state machines (sm 0–3). The RP2350 has **three** PIO blocks (0/1/2) = 12 SMs total, so 4 encoders fit easily with 8 to spare. Assign a *unique* `(pio, sm)` to every encoder or the second silently clobbers the first. Spreading them across two blocks (e.g. block 0 and block 1) keeps it clean.

### 2. Counts → angular velocity (a fixed-rate loop)

```python
import math, time

COUNTS_PER_REV = 1040.0     # ← PLACEHOLDER. Replace with your MEASURED value (Lab 1).
RADS_PER_COUNT = 2 * math.pi / COUNTS_PER_REV
DT = 0.02                   # 50 Hz control loop

prev = {w: enc[w].read() for w in enc}
while True:
    t0 = time.ticks_ms()
    omega = {}
    for w in enc:
        now = enc[w].read()
        delta = now - prev[w]
        prev[w] = now
        omega[w] = delta * RADS_PER_COUNT / DT     # rad/s, signed
    # ... PID + motor drive here ...
    # hold the loop period steady so DT is real:
    while time.ticks_diff(time.ticks_ms(), t0) < int(DT * 1000):
        pass
```

A wheel-radius conversion (`v = omega * WHEEL_RADIUS`) is available if you prefer m/s, but **hold the PID loop in rad/s** — it keeps the controller independent of wheel size and is what Phase 3's kinematics wants.

### 3. Per-wheel PID with anti-windup

```python
# pid.py
class PID:
    def __init__(self, kp, ki, kd, out_limit=1.0):
        self.kp, self.ki, self.kd = kp, ki, kd
        self.out_limit = out_limit
        self.integral = 0.0
        self.prev_err = 0.0

    def reset(self):
        self.integral = 0.0
        self.prev_err = 0.0

    def step(self, setpoint, measured, dt):
        err = setpoint - measured
        deriv = (err - self.prev_err) / dt
        self.prev_err = err

        out_unclamped = self.kp * err + self.ki * (self.integral + err * dt) + self.kd * deriv
        # Conditional integration anti-windup: only accumulate I if we are NOT
        # saturated, OR if integrating would pull us back out of saturation.
        if abs(out_unclamped) < self.out_limit or (out_unclamped * err < 0):
            self.integral += err * dt

        out = self.kp * err + self.ki * self.integral + self.kd * deriv
        return max(-self.out_limit, min(self.out_limit, out))   # clamp to motor range
```

```python
# seed gains for the 310 motors
pid = {w: PID(kp=0.5, ki=2.0, kd=0.0) for w in enc}
```

> **Anti-windup, in one sentence:** if you command a speed the motor physically can't reach, raw `out` keeps climbing while PWM is pinned at 100% — the integral "winds up" — and when load drops the wheel rockets past setpoint. The conditional-integration guard above stops the integral from growing while saturated, so recovery is clean.

### 4. Wire PID → motors in the control loop

```python
setpoint = {w: 0.0 for w in enc}   # rad/s, set by command parser
for w in enc:
    u = pid[w].step(setpoint[w], omega[w], DT)   # signed [-1, +1]
    motors[w].set_speed(u)                        # your Phase 1 Motor class
```

### 5. Command interface (throwaway, replaced in Phase 3)

Read lines from `sys.stdin` so you can set per-wheel setpoints from the REPL — e.g. `w FL 4.0` to command FL to 4.0 rad/s, `all 4.0`, `stop`. **Per-wheel, not left/right** — this is a Mecanum bot, and Phase 3 will add the `(vx, vy, ωz)` kinematics on top. Don't bake a differential `left/right` API in here; it's the wrong model and you'd just rip it out.

### 6. Telemetry stream for the step-response lab

Add a mode that streams CSV lines `t_ms, setpoint, measured, pwm` for one chosen wheel over USB serial at the loop rate, so the laptop can log and plot them (see Measurement Lab). This single feature turns PID tuning from guesswork into engineering.

### 7. (Carry-forward) command-timeout watchdog — stub it now

Per `safety.md`, the Pico must coast the motors if Pi comms drop. You wire the real serial link in Phase 3, but stub the watchdog now: if no setpoint update arrives for >0.5 s, zero all setpoints and call `motor.set_speed(0)`. Cheap insurance for a closed-loop bot that can run away.

---

## Verification checklist

Do these **in order** — each assumes the previous passed. Wheels off the ground / on a stand for anything that spins.

- [ ] **Continuity ritual first.** Battery disconnected, master switch off, e-stop pressed. Beep-test that each encoder VCC reads to the Pico **3V3(OUT)**, *never* to the 7.4 V motor rail. No beep between any V+ and any GND. (See `safety.md` continuity ritual.)
- [ ] **Encoder power present.** Pico on USB only (motors unpowered). Measure **3.3 V** across each motor's encoder VCC↔GND with a multimeter. If any reads ~5 V or ~7.4 V, **stop** — you'll exceed the Pico's GPIO limit on the A/B lines.
- [ ] **Each encoder counts by hand.** From the REPL, `enc["FL"].read()` then rotate the FL wheel by hand. The count must change. Repeat for RL, FR, RR with their own reads.
- [ ] **Direction is correct.** Rotating a wheel in the *forward* drive direction makes its count go **up**; reverse makes it go **down**. If a wheel counts backward, swap its A/B at the carrier terminals (don't fix it in code).
- [ ] **MEASURE `COUNTS_PER_REV` (Lab 1).** Mark the wheel, rotate exactly one output-shaft turn, read the delta. Record it per wheel; they should agree within a few counts. **Set `COUNTS_PER_REV` in firmware to this measured value** — do not leave 1040 hardcoded if your bench says otherwise.
- [ ] **Open-loop velocity sanity.** Power the motor rail (correct power-up order from `safety.md`: logic first, then arm motors). Command 0.5 duty open-loop; print each wheel's measured rad/s. The four should land within ~10% of each other.
- [ ] **A-leads-B flips on reverse (Lab 2).** With the logic analyzer on one motor's A and B, confirm A leads B forward and B leads A in reverse. Screenshot both.
- [ ] **PID holds a setpoint.** Command 4.0 rad/s on one wheel. Measured velocity should settle within ~5% of 4.0. Press a finger on the wheel to load it — velocity should sag briefly, then **recover**. That recovery is the integral term doing its job.
- [ ] **Step-response plot looks healthy (Lab 3).** Stream the CSV, plot it. Rise time reasonable, overshoot modest (<~20%), settles without sustained oscillation. Tune from the plot, not the wheel.
- [ ] **All four track together.** Command all wheels to 4.0 rad/s for 5 s; the four measured traces should overlap within encoder noise.
- [ ] **Reverse tracks too.** Negative setpoints hold just as well.
- [ ] **Watchdog coasts on comms loss.** Stop sending setpoints; within ~0.5 s all wheels should drop to zero.

---

## Common pitfalls

- **Hardcoding 560 (or trusting 1040 without measuring).** Every downstream distance/velocity scales by `COUNTS_PER_REV`. The 13-line ring computes to **1040**, not the old guess of 560 — but you still *measure* it. This is the #1 silent error of the whole project.
- **Encoder VCC on 5 V or 7.4 V.** Powering the encoder above 3.3 V raises the A/B output swing above the Pico's GPIO input limit. Some encoders "work" at 5 V for a while, then you've quietly stressed the pins. **3V3(OUT) only.** (And RR's channels are on GP26/27, which are ADC-capable pins that are *never* 5 V tolerant — doubly important here.)
- **A and B swapped.** The count runs the wrong way. Fix it by swapping the two wires at the carrier, not in software — keeps the sign convention honest everywhere else.
- **Noisy / jittery counts when motors run.** The Phase-1 0.1 µF caps at each motor body should handle brush noise. If counts still jitter under power, add a 100 nF cap from each A/B line to GND right at the motor, and keep encoder signal wires away from the 18 AWG motor-power pair.
- **PIO state-machine ID collision.** Reusing the same `(pio, sm)` for two encoders silently overwrites the first. Give each a unique pair; the RP2350's three PIO blocks (4 SMs each) leave plenty of room.
- **Integral windup.** Commanding an unreachable speed pins PWM and winds the integral up; load drop → big overshoot. The conditional-integration guard above prevents it; if you still see overshoot, lower `ki`.
- **Unsteady loop period.** PID math assumes a fixed `dt`. If your loop period wanders (because `time.sleep` drifts or work varies), `omega` and the I/D terms are computed against the wrong `dt` and tuning won't reproduce. Pin the loop to 50 Hz with a `ticks_diff` busy-wait (see task 2).
- **Driving derivative on a noisy signal.** `kd > 0` amplifies encoder quantization noise into PWM chatter. Keep `kd = 0` for these motors unless a plot proves it helps.
- **Forgetting STBY.** Inherited from Phase 1: if **GP14 (STBY)** isn't HIGH, the TB6612 outputs stay disabled and the motors do nothing no matter how good your PID is.

---

## 📐 MEASUREMENT LAB

Three measurements turn this phase from "I think it works" into "I measured it." Capture each as a number and a screenshot — they are résumé/interview gold.

### Lab 1 — `COUNTS_PER_REV`, empirically

- **Quantity:** counts per output-shaft revolution.
- **Instrument:** a marker (mark the wheel + a fixed reference) and the encoder count via REPL.
- **Method:** zero the count, rotate the wheel **exactly one full output turn** by hand, read the delta. Repeat 3× per wheel and average.
- **Expected:** ~**1040** (13 × 4 × 20). If you instead see ~260, your decoder is counting one channel's single edges, not full ×4 quadrature. Whatever you measure *is* your constant — write it into firmware.

### Lab 2 — quadrature direction (A leads B → flips on reverse)

- **Quantity:** the phase relationship of A and B.
- **Instrument:** the 8-channel USB logic analyzer (PulseView), 2 channels on one motor's A and B at the carrier.
- **Method:** spin the wheel forward by hand; confirm **A leads B**. Spin it backward; confirm **B now leads A** — the waveform's lead/lag visibly flips. Capture both screenshots.
- **Expected:** clean 90°-offset square waves; lead/lag inverts with direction. This is *seeing* quadrature, not just reading about it.

### Lab 3 — PID step response

- **Quantity:** rise time, overshoot %, settling time of the closed loop.
- **Instrument:** the firmware CSV telemetry (`t, setpoint, measured, pwm`) + `matplotlib` on the laptop.
- **Method:** command a step (e.g. 0 → 4.0 rad/s), log a couple seconds of CSV, plot `setpoint` and `measured` vs `t`. Read off rise time (10→90%), peak overshoot, and settling time (to ±5%). Re-tune `kp`/`ki` and re-plot.

```python
# plot_step.py  — run on the laptop after capturing telemetry to step.csv
import csv, matplotlib.pyplot as plt
t, sp, meas, pwm = [], [], [], []
with open("step.csv") as f:
    for row in csv.reader(f):
        if row and row[0].lstrip("-").isdigit():
            ti, s, m, p = map(float, row)
            t.append(ti/1000.0); sp.append(s); meas.append(m); pwm.append(p)
plt.plot(t, sp, "--", label="setpoint")
plt.plot(t, meas, label="measured")
plt.xlabel("time (s)"); plt.ylabel("wheel speed (rad/s)"); plt.legend()
plt.title("FL wheel PID step response")
plt.show()
```

- **Expected (well-tuned):** rises to setpoint in a few tenths of a second, overshoots <~20%, settles within ±5% and stays. Sustained oscillation → lower `kp`/`ki`. Sluggish, never quite arrives → raise them (raise `ki` to kill steady-state offset).

---

## 🎓 PHASE WRAP-UP

**What you learned.** Quadrature encoding — how two 90°-offset square waves encode both displacement and direction, and what "×4" resolution means. Why hard-real-time edge counting belongs in dedicated hardware (the RP2350 PIO state machines) rather than CPU interrupts, and the ~17 kHz aggregate-edge math that proves it. Closed-loop control: a per-wheel PID with conditional-integration anti-windup that holds a velocity setpoint against disturbances. And the engineering discipline of *measuring* a critical constant (`COUNTS_PER_REV`) on the bench instead of trusting a guessed spec — plus characterizing a controller with a logged step response (rise/overshoot/settling) instead of eyeballing a spinning wheel.

**Résumé bullet (copy-ready):**
> Implemented hard-real-time wheel odometry and per-wheel PID velocity control on an RP2350 (Pi Pico 2W) co-processor, decoding four quadrature encoders (~17 kHz aggregate edges) entirely in PIO hardware state machines to offload the CPU; empirically calibrated counts-per-revolution and tuned the controllers from logged step-response data (rise time / overshoot / settling).

**Interview talking point.** "Why a co-processor with PIO instead of just counting edges on the Pi or with interrupts?" Answer: at full speed four encoders emit ~30k edges/s, ~17 kHz at typical drive speeds. Interrupt-counting that on a CPU that's also running four PID loops and serial means the ISRs dominate the schedule, inject jitter into the control timing, and start dropping edges at high RPM — which silently corrupts odometry. The RP2350's PIO is a deterministic hardware state machine that counts every edge with zero CPU load; the CPU just samples the accumulated count at 50 Hz. That's the canonical lesson of the two-brain design: put the hard-real-time, deterministic work on hardware built for it, and keep the general-purpose CPU free for control and communication.

---

## What's next

**Phase 3** brings the two brains together: a clean Pi 5 ↔ Pico serial protocol over the USB "spinal cord" replaces the REPL, the Pi 5 gets a web UI you can drive from your phone, and — crucially — the full **Mecanum kinematics** land on top of the calibrated per-wheel velocity controllers you built here: a body twist `(vx, vy, ωz)` maps to four wheel setpoints (inverse kinematics), and the four measured wheel speeds map back to an odometry estimate (forward kinematics).
