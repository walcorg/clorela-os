SUMMARY = "Clorela-OS applications: acquisition service, interface and status display"
DESCRIPTION = "clorela-acqd reads the photobioreactor nodes over I2C and stores the \
measurements; clorela-ui is the full-screen Qt 6 interface on HDMI; \
clorela-status writes messages on the shield's OLED display."
HOMEPAGE = "https://github.com/walcorg/clorela-os"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=a5f99c0752e0bebe1789b6a559648c65"

# The sources are this repository itself (the layer is inside it), so the
# recipe builds the checked-out tree directly: what is committed is what is
# built, with no separate SRCREV to keep in sync.
inherit externalsrc
EXTERNALSRC = "${CLORELA_SRC_DIR}"
EXTERNALSRC_BUILD = "${WORKDIR}/build"

inherit qt6-cmake systemd useradd

DEPENDS = "qtbase qttools-native"

EXTRA_OECMAKE += "-DCLORELA_BUILD_TESTS=OFF"

SRC_URI = " \
    file://clorela-acqd.service \
    file://clorela-ui.service \
    file://clorela-boot-status.service \
    file://clorela-wifi.service \
    file://clorela-wifi-setup \
    file://60-clorela.rules \
    file://20-wlan0.network \
    file://wpa_supplicant.conf.example \
"
# externalsrc ignores remote SRC_URI entries but still fetches file:// ones.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    for unit in clorela-acqd.service clorela-ui.service clorela-boot-status.service clorela-wifi.service; do
        install -m 0644 ${WORKDIR}/${unit} ${D}${systemd_system_unitdir}/
    done

    install -d ${D}${libexecdir}/clorela
    install -m 0755 ${WORKDIR}/clorela-wifi-setup ${D}${libexecdir}/clorela/

    install -d ${D}${nonarch_base_libdir}/udev/rules.d
    install -m 0644 ${WORKDIR}/60-clorela.rules ${D}${nonarch_base_libdir}/udev/rules.d/

    install -d ${D}${systemd_unitdir}/network
    install -m 0644 ${WORKDIR}/20-wlan0.network ${D}${systemd_unitdir}/network/

    install -d ${D}${datadir}/clorela
    install -m 0644 ${WORKDIR}/wpa_supplicant.conf.example ${D}${datadir}/clorela/
}

# Recipe name: not "clorela", which is the distro name and therefore an
# override (OVERRIDES); a package with that name would break :clorela vars.
#
# clorela-apps : acquisition service, status display, configuration, Wi-Fi setup
# clorela-ui   : the interface (needs fonts and the Qt platform plugins)
PACKAGES =+ "clorela-ui"

FILES:clorela-ui = " \
    ${bindir}/clorela-ui \
    ${systemd_system_unitdir}/clorela-ui.service \
"
FILES:${PN} += " \
    ${systemd_system_unitdir} \
    ${libexecdir}/clorela \
    ${nonarch_base_libdir}/udev/rules.d \
    ${systemd_unitdir}/network \
    ${datadir}/clorela \
"
CONFFILES:${PN} = "${sysconfdir}/clorela/clorela.conf"

# qtbase-plugins carries the SQLite driver (both programs) and the linuxfb
# platform plugin (interface).
RDEPENDS:${PN} = "qtbase qtbase-plugins wpa-supplicant"
RRECOMMENDS:${PN} = "kernel-module-i2c-dev kernel-module-spidev"
RDEPENDS:clorela-ui = "${PN} qtbase qtbase-plugins fontconfig ttf-dejavu-sans ttf-dejavu-common"

SYSTEMD_PACKAGES = "${PN} clorela-ui"
SYSTEMD_SERVICE:${PN} = "clorela-acqd.service clorela-boot-status.service clorela-wifi.service"
SYSTEMD_SERVICE:clorela-ui = "clorela-ui.service"
SYSTEMD_AUTO_ENABLE = "enable"

# System user that runs both programs. i2c and spi own the bus device nodes
# (see 60-clorela.rules); video and input give the interface the framebuffer,
# keyboard and mouse.
USERADD_PACKAGES = "${PN}"
GROUPADD_PARAM:${PN} = "-r i2c; -r spi"
USERADD_PARAM:${PN} = "--system --home-dir /var/lib/clorela --no-create-home \
    --shell /sbin/nologin --user-group --groups i2c,spi,video,input,tty clorela"
