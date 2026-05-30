# Phase 3 — Bill of Materials

Parts **introduced or first used** in Phase 3 only. This is a slice of the master list — see [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md) for the full BOM with every phase's parts, links, and prices.

| Item | Qty | ~$ | Why (electrical reason) | Link | Status |
|---|---|---|---|---|---|
| Pololu **D24V50F5** 5 V/5 A buck regulator (#2851) | 1 | $25 | The Pi 5 needs to see **5 V / 5 A** or it caps USB to 600 mA and browns out; a 5 V/3 A bank can't. 6–38 V in (holds 5 V even at a 6.0 V empty pack), 5 V/5 A out, reverse-protect + soft-start. | https://www.pololu.com/product/2851 | **buy** |
| Pololu **#2815** reverse-polarity master switch *(or rocker + #5381)* | 1 | $8 | With a second branch now present, this is the system master ON/OFF **+** reverse-polarity protection in one P-FET part (no diode drop). **Especially worth it here because the LiPo's leads are bare/unkeyed** — this part replaces the Phase-1 "meter polarity every time" habit with hardware that simply blocks a reversed battery. (Deferred out of Phase 1 to keep the bench build minimal.) | https://www.pololu.com/product/2815 | **buy** |
| Inline **ATC/ATO fuse holders + blades**: **10 A main / 7.5 A motor / 5 A Pi** | 3 + asst | $10 | The coordinated set: main at the battery, smaller branch fuses downstream (selectivity). **Slow-blow** on main + motor branch (ride motor inrush); fast-acting OK on the Pi branch. First useful now that there's >1 branch. | https://www.amazon.com/dp/B0DT4NCD5V | **buy** |
| **22 mm latching mushroom e-stop (1× NC)** | 1 | $8 | First floor-driving phase → a fail-safe physical motor kill in series in the motor branch only; **NC** so a broken wire also trips it. Pi/Pico stay alive to log why. | https://www.amazon.com/dp/B00W947PS0 | **buy** |
| **USB‑A → micro‑USB cable**, short (~20 cm), **data-capable** | 1 | $4 | The Pi↔Pico "spinal cord": 5 V to the Pico's VBUS **+** the serial bridge in one cable. Must carry data, not charge-only. | https://www.amazon.com/s?k=usb+a+to+micro+usb+short+data+cable | **buy** |

> **Already on the bot (from Phases 1–2), used again here:** Raspberry Pi 5, Pico 2W, 2× TB6612FNG, 4× 310 motors+encoders, 2S LiPo (+ its ferruled flying leads and WAGO splices), bulk + ceramic caps, screw-terminal carrier. No re-purchase.
>
> **New here, deferred out of Phase 1:** the **#2815 master switch**, the **coordinated fuse set (10 A / 7.5 A / 5 A)**, and the **NC e-stop**. Phase 1's single-branch bench build on a self-protected LiPo, wheels-in-air, didn't need them; this two-branch, floor-driving phase does. The Phase 1/2 ferruled-leads → WAGO path is rebuilt into the full tree (ferruled leads → main fuse → #2815 switch → motor fuse → e-stop → WAGO → VM).
>
> **Retired this phase:** the 5 V/3 A USB power bank leaves the robot — it stays on the shelf as a **bench-only** tethered supply for Pi OS setup. Not a "buy"; just no longer the robot's supply.

**Phase cost (new spend): ~$55** — the **buck ($25)** is the headline purchase that turns a tethered prototype into a self-contained battery-powered robot; the protection tree (#2815 switch, fuse set, e-stop ≈ $26) is the safety the bot now needs once it leaves the stand.
