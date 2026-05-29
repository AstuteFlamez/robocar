# Phase 1 — Powertrain

## Goal
All four motors spin under Pico 2W software control. You can command forward, reverse, and turn from a serial terminal on your laptop.

## Difficulty
Medium · 1 weekend

## Prerequisites
None — this is the starting point.

## New parts used this phase
- 4× JGA27-310R-74 motors (already have)
- 2× DRV8833 dual H-bridge modules
- 1× Pi Pico 2W
- 1× 2S LiPo battery (already have)
- 1× power switch + inline fuse
- 12× 0.1 µF ceramic capacitors (for motor noise suppression)
- Wires, breadboard, headers

## Schematic — current state of the build

```
                ┌──────────────────┐
                │     Pico 2W      │   (USB to laptop for now)
                └────────┬─────────┘
                         │
              GP2–GP9    │    3V3, GND
              (motor     │    (logic power
               control)  │     for drivers)
                         │
              ┌──────────┴───────────┐
              │                      │
              ▼                      ▼
      ┌───────────────┐      ┌───────────────┐
  VM─►│  DRV8833 #1   │  VM─►│  DRV8833 #2   │
      │  LEFT side    │      │  RIGHT side   │
      │ AIN1/2 BIN1/2 │      │ AIN1/2 BIN1/2 │
      │ AO1/2  BO1/2  │      │ AO1/2  BO1/2  │
      │ nSLEEP→3V3    │      │ nSLEEP→3V3    │
      └──┬─────────┬──┘      └──┬─────────┬──┘
       M1,M2     M1,M2         M1,M2     M1,M2
         ▼         ▼             ▼         ▼
      [FL mtr] [RL mtr]      [FR mtr] [RR mtr]
        (encoders unused this phase — wires can be left dangling)

                ┌──────────────────┐
                │  2S LiPo 7.4V    │  + ──[fuse]──[switch]──► both DRV8833 VM
                │                  │  − ──► common ground rail
                └──────────────────┘

   Common ground:  LiPo− ── DRV8833 GND ── Pico GND
                   (Pico GND ↔ laptop GND via USB cable for now)
```

## Pico 2W → DRV8833 #1 (left side)

| Pico pin   | Driver pin | Motor          | Function          |
|------------|------------|----------------|-------------------|
| GP2        | AIN1       | Front-left     | PWM forward       |
| GP3        | AIN2       | Front-left     | PWM reverse       |
| GP4        | BIN1       | Rear-left      | PWM forward       |
| GP5        | BIN2       | Rear-left      | PWM reverse       |
| 3V3 (OUT)  | nSLEEP     | —              | tie high (enable) |
| GND        | GND        | —              | common ground     |

## Pico 2W → DRV8833 #2 (right side)

| Pico pin   | Driver pin | Motor          | Function          |
|------------|------------|----------------|-------------------|
| GP6        | AIN1       | Front-right    | PWM forward       |
| GP7        | AIN2       | Front-right    | PWM reverse       |
| GP8        | BIN1       | Rear-right     | PWM forward       |
| GP9        | BIN2       | Rear-right     | PWM reverse       |
| 3V3 (OUT)  | nSLEEP     | —              | tie high (enable) |
| GND        | GND        | —              | common ground     |

## Driver outputs → motors

| Driver output     | Motor       | Motor pin |
|-------------------|-------------|-----------|
| #1 AO1, AO2       | Front-left  | M1, M2    |
| #1 BO1, BO2       | Rear-left   | M1, M2    |
| #2 AO1, AO2       | Front-right | M1, M2    |
| #2 BO1, BO2       | Rear-right  | M1, M2    |

## Battery wiring

```
  LiPo +  ──► [5A fuse] ──► [SPST switch] ──┬─► DRV8833 #1 VM
                                             └─► DRV8833 #2 VM
  LiPo −  ──► common ground rail (DRV8833 #1 GND, #2 GND, Pico GND)
```

The fuse goes between the battery + and the switch. The switch goes between the fuse and the drivers. This way if anything shorts upstream, the fuse pops first. Don't skip the fuse — a shorted LiPo can output 100+ amps for a brief, fiery moment.

## Wire-by-wire connection list

One line per wire. Tick each box once the wire is in place and verified with a multimeter beep test.

### Pico 2W → DRV8833 #1 (LEFT side)

- [ ] `Pico GP2` (pin 4) ──► `DRV8833 #1 AIN1` — PWM forward, FL motor
- [ ] `Pico GP3` (pin 5) ──► `DRV8833 #1 AIN2` — PWM reverse, FL motor
- [ ] `Pico GP4` (pin 6) ──► `DRV8833 #1 BIN1` — PWM forward, RL motor
- [ ] `Pico GP5` (pin 7) ──► `DRV8833 #1 BIN2` — PWM reverse, RL motor
- [ ] `Pico 3V3 OUT` (pin 36) ──► `DRV8833 #1 nSLEEP` — tie HIGH to enable driver
- [ ] `Pico GND` (pin 38) ──► `DRV8833 #1 GND` — logic-side ground

### Pico 2W → DRV8833 #2 (RIGHT side)

- [ ] `Pico GP6` (pin 9) ──► `DRV8833 #2 AIN1` — PWM forward, FR motor
- [ ] `Pico GP7` (pin 10) ──► `DRV8833 #2 AIN2` — PWM reverse, FR motor
- [ ] `Pico GP8` (pin 11) ──► `DRV8833 #2 BIN1` — PWM forward, RR motor
- [ ] `Pico GP9` (pin 12) ──► `DRV8833 #2 BIN2` — PWM reverse, RR motor
- [ ] `Pico 3V3 OUT` (pin 36) ──► `DRV8833 #2 nSLEEP` — jumper from `#1 nSLEEP` is fine
- [ ] `Pico GND` (pin 23) ──► `DRV8833 #2 GND` — any Pico GND pin works

### DRV8833 #1 outputs → motors

- [ ] `DRV8833 #1 AO1` ──► `FL motor M1`
- [ ] `DRV8833 #1 AO2` ──► `FL motor M2`
- [ ] `DRV8833 #1 BO1` ──► `RL motor M1`
- [ ] `DRV8833 #1 BO2` ──► `RL motor M2`

### DRV8833 #2 outputs → motors

- [ ] `DRV8833 #2 AO1` ──► `FR motor M1`
- [ ] `DRV8833 #2 AO2` ──► `FR motor M2`
- [ ] `DRV8833 #2 BO1` ──► `RR motor M1`
- [ ] `DRV8833 #2 BO2` ──► `RR motor M2`

### Battery & power switching

- [ ] `LiPo +` (red lead) ──► `5A inline fuse` (input terminal)
- [ ] `5A inline fuse` (output terminal) ──► `SPST switch` (one terminal)
- [ ] `SPST switch` (other terminal) ──► `DRV8833 #1 VM`
- [ ] `DRV8833 #1 VM` ──► `DRV8833 #2 VM` — short jumper between the two drivers
- [ ] `LiPo −` (black lead) ──► `DRV8833 #1 GND` (power-side, same GND pin as logic)
- [ ] `DRV8833 #1 GND` ──► `DRV8833 #2 GND` — short jumper
- [ ] `LiPo −` ──► `Pico GND` (pin 8 or any GND) — closes the common ground loop

### Noise suppression (one cap per motor, soldered at the motor body)

- [ ] `0.1 µF ceramic cap` across `FL motor M1` ↔ `FL motor M2`
- [ ] `0.1 µF ceramic cap` across `RL motor M1` ↔ `RL motor M2`
- [ ] `0.1 µF ceramic cap` across `FR motor M1` ↔ `FR motor M2`
- [ ] `0.1 µF ceramic cap` across `RR motor M1` ↔ `RR motor M2`

## Software task list

### 1. Flash MicroPython onto the Pico 2W
1. Hold BOOTSEL on the Pico while plugging into laptop USB
2. It appears as a USB drive
3. Drag the latest Pico 2W `.uf2` from micropython.org onto it
4. Pico reboots and now shows up as a USB serial device
5. Open Thonny → set interpreter to MicroPython (Raspberry Pi Pico)

### 2. Write a `motor.py` module

A class that takes two GPIO numbers and exposes `set_speed(value)` where value is `-1.0` to `+1.0`. Internally it sets up two PWM channels on those pins and modulates the right one based on sign:

```python
from machine import Pin, PWM

class Motor:
    def __init__(self, pin_a, pin_b, freq=20000):
        self.pwm_a = PWM(Pin(pin_a))
        self.pwm_b = PWM(Pin(pin_b))
        self.pwm_a.freq(freq)
        self.pwm_b.freq(freq)
        self.set_speed(0)

    def set_speed(self, v):
        v = max(-1.0, min(1.0, v))
        duty = int(abs(v) * 65535)
        if v >= 0:
            self.pwm_a.duty_u16(duty)
            self.pwm_b.duty_u16(0)
        else:
            self.pwm_a.duty_u16(0)
            self.pwm_b.duty_u16(duty)
```

20 kHz PWM is above human hearing — your motors won't whine. (Try 1 kHz once to see why this matters.)

### 3. Wire up four `Motor` instances

```python
fl = Motor(2, 3)
rl = Motor(4, 5)
fr = Motor(6, 7)
rr = Motor(8, 9)
```

### 4. Add a simple serial command loop

Read lines from `sys.stdin`, parse simple commands like `f 0.5` (forward at 50%), `r 0.3` (reverse), `l` (turn left), `r` (turn right), `s` (stop). This is throwaway — Phase 3 replaces it with a proper protocol.

## Verification checklist

Do these in order. Each step assumes the previous one passed.

- [ ] **Continuity check (battery disconnected, switch off).** Multimeter beep mode. Verify GND continuity between LiPo−, both DRV8833 GND pins, and Pico GND. Also verify *no* continuity between any GND and any V+ rail.
- [ ] **Power on the Pico from laptop USB only.** No LiPo connected yet. Pico LED should come on. nSLEEP should read 3.3V on both drivers (measure with multimeter).
- [ ] **Bench-test one motor wire pair.** Don't touch the wheels yet. With battery disconnected, run `fl.set_speed(0.5)` from the REPL. The driver's outputs (AO1/AO2) should not have voltage (no motor power yet — this just confirms PWM signals reach the driver inputs).
- [ ] **Connect battery, fuse in, switch off.** Check fuse continuity. Switch on. Measure 7.4V on both DRV8833 VM pins. If it's anything else, switch off immediately.
- [ ] **Test FL motor only.** With chassis on a stand or just held in the air (wheels not on ground): `fl.set_speed(0.3)`. Should spin slowly in one direction. `fl.set_speed(-0.3)` should reverse. Stop.
- [ ] **Test the other three the same way.** One at a time.
- [ ] **Check spin direction.** Once mounted on the chassis, each wheel needs to roll the *bot* forward when commanded forward. If a motor spins the wrong way: swap M1/M2 wires at the driver output (don't flip it in code — fix the wiring).
- [ ] **All four forward at 50% for 2 seconds.** Hold the bot in the air. All four wheels turn the right way.
- [ ] **All four reverse for 2 seconds.** Same check.
- [ ] **Turn-in-place command.** Left side forward, right side reverse. Hold the bot, wheels should be spinning opposite directions on each side.

## Common pitfalls

- **No common ground.** The driver has logic inputs from the Pico (3.3V referenced) and motor outputs running off LiPo (7.4V referenced). If those grounds aren't physically tied together, the driver sees garbage and behaves randomly. *Always* connect Pico GND to driver GND with a wire.
- **PWM frequency too low.** Audible whine, especially around 1–5 kHz. Set frequency to 20 kHz or higher.
- **DRV8833 nSLEEP floating.** If you forget to tie nSLEEP to 3.3V, the outputs stay disabled and the motor does nothing. Easy to miss.
- **Wrong motor direction.** Don't fix this in software — swap M1/M2 wires at the driver output. Future-you will thank you when reading code.
- **Battery on first, then logic.** Wrong order. Always: switch *off*, plug Pico to laptop USB first to bring up logic, then switch battery *on*. Powering up the motor side without logic settled can cause the driver to glitch its outputs.
- **No noise capacitors.** Skipping the 0.1 µF caps across motor terminals usually works on the bench but causes weird crashes later when the Pico USB connection drops mid-drive. Solder them in now.
- **Stalling a motor for too long.** DRV8833 will current-limit and protect itself, but you'll see the motor stutter or stop. Don't drive into a wall and hold throttle.

## What's next
Phase 2 wires up the encoders and adds closed-loop speed control. Once that's working, commanded speed actually corresponds to wheel velocity, which is what every later phase relies on.
