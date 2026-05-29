# Archived — original first-draft plan (superseded)

These ten files are the **original** build plan, kept here for reference and transparency. They are **superseded** by the reorganized, hardware-reconciled, electrically-verified docs in the repo root (`_engineering/` + the eight `phase_*/` folders).

**Why they were superseded.** The original plan was written for a generic differential-drive 4WD car. Once the actual kit was identified as a **Hiwonder MentorPi M1** and every component was checked against its datasheet (plus three adversarial electrical verifications), several things had to change:

- The chassis is **Mecanum**, not differential → full `(vx, vy, ωz)` kinematics.
- A **5 V/3 A power bank can't safely power a Pi 5** → single-LiPo power tree with a 5 V/5 A buck, a servo UBEC, reverse-polarity protection, coordinated fusing, and an e-stop.
- **DRV8833 → TB6612FNG** (thermal headroom over the 1.4 A motor stall).
- **BNO055 → UART mode** (the Pi mishandles its I²C clock stretching).
- Correct **cables/connectors** (Pi 5 22-pin CSI; 3.3 V lidar UART; factory JST-PH motor harness).
- Reuse the kit's chassis/wheels/motors/lidar/servos/battery instead of buying a second chassis.

The full reasoning for each change is in [`../../_engineering/engineering_decisions.md`](../../_engineering/engineering_decisions.md).

Nothing here is wrong to *read* — it's a useful before/after comparison — but **build from the new docs**, not these.
