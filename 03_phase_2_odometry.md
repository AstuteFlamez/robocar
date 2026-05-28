# Phase 2 — Odometry & closed-loop wheel speed

## Goal
The Pico reads all four encoders, tracks each wheel's velocity in radians per second, and holds a commanded velocity using a per-wheel PID loop. "Drive forward at 0.3 m/s" actually produces 0.3 m/s on the floor, regardless of friction or battery droop.

## Difficulty
Medium · 1 weekend

## Prerequisites
- Phase 1 complete (all motors spin under software control)

## New parts used this phase
- The encoder wires on the JGA27 motors (already connected to the motors — just need to route them to the Pico)

## Schematic — current state of the build

```
                ┌──────────────────┐
                │     Pico 2W      │   (still USB to laptop)
                └────────┬─────────┘
              GP2–GP9    │     ┌──── GP10–GP17 (encoder A,B in)
              (motor     │     │
               control)  │     ├──── 3V3 ──► encoder VCC (all 4 motors)
                         │     └──── GND ──► encoder GND (all 4 motors)
                         │
              ┌──────────┴───────────┐
              │                      │
              ▼                      ▼
      ┌───────────────┐      ┌───────────────┐
  VM─►│  DRV8833 #1   │  VM─►│  DRV8833 #2   │
      │  LEFT side    │      │  RIGHT side   │
      └──┬─────────┬──┘      └──┬─────────┬──┘
       M1,M2     M1,M2         M1,M2     M1,M2
         ▼         ▼             ▼         ▼
      [FL mtr] [RL mtr]      [FR mtr] [RR mtr]
       │A,B    │A,B           │A,B    │A,B
       │VCC,GND│VCC,GND        │VCC,GND│VCC,GND
       └───────┴──────┬────────┴───────┘
                      │
                      └──► all 4 encoders feed back to Pico GP10–GP17
                           VCC of all 4 ◄── Pico 3V3
                           GND of all 4 ◄── common ground

                ┌──────────────────┐
                │  2S LiPo 7.4V    │  + ──► drivers VM (via fuse + switch)
                │                  │  − ──► common ground
                └──────────────────┘
```

## New pin assignments — encoders → Pico 2W

| Motor pin            | Pico pin    | Notes                          |
|----------------------|-------------|--------------------------------|
| VCC (all 4, shared)  | 3V3 (OUT)   | One wire goes to all 4 motors  |
| GND (all 4, shared)  | GND         | One wire goes to all 4 motors  |
| Front-left  A        | GP10        |                                |
| Front-left  B        | GP11        |                                |
| Rear-left   A        | GP12        |                                |
| Rear-left   B        | GP13        |                                |
| Front-right A        | GP14        |                                |
| Front-right B        | GP15        |                                |
| Rear-right  A        | GP16        |                                |
| Rear-right  B        | GP17        |                                |

Important: **A and B for the same motor must be on adjacent GPIO pins** (10–11, 12–13, etc.). The PIO quadrature decoder reads them as a pair, and the PIO program reads two consecutive pins starting from a base pin.

## Full Pico 2W pin allocation as of Phase 2

| GPIO   | Used for                |
|--------|--------------------------|
| GP0    | (free — reserved for UART later if needed) |
| GP1    | (free)                   |
| GP2–9  | Motor control (8 pins)   |
| GP10–17| Encoder A/B inputs (8)   |
| GP18+  | (free for future use)    |

## Background — quadrature encoders in 60 seconds

Each encoder has two outputs (A and B) that go high/low as the motor spins. They're 90° out of phase. If A leads B, the motor is going one direction; if B leads A, the other. You count *transitions* (any edge on A or B), and each transition tells you the wheel moved by a fixed angular amount.

For your JGA27-310R-74 with the 20:1 gearbox: typically 7 pulses per motor revolution × 20 gear ratio × 4 (quadrature edges per pulse) = **560 counts per output shaft revolution**. Confirm this number from your motor's datasheet — if it's different (e.g., 11 PPR encoders are also common), update everywhere it appears in code.

At 450 RPM = 7.5 rev/s, that's ~4,200 counts/second per motor, ~17 kHz total across 4 motors. Way too fast for interrupt-driven counting on a CPU also doing PID and serial — but trivial for the Pico's PIO state machines, which were designed for exactly this.

## Wire-by-wire connection list — new wires this phase

Every motor's encoder block has 4 wires already running from the motor: VCC, GND, A, B. This phase routes them to the Pico.

### Encoder power (shared across all 4 motors)

- [ ] `Pico 3V3 OUT` (pin 36) ──► `FL motor encoder VCC` — daisy-chain or breadboard rail
- [ ] `Pico 3V3 OUT` (pin 36) ──► `RL motor encoder VCC`
- [ ] `Pico 3V3 OUT` (pin 36) ──► `FR motor encoder VCC`
- [ ] `Pico 3V3 OUT` (pin 36) ──► `RR motor encoder VCC`
- [ ] `Pico GND` (pin 28) ──► `FL motor encoder GND` — daisy-chain or rail
- [ ] `Pico GND` (pin 28) ──► `RL motor encoder GND`
- [ ] `Pico GND` (pin 28) ──► `FR motor encoder GND`
- [ ] `Pico GND` (pin 28) ──► `RR motor encoder GND`

### Front-left encoder signals

- [ ] `FL motor encoder A` ──► `Pico GP10` (pin 14)
- [ ] `FL motor encoder B` ──► `Pico GP11` (pin 15)

### Rear-left encoder signals

- [ ] `RL motor encoder A` ──► `Pico GP12` (pin 16)
- [ ] `RL motor encoder B` ──► `Pico GP13` (pin 17)

### Front-right encoder signals

- [ ] `FR motor encoder A` ──► `Pico GP14` (pin 19)
- [ ] `FR motor encoder B` ──► `Pico GP15` (pin 20)

### Rear-right encoder signals

- [ ] `RR motor encoder A` ──► `Pico GP16` (pin 21)
- [ ] `RR motor encoder B` ──► `Pico GP17` (pin 22)

Note: each motor's A and B must land on adjacent GP numbers (10-11, 12-13, 14-15, 16-17). The PIO quadrature decoder reads them as a pair starting from a base pin.

## Software task list

### 1. Use a PIO quadrature decoder

The Pico's PIO blocks have hardware support for this. The official MicroPython PIO examples include a quadrature decoder. Each decoder uses one state machine. The Pico 2W has 8 state machines total (2 PIO blocks × 4 SMs each), so 4 encoders is comfortable.

You don't have to write the PIO program from scratch — grab a known-working one:

- For MicroPython: the `rp2` module has examples. Search "MicroPython rp2 quadrature encoder" — Peter Hinch's repo and several others have battle-tested versions.
- For C SDK: `pio_rotary_encoder.c` from the official examples.

Either way, the API you build on top is the same:

```python
enc_fl = QuadEncoder(pio=0, sm=0, base_pin=10)
enc_rl = QuadEncoder(pio=0, sm=1, base_pin=12)
enc_fr = QuadEncoder(pio=0, sm=2, base_pin=14)
enc_rr = QuadEncoder(pio=0, sm=3, base_pin=16)

count = enc_fl.read()   # signed 32-bit, counts up or down
```

### 2. Convert counts to angular velocity

In a 50 Hz loop:
```python
COUNTS_PER_REV = 560.0
RADS_PER_COUNT = 2 * math.pi / COUNTS_PER_REV
DT = 0.02  # 50 Hz

prev = enc_fl.read()
while True:
    now = enc_fl.read()
    delta = now - prev
    prev = now
    omega = delta * RADS_PER_COUNT / DT  # rad/s
    ...
    time.sleep(DT)
```

### 3. Convert wheel angular velocity to linear velocity

If your wheel diameter is 65 mm:
```python
WHEEL_RADIUS = 0.0325  # m
v_wheel = omega * WHEEL_RADIUS  # m/s
```

### 4. PID per wheel

Each wheel gets its own PID controller closing the loop on velocity:

```python
class PID:
    def __init__(self, kp, ki, kd, out_limit=1.0):
        self.kp, self.ki, self.kd = kp, ki, kd
        self.out_limit = out_limit
        self.integral = 0.0
        self.prev_err = 0.0

    def step(self, setpoint, measured, dt):
        err = setpoint - measured
        self.integral += err * dt
        deriv = (err - self.prev_err) / dt
        self.prev_err = err
        out = self.kp * err + self.ki * self.integral + self.kd * deriv
        # clamp + simple anti-windup
        if out > self.out_limit:
            out = self.out_limit
            self.integral -= err * dt
        elif out < -self.out_limit:
            out = -self.out_limit
            self.integral -= err * dt
        return out
```

Starting gains for these little motors: `kp=0.5, ki=2.0, kd=0.0`. Tune from there.

### 5. Tuning the PID (the only annoying part)

Procedure:
1. Set ki=0, kd=0. Start with kp=0.3
2. Command 0.2 m/s. Lift the bot off the ground (so there's no load).
3. If response is sluggish, raise kp until it overshoots, then back off ~30%
4. Now add ki. Start at kp/2 (so 0.15) and double it until you see oscillation. Back off.
5. kd usually stays at 0 for hobby motors — noisy derivative does more harm than good.

You're done when commanded velocity tracks within ~5% under nominal load.

### 6. Add a top-level command loop

Two commands now: `set_velocity(left_mps, right_mps)` for differential drive. The Pico runs the PID loops; the laptop just sends targets.

## Verification checklist

- [ ] **Encoder VCC and GND wired.** Measure 3.3V across each motor's VCC–GND with a multimeter (Pico powered, motors off).
- [ ] **Encoder pulses visible.** From the REPL, manually rotate one wheel by hand. `enc_fl.read()` should change. Try both directions: forward rotation should make the count go up, reverse should make it go down. If reversed: swap A and B wires for that motor.
- [ ] **All 4 encoders count.** Repeat for each motor.
- [ ] **One revolution = expected count.** Mark the wheel, rotate exactly one turn by hand, read the count delta. Should be ~560 (or whatever your motor spec says).
- [ ] **Open-loop velocity sanity.** Command 0.5 PWM, measure resulting wheel velocity. Note it down for each motor — they should be within ~10% of each other.
- [ ] **PID holds setpoint.** Command 0.2 m/s. Resulting velocity should sit within 5% of 0.2 m/s. Push down on a wheel with your finger to add load — velocity should recover.
- [ ] **All four wheels track together.** Drive forward at 0.2 m/s for 5 seconds. All 4 wheels should match closely (within encoder noise).
- [ ] **Reverse works the same.** Negative setpoints should track too.

## Common pitfalls

- **Wrong counts-per-rev.** If you guess and it's wrong, all your velocity numbers will be scaled wrong. Confirm by marking a wheel and rotating one turn.
- **Encoder VCC plugged into 5V instead of 3.3V.** Some encoders tolerate 5V, but the A/B output level then exceeds Pico's 3.3V input rating. Use the Pico's 3V3 pin, not VBUS/5V.
- **A and B swapped.** Count goes the wrong way. Just swap the two wires at the Pico end.
- **Noisy counts.** The 0.1 µF caps from Phase 1 *should* fix this. If counts jitter randomly when motors are spinning, add a 100 nF cap from each encoder signal pin to ground, right at the motor. Or shield the encoder wires.
- **PID integral windup.** If you ask for a velocity the motor physically can't reach, the integral builds up forever and the motor blows past setpoint as soon as load decreases. The anti-windup in the code above handles the common case; if you still see overshoot, lower ki.
- **Different PWM frequencies on the two pins of a motor.** Both pins driven by the same `Motor` instance must run the same PWM frequency. The class in Phase 1 already does this — just don't override one of them later.
- **PIO state machine ID collisions.** Each PIO has 4 state machines (0–3). If you allocate the same `(pio, sm)` pair to two encoders, the second silently overwrites the first. Use unique IDs.

## What's next
Phase 3 connects the Pico to the Pi 5 over USB, replaces the laptop-driven REPL commands with a clean Pi↔Pico serial protocol, and puts a web UI on the Pi 5 so you can drive the bot from your phone.
