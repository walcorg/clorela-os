# config.txt for the Clorela-OS node (Raspberry Pi Zero W + shield v4).
# Pin facts: docs/hardware-shield-v4.md.

# HDMI: 1920x1080 at 60 Hz (CEA mode 16), even if the monitor is switched on
# after the Pi.
HDMI_FORCE_HOTPLUG = "1"
HDMI_GROUP = "1"
HDMI_MODE = "16"

# No rainbow square at power-on: the WALCORG splash is the first image.
DISABLE_SPLASH = "1"

# Node bus on the hardware I2C1 pins (GPIO2/GPIO3) -> /dev/i2c-1.
ENABLE_I2C = "1"

# USB in host mode for the FE1.1s hub on the shield.
ENABLE_DWC2_HOST = "1"

# TROPIC01 on SPI0 with its chip select on GPIO25 (header pin 22), which is
# not a hardware CE line: spi0-1cs moves CE0 to GPIO25 -> /dev/spidev0.0.
# [TO VERIFY] on an assembled shield. If the chip select moves to CE0 (GPIO8),
# replace this line with ENABLE_SPI_BUS = "1".
#
# [TO VERIFY] node-routine recommends a software I2C bus for the nodes
# (clock stretching). To use it, set ENABLE_I2C = "0" and add
#   dtoverlay=i2c-gpio,i2c_gpio_sda=2,i2c_gpio_scl=3,bus=3
# below, then set acquisition/i2c_bus=3 in /etc/clorela/clorela.conf.
RPI_EXTRA_CONFIG:append = "\n# TROPIC01 secure element (shield v4)\ndtoverlay=spi0-1cs,cs0_pin=25\n"
