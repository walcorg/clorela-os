# Changes

All notable changes to Clorela-OS are listed here. Dates are UTC.

## [Unreleased]

### Added
- Repository skeleton: MIT license, `.gitignore` for CMake, Qt Creator and Yocto/kas build trees, this changelog.
- `clorela-ui`: Qt 6 Widgets interface ported from `clorela_os_v14` with the same menu tree (Account, View, Configure, Options, Help), real F1/F5 shortcuts, English source strings with Spanish and French translations switchable at runtime, Graph and Numeric views fed from the local measurement database, full-screen mode and a restricted mode that disables network-only functions when the node is offline.
- `clorela_common`: INI configuration (`data/clorela.conf`, `data/dev.conf`), SQLite measurement store (WAL), offline detection from the routing table.
- Unit tests (QtTest) for configuration, storage, network detection and the main window.
- `docs/legacy-v14.md`: what the `clorela_os_v14` sketch contained and how it maps to this repository.
