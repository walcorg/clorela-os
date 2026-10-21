# Building the Clorela-OS image

The image is built with the [Yocto Project](https://www.yoctoproject.org/) 5.0 "Scarthgap" (LTS) and [kas](https://kas.readthedocs.io), which fetches every layer at a pinned commit (see [`kas/clorela.yml`](../kas/clorela.yml)). The same inputs always give the same image.

| | |
|---|---|
| Machine | `raspberrypi0-wifi` (Raspberry Pi Zero W) |
| Distro | `clorela` (Poky-based, systemd, no X11/Wayland/OpenGL) |
| Image | `clorela-image` |
| Layers | poky, meta-openembedded (meta-oe, meta-python), meta-raspberrypi, meta-qt6 (Qt 6.8), meta-clorela (this repository) |
| Output | `clorela-image-raspberrypi0-wifi.rootfs.wic.xz`, `.wic.bmap`, `.wic.xz.sha256sum` |

## Status

The layer is verified up to BitBake's full parse and dependency resolution (`bitbake -g clorela-image`: 2589 recipes, 0 errors). **The first complete build has not been run yet**; it has to happen on the build server. Items to watch in that first build are listed at the end of this page.

## Build server

The WALCORG build server is a Dell PowerEdge R710 with 12 GB of RAM and Ubuntu 26.04.1. Its successor will be a Dell R720. The steps below work for both.

- **Disk:** at least 150 GB free on the SSD for `build/`, `downloads/` and `sstate-cache/`.
- **RAM:** 12 GB is enough with the limits in [`kas/r710.yml`](../kas/r710.yml) (3 tasks, `make -j3`). Add swap as a safety net:
  ```bash
  sudo fallocate -l 8G /swapfile && sudo chmod 600 /swapfile
  sudo mkswap /swapfile && sudo swapon /swapfile
  echo '/swapfile none swap sw 0 0' | sudo tee -a /etc/fstab
  ```
  If a memory module fails and the server has less RAM, lower both numbers in `kas/r710.yml`.
- **Time:** the first build compiles the cross toolchain and Qt 6 and takes many hours on the R710's Xeons. Later builds reuse `sstate-cache` and take minutes.

### About Ubuntu 26.04

Scarthgap officially supports Ubuntu 22.04 and 24.04 as build hosts. At the pinned poky commit, Ubuntu 26.04 is not in the list of tested hosts, so BitBake prints a warning ("host distribution has not been validated") and continues. If the build hits a host-tool problem, build inside the official kas container instead. It runs a supported Debian environment and only needs Docker or Podman on the host (see step 3b).

## Steps

### 1. Install the host packages (once)

```bash
sudo apt install build-essential chrpath cpio debianutils diffstat file gawk gcc git \
  iputils-ping libacl1 liblz4-tool locales python3 python3-git python3-jinja2 \
  python3-pexpect python3-pip python3-subunit socat texinfo unzip wget xz-utils zstd \
  pipx bmap-tools
sudo locale-gen en_US.UTF-8
pipx install kas
```

### 2. Allow user namespaces (Ubuntu 24.04 and newer)

BitBake isolates tasks from the network with unprivileged user namespaces, which AppArmor restricts on recent Ubuntu. If the build stops with a namespace or permission error, run:

```bash
sudo sysctl kernel.apparmor_restrict_unprivileged_userns=0
```

To keep it after a reboot, write it to `/etc/sysctl.d/60-yocto.conf`.

### 3. Get the sources

Work in a directory **next to** the repository, not inside it, so the fetched layers and the build stay out of the source tree:

```bash
mkdir -p ~/yocto && cd ~/yocto
git clone https://github.com/walcorg/clorela-os.git
```

Never run BitBake as root.

### 3b. (Alternative) Build in a container

```bash
cd ~/yocto
kas-container build clorela-os/kas/clorela.yml:clorela-os/kas/r710.yml
```

With the container, skip steps 1, 2 and 4.

### 4. Build

```bash
cd ~/yocto
kas build clorela-os/kas/clorela.yml:clorela-os/kas/r710.yml
```

kas clones poky, meta-openembedded, meta-raspberrypi and meta-qt6 at the pinned commits into `~/yocto/`, writes `build/conf/local.conf`, and builds `clorela-image`. Downloads go to `~/yocto/downloads` and shared state to `~/yocto/sstate-cache`.

For a development image with root login without password and an SSH server, add `:clorela-os/kas/debug.yml`. **Never distribute that image.**

### 5. Find the image

```bash
ls ~/yocto/build/tmp/deploy/images/raspberrypi0-wifi/clorela-image-raspberrypi0-wifi.rootfs.wic*
```

### 6. Write it to a microSD card

Use Raspberry Pi Imager (see [`flashing.md`](flashing.md)), or from the build server:

```bash
lsblk                                   # identify the card, e.g. /dev/sdX
sudo bmaptool copy ~/yocto/build/tmp/deploy/images/raspberrypi0-wifi/clorela-image-raspberrypi0-wifi.rootfs.wic.xz /dev/sdX
```

### 7. Rebuild after changing the code

`meta-clorela` builds the applications straight from the repository checkout (`externalsrc`), so a change committed or simply saved in `~/yocto/clorela-os` is picked up by the next build:

```bash
cd ~/yocto
kas build clorela-os/kas/clorela.yml:clorela-os/kas/r710.yml                 # whole image
kas shell clorela-os/kas/clorela.yml -c "bitbake clorela-apps"                # only the applications
```

## What meta-clorela contains

| Path | Purpose |
|---|---|
| `conf/distro/clorela.conf` | Distro: systemd, no graphics stack, legacy framebuffer (`DISABLE_VC4GRAPHICS`), Qt SQLite driver, Wi-Fi firmware licence, `i2c-dev`/`spidev` at boot, host name `clorela` |
| `recipes-core/images/clorela-image.bb` | The image: boot splash, applications, Wi-Fi, `i2c-tools`, `socat`; outputs `wic.xz`, `wic.bmap`, SHA-256 |
| `recipes-clorela/clorela-apps/` | Builds `clorela-acqd`, `clorela-status`, `clorela-ui` from this repository; systemd units, `clorela` service user, udev rules for I2C/SPI, Wi-Fi setup |
| `recipes-core/psplash/` | Boot splash: WALCORG logo on white, green progress bar |
| `recipes-bsp/bootfiles/rpi-config_git.bbappend` | `config.txt`: HDMI 1920×1080, I2C1, USB host mode, SPI0 with chip select on GPIO25 |

### Boot sequence

1. **Splash on HDMI** (psplash): the WALCORG logo with a progress bar that systemd advances as services start.
2. **BOOTING** on the shield display (`clorela-boot-status.service`).
3. **Wi-Fi setup** (`clorela-wifi.service`): imports `wpa_supplicant.conf` from the boot partition if present and connects.
4. **Acquisition service** (`clorela-acqd.service`, user `clorela`).
5. **Interface** (`clorela-ui.service`): closes the splash and opens full screen on HDMI.

![Boot splash at 1920x1080](images/boot-splash-1080p.png)

*Boot splash as psplash lays it out at 1920×1080 (simulated): logo centred above the split line at 5/6 of the height, bar on the line.*

## To verify in the first build and on the hardware

- **Translations.** `qt_add_translations` finds `lrelease` from `qttools-native`. If CMake does not find `Qt6LinguistTools` when cross-compiling, add `qttools` to `DEPENDS` in `clorela-apps_0.1.0.bb`.
- **Wi-Fi units.** `wpa_supplicant@wlan0.service` comes from the wpa-supplicant package. `systemd-networkd` (DHCP) must be enabled in the image.
- **Interface on tty1.** `clorela-ui` should draw over the text console without the cursor showing through. Check `TTYPath` and `QT_QPA_FB_HIDECURSOR`.
- **Splash.** The logo should be centred, the bar should be green and advance with each service, and the splash should close when the interface opens.
- **Resolution.** `hdmi_group=1`, `hdmi_mode=16` with the legacy framebuffer should give 1920×1080 on your monitor.
- **Shield buses.** Check that `/dev/i2c-1` and `/dev/spidev0.0` exist. Then run `i2cdetect -y 1`: nodes appear at 0x10–0x13 and the display at 0x3C/0x3D.
- **Disk space.** The root partition has 512 MB extra for the database. Growing it to the whole card on first boot is still to do.
