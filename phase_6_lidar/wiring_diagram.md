# Phase 6 — Wiring Diagram (cumulative)

This is the **full system at the end of Phase 6**. Everything from Phases 1–5 is
still here; the only thing new is the **CP2102 → STL-19P lidar** hanging off one
of the Pi's USB-A ports. The lidar touches *nothing* on the GPIO header, the I²C
bus, or the motor rail — it's a self-contained USB-serial peripheral.

Every connection traces to
[device_contracts.md](../_engineering/device_contracts.md) (the contract behind
each interface) and the canonical pin map therein.

---

## Full block schematic (end of Phase 6)

```
                          2S LiPo 7.4V  (8.4V full → 6.0V empty, 10C ≈ 22A, built-in protection board)
                                   │  SM-2P (keyed) → pigtail
                                   ▼
                          [ 10 A MAIN FUSE ]
                                   ▼
                  [ Reverse-polarity + MASTER switch  (Pololu #2815) ]
                                   │  protected 7.4 V bus
        ┌──────────────────────────┼───────────────────────────────┐
        ▼ 7.5A fuse                 ▼ 5A fuse                        ▼ (servo branch)
   [ E-STOP (NC) ]            [ D24V50F5 buck ]                 [ Hobbywing UBEC ]
        ▼                      5 V / 5 A │ USB-C                  5 V / 3 A │ V+
   ┌──────────────┐                     ▼                                 ▼
   │ 2× TB6612FNG │          ┌────────────────────────┐          [ PCA9685 V+ ]
   │   VM = 7.4V  │          │     Raspberry Pi 5      │                 │
   │   STBY=GP14  │          │  usb_max_current_enable │          ┌──────┴──────┐
   │              │          │                         │          ▼             ▼
   │ AO/BO ──► 4× │          │  USB-A ×4   I²C-1   UART0  CSI    Servo CH0    Servo CH1
   │ 310 motors   │          └──┬───┬───┬──┬───┬────┬────┬─┘    (pan)        (tilt)
   │ + encoders   │             │   │   │  │SDA│SCL  │T R  │CSI
   └──────┬───────┘             │   │   │  │(3)│(5)  │(8)(10)
          │ JST-PH 6P           │   │   │  │   │     │      └─► Camera Module 3
          ▼                     │   │   │  └─┬─┴──────────────►  (22→15 FPC #5820)
   ┌──────────────┐            (a) (b) (c)  │
   │  Pico 2W     │◄── USB (a)  │   │   │    ├─► PCA9685 (0x40) ─► pan/tilt servos
   │  RP2350      │             │   │   │    └─► INA219  (0x41) ─► pack V & I monitor
   │  20kHz PWM   │             │   │   │
   │  PIO ×4 enc  │             │   │   │   UART0 (pin 8 TX / pin 10 RX)
   │  per-wheel   │             │   │   │        └─► BNO055 IMU (PS1=HIGH, UART mode)
   │  PID         │             │   │   │
   │  3V3 OUT ──► driver VCC    │   │   │
   │          ──► 4× enc VCC    │   │   │
   └──────────────┘             │   │   │
                                │   │   └─► (optional) Nuwa depth cam (USB)
                                │   │
                                │   └──────────────────────────────────────────┐
                                │                                               │
                                │   ★ NEW THIS PHASE ★                          │
                                ▼ (b) USB-A                                     │
                       ┌───────────────────────┐                               │
                       │  CP2102 USB-UART       │   jumper = 3.3 V  ⚠           │
                       │  adapter               │   (NOT 5 V — verify w/ DMM)   │
                       │                        │                               │
                       │  USB ─ 5V ─ GND ─ RXD ─ TXD(unused)                    │
                       └───┬──────┬─────┬───────┘                               │
                  5V (USB) │  GND │     │ RXD                                   │
                           ▼      ▼     ▲                                       │
                       ┌──────────────────────────────┐                        │
                       │     STL-19P 360° DTOF lidar   │                        │
                       │     JST-ZH 1.5mm 4-pin        │                        │
                       │  Pin4=+5V  Pin3=GND  Pin1=TX  │                        │
                       │  Pin2=PWM ──► GND  (10 Hz spin)│                       │
                       └──────────────────────────────┘                        │
                                                                               │
   ───────────────────────────── COMMON GROUND (star at LiPo −) ───────────────┘
   Separate leg to: each TB6612, the buck, the UBEC, the Pico/Pi. No daisy-chain.
   The CP2102's GND reaches the star through the Pi's USB ground.
```

Legend for the Pi USB-A ports: **(a)** = Pico micro-USB (power + serial), **(b)**
= CP2102 → lidar *(new this phase)*, **(c)** = optional depth cam / spare.

---

## Wire-by-wire — NEW wires this phase

Pick the **CP2102 path** (recommended). The direct-GPIO-UART path is documented
afterward as an advanced alternative — do **one or the other**, not both.

### Path A — CP2102 USB-UART adapter (recommended)

> **Before anything:** set the CP2102 logic jumper to **3.3 V** and confirm with
> a multimeter (VCCIO/3V3 pin ≈ 3.3 V). Contract:
> [device_contracts.md](../_engineering/device_contracts.md) — STL-19P "3.3 V
> TTL, NOT 5 V tolerant."

- [ ] `Pi 5 USB-A port (b)` --> `CP2102 USB-A connector` — enumerates the adapter
      as `/dev/ttyUSB0` and powers it from the 5 V USB bus.
- [ ] `STL-19P Pin1 (TX, 3.3 V UART out, 230400 8N1)` --> `CP2102 RXD` — the
      lidar talks, the Pi listens (one-way link).
- [ ] `STL-19P Pin4 (+5 V, ~290 mA)` --> `CP2102 5V` — lidar power from the
      adapter's USB bus (⚠ **not** the 7.4 V motor rail).
- [ ] `STL-19P Pin3 (GND)` --> `CP2102 GND` — signal + power return; reaches the
      common-ground star through the Pi's USB ground.
- [ ] `STL-19P Pin2 (PWM speed input)` --> `CP2102 GND` (tie low) — selects the
      default closed-loop **~10 Hz** spin. ⚠ Floating = erratic / no spin.
- [ ] `CP2102 TXD` --> *(leave unconnected)* — the lidar never receives.

That's all four lidar pins plus the USB plug. No GPIO, I²C, or motor-rail
changes.

### Path B — Direct Pi GPIO UART (advanced alternative; skip if using Path A)

Only if you have no CP2102. This shares the Pi's primary UART with the Bluetooth
chip, so you must reconfigure the Pi (below). It also consumes the GPIO header's
UART pins.

- [ ] `STL-19P Pin4 (+5 V)` --> a **dedicated, fused 5 V source** (a spare 5 V/GND
      off the D24V50F5 rail, or the lidar's own USB-bus 5 V) — lidar power, ~290 mA.
      ⚠ **Do NOT use Pi header pins 2/4.** [`device_contracts.md`](../_engineering/device_contracts.md)
      reserves the Pi's 5 V header pins as unused, to keep peripheral current and
      transients off the Pi's own rail — that reservation holds even in this
      fallback. (This is another reason Path A is cleaner: the CP2102 powers the
      lidar from its own USB bus and the question never comes up.)
- [ ] `Pi 5 GND (header pin 6)` --> `STL-19P Pin3 (GND)` — ground/return (to the star).
- [ ] `STL-19P Pin1 (TX, 3.3 V)` --> `Pi 5 GPIO15 / RXD (header pin 10)` — lidar
      talks, Pi listens. (⚠ This collides with the **BNO055**, which already owns
      UART0 on pins 8/10 in this build — you'd need a *second* UART, e.g. a
      `uart` dtoverlay on other pins, or a USB adapter. This is precisely why
      **Path A is recommended.**)
- [ ] `STL-19P Pin2 (PWM)` --> `Pi 5 GND` (tie low) — default 10 Hz spin.
      ⚠ Floating = no/erratic spin. (Do **not** route to 7.4 V.)

Pi config for Path B (`/boot/firmware/config.txt`):

```
enable_uart=1
dtoverlay=disable-bt        # frees the PL011 UART from the Bluetooth chip
```

```bash
sudo systemctl disable serial-getty@ttyAMA0.service   # free the serial console
sudo reboot
```

> ⚠ In this build UART0 (pins 8/10) is **already taken by the BNO055 IMU**
> (Phase 4). Sharing it with the lidar is not possible — that's the strongest
> reason to use the CP2102, which gives the lidar its own independent
> `/dev/ttyUSB0`.

---

## Pin table — STL-19P 4-pin connector (the only new interface)

| Lidar pin | Signal | V / level | → Endpoint (Path A) | Contract note |
|---|---|---|---|---|
| Pin 1 | TX (UART out) | 3.3 V TTL, 230400 8N1, one-way | CP2102 **RXD** | NOT 5 V tolerant — adapter jumper must be 3.3 V |
| Pin 2 | PWM (speed in) | logic | CP2102 **GND** (tie low) | GND = default closed-loop 10 Hz; floating = erratic |
| Pin 3 | GND | 0 V | CP2102 **GND** | returns to star via Pi USB ground |
| Pin 4 | +5 V | 5 V, ~290 mA | CP2102 **5V** (USB bus) | from adapter USB, never the motor rail |

---

## ⚠️ Footguns that apply to THIS phase's wires

- **CP2102 jumper on 5 V.** The lidar TX is **3.3 V and not 5 V tolerant**
  (device contract). Set the adapter to **3.3 V** and verify with a multimeter
  *before* connecting. This is the same 3.3 V-world rule that keeps encoder VCC
  at 3.3 V.
- **PWM pin (Pin2) floating.** Must be tied to **GND** for the default 10 Hz
  closed-loop spin. Floating → erratic or no rotation. (Mirror of the original's
  "lidar PWM pin to GND" footgun.)
- **TX→TX swap.** Lidar **Pin1 (TX) → adapter RXD**. The adapter's TXD is unused
  (one-way protocol). Wire TX→TX and the port stays silent though the rotor spins.
- **+5 V from the wrong rail.** Pin4 takes 5 V from the **adapter USB**, not the
  7.4 V motor bus. 7.4 V here destroys the lidar.
- **Path B UART collision.** UART0 (pins 8/10) is already the **BNO055** (on UART
  *not* I²C — itself a footgun preserved from Phase 4). Don't double-book it; use
  the CP2102.

### Footguns inherited from earlier phases (still live — do not regress)

- **TB6612 STBY must be HIGH** (GP14 → 3.3 V) or the motors stay disabled.
- **Encoder VCC = 3.3 V, never 7.4 V** — keeps A/B outputs inside the Pico's
  GPIO limit.
- **PCA9685 VCC ≠ V+** — VCC is 3.3 V logic (Pi); V+ is 5 V servo power (UBEC).
  Don't cross them.
- **BNO055 is on UART, not I²C** — dodges the Pi's I²C clock-stretch bug.
- **Star ground only** — one leg per converter/driver/brain to the LiPo− node;
  no daisy-chaining. The CP2102's ground arrives via the Pi's USB ground.
