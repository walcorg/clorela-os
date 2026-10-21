# WALCORG boot splash for Clorela-OS (HDMI, 1920x1080):
#   - white background, WALCORG TERRAFORMING CORPORATION logo
#   - simple green progress bar below it, driven by systemd (psplash-systemd)
# The interface closes it with "psplash-write QUIT" when it starts.
#
# Layout (psplash.c): the logo is centred above the "split line" at 5/6 of the
# screen height; the bar sits on that line. Source images:
#   files/psplash-clorela.png  logo on white (507x396)
#   files/psplash-bar.png      bar frame; psplash fills it 4 px inside
# The original logo is kept in data/branding/ at the repository root.

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# ":rpi" so it wins over meta-raspberrypi's own SPLASH_IMAGES:rpi (its
# Raspberry Pi logo). "default" makes it the psplash the image installs.
SPLASH_IMAGES = "file://psplash-clorela.png;outsuffix=default"
SPLASH_IMAGES:rpi = "file://psplash-clorela.png;outsuffix=default"

SRC_URI += " \
    file://psplash-colors.h \
    file://psplash-bar.png \
"

# Logo centred in the area above the bar instead of the whole screen.
PACKAGECONFIG:remove = "fullscreen"

do_configure:prepend() {
    # Colours.
    cp ${WORKDIR}/psplash-colors.h ${S}/psplash-colors.h
    # Bar frame: regenerate the C header the same way upstream does.
    (cd ${WORKDIR} && sh ${S}/make-image-header.sh ${WORKDIR}/psplash-bar.png BAR)
    cp ${WORKDIR}/psplash-bar-img.h ${S}/psplash-bar-img.h
}
