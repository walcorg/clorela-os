# Changes

All notable changes to Clorela-OS are listed here. Dates are UTC.

## [Unreleased]

### Added
- **Laura Encoding (Codificado de Laura)**, optical challenge protocol v1 (`docs/laura-encoding.md`). Requires node-routine firmware 0x02.
  - Node protocol: `X` (each of the 12 visible LEDs at its own 12-bit intensity, 16 readings) and `NodeClient::measurePattern()`; `NodeClient::calibrate()` reads the `C` pages (dark baseline and each LED alone).
  - `clorela-acqd`: `laura_calibrate` and `laura_challenge` on the control socket. Each answer becomes a canonical record (domain-separated, little-endian) with a SHA-256 digest, signed by the configured `Signer` (unsigned with `NullSigner`). Nodes with firmware 0x01 are reported as needing an update.
  - Simulated node: `X`, firmware version 0x02, and a per-node optical coupling so every simulated node has its own fingerprint (`setGrowth()` test hook to freeze the culture).
  - `tests/tst_laura.cpp`: the pattern follows the calibrated matrix, a wrong answer is rejected, two nodes have different matrices, the digest is canonical, the control socket validates its input, a sterilizing node refuses.
  - `tools/laura/laura_server_reference.py`: server-side reference (calibration, fresh challenges, verification, digest check, four attacks, culture signature over time). Not installed on the node.
- Repository skeleton: MIT license, `.gitignore` for CMake, Qt Creator and Yocto/kas build trees, this changelog.
- `clorela-ui`: Qt 6 Widgets interface ported from `clorela_os_v14` with the same menu tree (Account, View, Configure, Options, Help), real F1/F5 shortcuts, English source strings with Spanish and French translations switchable at runtime, Graph and Numeric views fed from the local measurement database, full-screen mode and a restricted mode that disables network-only functions when the node is offline.
- `clorela_common`: INI configuration (`data/clorela.conf`, `data/dev.conf`), SQLite measurement store (WAL), offline detection from the routing table.
- `clorela-acqd`: acquisition daemon, separate from the interface. Speaks the `walcorg/node-routine` I2C protocol (status, data pages, darkness check, single-LED measurement, UVC lockout) over `/dev/i2c-N`, or against a simulated node (`sensor=sim`) when there is no hardware. Each cycle stores a dark reading plus one reading per LED in SQLite. Waits for Wi-Fi when `require_network=true`, skips nodes in UVC disinfection, turns the LEDs off on stop and shuts down cleanly on SIGTERM.
- Control socket (`/run/clorela/acqd.sock`, JSON lines: `status`, `start`, `stop`, `sterilize`, `stop_uv`). The interface's Start/Stop acquisition items (F1/F5) and status bar use it.
- `clorela-status` and the shield status display: SSD1306 128x64 I2C OLED driver with a 5x7 font, showing BOOTING, READY, SAMPLING, NO WI-FI, STERILIZING, NODE ERROR. Console and `none` backends, PBM preview for desktops.
- Signing extension point for the TROPIC01 (`src/signer/`): `Signer` interface (anchor public key, digest signing with low-s P-256 `r || s`, TRNG bytes), contract checks in the base class, `NullSigner` as the only backend, `shieldId` helper (Keccak-256) and `docs/signer-tropic01.md` with the steps and open questions for the Anchor Protocol. `[signer] backend=none` in the configuration.
- Unit tests (QtTest) for configuration, storage, network detection, the signer contract, the main window, the OLED framebuffer, the node protocol against the simulator, the acquisition cycle and the control socket.
- `docs/hardware-shield-v4.md` with the shield v4 PCB render, the v3 3D model (illustrative), the seven schematic sheets and the pin facts the software depends on (I2C1 node bus, TROPIC01 on SPI0 with chip select on GPIO25, USB hub in host mode). README hardware and screenshot sections.
- Complete README: how the node works (photoresistors, transmission and backscatter, dark baseline), 8-LED ASCII encoding, UVC sterilization and its safety rules, system overview, feature status, installation on a microSD, menus, views and display messages, troubleshooting, PC build, configuration reference, roadmap and FAQ.
- `docs/flashing.md` (installation guide based on Tutorial 1/5, with the corrected LED codes) and the original tutorial PDF in `docs/tutorials/`.
- `meta-clorela` Yocto layer and kas build (`kas/clorela.yml` with poky, meta-openembedded, meta-raspberrypi and meta-qt6 6.8 pinned to commits on Scarthgap 5.0; `kas/r710.yml` limits for a 12 GB build server; `kas/debug.yml` for development images). Distro `clorela` (systemd, no X11/Wayland/OpenGL, legacy framebuffer), image `clorela-image` for `raspberrypi0-wifi` with `wic.xz`, `wic.bmap` and SHA-256 outputs.
- Recipe `clorela-apps` building the applications from the repository, with systemd units (`clorela-acqd`, `clorela-ui`, `clorela-boot-status`, `clorela-wifi`), a `clorela` service user, udev rules for I2C/SPI and Wi-Fi setup from `wpa_supplicant.conf` on the boot partition.
- Boot splash on HDMI with psplash: WALCORG TERRAFORMING CORPORATION logo on white and a green progress bar driven by systemd; closed by the interface when it starts.
- `config.txt`: HDMI 1920×1080, I2C1, USB host mode, SPI0 with the TROPIC01 chip select on GPIO25.
- `docs/building.md`: how to build the image on the build server, and what to verify in the first build.
- `docs/legacy-v14.md`: what the `clorela_os_v14` sketch contained and how it maps to this repository.
