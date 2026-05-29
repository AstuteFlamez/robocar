# Phase 3 — Bill of Materials

Parts **introduced or first used** in Phase 3 only. This is a slice of the master list — see [`_engineering/products_to_buy.md`](../_engineering/products_to_buy.md) for the full BOM with every phase's parts, links, and prices.

| Item | Qty | ~$ | Why (electrical reason) | Link | Status |
|---|---|---|---|---|---|
| Pololu **D24V50F5** 5 V/5 A buck regulator (#2851) | 1 | $25 | The Pi 5 needs to see **5 V / 5 A** or it caps USB to 600 mA and browns out; a 5 V/3 A bank can't. 6–38 V in (holds 5 V even at a 6.0 V empty pack), 5 V/5 A out, reverse-protect + soft-start. | https://www.pololu.com/product/2851 | **buy** |
| **USB‑A → micro‑USB cable**, short (~20 cm), **data-capable** | 1 | $4 | The Pi↔Pico "spinal cord": 5 V to the Pico's VBUS **+** the serial bridge in one cable. Must carry data, not charge-only. | https://www.amazon.com/s?k=usb+a+to+micro+usb+short+data+cable | **buy** |
| **5 A blade fuse** + inline ATC/ATO holder (Pi-buck branch) | 1 | from the fuse kit | Selectivity: a Pi/buck fault opens *this* branch only, not the 7.5 A motor branch or the 10 A main. Sized just above the Pi branch's ~3.5 A worst-case pack draw. | https://www.amazon.com/dp/B0DT4NCD5V | **buy** (kit bought Phase 1) |

> **Already on the bot (from Phases 1–2), used again here:** Raspberry Pi 5, Pico 2W, 2× TB6612FNG, 4× 310 motors+encoders, 2S LiPo, 10 A/7.5 A fuses, reverse-polarity/master switch, NC e-stop, bulk + ceramic caps, screw-terminal carrier. No re-purchase.
>
> **Retired this phase:** the 5 V/3 A USB power bank leaves the robot — it stays on the shelf as a **bench-only** tethered supply for Pi OS setup. Not a "buy"; just no longer the robot's supply.

**Phase cost (new spend): ~$29** (the 5 A fuse comes from the Phase-1 fuse kit). The buck is the one meaningful purchase — and it's the part that turns a tethered prototype into a self-contained battery-powered robot.
