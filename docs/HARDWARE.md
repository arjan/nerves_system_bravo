# Hardware

Findings come from probing a running phone with this system (stock 2.6.37.6
kernel). "Kernel exposes it" means the device exists in sysfs/evdev; this repo
ships **no** application code that uses any of it. Event numbers
(`/dev/input/eventN`) vary: look up devices by name in
`/proc/bus/input/devices`.

| Hardware | State | Interface | Missing |
|----------|-------|-----------|---------|
| CPU/RAM | works | QSD8250, ARMv7 cortex-a8 + NEON, ~415 MB RAM, one core | |
| NAND | works | YAFFS2 on mtd2/4/5 | fwup support (cannot write this NAND) |
| USB gadget | works | RNDIS via `/sys/class/usb_composite/rndis/enable` -> `usb0` | other gadget functions untested |
| Display | works from userspace | 480x800 AMOLED, legacy `msmfb` fbdev (`/dev/fb0`), 16 bpp RGB565, no DRM, no fbcon; mmap + `FBIOPAN_DISPLAY` works (`bravo_hello` draws a UI this way) | no text console; the screen stays on the HTC splash until userspace draws |
| Battery | kernel exposes | `/sys/class/power_supply/battery/` (capacity, voltage, current, temp, status; exact file names: `ls` the directory) | monitoring app |
| Buttons | kernel exposes | evdev `bravo-keypad`: home, menu, back, search, volume up/down, power | input handler app |
| Optical joystick | kernel exposes | evdev `curcial-oj`: REL x/y, direction keys, click | input handler app |
| Touchscreen | kernel exposes | evdev `synaptics-rmi-touchscreen` (multitouch) | input handler app; calibration untested |
| LEDs | kernel exposes | `/sys/class/leds/*` incl. flashlight and button backlight | |
| Light, proximity | works | enable via `/dev/lightsensor` (ioctl `_IOW('l',2,int*)`) and `/dev/cm3602` (`_IOW('c',2,int*)`); evdev `lightsensor-level` (ABS_MISC level 0-9, only on change) and `proximity` (ABS_DISTANCE 0 = near, 1 = far; a comparator, not a distance) | see `bravo_hello/c_src/sensors.c` |
| Compass, accelerometer | present as input devices | akm8973, bma150; not read | userland |
| Wi-Fi (BCM4329) | works | vendor `bcm4329.ko` + firmware create `wlan0`; WPA2-PSK via `wpa_supplicant -Dwext` (built into the system), `udhcpc`, private-ioctl scanning | see [WIFI.md](WIFI.md); the app starts it |
| Camera | not supported | sensor S5K3E2FX is probed by the kernel | legacy `msm_camera` needs HTC's proprietary `libmmcamera` |
| Bluetooth, audio, GPS, modem | untested, not supported | | everything |

Notes:

* The stock kernel has no fbcon, so no text console on the screen. Use
  `diag/fbdmesg` (bring-up only) to put kernel log text on the display.
* `/dev` is built by mdev. If `/dev/fb0` is missing,
  `mknod /dev/fb0 c 29 0` (as `scripts/bravo-boot` does).
* The LCD may keep the splash and the backlight on; power saving was not
  investigated.

## Wi-Fi

Wi-Fi works with WPA2-PSK; the full recipe, the file locations on the phone
(`/system/lib/modules/bcm4329.ko`, `/system/vendor/firmware/fw_bcm4329.bin`),
the required `wpa.conf` and the reasons behind each workaround are in
[WIFI.md](WIFI.md). Short version: the driver is a legacy wext driver that
rejects the standard scan call, so `wpa_supplicant` runs with `ap_scan=2` and a
private-ioctl scan (`CSCAN`) must run before associating. Redistribution: do not
publish an image or tarball that contains the vendor files.

## Display / input hints for application authors

* Framebuffer: open `/dev/fb0`, query `FBIOGET_VSCREENINFO`/`FSCREENINFO`
  first. The stock driver's mode on this kernel was not documented here; the
  diagnostic tool `diag/fbdmesg.c` uses RGB565 with yoffset flipping, which
  worked for it.
* The `bravo_hello` Emerge/Skia UI that tries to present to `/dev/fb0` is work
  in progress and not part of the release.
