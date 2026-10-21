SUMMARY = "Clorela-OS image for the WALCORG photobioreactor node"
DESCRIPTION = "Raspberry Pi Zero W with the WALCORG shield: boot splash, \
full-screen Qt 6 interface on HDMI, acquisition service for the nodes, \
Wi-Fi and the tools needed to diagnose the shield buses."
LICENSE = "MIT"

inherit core-image

# "splash" brings psplash (WALCORG logo and progress bar, see
# recipes-core/psplash).
IMAGE_FEATURES += "splash"

IMAGE_INSTALL = " \
    packagegroup-core-boot \
    ${CORE_IMAGE_EXTRA_INSTALL} \
    clorela-apps \
    clorela-ui \
    wpa-supplicant \
    iw \
    linux-firmware-rpidistro-bcm43430 \
    i2c-tools \
    socat \
    tzdata \
    ca-certificates \
"

# Release artefacts: compressed image for Raspberry Pi Imager, bmap for
# bmaptool and a SHA-256 checksum for the GitHub release.
IMAGE_FSTYPES = "wic.xz wic.bmap wic.xz.sha256sum"

# Room for the measurement database on the root partition (in KiB).
# [TODO] grow the root partition to the whole card on first boot.
IMAGE_ROOTFS_EXTRA_SPACE = "524288"

