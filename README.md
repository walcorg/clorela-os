# Clorela-OS

Embedded Linux image for the **WALCORG** photobioreactor node: a Raspberry Pi Zero W with the WALCORG shield, running a *Chlorella vulgaris* culture that captures CO2.

> **Status: under construction.** This README is completed phase by phase. See [`CHANGES.md`](CHANGES.md).

## Hardware

<p align="center">
  <img src="docs/images/shield-v4-pcb.png" alt="Shield v4 for the Raspberry Pi Zero W" width="600">
</p>

The shield v4 connects the Pi Zero W to up to two nodes over I2C (through a 5 V level shifter), powers them and carries the TROPIC01 secure element and a two-port USB hub. Schematic, pinout and what it means for the software: [`docs/hardware-shield-v4.md`](docs/hardware-shield-v4.md).

## Software at a glance

| Graph view | Numeric view |
|---|---|
| ![Graph view with a simulated node](docs/images/clorela-ui-graph.png) | ![Numeric view with a simulated node](docs/images/clorela-ui-numeric.png) |

`clorela-ui` (Qt 6 Widgets) showing data from `clorela-acqd` running against the simulated node. On the shield, the status display shows short messages:

<p align="center">
  <img src="docs/images/oled-status-messages.png" alt="Status display messages" width="260">
</p>

## License

[MIT](LICENSE) © 2026 WALCORG TERRAFORMING CORP.

Maintained by WALCORG · [walcorg.com](https://walcorg.com)
