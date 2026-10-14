# nerves_system_bravo

A custom [Nerves](https://nerves-project.org) system for the **HTC Desire**
(codename `bravo`, Qualcomm QSD8250, ARMv7 + NEON, ~415 MB RAM, 2010).

It boots the phone straight into the BEAM (Erlang/OTP 29) with erlinit as
PID 1, SSH over USB networking, and nothing else. **It replaces Android.**
Read [Before you start](#before-you-start) before you touch a device.

This is an experimental hobby port. Version 0.1.0. It was developed and tested
on one phone. There are no prebuilt artifacts: you build everything locally.

## Status

| Area | State |
|------|-------|
| Boot (stock kernel, erlinit, BEAM, shoehorn) | works |
| USB networking (RNDIS, VintageNetDirect) + SSH (NervesSSH) | works |
| Battery, buttons, joystick, touchscreen, LEDs, light/proximity | work; example application code in `../bravo_hello` |
| Display | works from userspace (legacy `msmfb` fbdev, RGB565, mmap + pan); no console on screen |
| Wi-Fi (BCM4329) | works: WPA2-PSK via `wpa_supplicant -Dwext` + `udhcpc` (the app starts them), vendor driver files from your phone; see [docs/WIFI.md](docs/WIFI.md). VintageNetWiFi (nl80211) does not work |
| Camera, Bluetooth, audio, GPS, modem | not supported / untested |
| Over-the-air firmware updates (fwup) | not possible: fwup cannot write this NAND. Use `scripts/bravo-deploy` |

Details: [docs/HARDWARE.md](docs/HARDWARE.md) (older) and the project writeup `../README.md` (authoritative).

## Why this system is unusual

* **Stock kernel, no kernel build.** It reuses the CyanogenMod 2.6.37.6 kernel
  from the phone itself. That kernel has no devtmpfs, no squashfs and no
  fbcon. The boot image is the phone's kernel plus an empty ramdisk and a new
  command line (`root=/dev/mtdblock5 rootfstype=yaffs2 rw init=/sbin/init ...`).
  You must supply that kernel from your own phone; it is not in this repo.
* **musl userland, local toolchain.** glibc binaries from the standard Nerves
  toolchain fail with `ENOSYS` (Error 38) on this old kernel. Userland is built
  with a Buildroot GCC 14 / musl toolchain (kernel headers 5.10) that you build
  once locally.
* **YAFFS2 root filesystem, no squashfs.** The rootfs is a read-write YAFFS2
  on the `userdata` NAND partition. It cannot be written as a raw image; it is
  populated from recovery with `tar` (see [docs/INSTALL.md](docs/INSTALL.md)).
* **`/dev` by mdev.** `usr/libexec/bravo-init` (run by erlinit `-s`) mounts a
  tmpfs on `/dev` and runs `mdev -s`.

Boot flow and partition map: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Before you start

This project **overwrites the boot and userdata partitions** of the phone.
After installation Android no longer boots. You can go back, but only if you
prepared for it.

* **Make your own backups first**: the boot partition and `/data`. Do not use
  `adb shell cat > file` for binaries: it silently corrupts them (see
  [docs/LESSONS.md](docs/LESSONS.md)). Procedure in
  [docs/INSTALL.md](docs/INSTALL.md).
* **You need a rooted phone with a custom recovery** that provides adb as root,
  `flash_image`, `erase_image` and a yaffs2-capable mount. The system was
  developed against CyanogenMod 7 (2.6.37.6) and ClockworkMod recovery. Other
  ROMs/kernels are untested (unverified whether they work).
* **Know how to reach recovery/HBOOT** (Volume Down + Power) before flashing.
  That is your only way back if the new system does not boot.
* **Risks, honestly:** a bad write to the boot partition can leave the phone
  unbootable until you reflash it from the bootloader/recovery; a wrong
  partition number can destroy `/system` or recovery; an interrupted NAND
  write can leave bad blocks. This project deliberately never touches
  `/system` (mtd3) or recovery (mtd1), which keeps recovery possible, but it
  cannot rule out a hard brick, and the authors have not tested every ROM or
  hardware revision.
* The debug tooling in `diag/` and `bravo_hello/patch/` opens
  **unauthenticated root shells** over USB. Never leave it installed.
* **No warranty.** See the license notes below.

## Quick start (high level)

1. Back up boot and `/data`; dump the phone's kernel image into
   `board/boot-stock.img` ([INSTALL.md](docs/INSTALL.md)).
2. Install the host tools and build the Buildroot toolchain once
   ([BUILDING.md](docs/BUILDING.md)).
3. Build the system and a firmware (see [`../bravo_hello`](../bravo_hello)).
4. `scripts/bravo-image firmware.fw out/` produces `boot.img` and
   `userdata.tar`.
5. In recovery: `erase_image userdata`, mount as yaffs2, extract the tar;
   `flash_image boot boot.img` ([INSTALL.md](docs/INSTALL.md)).
6. Boot, give the host a DHCP profile for the `rndis_host` interface, `ssh` in.

## Repository layout

```
mix.exs, VERSION        Nerves system package (platform: Nerves.System.BR)
nerves_defconfig        Buildroot config (external musl toolchain)
busybox.fragment        BusyBox additions (mdev, insmod, modprobe, ...)
                        (nerves_defconfig also builds wpa_supplicant, wext only)
fwup.conf               carrier for the rootfs only; fwup cannot write this NAND
post-build.sh           (empty hook)
post-createfs.sh        copies board/boot-stock.img and scripts/ into the artifact
rootfs_overlay/         erlinit.config, mdev.conf, usr/libexec/{bravo-init,bravo-udhcpc}
scripts/                bravo-image, bravo-deploy, bravo-flash-boot, mkbootimg.py,
                        extract-kernel.py, bravo-boot (debug PID 1)
toolchain/              Buildroot defconfig for the musl toolchain
diag/                   diagnostic ramdisk: unauthenticated shells, bring-up only
board/                  put boot-stock.img here (your phone's kernel; git-ignored)
docs/                   documentation
```

Sibling repos expected by the path dependencies:

* `../nerves_toolchain_bravo`: stub Nerves toolchain package. The real
  toolchain is built with Buildroot and carried in the system artifact.
* `../bravo_hello`: example firmware (`MIX_TARGET=bravo`).

Not part of the release: the abandoned kernel experiment
(see [docs/KERNEL-EXPERIMENT.md](docs/KERNEL-EXPERIMENT.md)) and any personal
backups.

## Hardware support

Short version in the status table; full matrix with sysfs/evdev paths and
what userland is missing in [docs/HARDWARE.md](docs/HARDWARE.md).

## Known limitations

* Display stays on the HTC splash; no console on screen (no fbcon).
* Wi-Fi is WPA2-PSK (AES) only, tested on one access point; no camera, Bluetooth, audio, GPS, modem.
* Updates need a custom flow (`bravo-deploy`, `bravo-flash-boot`); no fwup,
  no A/B, no rollback partition.
* Boot partition is limited to 3 MB.
* Needs the phone's own kernel; only the CyanogenMod 2.6.37.6 kernel was used.
* Single core, ~415 MB RAM: the example uses `+S 1:1`, a 60 s heart timeout.
* Built and tested on one Linux x86_64 host and one phone.
* The `scripts/` tools use `StrictHostKeyChecking=no` on the USB link
  (point-to-point, but not authenticated).

## Updating a running device

`scripts/bravo-deploy` syncs changed files over SSH; `scripts/bravo-flash-boot`
flashes the boot partition with verification. Recovery remains the fallback.
See [docs/UPDATING.md](docs/UPDATING.md).

## Rolling back to Android

Flash your own boot image backup, erase userdata, extract your `/data` backup.
`/system` and recovery are untouched. Steps in
[docs/INSTALL.md](docs/INSTALL.md#rolling-back-to-android).

## License and third-party notices

The original files in this repo are MIT-licensed, see [LICENSE](LICENSE). This
does not cover the third-party components below.
Components and their licenses (summary, not legal advice):

* Nerves, `nerves_system_br`: Apache-2.0 plus Buildroot-derived parts.
* Buildroot and the packages it builds (BusyBox, mtd-utils, musl, GCC,
  binutils, Linux headers, ...): mostly GPL-2.0 / MIT / others. Redistributing
  built images or the toolchain tarball needs the corresponding sources and
  notices; `make legal-info` output ends up in the system artifact's
  `legal-info/`. This repo does not distribute binaries.
* The phone's kernel: GPL-2.0. Not distributed here; you dump it from your own
  device.
* Broadcom BCM4329 firmware and `bcm4329.ko`: proprietary/vendor, **never
  distributed here**; copy them from your own phone.
* `diag/font8x16.h`: generated from `Lat15-Fixed16.psf` (from the Linux
  console fonts, Debian `console-setup-linux`). Debian states its console fonts
  are public domain, and `Fixed` derives from the X11 misc-fixed fonts, which
  are public domain as far as known; the exact BDF source was not verified.

## Credits

* The [Nerves project](https://nerves-project.org) and `nerves_system_br`.
* [Buildroot](https://buildroot.org).
* yaffs2utils (`mkyaffs2`), used while finding out that raw images do not work.
* postmarketOS and spezi77 for the QSD8250 / 3.0.16 kernel research used in the
  abandoned kernel experiment.
* erlinit, NervesSSH, VintageNet and the rest of the Nerves ecosystem.

## Contributing

Issues and pull requests are welcome, especially: other ROM/kernel
combinations, other Wi-Fi security modes, and portability fixes. Please say which phone
revision and ROM you tested on, and never attach backups, firmware blobs or
kernel images.
