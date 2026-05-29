# `robot_reference/` — manufacturer robot PDFs (MentorPi M1)

The Hiwonder MentorPi M1 documentation that describes the **whole robot** (not just the controller),
extracted from `MentorPi A1 & M1 2024-…-3-001.zip`. Each `.pdf` has a co-located `.txt` (`pdftotext
-layout`); several PDFs are image-heavy (assembly/wiring diagrams live in the figures, not the text).

> For the synthesised picture — every component + the **interface/connection map** — read
> **[`../mentorpi_m1_hardware.md`](../mentorpi_m1_hardware.md)** first. This folder is its primary source.

| Doc | What's in it |
|---|---|
| `00. Important Notice Before Using the MentorPi Robot.(pdf/txt)` | ★ battery-charging cautions (charge <6.4 V), and the **system config tool** (camera type `ascamera`/`usb_cam` = "Orbbec depth"/"monocular"; robot type `MentorPi_Mecanum`/`_Acker`) |
| `1. Getting Ready/1.1 MentorPi Introduction/` | product overview: M1 = Mecanum, A1 = Ackermann; Pi 5 + TOF lidar + 3D depth cam; depth vs monocular variants (mostly images) |
| `1. Getting Ready/1.2 Quick Start Guide/1.2.1 Charging & Usage Guide/` | battery charging + basic usage |
| `1. Getting Ready/1.2 Quick Start Guide/1.2.2 Assembly/` | physical build + wiring (image-heavy; diagrams not in the text) |
| `1. Getting Ready/1.3 Startup Guide & Status Overview/` | ★ power-on behaviour: LED1 + beep = ROS up, LED2 1 Hz = AP, `HW…` hotspot (pw `hiwonder`), lidar spins |
| `10. Lidar Lesson/Lesson 1 Lidar Introduction/` | ★★ lidar electrical/optical/performance specs + the **UART 230400 / ZH1.5T-4P** interface |
| `10. Lidar Lesson/Lesson 2 Lidar Working and Ranging Principle/` | how the TOF lidar works |
| `11. Depth Camera Basic Lesson/Lesson 1 Depth Camera Configuration/` | depth-camera **software** setup (ROS2 SDK) — not hardware specs |

*(Usage-only docs — App Control, Wireless Controller — and the unrelated "Raspberry Pi Expansion Board"
course, A/B/C, which belongs to **other** Hiwonder kits, not the MentorPi's RRC Lite — were intentionally
left out.)*
