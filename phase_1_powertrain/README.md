# Phase 1 — Powertrain

> **Where we are in the story.** The kit you have — a Hiwonder MentorPi M1 — already drives itself out of the box, on a vendor controller (the RRC Lite) you're deliberately setting aside. This whole project rebuilds that low-level stack by hand so that *you* are the one who understands every PWM edge, every encoder tick, every amp. Phase 1 is the very first brick: we make the four wheels turn under *your* code, from *your* serial terminal, through *your* wiring. Nothing fancy yet — no feedback, no kinematics, no autonomy. Just: command a wheel, watch it spin the way you told it to, and know exactly why.

---

## Goal

**All four "310" Mecanum drive motors spin forward and reverse under Raspberry Pi Pico 2W control, commanded one wheel at a time from a serial terminal on your laptop, with the wheels in the air on a stand.** By the end you will have verified each wheel's direction by hand, scoped the 20 kHz PWM that drives them, and stood up the *motor side* of the robot's power path — from the LiPo's keyed SM-2P output, split through lever-nut connectors, into both driver boards at real current numbers. (The full protection chain — coordinated fuses, a reverse-polarity/master switch, a fail-safe e-stop — is explained here but *built later*, once the robot leaves the stand and drives on the floor. See "Concepts," below, for why each is deferred.)

This phase is open-loop: you tell a wheel "go 50% forward" and it goes *roughly* 50% of full speed. It does **not** yet know how fast it's actually turning — that's Phase 2 (encoders + PID). Here we just prove the muscle works before we add the nervous system.

## Difficulty / Time

**Medium · one weekend.** The soldering (motor noise caps, the screw-terminal blocks on each driver's power edge) and the careful power bring-up are what take the time — the code is short. Do *not* rush the power section; that's the part that bites.

## Prerequisites

- None in terms of prior phases — this is the starting point.
- Background we assume (and will build on): Ohm's law (`V = IR`), what a capacitor does, intro Python, and that "PWM" is something you've at least heard of. Everything else — H-bridges, why 20 kHz, fuse coordination, star grounds — we explain here from the ground up.
- A multimeter you can set to **continuity (beep)** and **DC volts**. Non-negotiable. A $13 USB logic analyzer is strongly recommended (it turns the invisible PWM into a picture).
- **Read [`../_engineering/safety.md`](../_engineering/safety.md) fully before you connect the LiPo.** A 2S LiPo can dump ~22 A into a short. Respect it and it's perfectly safe.

## New parts this phase

| Part | One-line reason it's here |
|---|---|
| **Raspberry Pi Pico 2W** (already have) | The real-time motor brain. This phase it's powered from your laptop's USB and we use only its PWM outputs. |
| **2× Pololu TB6612FNG** dual motor drivers (#713) | Each board is two H-bridges; one motor per channel → 2 boards = 4 independent wheels (required for Mecanum). |
| **4× "310" motors** w/ Hall encoders (already have) | The drive wheels. We use the motor leads now; the encoder conductors ride along in the same connector but stay parked until Phase 2. |
| **4× JST-PH 2.0 6-pin pigtail leads** | Mate the motors' *factory* 6-pin connector (motor power + encoder in one keyed plug). Don't cut the factory plug. |
| **2S LiPo 7.4 V 2200 mAh 10C, w/ built-in protection board** (already have) | The one and only energy source for the whole robot. Its protection board already guards against overcharge, over-discharge, overcurrent, and short circuit. Output is a keyed **SM-2P male** plug; a separate **DC5.5×2.5 barrel jack** is for charging only. |
| **SM-2P female pigtail** | Mates the battery's SM-2P male output and breaks it out to red (+) / black (−) leads. Don't cut the battery's connector. |
| **2× WAGO 221-413 lever-nut splices (3-conductor)** | The no-solder way to split one battery rail into two: one WAGO splits the **+** (LiPo + → both drivers' VM), the other splits the **−** (LiPo − → both drivers' GND). Rated ~20 A — far above the ~5.6 A all-stall load. |
| **0.1″ (2.54 mm) PCB screw-terminal blocks** | Soldered onto each driver's *power-edge* pads (VM, GND, and the motor-output pads) so power and motor wires land by screw, not by soldering each wire. One easy solder job per block, then the wiring is tool-free. |
| **Bulk electrolytic caps, 470–1000 µF, ≥16 V** | Absorb the millisecond current surge when motors start/stall so the 7.4 V rail doesn't sag. |
| **0.1 µF ceramic caps** (12-pack) | *Optional* brush-noise suppression — add only if needed. The connectorized 310 motors may already carry a factory cap; if not, add one across each channel's driver outputs (not the motor). |
| **18 AWG silicone wire (power) + 22 AWG (signal)** | 18 AWG safely carries the ~5.6 A all-stall motor current; 22 AWG for logic. |
| **Breadboard + Dupont jumpers** | For the *signal* bring-up only (Pico → driver inputs). **Motor power never touches a breadboard.** |
| **8-ch USB logic analyzer** (recommended) | See the 20 kHz PWM duty change as you command speed. |

Full master list with links/prices: [`../_engineering/products_to_buy.md`](../_engineering/products_to_buy.md). This phase's slice is in [`bom.md`](./bom.md).

---

## The cumulative system state (in prose)

This is Phase 1, so "cumulative" is short — but every later phase's README opens this way, building on what came before, so you always know the whole robot's state at a glance.

Right now the robot is a **drivetrain and nothing else.** A laptop's USB cable carries both power and a serial link to the **Pico 2W**, our real-time co-processor. The Pico generates six logic signals per driver board — for each of its two motor channels, two *direction* pins (IN1, IN2) and one *speed* pin (PWM) — plus one shared **STBY** enable line that must be held HIGH or the drivers sit dead. Those signals run over short Dupont jumpers on a breadboard into **two TB6612FNG** dual H-bridge boards. The drivers' logic side is powered from the Pico's own **3V3(OUT)** pin (a few tens of milliamps — well within its ~300 mA budget).

The *muscle* power is completely separate, and for this bench phase it's deliberately simple. The **2S LiPo** has a **built-in protection board** (overcharge, over-discharge, overcurrent, and short-circuit cut-offs) and a keyed **SM-2P male** output plug. A matching **SM-2P female pigtail** mates that plug and breaks it out to a red (+) and a black (−) lead. Each lead then lands in a **WAGO 221-413 lever-nut connector** — one WAGO fans the **+** out to both TB6612 **VM** (motor-voltage) pins, the other fans the **−** out to both boards' **GND**. Both drivers sit in **parallel** on the same 7.4 V rail. Each board receives that power at **soldered 2.54 mm screw terminals** on its power edge, so wires land by screw rather than by soldering each one. A **470–1000 µF bulk capacitor** sits across VM to swallow current surges; a **0.1 µF ceramic** can be added across each driver's motor outputs to quiet brush noise *if needed* (the motors are connectorized and may already carry a factory cap). The four motors connect through their **factory JST-PH 6-pin connectors** — the motor leads are live, the encoder conductors are present but parked (unread) until Phase 2.

There is **no fuse, master switch, or e-stop in the bench path yet** — and that's a deliberate choice, not an oversight. The LiPo's own protection board already opens on a short or overcurrent, and with the wheels in the air on a stand you have three instant kills already: unplug the SM-2P, type `stop` in the serial loop, or pull STBY (GP14) low in firmware. The external coordinated fuses, reverse-polarity P-FET, and physical fail-safe e-stop earn their place once a second branch (the Pi, in Phase 3) and floor driving arrive — they're explained in full below so you understand *why* they're coming, then built when they actually pay off.

Everything shares **one ground**, joined at a single star point at the LiPo negative terminal. The Pico's ground reaches that star through the laptop USB cable's ground for now (in Phase 3 the Pi takes over, and the Pico is powered from the Pi). The high-level Pi 5, the IMU, the lidar, the camera, the servos — none of that exists yet. We add them one phase at a time.

---

## Concepts this phase teaches

You can skim these if you already know them, but they're the *why* behind every wire.

### What an H-bridge is, and why we need one

A motor is just a coil. Put voltage across it one way and the shaft spins clockwise; flip the voltage and it spins counter-clockwise. The problem: your battery only has one polarity. To *reverse* a motor you'd have to physically swap its two leads — useless for software control.

An **H-bridge** solves this with four electronic switches (transistors) arranged in an "H," with the motor as the crossbar. Close the top-left + bottom-right switches and current flows left-to-right through the motor (forward). Close top-right + bottom-left and current flows right-to-left (reverse). Close both top switches (or both bottom) and the motor terminals are shorted together — that's an active **brake**. The one thing you must *never* do is close both switches in one vertical leg at once: that's a dead short across the battery (called *shoot-through*), which is exactly what fries cheap drivers. The TB6612FNG's internal logic prevents shoot-through for you, which is part of what you're paying for over a bag of discrete transistors.

The TB6612FNG packs **two** H-bridges (two motors) into one chip, with clean digital inputs. Two boards → four bridges → four independently controlled wheels. That independence is the whole reason we can do Mecanum later.

### Direction + speed: IN1/IN2 set direction, PWM sets speed

The TB6612 (in the "standard" mode we use) splits the job cleanly:

- **IN1 and IN2** are two digital pins that pick the bridge's *state*: `IN1=1, IN2=0` → forward; `IN1=0, IN2=1` → reverse; `IN1=IN2=0` → coast (outputs float); `IN1=IN2=1` → brake.
- **PWM** is a separate pin that *gates* that state on and off rapidly. **PWM (pulse-width modulation)** means switching a signal fully ON then fully OFF many thousands of times per second. The fraction of time it's ON is the **duty cycle**. At 50% duty, the motor effectively sees half the voltage *on average*, so it turns at roughly half speed — but the switches are always either fully on (low loss) or fully off (no loss), so almost no power is wasted as heat. That's why we PWM instead of using a variable resistor.

So "spin forward at 50%" = set IN1/IN2 for forward, set PWM to 50% duty. "Reverse at 30%" = flip IN1/IN2, set PWM to 30%. Simple and clean.

> **Note on our `Motor` class:** to keep the same code shape for both TB6612 (now) and the closed-loop work later, our class takes a signed value in `[-1.0, +1.0]`. Sign → direction (IN1/IN2), magnitude → PWM duty. One number in, correct direction and speed out.

### Why 20 kHz PWM (above hearing)

A motor + driver is a little speaker you didn't ask for. The PWM switching frequency literally vibrates the motor windings and the bridge, and if that frequency lands in the audible band (roughly 20 Hz–20 kHz) you get an annoying whine — worst around 1–8 kHz. Push the PWM frequency **up to 20 kHz**, right at the top edge of human hearing, and the whine disappears (most adults can't hear it at all). It also keeps the current ripple in the motor inductance smooth. Try 1 kHz once on the bench just to hear *why* this matters — then put it back to 20 kHz.

> **★ Cross-check against the working vendor car.** The Hiwonder RRC Lite controller — the proven, shipping board for this exact chassis/motors — drives these motors at **200 Hz**, ~100× lower. 20 kHz is a deliberate, defensible choice (inaudible, smoother current ripple), *not* a transcription of what's known-good — so it carries a small cost the vendor's 200 Hz avoids: higher per-cycle switching loss and EMI on the motor leads. **Bench-verify it pays off:** confirm the TB6612 stays cool under a sustained 1.4 A stall at 20 kHz, and that 20 kHz switching doesn't couple onto the encoder A/B lines (the star ground + signal-vs-power routing in Phase 2 address this). If heat is a problem, you have headroom to drop toward a few kHz.

### The STBY pin: the driver's master enable

The TB6612 has a **STBY** (standby) pin. While it's LOW, the chip's outputs are disabled no matter what you put on the inputs — the motors stay dead. You must tie **STBY HIGH (to 3.3 V)** to wake the driver up. We drive it from a Pico GPIO (GP14) so firmware can also *disable* all motors instantly by pulling it low — a useful software kill. Forgetting STBY is the single most common "why won't my motor move?" — the wiring is perfect, the code is perfect, and nothing happens because the chip is asleep.

### Fuses protect *wires*, not batteries — and why Phase 1 skips the external one

A common misconception: the fuse is there to protect the battery. It isn't — the 22 A-capable LiPo is the *strongest* thing in the chain. A fuse protects the **wires and connectors** downstream: if a wire shorts, the cell will happily push tens of amps through it until the insulation melts and something catches fire. A fuse is a deliberately weak link that opens *first*, before the wire overheats. You'd size it just above the worst-case current the wire should ever carry (motor branch worst case ≈ 5.6 A all-stall → a 7.5 A fuse) and below the wire's safe ampacity (18 AWG handles ~10 A in short runs).

So why doesn't Phase 1 have one? Two reasons. First, **this LiPo has a built-in protection board** that already opens on a short or sustained overcurrent — it *is* a (resettable) fuse, sitting right at the cells. Second, **Phase 1 has exactly one branch** (the motors), so there's nothing to coordinate yet. An external inline fuse here would be belt-and-suspenders, not load-bearing, so we leave it out to keep the bench build simple. (A single ~7.5 A inline blade fuse on the + lead is a perfectly reasonable optional addition if you want the extra margin — it just isn't required.)

The reason to learn fuse *sizing* now is **coordinated fusing** (a.k.a. *selectivity*), which arrives for real in **Phase 3** when the Pi becomes a second branch: a **10 A main fuse** right at the battery, with *smaller* branch fuses downstream (7.5 A motor branch, 5 A Pi branch). Because each branch fuse is smaller than the main, a fault on one branch blows *that branch's* fuse first — the main survives and the rest of the robot stays alive. If you made every fuse the same value you'd lose selectivity and a single motor short could take down the whole machine. **Selectivity only buys you something once there's more than one branch to protect** — which is exactly why it's deferred out of this single-branch phase.

### Reverse-polarity protection: P-FET vs diode *(deferred — built when a master switch is added)*

Plugging a LiPo in backwards is the most common *fatal* mistake in this hobby — it instantly destroys drivers and regulators. Here, the **SM-2P output connector is keyed**, so it physically can't mate backwards into a matching pigtail — which is most of the reason a dedicated reverse-protection part is *deferred* in Phase 1 (the connector already enforces polarity). You'll want one anyway once you add a panel-mount master switch and start swapping leads often, so the concept is worth knowing now.

The naive fix is a **series diode**: it only conducts one way, so a backwards battery does nothing. But a diode drops ~0.7 V *all the time*, even when correct, and at several amps that's several watts burned as heat for no benefit. The better fix is a **P-channel MOSFET high-side switch** (e.g. the Pololu #2815). A P-FET wired as an ideal-diode/reverse-protector has milliohms of resistance when conducting the right way — almost no voltage drop, almost no heat — yet blocks a reversed battery completely. Bonus: the #2815 is *also* a master ON/OFF switch, so one part does two safety jobs. The "P-FET beats a diode" reasoning is a classic power-electronics lesson worth being able to explain — you'll wire one in when the robot gets a real master switch.

### Fail-safe e-stop (normally-closed) *(deferred to the first mobile phase)*

A robot that drives on the floor needs an instant physical kill that doesn't depend on software. The standard answer is a **latching mushroom-button e-stop** wired **normally-closed (NC)** *in series in the motor branch only*. NC means the contacts are closed (current flows) in normal operation; pressing the mushroom *opens* them and cuts motor power. Two beautiful properties: (1) it kills only the wheels — the Pi and Pico stay powered so they can log *why* you hit it; and (2) because it's normally-closed, a **broken wire or loose connector also opens the circuit** and stops the motors. The *safe* state (motors off) is the *default* state. That's the essence of fail-safe design.

In Phase 1, though, **the wheels are in the air on a stand** — there's no runaway robot to chase down — and you already have three instant kills: unplug the SM-2P, type `stop` in the serial loop, or drop STBY (GP14) low. So the physical e-stop is **deferred to the first mobile phase**, where it actually earns its place; the design reasoning lives here so it's ready when you need it.

### Star ground: why "it's all ground" stops being true at high current

Logic signals (your 3.3 V PWM) are voltages measured *relative to ground*. The moment four motors yank ~5.6 A through the ground wires — switching at 20 kHz — that current creates real voltage drops along the ground path (`V = I × R_wire`). If the Pico's logic ground and the motor's power ground share a wire, those amps shift the very reference your 3.3 V signals are measured against, and you get noise, glitches, and resets. The fix is a **single-point star ground**: every ground returns on its *own* leg to one node at the LiPo negative. Keep the motor-return leg short and heavy (18 AWG); give logic its own thin leg. Don't daisy-chain (LiPo− → driver1 → driver2 → Pico) — that's how ground loops and noise creep in.

### Why motor power NEVER goes through a breadboard

A breadboard's spring contacts are rated for roughly **1 A**. Your motors can pull **~1.4 A each at stall** and ~5.6 A as a group. Push that through breadboard springs and they heat up, the contact resistance climbs, the voltage droops, and at worst the contact arcs and melts the board. The breadboard is for **signals only** (the 3.3 V PWM/direction jumpers, milliamps). All *motor power* — VM, the LiPo leads, the motor outputs — goes over soldered or screw-terminal 18 AWG. (In Phase 2 the interconnect graduates onto a soldered Pico screw-terminal carrier; for now keep the power path soldered/terminal and only the signal jumpers on the breadboard.)

---

## Pin tables (from the canonical map)

These are pulled verbatim from the canonical pin assignments in [`../_engineering/device_contracts.md`](../_engineering/device_contracts.md). **Use these exact pins** — every later phase depends on them. The wheels are named **FL** (front-left), **RL** (rear-left), **FR** (front-right), **RR** (rear-right).

### Pico 2W → motor control (2× TB6612FNG, standard mode: IN1/IN2 = direction, PWM = speed)

| Wheel | Board · Channel | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | Board A · ch A | GP2 | GP3 | GP4 |
| RL | Board A · ch B | GP5 | GP6 | GP7 |
| FR | Board B · ch A | GP8 | GP9 | GP10 |
| RR | Board B · ch B | GP11 | GP12 | GP13 |
| **STBY** (both boards, tied together) | — | **GP14 → must be driven HIGH** | | |

### Power & enable rails on the Pico

| Pico pin | Goes to | Note |
|---|---|---|
| **3V3(OUT)** (pin 36) | TB6612 **VCC** on *both* boards (logic supply) | A few tens of mA; well under the ~300 mA the 3V3 pin can source. |
| **GND** (any of several) | TB6612 GND on both boards + star ground | Common ground is mandatory (see pitfalls). |
| **VBUS** (pin 40) | (this phase) ← laptop USB 5 V | Powers the Pico. Phase 3 moves this to the Pi's USB. |

### Encoder pins (wired through the same connector but PARKED this phase)

You will mate the encoder conductors now (they share the motor's 6-pin plug) but you will **not** wire or read them until Phase 2. Listed here only so you don't accidentally reuse these GPIOs:

| Wheel | A (base) | B | Wheel | A (base) | B |
|---|---|---|---|---|---|
| FL | GP16 | GP17 | FR | GP20 | GP21 |
| RL | GP18 | GP19 | RR | GP26 | GP27 |

> ⚠️ **Encoder VCC is 3.3 V, never 7.4 V.** When you do power the encoders (Phase 2) their VCC comes from the Pico's 3V3(OUT) so their A/B outputs stay within the Pico's 3.3 V GPIO limit. Feeding an encoder 7.4 V puts 7.4 V logic onto a 3.3 V pin and kills it. For now, leave the encoder conductors parked — *don't* connect Enc-VCC to anything.

---

## Software task list

The firmware this phase is intentionally tiny. We use **MicroPython** on the Pico because it gives you a live REPL — you can poke a motor from an interactive prompt, which is perfect for bring-up. (Later phases may move performance-critical loops to C/PIO, but that's not needed yet.)

### 1. Flash MicroPython onto the Pico 2W

1. Hold the **BOOTSEL** button while plugging the Pico into your laptop's USB.
2. It mounts as a USB mass-storage drive (`RP2350`).
3. Download the latest **Pico 2 W** MicroPython `.uf2` from micropython.org and drag it onto that drive.
4. The Pico reboots and now appears as a USB **serial** device (e.g. `/dev/ttyACM0` on Linux/Mac, a COM port on Windows).
5. Open Thonny → set the interpreter to "MicroPython (Raspberry Pi Pico)". You should get a `>>>` REPL.

### 2. Write `motor.py` — the `Motor` class skeleton

A `Motor` owns one channel: its two direction pins (IN1/IN2) and its PWM pin. It exposes `set_speed(v)` with `v` in `[-1.0, +1.0]`: **sign picks direction, magnitude is PWM duty.** Keeping this signed interface now means Phase 2's PID can drive the exact same method.

```python
# motor.py  — TB6612FNG standard mode: IN1/IN2 = direction, PWM = speed
from machine import Pin, PWM

class Motor:
    def __init__(self, in1, in2, pwm, freq=20000):
        # IN1/IN2 are plain digital outputs that select direction.
        self.in1 = Pin(in1, Pin.OUT)
        self.in2 = Pin(in2, Pin.OUT)
        # PWM gates the bridge on/off at 20 kHz (above hearing).
        self.pwm = PWM(Pin(pwm))
        self.pwm.freq(freq)
        self.set_speed(0)

    def set_speed(self, v):
        v = max(-1.0, min(1.0, v))          # clamp to [-1, +1]
        duty = int(abs(v) * 65535)          # 16-bit duty: 0..65535
        if v > 0:                           # forward
            self.in1.value(1); self.in2.value(0)
        elif v < 0:                         # reverse
            self.in1.value(0); self.in2.value(1)
        else:                               # coast (both low)
            self.in1.value(0); self.in2.value(0)
        self.pwm.duty_u16(duty)

    def stop(self):
        self.set_speed(0)
```

### 3. A standby (STBY) helper

STBY must be HIGH to enable both drivers. Make it explicit so you can also disable everything instantly.

```python
from machine import Pin
stby = Pin(14, Pin.OUT)
def enable_drivers(on=True):
    stby.value(1 if on else 0)   # GP14 HIGH = drivers awake
```

### 4. Instantiate the four motors on the canonical pins

```python
# wheel = Motor(IN1, IN2, PWM)  -- exact pins from the canonical map
fl = Motor(2, 3, 4)
rl = Motor(5, 6, 7)
fr = Motor(8, 9, 10)
rr = Motor(11, 12, 13)
enable_drivers(True)            # GP14 HIGH or nothing moves
```

### 5. A throwaway serial command loop

Read lines from `sys.stdin` and parse a tiny per-wheel command set so you can test each wheel from your laptop terminal. This is intentionally crude — **Phase 3 replaces it with a real Mecanum protocol** (`v vx vy wz`). Do **not** build differential left/right logic here; we drive each wheel individually now.

```python
import sys
motors = {"fl": fl, "rl": rl, "fr": fr, "rr": rr}

# commands:  "<wheel> <value>"  e.g.  "fl 0.3"   |  "fr -0.5"  |  "stop"
while True:
    line = sys.stdin.readline().strip().lower()
    if not line:
        continue
    if line == "stop":
        for m in motors.values():
            m.stop()
        continue
    try:
        name, val = line.split()
        motors[name].set_speed(float(val))
    except (ValueError, KeyError):
        print("usage: <fl|rl|fr|rr> <-1.0..1.0>  |  stop")
```

> **Mecanum reminder:** there is **no** left/right grouping anywhere in this code. Each of the four wheels is addressed independently. Differential (left vs right) drive is *wrong* for this chassis and full kinematics arrive in Phase 3.

---

## Verification checklist

Do these **in order.** Each step assumes the previous one passed. Stop and fix at the first failure — don't push past a bad rail or a missing ground.

- [ ] **Dry continuity ritual (battery unplugged at the SM-2P).** Multimeter on beep. Confirm a continuous ground from the star point to: both TB6612 GND pins, the Pico GND, and the pigtail's black (−) lead. Confirm **no** beep between any V+ (VM, 3.3 V) and any ground — that would be a short. Also check each WAGO: with one wire wiggled, all three ports of that connector should beep continuous to each other (a WAGO joins *all* inserted wires — there is no "in" vs "out").
- [ ] **STBY and logic-supply sanity (still no LiPo).** Power the Pico from laptop USB only. Confirm 3.3 V on both TB6612 VCC pins (from Pico 3V3). Run `enable_drivers(True)` and confirm GP14 / STBY reads ~3.3 V with the multimeter.
- [ ] **Signals reach the driver inputs (still no motor power).** With VM *disconnected*, run `fl.set_speed(0.5)` from the REPL. Probe the FL channel's IN1/IN2 with the multimeter (or scope the PWM pin): one direction pin high, PWM toggling. Outputs (AO1/AO2) should show *no* motor voltage — there's no VM yet. This confirms the control path before any current flows.
- [ ] **Build and arm the power path (battery still unplugged).** Assemble it dry: SM-2P female pigtail → red (+) into WAGO #1 → both TB6612 VM; black (−) into WAGO #2 → both TB6612 GND. Land every power wire in its screw terminal and tug-test it. Double-check polarity with the meter (red = +) *before* plugging in. Now **plug in the SM-2P.** Measure **VM = 7.4 V** (up to ~8.4 V if freshly charged) on *both* TB6612 VM pins. Anything wildly off → **unplug the SM-2P immediately** and recheck.
- [ ] **First motion: FL only, wheels in the air.** Put the chassis on a stand. `fl.set_speed(0.3)` → FL turns slowly one way. `fl.set_speed(-0.3)` → it reverses. `fl.stop()`.
- [ ] **The other three, one at a time.** Repeat for `rl`, `fr`, `rr`. Each must spin both directions on command.
- [ ] **Direction convention.** With the chassis on its stand, a "+ value" on each wheel should roll the *robot* forward. If a wheel runs backwards, **swap that motor's two output leads (AO1↔AO2) at the driver — do NOT flip it in code.** Future-you reading the firmware will thank you.
- [ ] **All four forward, then all four reverse, 50% for 2 s each** (bot in the air). All four turn the correct way both times.
- [ ] **Spin a "twist" by hand** — command, say, `fl 0.4`, `rl 0.4`, `fr -0.4`, `rr -0.4` individually and watch the pattern. (This is just to feel independent control; real Mecanum twists come in Phase 3.)
- [ ] **Instant-kill test (the bench stand-ins for an e-stop).** With wheels spinning, verify each of your three kills works: (1) type `stop` in the serial loop → all four halt; (2) `enable_drivers(False)` (drops STBY/GP14 low) → all four go dead; (3) **unplug the SM-2P** → motors lose power while the Pico, still on laptop USB, stays alive (its LED stays on). The physical mushroom e-stop joins this list in the first mobile phase.

## Common pitfalls

- **STBY left LOW (the #1 "nothing happens").** Wiring perfect, code perfect, motors dead — because STBY is the driver's master enable and it's still low. Drive **GP14 HIGH** and confirm 3.3 V at both STBY pins. *Check this first* whenever a motor won't move.
- **No common ground.** The driver references its logic inputs to 3.3 V (Pico ground) and its motor outputs to 7.4 V (LiPo ground). If those grounds aren't physically tied, the chip sees garbage and behaves randomly. Always tie Pico GND → driver GND → LiPo− at the star point.
- **Confusing VCC with VM.** TB6612 **VCC = 3.3 V logic** (from the Pico). **VM = 7.4 V motor** (from the LiPo). They are different pins and different rails. Putting 7.4 V on VCC destroys the logic side; putting 3.3 V on VM means the motors barely move.
- **PWM frequency too low.** Audible whine, worst around 1–8 kHz. Keep it at **20 kHz** (above hearing). Drop to 1 kHz once just to hear the difference, then put it back.
- **Motor power through a breadboard.** Spring contacts (~1 A) can't carry ~1.4 A/motor; they heat, droop, and can arc. Keep *all* motor power on soldered/screw-terminal 18 AWG; breadboard for signals only.
- **Fixing wrong direction in software.** Don't. Swap AO1↔AO2 at the driver. Code stays clean; the sign convention stays universal across all four wheels.
- **Battery on before logic.** Wrong order. Bring up logic first (Pico on laptop USB), *then* plug in the SM-2P to power the motor rail. Logic must settle before motor power so the drivers don't glitch their outputs on a cold start.
- **Thinking a WAGO has an "in" and an "out."** It doesn't — all three ports of a 221-413 are electrically joined. "In vs out" is just *which wire happens to come from the battery*; the other two are both outputs to the two drivers. Strip ~11 mm of insulation, lift the lever, insert the bare copper fully (no stray strands), close the lever, and tug-test. A half-seated wire is the usual cause of a driver that browns out under load.
- **Cutting the battery's SM-2P connector.** Don't. The pack's keyed SM-2P male output is what prevents a reverse-polarity mating — buy the matching female pigtail and keep the factory plug. (Charge through the separate DC5.5×2.5 barrel jack with the 8.4 V charger; never charge through the SM-2P output.)
- **Skipping the bulk cap.** It usually "works" on the bench, then later sags the rail on stall. Fit the **470–1000 µF across VM** on each driver *now*. (The **0.1 µF motor cap is optional** — the connectorized 310s may already carry one; if brush noise crashes the USB link mid-drive or jitters encoder counts in Phase 2, add a 0.1 µF across each channel's driver outputs then.)
- **Cutting the motors' factory JST-PH connector.** Don't. It's a keyed 6-pin plug carrying motor + encoder. Buy mating pigtails and keep the factory plug — you'll need those encoder conductors intact in Phase 2.
- **Holding a stalled motor.** The TB6612 has thermal + overcurrent protection and will save itself, but don't drive into a wall and hold throttle. (Phase 2 adds a software stall-timeout.)

---

## 📐 Measurement Lab

> *The habit this project builds: don't believe a number until you've measured it. Each hardware phase names a quantity, the instrument, and the value you should see.*

**1. Scope the 20 kHz PWM (logic analyzer or pocket scope).** Probe a motor channel's PWM pin (e.g. GP4 for FL) while you command different speeds. **Expected:** a square wave at **~20 kHz** (period ≈ 50 µs). At `set_speed(0.5)` the high time should be ~50% of the period; at `0.25`, ~25%. Watch the duty visibly change as you change the command — that *is* speed control, made visible. (Bonus: probe IN1/IN2 and confirm one flips when you reverse.)

**2. Confirm the motor rail (multimeter, DC volts).** With the SM-2P plugged in, measure across each TB6612 **VM ↔ GND**. **Expected: 7.4 V** nominal (up to ~8.4 V freshly charged, sagging toward 6.0 V as the pack empties). If you see 0 V, suspect a half-seated wire in a WAGO or an unplugged SM-2P; if both boards read 0 V together, the LiPo's protection board may have latched off (let it rest / recharge).

**3. (Optional) Measure a motor's stall current vs spec.** Put a multimeter in *current* mode (10 A range) in series with one motor lead, hold the wheel firmly so it can't turn, and briefly command full speed. **Expected: ≤ ~1.4 A** (the 310's stall spec). Compare to the datasheet — this is exactly the number that justified choosing the TB6612 (3.2 A/channel peak) over a driver whose limit sits right on that stall knee. Keep the stall *brief*.

---

## 🎓 Phase wrap-up

**What you learned.** You now understand, concretely, how a microcontroller turns a number into motion: an H-bridge reverses current direction, IN1/IN2 pick that direction while a 20 kHz PWM sets average voltage (and stays above hearing), and a STBY pin gates the whole driver. You also stood up the *muscle half* of the power path to actual current numbers — a protected LiPo broken out at its keyed SM-2P plug, split through lever-nut connectors into two parallel driver rails landing on soldered screw terminals, with a bulk cap to ride out stall transients and a single-point star ground that keeps amps of motor-return current out of your logic reference. Just as important, you learned to *right-size protection to the situation*: why a single-branch bench build with a self-protected battery and wheels-in-the-air doesn't yet need the coordinated fuses, reverse-polarity P-FET, and fail-safe e-stop that a multi-branch, floor-driving robot does — and exactly when each of those gets added. And you learned the discipline that runs through the whole project: scope the PWM, measure the rail, and never trust a number you haven't seen on an instrument.

**Résumé bullet (copy-ready).**
> Designed and bench-validated the motor-control front end of a 4-wheel Mecanum robot: drove four independent brushed-DC motors via dual TB6612FNG H-bridges at 20 kHz PWM from an RP2350 (Pico 2W) co-processor on a single-point star ground, sized to the spec-derived ~5.6 A all-stall load (4 × 1.4 A stall); specified a coordinated-fuse, reverse-polarity-protected, e-stopped LiPo power tree and staged its build to match real risk as the system grew.

**Interview talking point.**
> "Ask me why I picked the TB6612FNG over a cheaper DRV8833. The motors stall at 1.4 A; the DRV8833 sustains only ~1.2–1.3 A before thermal shutdown, so the stall current sits right on its thermal knee — and a student *will* drive into a wall and hold throttle. The TB6612 gives 3.2 A/channel peak, real thermal headroom, built-in overcurrent protection, and a clean STBY enable, at the same price and identical control code. That's sizing a driver from the *stall* spec and the *thermal* limit, not from the average — the exact mistake that kills hobby drivers."

---

## What's next

**Phase 2 — Odometry** wires up the encoders that are already riding along in the motors' 6-pin connectors, decodes their quadrature signals in the Pico's PIO hardware, *measures* the counts-per-revolution empirically (don't trust the calculated 1040), and closes the speed loop with per-wheel PID. After that, a commanded speed actually corresponds to a real wheel velocity — the foundation every later phase (kinematics, SLAM, navigation) stands on.
