# Phase 5 — Camera + Pan/Tilt

## Goal
By the end of this phase you can open the robot's web UI on your phone, see **live video** streaming from a camera bolted to the top of the bot, and **point that camera anywhere** with two on-screen sliders — while you're *also* driving. Streaming and driving have to coexist: a video feed that freezes the instant you push the joystick is useless for teleoperation. The camera rides on a two-servo pan/tilt gimbal driven by a **PCA9685** PWM chip over I²C, and — this is the headline electrical lesson of the phase — those servos get their **own regulated 5 V rail from a UBEC**, never the Pi.

When this works you have a low-budget FPV (first-person-view) robot: drive it into the next room, pan the camera around, and look at the world through the bot's eyes.

## Difficulty / Time
**Medium · 1 weekend.** The wiring is light (one ribbon cable, one I²C breakout, two servos, one small power converter). The time goes into picamera2 streaming, getting the MJPEG feed to survive simultaneous driving, and tuning the servo limits so the gimbal never binds.

## Prerequisites
- **Phase 4 complete.** The BNO055 IMU is up and reporting heading over **UART** (not I²C — see below), the Pi tracks pose `(x, y, θ)`, and the web UI shows a compass.
- **Phase 3 web stack running.** Flask server on the Pi, joystick teleop, telemetry at 20 Hz, the Pi↔Pico USB "spinal cord."
- The Pi is powered from the **D24V50F5 5 V/5 A buck** off the LiPo (Phase 3), *not* a phone power bank. This matters here: the camera adds ~0.25–0.4 A to the Pi's 5 V rail and you want headroom.
- I²C-1 is enabled on the Pi (`sudo raspi-config nonint do_i2c 0`) and currently hosts the **INA219 at 0x41** (Phase 4). This phase adds the **PCA9685 at 0x40** to the same bus.

> **Why I²C is free for the camera servos now.** In the *original* plan the IMU sat on I²C-1 at 0x28 and the servo driver had to share with it. In this build the BNO055 was moved to **UART** specifically because the Pi's hardware I²C mishandles the BNO055's clock stretching (you get `OSError`s and, worse, *silently corrupted* heading bytes — see `_engineering/engineering_decisions.md`). A nice side effect: the I²C-1 bus is now clean for the two devices that behave well on it — the INA219 (0x41) and this phase's PCA9685 (0x40).

## New parts this phase
- **Raspberry Pi Camera Module 3** — CSI camera, light enough to gimbal; streams MJPEG via picamera2.
- **Pi 5 CSI cable, 22-pin → 15-pin (Adafruit #5820, 300 mm)** — the *only* cable that fits a Pi 5 to a 15-pin camera. A generic "longer 15-to-15 CSI cable" physically will not mate to a Pi 5.
- **PCA9685 16-channel PWM driver (Adafruit #815 or generic)** — hardware servo PWM over I²C at address **0x40**. Keeps jittery servo timing off both the Pi's OS and the Pico's real-time loop.
- **2× Hiwonder LFD-01 servos** (reuse from kit; fallback **MG90S**, never SG90) — the pan and tilt actuators on the gimbal.
- **Hobbywing UBEC-3A (2–6S → 5 V/3 A)** — a dedicated 5 V regulator that feeds *only* the servos' V+ rail.
- **470–1000 µF bulk electrolytic cap** (≥16 V, low-ESR) — sits across the servo V+ rail to swallow the 3–4 A servo inrush spike.
- **2-DOF pan/tilt bracket** (reuse from kit), cable-tie mounts + VHB foam tape for mounting.
- **Optional:** the kit **Nuwa-HP60C** depth camera, mounted body-fixed (USB), started as a plain RGB UVC stream.

## Cumulative system state (in prose)

Here's the whole robot as it stands the moment Phase 5 is done.

A single **2S LiPo (7.4 V — the URGENEX 1800 mAh 35C pack swapped in at Phase 3, ≈ 63 A capable, no PCB)** is the only energy source. Its positive lead runs through its **Deans T-plug pigtail** into a **10 A main fuse**, then a **reverse-polarity + master switch** (Pololu #2815), onto a protected 7.4 V bus. From that bus, **three branches** fan out, each separately fused:

1. **Motor branch (7.5 A fuse → NC e-stop):** feeds the two **TB6612FNG** dual motor drivers' **VM** inputs at 7.4 V. Each board drives two of the four 310 metal-gear Mecanum motors — *one motor per channel*, which is exactly what Mecanum needs. The **Pico 2W** generates 20 kHz PWM on three logic pins per channel (IN1/IN2 set direction, PWM sets speed), holds **STBY high** to enable the drivers, and reads all four quadrature encoders in hardware using its **PIO** state machines. The Pico runs a per-wheel PID loop and a command-timeout watchdog. Its logic rail (3V3 OUT) powers both drivers' VCC and all four encoder VCCs at **3.3 V** (never 7.4 V — that would push the encoder A/B outputs past the Pico's 3.3 V GPIO limit).

2. **Pi branch (5 A fuse → D24V50F5 buck):** the buck turns 7.4 V into a clean **5 V / 5 A**, fed to the Pi's USB-C. The Pi 5 is the high-level brain: WiFi, the Flask web UI, pose tracking, and — new this phase — the camera. `usb_max_current_enable=1` is set so the Pi trusts the rail and doesn't cap its USB ports to 600 mA. Off the Pi's USB-A ports hang the **Pico** (power + serial, `/dev/ttyACM0`) and, from Phase 6 onward, the lidar's CP2102 adapter.

3. **Servo branch (NEW this phase → UBEC-3A):** the UBEC makes its own **5 V / 3 A** and feeds *only* the **PCA9685's V+** terminal (servo power), with a **470–1000 µF bulk cap** across it. The PCA9685's **VCC (logic)** is a *separate* pin powered at **3.3 V from the Pi**, and its **SDA/SCL** join I²C-1. The two LFD-01 servos plug into channels **CH0 (pan)** and **CH1 (tilt)**.

On the sensing side, the **BNO055 IMU** talks to the Pi over **UART** (PS1 = HIGH) at 115200 baud, giving drift-free heading. The **INA219** current/voltage monitor sits on I²C-1 at **0x41**, letting the Pi watch pack voltage and current. New this phase: the **Camera Module 3** connects to the Pi's **22-pin CSI** port via the **#5820 adapter cable** and rides the pan/tilt gimbal.

Every ground returns to a **single-point star at the LiPo negative terminal** — separate legs from each converter, driver, the Pico, and the Pi. No daisy-chaining: motor return current (amps, switching at 20 kHz) must never share a wire with the logic ground reference.

## Pin tables (from the canonical map)

### Pi 5 — I²C-1 and power (40-pin header) — *PCA9685 added this phase*

| Function | Pin(s) | Device |
|---|---|---|
| I²C-1 SDA (GPIO2) | 3 | **PCA9685 (0x40)** + INA219 (0x41) |
| I²C-1 SCL (GPIO3) | 5 | **PCA9685 (0x40)** + INA219 (0x41) |
| 3.3 V | 1, 17 | **PCA9685 VCC**, BNO055 VIN, INA219 VCC |
| GND | 6, 9, 14, 20, 25, 30, 34, 39 | common ground (star) |
| UART0 TXD (GPIO14) | 8 | → BNO055 RX (Phase 4) |
| UART0 RXD (GPIO15) | 10 | ← BNO055 TX (Phase 4) |
| CSI (22-pin 0.5 mm) | — | **Camera Module 3** via #5820 cable |
| USB-A ×4 | — | Pico; (Phase 6) CP2102→lidar; (opt) depth cam |
| USB-C | — | ← D24V50F5 5 V/5 A buck |

> ⚠️ **The PCA9685's V+ and the servos do NOT touch the Pi header.** V+ comes from the UBEC. The Pi's 5 V header pins (2, 4) stay **unused** in this build — that's deliberate, to keep servo current transients off the Pi rail.

### PCA9685 — pinout and what each pin connects to

| PCA9685 pin | Connects to | Rail / level | Note |
|---|---|---|---|
| VCC | Pi 3.3 V (pin 1 or 17) | 3.3 V logic | references the Pi's I²C pull-ups; **≠ V+** |
| GND | common-ground star | 0 V | shared logic + servo return |
| SDA | Pi GPIO2 (pin 3) | 3.3 V I²C | shared with INA219 |
| SCL | Pi GPIO3 (pin 5) | 3.3 V I²C | shared with INA219 |
| V+ (screw terminal) | **UBEC 5 V out** | 5 V servo power | **never the Pi**; bulk cap across it |
| CH0 (S / V+ / GND) | Pan servo (LFD-01) | 50 Hz PWM, 5 V | signal pin 3.3/5 V tolerant |
| CH1 (S / V+ / GND) | Tilt servo (LFD-01) | 50 Hz PWM, 5 V | |

### Servo channel assignment

| PCA9685 channel | Servo | Motion |
|---|---|---|
| CH0 | Pan (LFD-01) | rotate camera left/right (azimuth) |
| CH1 | Tilt (LFD-01) | rotate camera up/down (elevation) |

## Concepts you'll actually use (from the ground up)

**CSI vs USB cameras.** There are two ways to attach a camera to a Pi. A **USB camera** is a UVC device — it plugs into a USB-A port, shows up as `/dev/video*`, and steals USB bandwidth and a slice of the Pi's USB power budget. A **CSI camera** (Camera Serial Interface) connects over a dedicated **MIPI ribbon** straight into the Pi's image pipeline — it doesn't touch the USB bus at all, gets handled by the Pi's hardware ISP, and is supported by **picamera2**. We use CSI for the streaming camera precisely because the USB bus is busy: the Pico's serial link lives there, and Phase 6 adds the lidar's USB adapter. Keeping the camera off USB means the camera and the lidar don't fight for bandwidth or power. (The *optional* depth camera is USB-only, which is one reason it's optional — it costs a USB port and ~0.4 A of the budget.)

> ⚠️ **The connector is part of the contract.** The Pi 4 used a 15-pin 1 mm CSI connector. The **Pi 5 moved to a 22-pin 0.5 mm** connector. The Camera Module 3 is still 15-pin. So you need a **22-to-15 adapter cable (Adafruit #5820)** — the pin *count and pitch* are the spec, not "a longer cable." A generic 15-to-15 ribbon won't even physically seat in a Pi 5. This is the exact "stall mid-build" trap the original parts list fell into.

**MJPEG streaming over WiFi.** Video is just a sequence of still images. The simplest way to stream it to a browser is **MJPEG (Motion JPEG)**: encode each frame as an ordinary JPEG and send them back-to-back over one long-lived HTTP response, separated by a boundary marker. An `<img>` tag pointed at that endpoint just keeps swapping in the newest frame. There's no fancy codec, no client-side decoder, and no plugin — it works in every browser. The cost is **bandwidth**: MJPEG doesn't compress *between* frames the way H.264 does, so a 640×480 stream is the sweet spot for WiFi. Higher resolution means more JPEG-encoding CPU on the Pi and more bytes over the air — the two things that make the feed stutter when you also drive.

**The I²C-to-PWM abstraction (what the PCA9685 buys you).** A servo is told what to do by a **pulse-width signal** (next paragraph). Generating a stable, exact-width pulse needs a timer that *never* gets interrupted. The Pi runs Linux, which preempts threads constantly — software PWM from a GPIO jitters, and the camera visibly shakes. The Pico could do it, but we don't want a servo glitch perturbing the real-time PID loop. The **PCA9685** is a dedicated 16-channel, 12-bit hardware PWM chip with its own oscillator. The Pi just sends it I²C messages — "channel 0, this duty cycle" — and the chip holds rock-steady pulses on its own. That's the abstraction: you trade a bit-banged timing problem for a clean register write over a bus. As a bonus you get 14 spare channels for later (lights, more servos).

**The 50 Hz / 500–2500 µs servo signal.** A hobby servo expects a pulse **every 20 ms (50 Hz)**. The *width* of that pulse, not its frequency, encodes the commanded angle: roughly **500 µs → one end, 1500 µs → center, 2500 µs → the other end**, covering ~0–180°. (Older specs say 1000–2000 µs; modern servos like the LFD-01 use the wider 500–2500 µs range — you'll find your servo's real limits by sweeping.) So "set the servo to 90°" becomes "make the PCA9685 emit a 1500 µs-wide pulse at 50 Hz on this channel." The Adafruit ServoKit library hides the µs↔angle math, but knowing the real signal is what lets you debug a servo that won't reach its endpoints (your pulse range is too narrow) or one that buzzes at the end (you're driving it into a mechanical stop).

**The rule: high-current actuators get their own regulated rail.** This is the single most important electrical idea in the phase, and it's *why the UBEC exists.* Two LFD-01 servos draw ≤60 mA each while idle but **~700 mA each at stall — ~1.5 A combined — plus a 3–4 A inrush spike** when they suddenly slew (see `_engineering/power_budget.md`). If you took that current from the Pi's 5 V header (as the *original* Phase 5 did — a real bug), the rail would sag below the Pi's **4.63 V undervoltage line** and the Pi would brown out and **reboot mid-SLAM**. The fix: give the servos a **dedicated UBEC-3A** that makes its own 5 V straight from the LiPo, with a **bulk cap** to absorb the inrush. The servo current never flows anywhere near the Pi or the logic rail. The physics term for the problem you're avoiding is **shared-impedance coupling**: any wire has resistance, so a big current pulse through a shared wire creates a voltage dip that *everything* on that wire sees. Separate rails = separate wires = the dip stays where it belongs.

## Software task list

Build on the Phase 3 Flask app — you're *adding* a camera stream and a `/cam` endpoint, not starting over.

### 0. One-time setup

```bash
# CSI camera stack (Bookworm)
sudo apt install -y python3-picamera2

# PCA9685 / servo driver libs (in your project venv)
pip install adafruit-circuitpython-pca9685 adafruit-circuitpython-servokit

# Confirm the camera is detected by the firmware
rpicam-hello -t 5000     # (older OS: libcamera-hello -t 5000) — shows a 5 s preview
```

### 1. Bring up the PCA9685 + servos

```python
from adafruit_servokit import ServoKit

# 16-channel board at the default I2C address 0x40 on bus 1
kit = ServoKit(channels=16)              # add address=0x40 explicitly if you like

# LFD-01 expects a ~500–2500 us pulse range. Tell the library the real range
# so 0..180 deg maps to the servo's true mechanical span:
kit.servo[0].set_pulse_width_range(500, 2500)   # pan  (CH0)
kit.servo[1].set_pulse_width_range(500, 2500)   # tilt (CH1)

kit.servo[0].angle = 90   # pan center
kit.servo[1].angle = 90   # tilt center
```

> Find the **real** safe limits by sweeping slowly and listening: where the servo starts to **buzz**, it's pushing against the bracket's mechanical stop. Set your software `PAN_MIN/PAN_MAX/TILT_MIN/TILT_MAX` ~5° *inside* those points so the servo never stalls against the frame (a stalled servo draws its full 700 mA and overheats).

### 2. MJPEG streaming with picamera2

```python
import io, threading
from picamera2 import Picamera2
from picamera2.encoders import MJPEGEncoder
from picamera2.outputs import FileOutput

class StreamingOutput(io.BufferedIOBase):
    def __init__(self):
        self.frame = None
        self.condition = threading.Condition()
    def write(self, buf):                       # called by the encoder per frame
        with self.condition:
            self.frame = buf
            self.condition.notify_all()          # wake any waiting HTTP clients

picam = Picamera2()
config = picam.create_video_configuration(main={"size": (640, 480)})
picam.configure(config)
output = StreamingOutput()
picam.start_recording(MJPEGEncoder(), FileOutput(output))
```

### 3. Flask endpoints (add to the Phase 3 server)

```python
from flask import Response, request

@app.route('/stream.mjpg')
def stream():
    def gen():
        while True:
            with output.condition:
                output.condition.wait()         # block until a NEW frame exists
                frame = output.frame
            yield (b'--FRAME\r\n'
                   b'Content-Type: image/jpeg\r\n'
                   b'Content-Length: ' + str(len(frame)).encode() + b'\r\n\r\n'
                   + frame + b'\r\n')
    return Response(gen(),
                    mimetype='multipart/x-mixed-replace; boundary=FRAME')

PAN_MIN, PAN_MAX   = 15, 165      # set from your sweep test
TILT_MIN, TILT_MAX = 30, 150

def clamp(v, lo, hi): return max(lo, min(hi, v))

@app.route('/cam', methods=['POST'])
def cam():
    d = request.json
    if 'pan'  in d: kit.servo[0].angle = clamp(float(d['pan']),  PAN_MIN,  PAN_MAX)
    if 'tilt' in d: kit.servo[1].angle = clamp(float(d['tilt']), TILT_MIN, TILT_MAX)
    return 'ok'

# Make sure the server is reachable from your phone:
# app.run(host='0.0.0.0', port=5000)
```

### 4. Browser: video + sliders + center button

```html
<img id="cam" src="/stream.mjpg" width="640" height="480">
<input id="pan"  type="range" min="0" max="180" value="90">
<input id="tilt" type="range" min="0" max="180" value="90">
<button id="center">Center</button>
<script>
const send = (o) => fetch('/cam', {
  method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(o)});
// throttle to ~20 Hz so you don't flood the servo bus while dragging
let last = 0;
function onSlide(){
  const now = Date.now();
  if (now - last < 50) return; last = now;
  send({pan:+pan.value, tilt:+tilt.value});
}
pan.oninput = tilt.oninput = onSlide;
center.onclick = () => { pan.value=90; tilt.value=90; send({pan:90, tilt:90}); };
</script>
```

### 5. (Optional) Depth camera RGB stream
If your kit has the **Nuwa-HP60C**, mount it **body-fixed forward** (it's too heavy/long to gimbal), plug it into a Pi USB-A port, and start with just its **plain RGB UVC stream** (`/dev/video*` via OpenCV or as a second MJPEG endpoint). Depth comes later — get the easy RGB win first. Remember it counts ~0.4 A against the 5 V budget.

## Verification checklist
Each step assumes the previous one passed.

- [ ] **Continuity ritual first.** Battery disconnected, switch off. Confirm **PCA9685 VCC (3.3 V) and V+ (5 V) are NOT bridged** (no continuity between them). Confirm the UBEC's 5 V out goes only to V+, never to a Pi 5 V pin.
- [ ] **UBEC output is 5.0 V** measured at the PCA9685 V+ terminal *before* any servo is plugged in.
- [ ] **`rpicam-hello -t 5000` shows a live preview.** (Camera + #5820 cable seated correctly.)
- [ ] **`i2cdetect -y 1` shows BOTH `40` and `41`.** 0x40 = PCA9685, 0x41 = INA219. (No 0x28 — the IMU is on UART now.)
- [ ] **Servos center.** `kit.servo[0].angle = 90` and `[1] = 90` — gimbal sits level and forward.
- [ ] **Slow sweep, no buzz.** Sweep pan 0→180 over ~3 s, then tilt. Note where it buzzes (mechanical stops); set `PAN_MIN/MAX`, `TILT_MIN/MAX` ~5° inside those points.
- [ ] **Pi rail does NOT sag when servos slew.** While slewing both servos end to end, run `vcgencmd get_throttled` — it must stay **`0x0`**. (This is the Measurement Lab — proves the UBEC isolation works.)
- [ ] **MJPEG stream loads on your phone.** `http://<pi-ip>:5000/stream.mjpg` shows live video.
- [ ] **Sliders move the camera from the phone**, smoothly, with no noticeable lag.
- [ ] **Center button** snaps both servos to 90°.
- [ ] **Stream + drive simultaneously.** Drive with the joystick while watching the feed. Neither stutters; the bot stays responsive.
- [ ] **End-to-end FPV.** Drive into another room watching only the camera, panning to look around. Feels like (low-budget) FPV.

## Common pitfalls

- **⚠️ Cross-wiring VCC and V+.** These are *separate* rails on the PCA9685. **VCC = 3.3 V logic from the Pi; V+ = 5 V servo power from the UBEC.** Bridging the 5 V servo rail onto the 3.3 V logic pin kills the board (and can backfeed the Pi). Beep-test them apart before power-on.
- **⚠️ Servos on the Pi's 5 V header.** The single biggest electrical mistake here, and the one the original plan made. Two servos' ~1.5 A stall + 3–4 A inrush will sag the Pi rail and reboot it mid-drive. Servo V+ comes from the **UBEC**, full stop.
- **CSI ribbon inserted backwards or loose.** Camera doesn't enumerate (`rpicam-hello` errors). The metal contacts must face the correct way and both latches (camera end and Pi end) must be fully closed. With the #5820 cable, note that the two ends have different pin counts — don't force the wrong end into the wrong connector.
- **Used a generic 15-to-15 "longer CSI cable."** It will not seat in a Pi 5's 22-pin port. You need the **#5820 22-to-15** adapter.
- **Flask reachable on the Pi but not the phone.** The server is bound to `127.0.0.1`. Bind to `0.0.0.0` and check no firewall blocks port 5000 (`sudo ufw allow 5000`).
- **High CPU / stuttering stream.** MJPEG encoding eats a CPU core. Stay at 640×480, ~15–20 fps. Going to 1280×720 doubles the JPEG work and the WiFi bytes — that's usually what kills the "stream + drive" test.
- **Servos jitter / chatter.** Either you're commanding past a mechanical stop (tighten software limits) or the bulk cap on V+ is missing (add the 470–1000 µF). Persistent chatter under load with SG90s means the bracket is too heavy — use **MG90S** (metal gear), never SG90.
- **PCA9685 at an unexpected address.** Some boards have A0/A1/A2 solder jumpers; if any are bridged the address shifts off 0x40 — and could even collide with the INA219 if it lands on 0x41. Re-check `i2cdetect`.
- **No bulk cap on V+.** Without the 470–1000 µF cap the inrush spike on a fast slew dips the UBEC rail and the servo twitches. The cap is cheap insurance against di/dt.

## Measurement Lab

**Quantity:** servo stall current, and the Pi rail's immunity to it.

1. **Stall current of one servo (~700 mA).** Put a multimeter in **DC current (10 A) mode** in series with one servo's V+ lead (or clamp a current probe on it). Command it to drive gently against its mechanical stop and read the current — you should see roughly **700 mA** for a single LFD-01, matching the contract in `_engineering/device_contracts.md`. Now do both at once and confirm the combined draw lands near the budgeted **~1.5 A**. This is *why* the UBEC is a 3 A part and not a 1 A part.

2. **`i2cdetect -y 1` shows BOTH 0x40 and 0x41.** Two devices, two distinct addresses, one bus — the proof that moving the IMU to UART left I²C-1 clean for the PCA9685 + INA219.

3. **The Pi rail does NOT sag (`vcgencmd get_throttled` stays `0x0`).** With the UBEC feeding the servos, slew both end-to-end repeatedly and keep `watch -n1 vcgencmd get_throttled` open. It must stay `0x0`. If you (don't!) instead powered the servos from the Pi's 5 V pin, you'd watch this flip to a non-zero undervoltage code the moment they slew — *that* contrast is the whole lesson: **the UBEC isolation works.**

*(Optional, with the logic analyzer:)* probe a servo signal line and confirm the PCA9685 is emitting a **50 Hz** waveform whose **pulse width** moves between ~500 µs and ~2500 µs as you drag the slider.

## Phase wrap-up

**What you learned.** You now know the difference between CSI and USB cameras and *why* CSI keeps the camera off a busy USB bus; how MJPEG turns a video stream into plain back-to-back JPEGs that any browser renders; the I²C-to-PWM abstraction that lets a Linux box command jitter-free servos; the anatomy of the 50 Hz / 500–2500 µs servo signal; and — the big one — the rule that **high-current actuators get their own regulated rail**, plus the shared-impedance-coupling reason behind it. You also proved isolation empirically with `vcgencmd get_throttled`.

**Résumé bullet.** *"Added a CSI camera on a PCA9685-driven pan/tilt gimbal to a Raspberry Pi 5 robot, streaming MJPEG video over WiFi to a Flask web UI with browser pan/tilt control; isolated the servos on a dedicated 5 V UBEC rail (with bulk decoupling) so the Pi never browned out during simultaneous driving and streaming."*

**Interview talking point.** *"My first instinct was to power the camera servos from the Pi's 5 V header — it's one pin away. But two servos pull ~1.5 A at stall with a 3–4 A inrush, and the Pi browns out below 4.63 V, so it'd reboot mid-drive. I gave the servos their own UBEC and a bulk cap, kept only the logic pin (3.3 V VCC) and I²C on the Pi, and proved the isolation with `vcgencmd get_throttled` staying 0x0 while the servos slewed. That's the general rule I now reach for: noisy, high-current actuators get a dedicated rail; logic stays clean — and the reason is shared-impedance coupling on the supply wire."*

## What's next
**Phase 6** wires up the **STL-19P 360° lidar** via the CP2102 USB-UART adapter (3.3 V, 230400 baud, PWM pin to GND), putting a live 360° scan in the web UI. Phases 7 and 8 then layer SLAM and autonomous navigation on top of the odometry, IMU, and now the camera you've built.
