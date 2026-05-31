# Engineering Decisions

> **What this is.** A decision log (think "Architecture Decision Records"). Every meaningful change from the original plan is recorded here with four things: **what** changed, **why** (the engineering reason, with numbers), **what else we considered**, and **what you learn** from it. A good engineer can defend every choice — this is where you'll find the defense for each one. When an interviewer asks "why did you pick X over Y?", the answer is in this file.
>
> Decisions are tagged **[MUST]** (safety/correctness — do this), **[SHOULD]** (strong improvement), or **[OPTIONAL]** (deeper learning). They were produced by a research pass (datasheets + three adversarial electrical verifications) and a six-lens design brainstorm, then reconciled.

---

## The one decision that frames all the others

**Your kit is a Hiwonder MentorPi M1**, not a generic 4WD car. It ships with a Mecanum chassis, a *complete* working brain (the RRC Lite STM32 controller + a ROS2 image) that already does SLAM and navigation out of the box. So why rebuild it?

**Because the goal is learning, not a finished product you didn't build.** Using the vendor stack teaches you to run someone else's robot. Rebuilding the low-level stack yourself — motor control, odometry, sensor fusion, SLAM, navigation — on a two-brain architecture you fully understand teaches you *to be the engineer who could have built the RRC Lite.* That's the resume story: *"I took a commercial ROS2 robot, set aside its vendor controller, and re-implemented the entire real-time control and autonomy stack from first principles on a Raspberry Pi 5 + RP2350 co-processor."*

So the project **reuses the mechanical robot** (chassis, wheels, motors, lidar, servos, battery — they're good hardware) and **rebuilds the electronics and software.** The RRC Lite isn't trashed; it's set aside, and you can always restore stock firmware. Decisions below follow from this frame.

---

## Architecture

### [KEEP] Two-brain split: Pi 5 (Linux) + Pico 2W (real-time)
- **Why:** This is the pedagogical heart and a genuinely correct design. A Linux SBC can't guarantee microsecond-stable PWM or count ~17 kHz of aggregate encoder edges while also running SLAM — the OS will preempt it. A microcontroller is *deterministic*. Splitting "soft real-time, compute-heavy" (Pi) from "hard real-time, deterministic" (Pico) is exactly how real robots (including the RRC Lite) are built.
- **What it teaches:** the single most important systems-engineering idea in robotics — *where computation should live.* Plus inter-processor communication over serial, and the RP2350's PIO (a tiny programmable state machine that counts encoder edges in hardware, free of the CPU).
- **Note:** the Pico is *kept* per your directive and because there's no good reason to drop it. The RRC Lite you're replacing plays the identical role with an STM32 — so you're not doing something weird, you're doing the standard thing, by hand.

### [MUST] Firmware in C (Pico SDK) from Phase 2; a deterministic ~50–100 Hz control tick
- **What:** Phase 1 first-light (spin a motor open-loop) may stay in **MicroPython** — the gentlest on-ramp, and timing is irrelevant there. But from **Phase 2 onward the closed-loop firmware is C on the Pico SDK** (see [`../firmware/`](../firmware/)). The control loop runs on a fixed **~50–100 Hz** tick (Phase 2 uses 50 Hz) with one hard rule — **nothing blocks the tick** (no allocation, no blocking I/O, no `print` inside it) — and it is fed the **real measured `dt`**, never an assumed constant.
- **Why:** the Pico owns a genuinely hard-real-time load: 20 kHz PWM, four PIO quadrature decoders (~17 kHz aggregate edges), four PID loops, and two watchdogs. MicroPython is fine for bring-up, but its interpreter/GC jitter makes a *deterministic* tick at that load an untested gamble; C on the SDK gives predictable timing on the same silicon. C was chosen explicitly for determinism. (The working vendor RRC Lite reaches the same conclusion from the opposite start — bare-metal C at ~100 Hz.)
- **★ int64 wrap-safe encoder accumulator:** the PIO decoder returns a wide count; firmware keeps a cumulative **`int64` per wheel** and differences it **wrap-safely**. Odometry therefore never reconstructs a 16-bit counter overflow the way the STM32 vendor firmware was forced to (its `60000`-period overflow bookkeeping is a hardware-timer artifact robocar's PIO design sidesteps). Never hardcode the tick `dt` or `COUNTS_PER_REV` — both are measured.
- **Considered:** keeping the control loop in MicroPython (incl. `@micropython.native`/`viper`). Viable at low rates, unproven for a jitter-bounded tick under the full load — not worth the risk for the one loop that must not stutter. The buildable C lives in `firmware/` and is excerpted by the Phase 2/3 docs.
- **What it teaches:** why a control loop needs *bounded* timing (not merely *fast* code), how a fixed-rate scheduler + measured `dt` keep PID tuning reproducible, and why offloading edge-counting to PIO is what makes the CPU budget close.

### [MUST] Mecanum kinematics end-to-end (was: differential drive)
- **What:** Replace the original `v_left / v_right` differential model with full **3-DOF Mecanum kinematics**: a body twist `(vx, vy, ωz)` maps to four independent wheel speeds through the standard Mecanum matrix (inverse kinematics), and the four measured wheel speeds map back to `(vx, vy, ωz)` for odometry (forward kinematics). Touches Phases 1, 2, 3, 4, 8.
- **Why:** The chassis *is* Mecanum (angled rollers) — that's its whole point: it can **strafe sideways** and rotate while translating. Modeling it as differential is physically wrong; it makes strafing impossible and silently corrupts odometry the instant the bot moves laterally. The wiring barely changes (you already need 4 independent motors and 4 encoders) — the change is in the math and the serial protocol (`v vx vy wz` instead of `v left right`).
- **Considered:** driving the Mecanum wheels in "tank mode" (group left pair / right pair) to keep the differential code. Works, but wastes the hardware, scrubs the rollers, and skips the best learning.
- **What it teaches:** the inverse-kinematics matrix is a beautiful, concrete linear-algebra application — you derive it from wheel geometry and verify it empirically (command pure `vx`, pure `vy`, pure `ωz` one at a time and watch the bot). Holonomic vs non-holonomic motion is a real robotics concept you'll be able to explain.

---

## Motor driver & control

### [MUST] TB6612FNG instead of DRV8833 (2 boards, one motor per channel)
- **Why:** The verified numbers: the 310 motor stalls at **1.4 A**; a DRV8833 channel is rated 1.5 A RMS but a *bare module* only sustains ~1.2–1.3 A before thermal shutdown — so the stall current sits right on the DRV8833's thermal knee. A student *will* drive into a wall and hold the throttle. The **TB6612FNG is 1.2 A continuous / 3.2 A peak per channel** with real headroom, built-in thermal + overcurrent protection, and a clean STBY enable — at the same ~$5 price and the *identical* control code (sign-magnitude PWM on IN1/IN2/PWM). It was the unanimous call across the motor and mechanical design lenses.
- **Bonus:** one-motor-per-channel (4 channels for 4 wheels) is *required* for Mecanum anyway, and two TB6612 boards give exactly four channels.
- **PWM frequency — 20 kHz (deliberate divergence from the vendor's proven 200 Hz):** the shipping RRC Lite drives these motors at **200 Hz**; robocar runs **20 kHz** to keep switching above the audible band and the current ripple smooth. The trade is higher switching loss/EMI than 200 Hz — so **bench-verify** the TB6612 stays cool at the 1.4 A stall and that 20 kHz doesn't couple onto the encoder lines; drop toward a few kHz if thermals demand it. (Rationale + test in `phase_1_powertrain/README.md` → "Why 20 kHz PWM.")
- **Considered & rejected:** keep DRV8833 + add a current-sense resistor and firmware stall-timeout (works, but more parts/code to reach the same place); DRV8871 (3.6 A, but single-channel → need 4 boards); L298N (ancient, hot, huge voltage drop — *don't*); paralleling DRV8833 channels (unnecessary at 1.4 A).
- **What it teaches:** how to *size a driver from a stall-current spec and a thermal limit*, not from the average — the exact mistake that kills hobby drivers.

### [MUST] Measure `COUNTS_PER_REV`; never hardcode it
- **Why:** The original baked in `560` from a guessed `7 PPR × 20 × 4`. The datasheet points to a **13-line ring → 13 × 4 × 20 = 1040** (≈2× off). Every velocity, odometry, and SLAM distance is scaled by this constant, so a wrong value silently corrupts Phases 2/4/7/8. The fix: mark a wheel, rotate exactly one output turn by hand, read the count delta — *that* is your constant.
- **What it teaches:** datasheet numbers are starting points; the bench is the authority. This habit (verify the constant empirically) is the difference between "I think it works" and "I measured it."

### [SHOULD] Software stall detection (implemented in Phase 2) instead of per-channel current sensors
- **Why:** For ≤1.4 A motors, per-channel current ICs are overkill (and the TB6612 has no sense pin). The Pico already knows commanded PWM and measured velocity — "high duty + near-zero velocity = jam" is free. Detect it, back off, protect the driver.
- **Implemented, not just planned:** Phase 2 wires this as a real firmware task — if a wheel is commanded non-zero but its measured ω stays ~0 for N ticks (~0.5–1.0 s), force that wheel's PWM to 0 and raise a fault flag. Gradable bench test: **hand-hold one wheel while the others spin** → only the held wheel cuts. (The working vendor firmware has *no* stall check at all — this is a robocar addition.)
- **Optional stretch — a measured current trip:** a ~0.1 Ω low-side shunt into a spare Pico ADC gives the real stall current (~65 mV @ 0.65 A, ~140 mV @ 1.4 A) to validate the software inference. ⚠️ **ADC-pin tradeoff:** RR's encoder A/B sit on **GP26/GP27 = ADC0/ADC1**, so only **GP28 (ADC2)** is free — a full 4-motor shunt set isn't possible without remapping RR off the ADC pins. Treat it as a one-wheel teaching rig, not a fleet feature. Cross-references the INA219 ~3.2 A ceiling (open item, below).
- **What it teaches:** you can often *infer* a physical fault from signals you already have, no new sensor required — a real systems instinct.

---

## Sensing

### [MUST] BNO055 in UART mode, not the Pi's hardware I²C
- **Why:** The BNO055 stretches the I²C clock during the ACK bit, and the Pi's hardware I²C controller **mishandles clock stretching** — you get intermittent `OSError`s *and, worse, silently corrupted orientation bytes.* A corrupted heading on an autonomous robot is more dangerous than a clean failure. Adafruit explicitly recommends UART on a Pi. Set PS1 HIGH, wire to a Pi UART. (Bonus: moving the IMU off I²C-1 frees the bus for the PCA9685 and INA219.)
- **Considered:** slowing I²C to 100 kHz (the original's suggestion — *mitigates, doesn't fix*); a bit-banged software-I²C bus (works, more setup); hanging the IMU off the Pico's I²C (the Pico handles clock stretching, but adds load to the real-time core).
- **What it teaches:** not all I²C is equal; "it's just I²C" hides controller-level quirks. Also a second serial transport (UART) beyond the Pico link.

### [OPTIONAL] Write your own sensor fusion as a learning module
- **What:** Implement a complementary filter (and optionally a 1-state Kalman) on the BNO055's *raw* accel+gyro, and validate it against the chip's own fused output as ground truth. Plot gyro-drift vs accel-noise vs fused.
- **Why:** "Sensor fusion / Kalman filter" is one of the highest-signal phrases on a robotics resume — but only if you can show you understand it. The BNO055's easy fused output gets you to a working robot fast; this optional module gets you the deep understanding, with a built-in ground-truth reference on the *same board* and zero added cost.
- **What it teaches:** why you fuse at all (gyro drifts, accel is noisy), and how a complementary filter trades them off in one line of code.

### [MUST] Camera Module 3 with the Pi 5 22-to-15-pin CSI cable (not a generic "longer CSI cable")
- **Why:** The Pi 5 moved to a 22-pin 0.5 mm CSI connector; the camera is 15-pin 1 mm. A generic 15-to-15 cable **physically will not mate to a Pi 5** — exactly the "stall mid-build" trap the original parts list warned about, while listing the wrong cable. CSI (vs a USB camera) also keeps the USB bus free for the Pico/lidar and streams cleanly via picamera2.
- **What it teaches:** connectors are part of the contract; "longer cable" isn't a spec — pin count and pitch are.

### [SHOULD] STL-19P lidar via a CP2102 USB-UART adapter (not direct Pi GPIO UART)
- **Why:** In the kit the lidar is wired to the RRC Lite, not shipped as a USB stick — so the original's "Option A: just plug in the USB adapter" likely doesn't apply, and a beginner defaults to the fragile direct-UART path that fights the Pi's Bluetooth and serial console. A $6 CP2102 (jumper set to **3.3 V** — the lidar's TX is 3.3 V and *not* 5 V tolerant) gives a clean `/dev/ttyUSB0`, keeps Bluetooth, and isolates the lidar.
- **What it teaches:** USB-UART bridges, and why logic-level matching protects a 3.3 V receiver from a 5 V driver over time.

### [OPTIONAL] Reuse the kit depth camera, body-fixed
- **Why:** If included, it's a free, high-payoff sensor (compare passive 2D vs structured-light depth vs 2D lidar, then fuse depth+lidar). But it's heavy/long (don't gimbal it), USB (counts against the USB budget), and its SDK is ROS-centric — so it's optional and must not block the core build. Start with its plain RGB UVC stream.

---

## Actuation

### [SHOULD] Reuse kit LFD-01 servos (else MG90S), keep the PCA9685
- **Why:** Reuse what the kit ships (the LFD-01 even has anti-stall firmware). If absent, buy **MG90S** (metal gear), never SG90 — the original even listed "SG90 too weak" as a likely pitfall, so make the robust choice up front. Keep the **PCA9685**: it isolates jittery servo PWM from both the Pi's OS *and* the Pico's real-time loop (a servo bug can't perturb the PID timing), and it teaches the I²C-to-PWM abstraction.
- **Considered:** drive servos straight off spare Pico hardware PWM (one fewer part, valid — kept as a *documented alternative* for a leaner build) or off Pi GPIO (jittery — don't).
- **Conflict resolved:** one design lens wanted to delete the PCA9685, another to keep it. Keep won on the real-time-isolation argument, especially since moving the IMU to UART freed the I²C bus.

### [MUST] Servos on a dedicated 5 V UBEC, never the Pi rail
- **Why:** The single most important actuation fix and a real bug in the original (Phase 5 took servo V+ from Pi header pins 2/4). Two servos at ~1.5 A stall + 3–4 A inrush, stacked on the Pi's load, sag the rail past the 4.63 V undervoltage line and **reboot the Pi mid-SLAM.** A dedicated UBEC + bulk cap isolates the transient. Common ground only.
- **What it teaches:** the rule "noisy, high-current actuators get their own regulated rail; logic stays clean" — and *why* (shared-impedance coupling).

---

## Power & protection

> Sizing detail is in [power_budget.md](./power_budget.md); these are the *decisions*.

### [MUST] Single-LiPo power tree with a 5 V/5 A buck (retire the power bank)
- **Why:** A 5 V/3 A bank can't run a Pi 5 (600 mA USB cap + brownout — see power_budget). Powering the Pi from the 7.4 V LiPo via a **Pololu D24V50F5 (5 V/5 A)** fixes it, runs the robot untethered on one battery, and is the real DC-DC lesson. Set `usb_max_current_enable=1` so the Pi allows the higher USB draw the rail can supply.
- **Considered:** a 5 V/5 A USB-C PD power bank + a PD-trigger board (valid for bench/tethered use; the demoted power bank can serve that role in early phases). The single-battery buck is better for a *mobile* robot.
- **What it teaches:** battery → regulated rails, efficiency (`P_in = P_out / η`), and current budgeting.

### [MUST] Reverse-polarity protection + master switch
- **Why:** There was *zero* protection against the most common fatal LiPo mistake — plugging the battery in backwards — which instantly destroys the drivers, the buck, and maybe the Pi. A **P-FET high-side switch (#2815)** is one part that does master-disconnect *and* reverse protection.
- **What it teaches:** why a P-channel MOSFET high-side switch beats a series diode (no ~0.7 V drop, no heat) — a classic power-electronics lesson.
- **Staged:** Phases 1–2 lean on the kit pack's **keyed SM-2P** output (it physically can't mate reversed) and simply unplug to disconnect; the **#2815 P-FET switch is added in Phase 3**, when a second branch (the Pi buck) makes a real master-disconnect worthwhile and lead-swapping becomes routine. (Phase 3 also swaps in the higher-current **URGENEX pack**, whose keyed **Deans T-plug** likewise can't reverse-mate — so the P-FET backs up the keying, it doesn't replace it.)

### [MUST] Coordinated fusing (10 A main / 7.5 A motor / 5 A Pi)
- **Why:** The original's single 5 A fuse is below the realistic ~8–10 A total and would nuisance-trip — and a student who's sick of a nuisance-tripping fuse *removes* it (a safety regression). Right-size the main and add per-branch fuses so a motor short doesn't kill the Pi.
- **What it teaches:** fuses protect *wires*, not batteries; and **selectivity** — branch fuses smaller than the main so faults isolate to one branch.
- **Staged:** the coordinated set is **built in Phase 3**. Selectivity does nothing until there's more than one branch, and Phases 1–2 are a single motor branch fed by a LiPo whose **built-in protection board** already opens on a short/overcurrent — so the external fuses wait until the Pi buck adds the second branch. (A single optional 7.5 A inline blade in Phase 1 is fine but not required.)

### [MUST] Latching NC mushroom e-stop in the motor branch
- **Why:** A moving closed-loop Mecanum robot needs an instant physical kill. Wiring it **normally-closed** in the **motor branch only** means a press (or a broken wire) coasts the wheels while the Pi/Pico stay alive to log the fault. Pairs with the Pico's software comms-watchdog (defense in depth).
- **What it teaches:** fail-safe design (the safe state is the *default* state) — a core safety-engineering principle.
- **Staged:** **added in Phase 3**, the first floor-driving phase. While the wheels are in the air on a stand (Phases 1–2) there's no runaway to chase, and three instant kills already exist (unplug the SM-2P, `stop` in the serial loop, drop STBY/GP14 low) — the physical mushroom earns its place once the robot can actually drive away.

### [SHOULD] INA219 self-monitoring (at 0x41) + balance-port LVA buzzer
- **Why:** Two layers of over-discharge protection. The INA219 lets the Pi read pack V and I → a live dashboard and a graceful low-voltage park/halt (set its address to **0x41**, not the default 0x40 which collides with the PCA9685). The ~$5 balance-port buzzer is the dumb hardware backstop that works even if the Pi crashes.
- **What it teaches:** instrument your own system; never trust a single layer for safety (defense in depth); and watch for I²C address collisions.

### [SHOULD] Bulk + ceramic decoupling, and a star ground
- **Why:** 0.1 µF ceramics at each motor kill brush noise but not the low-frequency rail sag when four motors yank amps in milliseconds — that's what bulk caps (470–1000 µF) on the motor and servo rails absorb. And a single-point **star ground** at the LiPo negative keeps high-current motor return out of the logic reference (vs the original's daisy-chain).
- **What it teaches:** the decoupling hierarchy (bulk vs ceramic) and ground topology — why "it's all ground" isn't true once amps are switching.

---

## Mechanical & wiring

### [SHOULD] Reuse the kit Mecanum chassis (delete buy-a-chassis line)
- **Why:** The project rebuilds electronics, not the frame. A second chassis is wasteful and risks not fitting the 27 mm motors / STL-19P. The kit frame is purpose-built.

### [MUST] Build around the motors' factory JST-PH 6-pin harness; no motor power through a breadboard
- **Why:** Each 310 motor combines motor power + encoder in one keyed 6-pin connector. The original treated them as loose wires and said "leave encoders dangling" — discarding a reliable keyed connector and creating 24 mis-wireable conductors. Keep the connector (buy mating pigtails). And **breadboard spring contacts are ~1 A** — routing the ~5.6 A all-stall motor current through them heats, droops, and can arc. Breadboard is for Phase 1–2 *signal* bring-up only; motor/encoder interconnect moves to a **soldered Pico screw-terminal carrier.**
- **What it teaches:** connectors and current ratings are reliability engineering; the breadboard → soldered-carrier progression is how real prototypes mature.

### [SHOULD] Ferrules + crimp tool, M2.5 nylon standoffs, real strain relief (no hot glue)
- **Why:** Bare stranded wire in a screw terminal works loose; solder cold-flows under clamp pressure. **Ferrules** fix both. The Pi 5/STL-19P are **M2.5, not M3** (a generic M3 kit won't fit), and **nylon** standoffs are non-conductive and keep ferrous mass off the magnetometer. **Hot glue** gets brittle near a warm Pi and releases under vibration — use cable-tie mounts + VHB foam, especially on the Pi↔Pico "spinal cord" USB cable.
- **What it teaches:** the unglamorous 80% of reliability is mechanical — terminations, standoffs, strain relief, vibration.

---

## Educational scaffolding (cross-cutting)

### [SHOULD] A USB logic analyzer ($13) as a core tool
- **Why:** The build is full of invisible signals (PWM duty, quadrature phase, UART framing, I²C clock-stretch). A $13 analyzer + free PulseView turns each into a screenshot for your writeup — and is the *exact* right tool for the clock-stretch bug. The multimeter alone can't see a waveform.

### [SHOULD] A PID step-response lab (Phase 2)
- **Why:** PID is a top resume item, but only if you can show it. Stream `(t, setpoint, measured, pwm)` over serial, log to CSV, plot the step response (rise time, overshoot, settling). Eyeballing a spinning wheel teaches nothing; a plot teaches control theory you can talk about.

### [SHOULD] "Phase Wrap-Up" + "Measurement Lab" in every phase
- **Why:** The project already produces 8 resume-worthy milestones; this structure makes sure you can *capture and articulate* them. Each phase ends with **What you learned / a copy-ready résumé bullet / an interview talking point**, and each hardware phase has a **Measurement Lab** (a named quantity, an instrument, an expected number) to build the verify-it-yourself habit.

### [OPTIONAL] ROS2 bridge capstone (after Phase 8)
- **Why:** ROS2 is the most-requested robotics-internship keyword, and you're deliberately bypassing the kit's ROS2 image. A thin Pi-side bridge (`pyserial` → ROS2, republishing telemetry as `/odom` and `/scan`, subscribing `/cmd_vel` → your Mecanum kinematics) lets you *honestly* claim ROS2 — and you'll understand it as a translation layer over a stack you built, not a black box.

---

## Open questions for *you* to resolve on your physical kit
These don't block the build; the docs carry sensible defaults + fallbacks, but you should confirm:
1. **Wheels Mecanum?** (angled rollers) — if plain, keep differential kinematics instead.
2. **`COUNTS_PER_REV`** — measure it (calculated 1040; verify in Phase 2).
3. **LFD-01 servos / depth camera included?** — decides reuse vs buy / use vs ignore.
4. **INA219 vs INA226 + external shunt** — the INA219's ~3.2 A ceiling clips on stall; fine for monitoring, but an INA226 + shunt captures the full ~12 A transient if seeing stall spikes is a goal.
5. **Reverse-protection part** — #2815 slide switch (one part, ~16 A) vs rocker + #5381 (separate tactile switch, 12 A). Use #5381 only if you want a distinct/panel-mount master switch — the ~16 A #2815 already has more current margin than the 12 A #5381.
