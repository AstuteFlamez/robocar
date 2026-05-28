# Phase 5 — Camera + pan/tilt

## Goal
Pi Camera v3 mounted on a pan/tilt bracket on top of the bot. Live video stream in the web UI. Two on-screen sliders control pan and tilt; the camera looks where you point it. The PCA9685 drives both servos cleanly over I2C, sharing the bus with the IMU.

## Difficulty
Medium · 1 weekend

## Prerequisites
- Phase 4 complete (IMU working over I2C bus 1)

## New parts used this phase
- Pi Camera v3 (or v2 if v3 isn't available)
- Longer CSI ribbon cable (30–50 cm)
- 2× SG90 micro servos
- Pan/tilt bracket
- PCA9685 16-channel PWM/servo driver

## Why a PCA9685 instead of driving servos directly from GPIO
You *can* drive an SG90 from a Pi GPIO with software PWM, but it's jittery — the OS preempts the PWM thread, the servo twitches, and the camera shakes. The PCA9685 is a dedicated 12-bit hardware PWM chip that produces clean signals regardless of what Linux is doing. It also gives you 14 spare channels for later (lights, more servos, whatever). Worth the $6.

It also has its own 5V power input for the servos, so the servo current spike when they accelerate doesn't crash the Pi 5.

## Schematic — current state of the build

```
                ┌──────────────────┐
                │  USB power bank  │  5V / 3A+
                └────────┬─────────┘
                         │ USB-C
                         ▼
       ┌────────────────────────────────────┐
       │           Raspberry Pi 5           │
       │                                    │
       │  CSI       USB         I2C-1       │
       └────┬─────────┬─────────────┬───────┘
            │         │             │
   CSI ribbon │       │             │ SDA = GPIO2
   cable      │       │             │ SCL = GPIO3
            │       ▼               │ 3V3, GND
            ▼   ┌──────────┐        │
    ┌──────────────────┐  │Pico 2W │  ├──► BNO055 IMU (0x28)
    │  Pi Camera v3    │  │unchanged   │
    │  on pan/tilt     │  └──────────┘ └──► PCA9685    (0x40)
    │  bracket         │                       │
    └──────────────────┘                       ├─► Servo CH0 (pan, SG90)
            ▲                                   └─► Servo CH1 (tilt, SG90)
            │
       servo horns
       move the camera

   (motor/encoder/battery wiring unchanged from earlier phases)
```

## New pin assignments — PCA9685

### Pi 5 → PCA9685

| Pi 5 pin   | PCA9685 pin | Function                |
|------------|-------------|-------------------------|
| 3V3        | VCC         | logic supply            |
| GND        | GND         | ground                  |
| GPIO2 SDA  | SDA         | shared I2C bus with IMU |
| GPIO3 SCL  | SCL         | shared I2C bus with IMU |
| 5V (header pin 2 or 4) | V+ | servo supply (separate from logic) |

V+ is the servo power and **must be separate from VCC**. Two SG90s can pull 500–1000 mA briefly during fast moves; you don't want that current going through the Pi's logic 3.3V rail.

### PCA9685 → servos

| PCA9685 channel | Servo               |
|-----------------|---------------------|
| CH0 (signal)    | Pan servo (rotates camera left/right) |
| CH1 (signal)    | Tilt servo (rotates camera up/down)   |

Each servo connects with a standard 3-pin cable: signal, V+, GND. The PCA9685 has three pins per channel — they're all on the board.

## Wire-by-wire connection list — new wires this phase

### Pi 5 → PCA9685 (logic side — joins the I2C bus with the IMU)

- [ ] `Pi 5 3V3` (header pin 17) ──► `PCA9685 VCC` — logic supply
- [ ] `Pi 5 GND` (header pin 9) ──► `PCA9685 GND` — ground
- [ ] `Pi 5 GPIO2 / SDA1` (header pin 3) ──► `PCA9685 SDA` — already wired to BNO055; daisy-chain via breadboard rail
- [ ] `Pi 5 GPIO3 / SCL1` (header pin 5) ──► `PCA9685 SCL` — already wired to BNO055; daisy-chain via breadboard rail

### Pi 5 → PCA9685 (servo power — must be a separate run, not from 3V3)

- [ ] `Pi 5 5V` (header pin 2) ──► `PCA9685 V+` — supplies servo power, isolated from logic rail
- [ ] `Pi 5 GND` (header pin 14) ──► `PCA9685 GND` — servo-side ground (same pad as logic GND on the breakout)

### PCA9685 → servos

- [ ] `PCA9685 CH0 signal` ──► `Pan servo signal` (orange/yellow wire) — rotates camera left/right
- [ ] `PCA9685 CH0 V+` ──► `Pan servo V+` (red wire)
- [ ] `PCA9685 CH0 GND` ──► `Pan servo GND` (brown/black wire)
- [ ] `PCA9685 CH1 signal` ──► `Tilt servo signal` (orange/yellow wire) — rotates camera up/down
- [ ] `PCA9685 CH1 V+` ──► `Tilt servo V+` (red wire)
- [ ] `PCA9685 CH1 GND` ──► `Tilt servo GND` (brown/black wire)

Each servo connector lands on a 3-pin column on the PCA9685 board — the V+ and GND for each channel come from the V+ and GND rails on the board, so you only need to plug the cable on the correct channel.

### Pi 5 ↔ Pi Camera v3 (one ribbon cable)

- [ ] `Pi 5 CSI port` ──► `Pi Camera v3 connector` — single ribbon, metal contacts face toward the USB-C side of the Pi 5

## Pi Camera v3 install

1. Power off the Pi
2. Lift the CSI connector latch
3. Insert the ribbon with the metal contacts facing the right way (toward the HDMI ports on Pi 5)
4. Close the latch
5. Power on, enable camera in `sudo raspi-config` (Interface Options → Camera, if not already enabled)

Test:
```bash
libcamera-hello -t 5000
```
If it shows a preview, the camera works.

## Software task list

### 1. Install libraries

```bash
sudo apt install python3-picamera2
pip install adafruit-circuitpython-pca9685 adafruit-circuitpython-servokit flask-sock
```

### 2. PCA9685 + servos

```python
from adafruit_servokit import ServoKit
kit = ServoKit(channels=16)

# SG90 typical range: 0–180°
kit.servo[0].angle = 90   # pan to center
kit.servo[1].angle = 90   # tilt to center
```

SG90 servos vary — some only reliably go 0–170°. Find the actual limits by sweeping slowly and noting where it starts to buzz (= hitting the mechanical stop). Set software limits 5° in from there.

### 3. Video streaming with Picamera2

Picamera2 has built-in MJPEG streaming examples. Skeleton:

```python
from picamera2 import Picamera2
from picamera2.encoders import MJPEGEncoder
from picamera2.outputs import FileOutput
import io

class StreamingOutput(io.BufferedIOBase):
    def __init__(self):
        self.frame = None
        self.condition = threading.Condition()
    def write(self, buf):
        with self.condition:
            self.frame = buf
            self.condition.notify_all()

picam = Picamera2()
config = picam.create_video_configuration(main={"size": (640, 480)})
picam.configure(config)

output = StreamingOutput()
picam.start_recording(MJPEGEncoder(), FileOutput(output))
```

Then add a Flask endpoint:
```python
@app.route('/stream.mjpg')
def stream():
    def gen():
        while True:
            with output.condition:
                output.condition.wait()
                frame = output.frame
            yield (b'--FRAME\r\nContent-Type: image/jpeg\r\n'
                   b'Content-Length: ' + str(len(frame)).encode() + b'\r\n\r\n' + frame + b'\r\n')
    return Response(gen(), mimetype='multipart/x-mixed-replace; boundary=FRAME')
```

In the HTML:
```html
<img src="/stream.mjpg" width="640" height="480">
```

Resolution: 640×480 at 15–20 fps is the right balance. Higher resolution = bandwidth and CPU pain over WiFi.

### 4. Pan/tilt sliders

Two `<input type="range" min="0" max="180" value="90">` sliders. On change, POST to `/cam`:
```python
@app.route('/cam', methods=['POST'])
def cam():
    data = request.json
    kit.servo[0].angle = clamp(data['pan'], PAN_MIN, PAN_MAX)
    kit.servo[1].angle = clamp(data['tilt'], TILT_MIN, TILT_MAX)
    return 'ok'
```

Add a "center" button that sets both to 90°.

### 5. (Optional) Headlight

Wire an LED to one of the spare PCA9685 channels with a current-limiting resistor. Brightness control for free. The PCA9685's outputs are open-drain through a transistor — you'll need an external transistor for anything more than a tiny LED.

## Verification checklist

- [ ] **`libcamera-hello` works.** Live preview from camera.
- [ ] **`i2cdetect -y 1` shows both 0x28 and 0x40.** Both IMU and PCA9685 visible.
- [ ] **Servos sweep cleanly.** Test pan from 0 to 180 over 3 seconds. Should be smooth, no buzzing at endpoints.
- [ ] **Pan/tilt limits set.** Bracket doesn't bind at any commanded angle.
- [ ] **MJPEG stream loads on phone.** `http://<pi-ip>:5000/stream.mjpg` shows live feed.
- [ ] **Stream + drive work simultaneously.** Drive the bot while watching the video. No stuttering on either side.
- [ ] **Pan/tilt sliders work from phone.** Move slider, camera moves. No noticeable lag.
- [ ] **Center button works.** Tap it, both servos return to 90°.
- [ ] **End-to-end test.** Drive the bot to another room while watching the camera and panning to look at the environment. Should feel like a low-budget FPV experience.

## Common pitfalls

- **CSI ribbon inserted backwards.** Camera doesn't show up. Look carefully at the metal contacts — they should face away from the USB-C port on the Pi 5.
- **Servos jittering.** Almost always means servo V+ is shared with logic 3V3 instead of having its own 5V. Add the separate 5V wire to PCA9685 V+.
- **Camera stream fine on Pi but unreachable from phone.** Flask binding to `127.0.0.1` only. Bind to `0.0.0.0`: `app.run(host='0.0.0.0', port=5000)`.
- **High CPU usage.** MJPEG encoding eats a core. Don't go above 1280×720 unless you really need it. For navigation-quality vision, 640×480 is plenty.
- **PCA9685 found at unexpected address.** Some boards have A0/A1/A2 address jumpers; if any are bridged, address shifts. Re-check.
- **I2C collisions with IMU.** Shouldn't happen at standard 100 kHz bus speed — IMU and PCA9685 have different addresses and the library handles arbitration. If you do see issues, slow the bus to 100 kHz.
- **Pan/tilt bracket too heavy for the SG90s.** SG90s are weak. If the bracket has metal arms and a heavy camera mount, the servos chatter under load. Switch to MG90S (metal gears, still cheap).
- **Battery droop crashes camera.** The Pi Camera v3 is power-hungry. If the power bank is borderline, the camera may reset under load. Use a 3A+ power bank.

## What's next
Phase 6 wires up the STL-19P lidar. You'll see live 360° scan data in the web UI. After that, Phases 7 and 8 layer SLAM and autonomous navigation on top.
