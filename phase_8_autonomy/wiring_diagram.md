# Phase 8 — Wiring diagram (cumulative system, final state)

> **No new wires this phase.** Phase 8 is pure software. Electrically, the robot is *finished* — this diagram is the **complete system at the end of the build**, identical to the end of Phase 7. It's here so you have one authoritative picture of the whole machine while you write the navigation stack, and so you can re-run the continuity ritual one last time before a long autonomous run. Every pin, voltage, and bus below is pulled from `_engineering/device_contracts.md` (canonical pin map + end-to-end connection matrix).

---

## Full system block schematic (end of Phase 8)

```
                         ┌──────────────────────────────────────────────┐
                         │            2S LiPo  7.4 V  2200 mAh 10C        │
                         │         (8.4 V full → 6.0 V empty, ~22 A)      │
                         └───────────────────────┬──────────────────────┘
                                                 │ SM-2P (keyed) → pigtail
                                          ┌──────▼──────┐
                                          │ 10 A MAIN   │  (closest to LiPo +)
                                          │ FUSE        │
                                          └──────┬──────┘
                                       ┌─────────▼──────────┐
                                       │ Reverse-polarity + │  Pololu #2815
                                       │ MASTER switch      │  (P-FET high-side)
                                       └─────────┬──────────┘
                                  protected 7.4 V bus
          ┌─────────────────────────────┬────────┴────────────┬───────────────────────┐
          ▼                             ▼                      ▼                        
   ┌────────────┐               ┌──────────────┐       ┌──────────────┐                 
   │ 7.5 A fuse │               │  5 A fuse    │       │ UBEC input   │                 
   │ MOTOR br.  │               │  Pi branch   │       │ (servo br.)  │                 
   └─────┬──────┘               └──────┬───────┘       └──────┬───────┘                 
         ▼                             ▼                      ▼                          
   ┌────────────┐               ┌──────────────┐      ┌───────────────┐                 
   │ E-STOP (NC)│               │ D24V50F5     │      │ Hobbywing UBEC│                 
   │ in series  │               │ buck 5V / 5A │      │  5 V / 3 A    │                 
   └─────┬──────┘               └──────┬───────┘      └──────┬────────┘                 
         │ 7.4 V VM                     │ 5 V USB-C           │ 5 V  V+ (NOT VCC!)        
         ▼                             ▼                      ▼                          
 ┌────────────────┐        ┌──────────────────────┐   ┌──────────────┐                  
 │ 2× TB6612FNG   │        │   Raspberry Pi 5     │   │  PCA9685     │ I²C 0x40          
 │  VM = 7.4 V    │        │  (high-level brain)  │   │  V+ = 5 V    │                   
 │  VCC = 3.3 V ◄─┼──┐     │  usb_max_current=1   │   │  VCC = 3.3 V │◄─ Pi 3.3 V        
 │  STBY ◄ GP14   │  │     │                      │   └──┬────────┬──┘                   
 │ A:FL/RL B:FR/RR│  │     │  CSI(22pin)──FPC#5820│      │ CH0    │ CH1                  
 └───┬────────────┘  │     │   └► Camera Module 3 │      ▼ pan    ▼ tilt                 
     │ AO/BO 7.4 V   │     │                      │   ┌──────────────┐                   
     ▼               │     │  I²C-1 (pin 3/5) ────┼──►│ INA219        │ 0x41             
 ┌────────────┐      │     │  UART0 (pin 8/10)────┼──►│ BNO055 (UART) │ PS1=HIGH         
 │ 4× 310 motor│     │     │                      │   └──────────────┘                   
 │  + encoder  │     │     │  USB-A ×4 ───────────┤                                       
 │ FL RL FR RR │     │     │     ├─► Pico micro-USB (5 V VBUS + serial)                   
 └──────┬──────┘     │     │     ├─► CP2102 (3.3 V) ─► STL-19P lidar  (/dev/ttyUSB0)      
        │ A/B 3.3 V  │     │     └─► (opt) Nuwa depth cam                                 
        │ quadrature │     └──────────────────────┘                                       
        ▼            │ 3V3(OUT) ≤300 mA                                                   
 ┌──────────────────────────────┐                                                        
 │           Pico 2W            │  real-time co-processor                                 
 │  GP2..GP13  → TB6612 IN/PWM  │  20 kHz PWM, per-wheel PID                              
 │  GP14       → STBY (HIGH)    │  command-timeout watchdog (0.5 s)                       
 │  GP16..27   ← 4× encoder A/B │  PIO quadrature decode (1 SM each)                      
 │  3V3(OUT)   → TB6612 VCC ×2  │                                                         
 │             + 4× encoder VCC │  ◄─ VBUS 5 V from Pi USB                                │
 └──────────────────────────────┘                                                        

 ───────────────────────────── COMMON GROUND (STAR at LiPo −) ─────────────────────────
 Separate legs: motor return (heavy 18 AWG) · buck · UBEC · Pico/Pi logic. No chains.
```

## Software data flow added this phase (no wires — for orientation only)

Phase 8 adds these *logical* connections inside the Pi, all riding the **existing** Pi↔Pico USB serial link and the **existing** lidar USB:

```
  web click ─► A* (inflated map) ─► Pure Pursuit (holonomic) ─► safety bubble (lidar)
                                                                      │ (vx,vy,wz)
                              Phase-3 inverse kinematics ◄────────────┘
                                       │  "v vx vy wz\n"  (USB serial, /dev/ttyACM*)
                                       ▼
                                    Pico 2W ─► per-wheel PID ─► 2× TB6612 ─► 4 motors
       SLAM pose ◄─ Phase-7  ◄─ lidar (USB) + odom (serial telemetry) + BNO055 (UART)
```

---

## Wire-by-wire connection list — NEW wires this phase

**There are none.** Phase 8 introduces **zero new physical connections.** ✅ Confirm the existing harness is intact rather than adding to it:

- [ ] Pi↔Pico USB "spinal cord" cable seated and **strain-relieved** (VHB/cable-tie, not hot glue) — autonomy means it must survive sideways lunges without unseating. *(Contract: Pi USB-A → Pico micro-USB; provides 5 V VBUS + the `v vx vy wz` serial command path. device_contracts.md "Pi USB-A → Pico micro-USB".)*
- [ ] CP2102 → lidar link seated; lidar enumerates as `/dev/ttyUSB0`. *(Contract: CP2102 @ 3.3 V, lidar TX 3.3 V, PWM pin → GND. device_contracts.md "CP2102 (3.3 V) → STL-19P Tx".)*
- [ ] All four motor/encoder JST-PH 6-pin connectors fully seated. *(A jiggled-loose encoder mid-autonomy corrupts odometry → SLAM pose → the plan. device_contracts.md "310 encoder A/B → Pico PIO pins".)*

> If you find yourself *adding* a wire in Phase 8, stop — you've drifted out of scope. The only thing that changes this phase is code.

---

## ⚠️ Footguns that still bite in Phase 8

Even with no new wiring, autonomy *exercises* the whole system harder than any prior phase — the robot now moves on its own, so latent wiring faults surface as navigation misbehavior, not obvious smoke. Re-check these before a long run:

- **⚠️ STBY (GP14) must stay HIGH.** If it ever floats low (a loose jumper), the TB6612 outputs disable and the robot silently "ignores" navigation commands — wheels dead, no error. Beep-test GP14 → 3.3 V.
- **⚠️ Encoder VCC = 3.3 V (Pico 3V3 OUT), NEVER the 7.4 V motor rail.** This keeps the A/B outputs inside the Pico's GPIO limit. A drift to 7.4 V here doesn't just fry a pin — it corrupts odometry, which corrupts SLAM pose, which sends the planner to the wrong place. Verify continuity from each encoder VCC to 3V3, **not** to VM.
- **⚠️ PCA9685 VCC ≠ V+.** VCC = 3.3 V (logic, from the Pi), V+ = 5 V (servo power, from the UBEC). Cross-wiring kills the board and, mid-autonomy, kills the camera gimbal.
- **⚠️ BNO055 on UART, not I²C** (PS1 = HIGH). The heading feeding your holonomic controller's `ωz` and the pose feeding A* both depend on a *clean* IMU. On the Pi's hardware I²C the BNO055's clock-stretching silently corrupts orientation bytes — a corrupted heading on a moving autonomous robot is worse than a clean failure.
- **⚠️ Lidar PWM pin → GND.** Default closed-loop 10 Hz spin requires the lidar's PWM/CTRL pin tied to GND. Floating = erratic/no spin = a stale or empty scan feeding your safety bubble — the reflex that's supposed to stop the robot hitting your foot.
- **⚠️ Star ground intact.** Autonomous driving means sustained motor current. A daisy-chained ground lets that current modulate the logic reference and inject noise into encoder edges and UART framing. Beep every ground leg back to the single star point at LiPo −.
- **⚠️ E-stop is in the MOTOR branch only — and it's your fastest stop.** The software STOP button and the Pico watchdog are real, but the NC mushroom is faster than any code path. Keep it within reach during every autonomous run; confirm it still kills the wheels while leaving the Pi/Pico alive.

---

## Pin tables (reference — unchanged from the canonical map)

### Pico 2W — motors (2× TB6612FNG)

| Wheel | Board · ch | IN1 | IN2 | PWM |
|---|---|---|---|---|
| FL | A · A | GP2 | GP3 | GP4 |
| RL | A · B | GP5 | GP6 | GP7 |
| FR | B · A | GP8 | GP9 | GP10 |
| RR | B · B | GP11 | GP12 | GP13 |
| **STBY** (both boards) | — | **GP14 → HIGH** | | |

### Pico 2W — encoders (PIO quadrature, adjacent A/B)

| Wheel | A (base) | B |
|---|---|---|
| FL | GP16 | GP17 |
| RL | GP18 | GP19 |
| FR | GP20 | GP21 |
| RR | GP26 | GP27 |

Encoder VCC ← Pico **3V3(OUT)**; encoder GND → star ground.

### Raspberry Pi 5 — buses (40-pin header)

| Function | Pin(s) | Device |
|---|---|---|
| I²C-1 SDA / SCL | 3 / 5 | PCA9685 0x40, INA219 0x41 |
| UART0 TXD / RXD | 8 / 10 | BNO055 (PS1=HIGH) |
| 3.3 V | 1, 17 | BNO055 VIN, PCA9685 VCC, INA219 VCC |
| GND | 6, 9, 14, 20, 25, 30, 34, 39 | star ground |
| USB-A ×4 | — | Pico, CP2102→lidar, (opt) depth cam |
| USB-C | — | ← D24V50F5 5 V/5 A buck |
| CSI (22-pin) | — | Camera Module 3 via FPC #5820 |

> The Pi's 5 V header pins (2, 4) stay **unused** — servo power comes from the UBEC's V+, never the Pi rail, so servo transients can't sag the Pi mid-navigation.
