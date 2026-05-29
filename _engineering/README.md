# `_engineering/` — the source of truth

> These five documents are the **engineering backbone** of the whole project. The eight phase folders are *how you build it, step by step*; this folder is *why it's built that way, and the verified facts that make it correct.* Electrical mistakes are easy and expensive, so before any wire goes in, the relevant numbers should be traceable to a document here.
>
> **Golden rule:** if you change a part or a pin, change it **here first** (device contracts / BOM / power budget), then ripple it out to the phase folders. The phase docs cite these; they don't invent their own numbers.

## The documents

| File | What it answers | Read it when |
|---|---|---|
| **[device_contracts.md](./device_contracts.md)** | "Will these two parts actually connect — voltage, current, logic level, connector?" Per-device contracts + an end-to-end connection matrix. | Before wiring anything; whenever you swap a part. |
| **[products_to_buy.md](./products_to_buy.md)** | "What do I already have, what do I buy, and why?" Master BOM with links, prices, quantities. | Before ordering; the per-phase `bom.md` files are slices of this. |
| **[power_budget.md](./power_budget.md)** | "Where does the power flow and is every rail sized right?" Power tree + current budget + fuse/buck/UBEC sizing. | Before building the power tree (Phases 1, 3, 5). |
| **[engineering_decisions.md](./engineering_decisions.md)** | "Why this part/design and not the obvious alternative?" A decision log with rationale + alternatives + learning payoff. | When you wonder *why*; when an interviewer asks. |
| **[safety.md](./safety.md)** | "How do I not get hurt or destroy hardware?" LiPo rules, protection layers, power-up order, continuity ritual. | **Once, fully, before connecting a battery.** Then keep open. |

## How this was built
The contents were produced by:
1. **Identifying the actual hardware** — the kit is a Hiwonder **MentorPi M1** (Mecanum chassis, RRC Lite controller, 310 encoder motors, STL-19P lidar, depth camera, LFD-01 servos, 2S LiPo), confirmed against Hiwonder's product pages.
2. **A datasheet research pass** — one verification per component, pulling voltage/current/logic-level/connector facts from primary sources (TI, Bosch, NXP, Raspberry Pi, LDROBOT, Hiwonder).
3. **Three adversarial electrical verifications** — motor-vs-driver (SAFE → upgraded to TB6612 for margin), Pi-5 power (**UNSAFE** on a 3 A bank → buck), servo power (**UNSAFE** on the Pi rail → UBEC).
4. **A six-lens design brainstorm** (power, motor/control, sensing, actuation, mechanical/wiring, education) reconciled into the decision log.

Every electrical claim traces to a manufacturer source listed in the relevant document. **Still measure with a multimeter before connecting** — documents describe the design; the bench is the authority.

## Conventions used across all docs
- `FL / FR / RL / RR` = front-left / front-right / rear-left / rear-right wheels.
- **Pico pins:** `GP<n>` is the GPIO number; physical pin given as `(pin <m>)`.
- **Pi 5 pins:** `GPIO<n>` is the BCM number; physical position as `(header pin <m>)`.
- Voltages are **nominal** — verify before connecting.
- **Common ground** = a single-point *star* at the LiPo negative (not a daisy-chain).
- ⚠️ marks a footgun; ★ marks a number/fact the whole design hangs on.
