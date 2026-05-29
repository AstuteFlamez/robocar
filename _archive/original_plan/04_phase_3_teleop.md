# Phase 3 — Pi ↔ Pico bridge & WiFi teleop

## Goal
The Pi 5 talks to the Pico over USB serial with a clean text protocol. The Pi 5 hosts a small web server with a joystick UI. You open the URL on your phone (same WiFi) and drive the bot around in real time. The laptop is no longer in the loop.

## Difficulty
Medium · 1 weekend

## Prerequisites
- Phase 2 complete (Pico can hold commanded velocity)
- Pi 5 has been flashed with Raspberry Pi OS (Bookworm 64-bit), is on your WiFi, and SSH is enabled

## New parts used this phase
- USB-A to micro-USB cable (Pi 5 → Pico 2W) — short one (~20 cm) keeps the chassis tidy
- USB power bank (already have) — finally goes onto the bot

## Schematic — current state of the build

```
                ┌──────────────────┐
                │  USB power bank  │  5V / 3A+
                └────────┬─────────┘
                         │ USB-C
                         ▼
                ┌──────────────────┐
                │  Raspberry Pi 5  │   running Flask web server
                │  on WiFi         │   talks to Pico over /dev/ttyACM0
                └────────┬─────────┘
                         │ USB-A → micro-USB
                         │ (powers Pico + serial link)
                         ▼
                ┌──────────────────┐
                │     Pico 2W      │   runs PID loops, parses commands
                └─┬──────────────┬─┘
              GP2-9              GP10-17
                ▼                  ▲
       ┌──────────────────┐        │
       │  2× DRV8833      │        │
       │  (left + right)  │        │
       └─┬──┬──┬──┬───────┘        │
         ▼  ▼  ▼  ▼                │
       [FL][RL][FR][RR] ───────────┘
                          encoders back to Pico

                ┌──────────────────┐
                │  2S LiPo 7.4V    │  + ──► drivers VM
                │                  │  − ──► common ground
                └──────────────────┘

   Common ground: LiPo− ── DRV GND ── Pico GND ── Pi 5 GND
                   (Pi↔Pico ground tied via USB cable shield + GND wire)
```

No new GPIO connections — this phase is mostly software.

## Serial protocol — Pi 5 ↔ Pico 2W

Text-based, line-delimited. Easy to debug with a regular terminal.

**Pi → Pico (commands):**
```
v <left_mps> <right_mps>\n     # set left/right side velocity, m/s
s\n                            # stop (sets v 0 0)
ping\n                         # liveness check
```

**Pico → Pi (telemetry, sent at 20 Hz):**
```
t <ms> <fl> <rl> <fr> <rr>\n   # ms = monotonic ms, then per-wheel m/s
pong\n                         # response to ping
ok\n                           # ack of last command
err <message>\n                # something went wrong
```

This is deliberately boring text. JSON, binary, msgpack — all overkill until you've shipped one working version.

**Safety feature: command timeout.** The Pico expects a `v` command at least every 500 ms. If nothing arrives, it stops the motors. This prevents runaway if the Pi crashes or the WiFi drops.

## Wire-by-wire connection list — new wires this phase

Just two cables this phase: the USB-C charging cable for the Pi 5 (you already had this), and the USB-A to micro-USB cable linking the Pi 5 to the Pico.

### Pi 5 power

- [ ] `USB power bank USB-C output` ──► `Pi 5 USB-C input` — supplies 5V at up to 3A

### Pi 5 ↔ Pico 2W link (single USB cable, carries both power and serial data)

- [ ] `Pi 5 USB-A port` (any of the 4) ──► `Pico 2W micro-USB port` — provides 5V to Pico VBUS and creates `/dev/ttyACM0` on Pi 5

That's it. All other wires from the previous two phases stay in place.

## Software task list

### On the Pico — `main.py`

1. On boot, initialize motors, encoders, PID
2. Start a 50 Hz control loop that:
   - Reads encoders, computes wheel velocities
   - Runs PID per wheel, updates PWM
   - Decrements a "watchdog" counter — if it hits zero, force velocity setpoint to 0
3. Start a 20 Hz telemetry loop that prints `t ...` lines to stdout
4. In between, poll `sys.stdin` for incoming commands. On `v L R`, set the side setpoints and reset the watchdog. On `s`, force 0,0.

`sys.stdin` reading without blocking on MicroPython is the tricky bit — use `select.poll()`:

```python
import select, sys
poll = select.poll()
poll.register(sys.stdin, select.POLLIN)

if poll.poll(0):
    line = sys.stdin.readline()
    handle_command(line)
```

### On the Pi 5

1. Identify the Pico's serial device: `ls /dev/serial/by-id/` → it'll show up as something like `usb-MicroPython_Board_in_FS_mode-if00`. Use that long name in code, not `/dev/ttyACM0` — the latter changes between reboots.

2. Write a `pico_link.py` that opens the serial port at 115200 baud (or whatever the Pico's REPL uses — typically just `pyserial` default) and exposes:
   ```python
   class PicoLink:
       def __init__(self, device): ...
       def set_velocity(self, left_mps, right_mps): ...
       def stop(self): ...
       def get_telemetry(self) -> dict: ...
   ```

3. Write a `webserver.py` using Flask:
   - GET `/` → serves an HTML page with a joystick
   - POST `/cmd` → JSON `{"left": 0.3, "right": 0.3}` → calls `pico_link.set_velocity(...)`
   - GET `/telemetry` → returns latest telemetry as JSON

4. Joystick UI: nipplejs.com is a 5-line drop-in. Convert the joystick's x/y to differential drive:
   ```javascript
   // forward = -y (joystick up is forward)
   // turn = x
   const v = -y;
   const w = x;
   const left = v + w;
   const right = v - w;
   // scale to [-max_speed, max_speed]
   ```

5. Run the server with `gunicorn` or even just `python webserver.py` on port 5000. Access from your phone at `http://<pi-ip>:5000/`.

### Find your Pi's IP
```bash
hostname -I
```
Or set up Avahi/mDNS so you can use `http://raspberrypi.local:5000/`.

### Make it boot automatically (optional but worth it)

Create a systemd unit `/etc/systemd/system/robocar.service`:
```
[Unit]
Description=Robocar
After=network.target

[Service]
ExecStart=/home/pi/robocar/venv/bin/python /home/pi/robocar/webserver.py
Restart=on-failure
User=pi

[Install]
WantedBy=multi-user.target
```
Then `sudo systemctl enable --now robocar`. Now the bot is autonomous-ish from power-on.

## Verification checklist

- [ ] **Pico shows up as serial device on Pi.** `ls /dev/serial/by-id/` lists the Pico. If not: try a different USB cable (some are charge-only — no data).
- [ ] **Manual serial test.** From a Pi shell: `screen /dev/serial/by-id/<long-name> 115200`. Type `v 0.2 0.2`, press enter. Wheels spin at 0.2 m/s. Type `s`, they stop.
- [ ] **Stop on disconnect.** Run the manual test, then yank the USB cable. Wheels should stop within ~0.5 seconds (watchdog).
- [ ] **Stop on Pi crash.** Same test but kill the Python process instead. Same result.
- [ ] **Telemetry stream.** Watch `t ...` lines arriving at 20 Hz. Velocity values should match commanded ones once steady-state.
- [ ] **Web UI loads on phone.** Open `http://<pi-ip>:5000/` from a phone on the same WiFi. Joystick appears.
- [ ] **Joystick drives the bot.** Push joystick forward — bot drives forward. Left — turns left. Diagonal — arcs.
- [ ] **Round trip latency feels OK.** Should feel snappy. If it's laggy (>200 ms), check that you're sending commands at 20 Hz max from the browser, not on every joystick tick.
- [ ] **Drive around the room.** Take it for a spin. Carry the USB power bank if it's not strapped to the bot yet.

## Common pitfalls

- **Pico stops printing telemetry after a few minutes.** The MicroPython REPL has a default output buffer that fills up if the Pi isn't reading fast enough. Make sure your `PicoLink` is in a thread that constantly reads incoming lines, even when you're not actively using the data.
- **`/dev/ttyACM0` keeps changing.** Use `/dev/serial/by-id/` paths. They're stable across reboots.
- **Web UI works on the Pi but not on the phone.** Firewall on the Pi. `sudo ufw allow 5000` or just `sudo ufw disable` for now.
- **High-priority I/O blocking the control loop.** If you `print()` too much from the Pico, the USB-CDC buffer fills and the control loop stalls. Keep telemetry to one line per cycle.
- **Joystick "center" not actually center.** Browser joystick libraries usually emit values from a calibrated center, but if the joystick gets stuck off-center (touch released but state not updated), the bot keeps going. Always send `v 0 0` when the joystick is released.
- **WiFi falls off.** The Pi 5's WiFi is OK but you're in a metal chassis with the lidar's motor right next to it. If you see drops, try moving the Pi or using a USB WiFi dongle with an external antenna.

## What's next
Phase 4 adds the BNO055 IMU. Combined with the wheel odometry you already have, the bot can track its heading and (x, y) position as it moves. That's the foundation for everything in Phases 7–8.
