# Changes

All notable changes to Clorela-OS are listed here. Dates are UTC.

## [Unreleased]

### Added
- Repository skeleton: MIT license, `.gitignore` for CMake, Qt Creator and Yocto/kas build trees, this changelog.
- `clorela-ui`: Qt 6 Widgets interface ported from `clorela_os_v14` with the same menu tree (Account, View, Configure, Options, Help), real F1/F5 shortcuts, English source strings with Spanish and French translations switchable at runtime, Graph and Numeric views fed from the local measurement database, full-screen mode and a restricted mode that disables network-only functions when the node is offline.
- `clorela_common`: INI configuration (`data/clorela.conf`, `data/dev.conf`), SQLite measurement store (WAL), offline detection from the routing table.
- `clorela-acqd`: acquisition daemon, separate from the interface. Speaks the `walcorg/node-routine` I2C protocol (status, data pages, darkness check, single-LED measurement, UVC lockout) over `/dev/i2c-N`, or against a simulated node (`sensor=sim`) when there is no hardware. Each cycle stores a dark reading plus one reading per LED in SQLite. Waits for Wi-Fi when `require_network=true`, skips nodes in UVC disinfection, turns the LEDs off on stop and shuts down cleanly on SIGTERM.
- Control socket (`/run/clorela/acqd.sock`, JSON lines: `status`, `start`, `stop`, `sterilize`, `stop_uv`). The interface's Start/Stop acquisition items (F1/F5) and status bar use it.
- `clorela-status` and the shield status display: SSD1306 128x64 I2C OLED driver with a 5x7 font, showing BOOTING, READY, SAMPLING, NO WI-FI, STERILIZING, NODE ERROR. Console and `none` backends, PBM preview for desktops.
- Signing extension point for the TROPIC01 (`src/signer/`): `Signer` interface (anchor public key, digest signing with low-s P-256 `r || s`, TRNG bytes), contract checks in the base class, `NullSigner` as the only backend, `shieldId` helper (Keccak-256) and `docs/signer-tropic01.md` with the steps and open questions for the Anchor Protocol. `[signer] backend=none` in the configuration.
- Unit tests (QtTest) for configuration, storage, network detection, the signer contract, the main window, the OLED framebuffer, the node protocol against the simulator, the acquisition cycle and the control socket.
- `docs/hardware-shield-v4.md` with the shield v4 PCB render, the v3 3D model (illustrative), the seven schematic sheets and the pin facts the software depends on (I2C1 node bus, TROPIC01 on SPI0 with chip select on GPIO25, USB hub in host mode). README hardware and screenshot sections.
- `docs/legacy-v14.md`: what the `clorela_os_v14` sketch contained and how it maps to this repository.
