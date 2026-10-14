# Changelog

## 0.1.0 (unreleased, initial release)

First public version of an experimental Nerves system for the HTC Desire
(bravo). Developed and tested on one phone only.

### What is in it

* Nerves system on `nerves_system_br` 1.34.4: Buildroot, external musl
  toolchain (GCC 14, kernel headers 5.10), OTP 29 userland (erlinit as PID 1,
  `--boot shoehorn`).
* Reuses the phone's own CyanogenMod 2.6.37.6 kernel; no kernel build. The
  kernel image is not distributed; users dump it from their own device.
* YAFFS2 rootfs on userdata (mtd5), app data on cache (mtd4); recovery and
  system partitions are not touched.
* `/dev` through tmpfs + mdev (`usr/libexec/bravo-init`), USB RNDIS enabled
  at boot, VintageNetDirect + NervesSSH usable from the host.
* Tools: `scripts/bravo-image` (boot.img + userdata.tar from a `.fw`),
  `scripts/bravo-deploy` (file sync over SSH), `scripts/bravo-flash-boot`
  (verified boot partition flash), `scripts/mkbootimg.py`,
  `scripts/extract-kernel.py`, debug PID 1 `scripts/bravo-boot`.
* `diag/`: diagnostic ramdisk for bring-up (unauthenticated shells; not for
  production).
* Wi-Fi: `wpa_supplicant` 2.12 (wext only) in `nerves_defconfig`, and
  `usr/libexec/bravo-udhcpc` (the stock udhcpc script hangs on this image).
  Recipe and findings in `docs/WIFI.md`.
* Documentation in `docs/`.

### What works (verified on hardware)

Boot, BEAM, USB networking, SSH, Wi-Fi (WPA2-PSK through `wpa_supplicant -Dwext`
and `udhcpc`, with the vendor driver files from the phone, one access point;
SSH then also works over the LAN), display output through the fbdev from
userspace, and kernel exposure of battery, buttons, joystick, touchscreen, LEDs,
light and proximity sensors.

### What does not

A text console on the display (no fbcon), Wi-Fi security other than WPA2-AES,
camera, Bluetooth, audio, GPS, modem. fwup updates. Prebuilt artifacts (none exist; everything builds locally).

### Known issues

* `scripts/bravo-deploy` and `scripts/bravo-flash-boot` default to the
  author's phone address and disable SSH host key checking.
* `board/boot-stock.img` is required for the build and must be supplied.
* The example firmware (`bravo_hello`) has unfinished Emerge UI path
  dependencies on the author's machine.
* See `RELEASE-CHECKLIST.md` for the pre-release audit.
