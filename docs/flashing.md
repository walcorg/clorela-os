# Install Clorela-OS on a microSD card

This guide is based on WALCORG's *Tutorial 1/5: How to install Clorela-OS on a microSD* (4th generation node technical documentation). The original PDF is kept in [`tutorials/1-of-5-install-clorela-os-on-a-microsd.pdf`](tutorials/1-of-5-install-clorela-os-on-a-microsd.pdf). It is the alpha test version from July 2025 and shows the 2021 image.

## What changed since the tutorial

| Tutorial (alpha, 2021 image) | Now |
|---|---|
| Image `core-image-sato-raspberrypi0-wifi-…rootfs.rpi-sdimg` (480 MB) on Google Drive, linked from walcorg.com → DOWNLOADS | `clorela-image-raspberrypi0-wifi.wic.xz` on [GitHub Releases](https://github.com/walcorg/clorela-os/releases), with a `.wic.bmap` file and a SHA-256 checksum |
| Selected as `Clorela OS v1.zip` in Raspberry Pi Imager | Raspberry Pi Imager opens `.wic.xz` directly; no need to unzip |
| Name "Chorella OS" | **Clorela-OS** |
| "Powered by Google Compute Engine" | Built locally with Yocto on WALCORG's build server |

The image built from this repository is not published yet: it comes with the Yocto phase. Until then, the alpha image on walcorg.com is the only one available. It does **not** contain the software in this repository.

## You need

- Raspberry Pi Zero W
- microSD card from a known brand (SanDisk or Panasonic recommended)
- microSD reader or adapter for your computer
- 5.1 V / 3 A power supply. Do not power the Pi from a computer USB port.
- A computer running Windows, macOS or Ubuntu, with internet access

## Steps

1. **Download the image.** Download it from [Releases](https://github.com/walcorg/clorela-os/releases). Optionally, check its integrity:
   ```bash
   sha256sum -c clorela-image-raspberrypi0-wifi.wic.xz.sha256
   ```
2. **Download Raspberry Pi Imager** from <https://www.raspberrypi.com/software/> (Download for Windows / macOS / Ubuntu for x86). On Raspberry Pi OS you can run `sudo apt install rpi-imager` instead.
3. **Install it.** Open your downloads folder, double-click the installer and click **Install**.
4. **Open Raspberry Pi Imager.**
5. **Connect the microSD card** to the computer, either directly or through an adapter or card reader.
6. Click **CHOOSE OS**.
7. Go to the bottom of the list and click **Use custom** ("Select a custom .img from your computer").
8. **Select the Clorela-OS image** among your downloaded files and click **Open**. Its name replaces CHOOSE OS on the button.
9. Click **CHOOSE STORAGE** and select the **microSD card**. Check the size: every other drive on the list will be erased if you pick it by mistake.
10. Click **WRITE** and confirm. Wait until Raspberry Pi Imager finishes writing and verifying.
11. **Remove the microSD card** from the computer and insert it into the Raspberry Pi Zero W.

On Linux you can also write the card from the command line with `bmaptool`, which is faster and checks the data:

```bash
sudo bmaptool copy clorela-image-raspberrypi0-wifi.wic.xz /dev/sdX   # sdX = your card, check with lsblk
```

## First boot

1. Mount the Pi on the shield and connect the nodes to the shield's USB-A node ports.
2. Connect a monitor to the Pi's mini-HDMI port.
3. Power everything through the **USB-C input of the shield**.
4. The shield display shows **BOOTING**, then **READY**, and the interface opens full screen.

### Wi-Fi

Acquisition needs Wi-Fi. The **Configure → Configure Wi-Fi** window is not implemented yet. How the network is pre-configured on the card (SSID and password) will be documented together with the Yocto image. Without Wi-Fi, Clorela-OS starts in **restricted mode**: the interface can be browsed, and acquisition and account functions stay disabled.

## Common issues with the Raspberry Pi Zero

All Raspberry Pi boards are tested before they leave the factory, so a faulty Pi is rare. The most common reasons for a Pi Zero not to work as expected:

- **No lights at all.** 99 % of the time the microSD card is not being recognised. Unlike other Raspberry Pi models, **the Zero shows no light when no card is installed or detected**. Take the card out and refit it. If it still does not work, try another card or write a fresh copy of the image.
- **Power supply.** The Pi Zero does not need much power, but a poor supply or cable causes endless problems. Avoid USB ports on computers; use a 5.1 V / 3 A supply.
- **Poor-quality microSD card.** Cheap "no name" cards are a false economy. Use Panasonic or SanDisk.
- **Corrupted card.** Cards sometimes get corrupted no matter how careful you are. Write the image again with Raspberry Pi Imager; it takes a few clicks.
- **Status LED codes.** The green LED tells you a lot. Irregular flicker is normal card activity. A repeating pattern is an error code:

| Flashes | Meaning | What to do |
|---|---|---|
| 3 | `start.elf` not found | Write the image again |
| 4 | `start.elf` damaged | Write the image again |
| 7 | Kernel image not found | Write the image again |
| 8 | SDRAM not detected | Write the image again; if it persists, the board's RAM is likely damaged |

> The original tutorial lists 8 flashes as "corrupted SD card". On the Pi Zero, 8 flashes means the SDRAM was not detected. A bad image can cause it, but so can damaged RAM, so re-flashing is the first step but not always the fix.

## More

- How Clorela-OS was made (video series in Spanish): <https://youtu.be/jvU4fKmdEkM>
- Raspberry Pi Imager: <https://www.raspberrypi.com/software/>
