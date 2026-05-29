# `firmware/` — robocar Pico 2W motor-controller firmware (C / Pico SDK)

This is the **buildable C firmware** for robocar's motor brain (Raspberry Pi Pico 2W / RP2350A).
It realizes the **"C from Phase 2"** decision: closed-loop wheel control and Mecanum kinematics run as
bare-metal Pico-SDK C, not MicroPython. The Phase 2/3 docs keep their teaching skeletons and **excerpt**
the modules here.

> **Status: bring-up skeleton, not yet bench-validated.** The control/kinematics *math* is validated
> (see Provenance); the board glue (`tb6612`, `quadrature_encoder`, `main`) and **every numeric constant**
> still need the Phase 1/2/3 bench steps. Treat the `#define`s as starting points, not truth.

---

## Provenance & clean-room status  ★ read this

These modules were written by **studying the working vendor Hiwonder RRC Lite firmware as a reference
oracle** (it drives this exact chassis/motors) and **re-expressing the platform-independent math** in
Pico-SDK idiom with robocar's conventions. **No vendor source line is reproduced verbatim.** The Mecanum
relations are the standard, publicly-known X-config equations; the positional PID is robocar's *own* prior
Phase 2 design; the incremental PID is algorithmically equivalent to the vendor's (provided as a reference
only). The vendor inverse-kinematics matrix was numerically verified to be **sign-for-sign identical** to
robocar's existing Phase 3 `inverse()` — so this is cross-validation, not a new claim.

**License:** the vendor firmware is **proprietary** (© 2023 Hiwonder / Lu Yongping, no open license in-file).
It is a **read-only reference**, consistent with `_manufacturer_device/` being read-only input. The control
math itself is unpatentable and freely re-implementable; nothing here is copied. **A human must confirm
licensing before shipping derived firmware.** `quadrature_encoder.pio` (see below) is from Raspberry Pi's
`pico-examples` (BSD-3-Clause).

---

## Directory map

```
firmware/
  CMakeLists.txt
  src/
    mecanum_kinematics.{h,c}   body twist (vx,vy,wz) <-> 4 wheel speeds, FL,RL,FR,RR  [validated == Phase 3]
    wheel_pid.{h,c}            POSITIONAL PID + conditional-integration anti-windup    [robocar's Phase 2 design]
    incremental_pid.{h,c}      vendor velocity-form PID — REFERENCE ONLY, not wired in
    wheel_control.{h,c}        the control tick: encoder -> rad/s -> PID -> TB6612 + comms-watchdog
    tb6612.{h,c}               TB6612FNG sign-magnitude driver (IN1/IN2 + 20 kHz PWM + STBY)   [robocar-original]
    quadrature_encoder.{h,c}   wrap-safe int64 count over a PIO decoder                         [robocar-original]
    quadrature_encoder.pio     <-- DROP IN from pico-examples (see below)
    main.c                     init + 50 Hz control timer + ASCII 'v/s/ping' link + hardware watchdog + heartbeat
```

`quadrature_encoder.pio` is **not shipped here** (to avoid an unverified transcription). Copy it from
`pico-examples/pio/quadrature_encoder/quadrature_encoder.pio` — `quadrature_encoder.c` differences its
32-bit count into a **wrap-safe int64** (robocar's ADR: never reconstruct a 16-bit overflow the way the
STM32 vendor firmware had to).

---

## Build

```sh
# needs PICO_SDK_PATH set to your pico-sdk checkout, and arm-none-eabi-gcc
cp <pico-sdk>/../pico-examples/pio/quadrature_encoder/quadrature_encoder.pio src/
mkdir build && cd build
cmake -DPICO_BOARD=pico2_w ..
make
# -> robocar_firmware.uf2  (drag onto the Pico 2W in BOOTSEL mode)
```

---

## ⚠️ Caveats inherited from the vendor reference — DO NOT bake these in

Every one of these is a place the vendor's numbers are *wrong for robocar* and must come from the bench
(and land in `_engineering/` first — that stays the source of truth):

| Thing | Vendor value | robocar action |
|---|---|---|
| **PID gains** | kp=−36, ki=−1, kd=−1 | **Negative only because the vendor's encoder counts opposite its PWM.** robocar fixes polarity ONCE (PIO sign / TB6612 IN1·IN2) and uses **positive** gains. Seed: kp=0.5, ki=2.0, kd=0 (Phase 2). Re-tune. |
| **COUNTS_PER_REV** | 1040 (13×4×20) | **Measure** in Phase 2 (Lab 1). 1040 is a sanity-check target only — never hardcode. |
| **Wheel radius / track / wheelbase** | JetAuto preset (96.5 / 195 / 218 mm) | **Different chassis.** Measure R, lx, ly on the actual M1 in Phase 3. |
| **Rotation lever `L`** | 2·(lx+ly) = 413 mm | robocar's standard form uses **L = lx+ly** (HALF dims). Pasting 413 *doubles* the yaw gain. |
| **Velocity IIR α** | 0.9 | Loop-rate dependent (vendor 100 Hz, robocar 50 Hz). Use as a start, re-tune. |
| **PWM deadband** | 25 % | Per-motor/driver/battery stiction. Measure robocar's min-moving duty. |
| **Control rate** | 100 Hz | robocar chose **50 Hz**. Pass the *measured* `dt`, never assume 0.01 s. |
| **Motor PWM carrier** | 200 Hz (audible) | robocar uses **~20 kHz** (`tb6612.c`). Do not copy 200 Hz. |
| **Right-side sign flips** | `motors[2],[3]` negated | **Motor-wiring polarity, NOT kinematics.** Kinematics emits true +ω-forward speeds; fix polarity in the driver/PIO layer. Folding the vendor flips into kinematics *and* correcting in the driver double-negates the right wheels — the exact "two wheels swapped" bug CLAUDE.md warns about. |
| **Stall check** | none (vendor has none) | robocar-added (stub in `wheel_control.c`); also drive **STBY HIGH** at init (vendor source omits it). |

**Load-bearing open item:** the ±1 sign matrix assumes 45° X-config Mecanum rollers. If the wheels are
plain, all of this kinematics is invalid (CLAUDE.md open item #1) — confirm the rollers first.
