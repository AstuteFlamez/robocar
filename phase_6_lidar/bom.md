# Phase 6 — Bill of Materials (parts first used this phase)

This is the **slice** of the master BOM that Phase 6 adds. For the full project
parts list, links, prices, cost summary, and the "verify on YOUR kit" notes, see
[`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).

Phase 6 is electrically tiny: exactly **one** new buy (a $7 USB-UART bridge) and
**one** kit part you finally wire up (the lidar). The lidar's ~290 mA at 5 V is
already accounted for in the [power budget](../_engineering/power_budget.md) — it
rides the D24V50F5's 5 V rail through the adapter's USB, *not* the motor bus. The
real engineering payoff here isn't amps; it's **logic-level discipline** (3.3 V,
not 5 V) on a TX line the datasheet says is *not* 5 V tolerant.

## Buy / already-have

| Item | Qty | ~$ | Why (electrical reason) | Link | Have/Buy |
|---|---|---|---|---|---|
| **CP2102 USB-UART adapter (3.3 V)** | 1 | $7 | **buy** — bridges the lidar's one-way 3.3 V UART (230400 baud, 8N1) to a clean `/dev/ttyUSB0` that enumerates with zero config and leaves the Pi's Bluetooth + serial console untouched. ★ **Set its logic jumper to 3.3 V** (verify the VCCIO pin reads ~3.3 V on a multimeter *before* the lidar sees it) — a 5 V edge into the lidar's 3.3 V-only TX forward-biases its protection diodes and kills the receiver path over time. Also supplies the lidar's **5 V from the USB bus** (Pin4). | https://www.waveshare.com/cp2102-usb-uart-board-type-a.htm | **buy** |
| **LDROBOT STL-19P** 360° DTOF lidar | 1 | — | **already have (kit)** — the room-scanner. ⚠️ In the kit it was wired to the **RRC Lite** you're bypassing, so it arrives as a bare module on a **JST-ZH 1.5 mm 4-pin** pigtail, *not* a USB stick — which is exactly why you buy the CP2102. Signal lines are **3.3 V TTL, NOT 5 V tolerant**; ★ tie **Pin2 (PWM) to GND** for the default closed-loop ~10 Hz spin (floating = erratic/no spin). 5 V, ~290 mA from the adapter's USB. | (from kit) | **already have (kit)** |

**Notes**
- The CP2102 often **ships with a JST-ZH 4-pin pigtail and a couple of jumper
  wires** — enough to land the lidar's Pin1/Pin3/Pin4 on the adapter's RXD/GND/5V
  and tie Pin2→GND. If yours doesn't, the Dupont/JST kit from Phase 1 covers it;
  no separate line item.
- The lidar's **TXD-side of the adapter is unused** — this link is one-way (the
  lidar only ever transmits), so leave the CP2102's TXD pin disconnected.
- Everything else this phase — the **Pi 5, the D24V50F5 5 V/5 A buck** (which
  actually powers the lidar through the USB bus), the **multimeter and logic
  analyzer** (used to confirm the idle TX line sits at 3.3 V, not 5 V, and to
  decode 230400 8N1) — was introduced in earlier phases and is reused unchanged,
  so it is not re-listed here.
- Full master list with cost summary and verification notes:
  [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md).
