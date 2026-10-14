# Installing

This replaces Android on the phone. Read the safety section in the
[README](../README.md#before-you-start) first. Commands that run on the phone
are marked; everything else runs on the host.

Tested only on one phone with a CyanogenMod 2.6.37.6 kernel and a recovery
with root adb. Anything marked (unverified) was not confirmed on hardware.

## 0. Prerequisites

* A rooted HTC Desire (bravo) running CyanogenMod 7 / kernel 2.6.37.6, with a
  custom recovery (ClockworkMod in the author's case) that gives you:
  root `adb shell`, `flash_image`, `erase_image`, `busybox` with yaffs2 mount.
* The host build from [BUILDING.md](BUILDING.md) (or at least `userdata.tar`
  and `boot.img`).
* Know how to enter the bootloader/recovery: Volume Down + Power, then choose
  Recovery. Practice this before flashing anything.
* A charged battery. A power loss during a NAND write is a real risk.

## 1. Check the partition layout

In recovery, with the phone connected:

```sh
adb shell cat /proc/mtd
```

You expect this (verified on the author's phone):

| mtd | name     | used by this system |
|-----|----------|---------------------|
| 1   | recovery | untouched |
| 2   | boot     | stock kernel + empty ramdisk |
| 3   | system   | untouched (Android) |
| 4   | cache    | `/root` application data (YAFFS2) |
| 5   | userdata | rootfs (YAFFS2, read-write `/`) |

**If your layout differs, stop.** The numbers are hard-coded in
`scripts/bravo-image`, `rootfs_overlay/etc/erlinit.config` and the commands
below.

## 2. Back up (mandatory) and dump your kernel

Never capture binaries with `adb shell cat > file` or any shell redirection
through adb's pty: it converts `\n` to `\r\n` and silently corrupts the file
(see [LESSONS.md](LESSONS.md)). Write the file on the phone, then `adb pull`
it, and compare checksums on both sides.

Scratch space: in the author's recovery `/sdcard` was a RAM disk (unverified
for yours). Check `adb shell df`.

Boot partition (also gives you the kernel for the build). Device node names
below follow Android's `/dev/mtd/` layout (unverified on your recovery; list
`/dev/mtd/` first):

```sh
adb shell ls /dev/mtd/
adb shell "dd if=/dev/mtd/mtd2ro of=/sdcard/boot-mtd2.img bs=2048"
adb shell "sha256sum /sdcard/boot-mtd2.img"
adb pull /sdcard/boot-mtd2.img .
sha256sum boot-mtd2.img                       # must equal the phone's sum
head -c 8 boot-mtd2.img; echo                 # must print ANDROID!
stat -c %s boot-mtd2.img                      # 3145728 on the author's phone
```

Keep `boot-mtd2.img` somewhere safe. This is your rollback image. Then place
a copy where the build expects it:

```sh
cp boot-mtd2.img nerves_system_bravo/board/boot-stock.img
```

(`board/*.img` is git-ignored. Do not publish it: it is your vendor's kernel
build.) Optional sanity check that the kernel inside is intact:

```sh
python3 -I nerves_system_bravo/scripts/extract-kernel.py boot-mtd2.img /tmp/zImage
```

Then confirm the payload decompresses (the zImage embeds a compressed
kernel; locating and decompressing it is how the author verified a boot image;
there is no helper script for it yet).

`/data` (your Android apps and settings), two options:

* On the phone, mount `/data` in recovery and tar it to a place with enough
  space (an external SD card, not a RAM disk), then `adb pull` it and compare
  sums. Restoring this tar is how the author's rollback is done
  (unverified for other setups):
  ```sh
  adb shell mount /data
  adb shell "tar -c -p -f /sdcard/data-backup.tar -C /data ."
  adb shell "sha256sum /sdcard/data-backup.tar"
  adb pull /sdcard/data-backup.tar .
  sha256sum data-backup.tar
  ```
* Or a CWM nandroid backup to the SD card (not used with this procedure;
  unverified).

`/system` and recovery are never written by this procedure. Still, a
`/proc/mtd`-level dump of them is cheap insurance if you have the space.

## 3. Build the images

See [BUILDING.md](BUILDING.md#5-images-for-the-phone):

```sh
STOCK_BOOT=nerves_system_bravo/board/boot-stock.img \
  nerves_system_bravo/scripts/bravo-image bravo_hello.fw out/
sha256sum out/boot.img out/userdata.tar
```

## 4. Copy the images to the phone (in recovery)

```sh
adb push out/userdata.tar /sdcard/userdata.tar
adb push out/boot.img /sdcard/boot.img
adb shell "sha256sum /sdcard/userdata.tar /sdcard/boot.img"
```

The sums must match the host's. If not, do not continue.

## 5. Write the root filesystem (YAFFS2 via tar)

Raw YAFFS2 images (`mkyaffs2`) do **not** work: Android's `flash_image`
ignores OOB data and mkyaffs2's OOB layout does not match this NAND. The
phone's own YAFFS2 driver must write the tags and ECC, so the filesystem is
created by erasing, mounting and extracting a tar.

On the phone (`adb shell` in recovery). This erases all of userdata:

```sh
cat /proc/mtd                      # re-check: mtd5 is "userdata"
erase_image userdata
mkdir -p /nroot
busybox mount -t yaffs2 /dev/block/mtdblock5 /nroot
busybox tar -x -p -f /sdcard/userdata.tar -C /nroot   # exact flags unverified
sync
ls -l /nroot/sbin/init /nroot/usr/libexec/bravo-init /nroot/dev/console
busybox umount /nroot
```

The `mount -t yaffs2 /dev/block/mtdblock5` form is the one the author's patch
scripts used. The tar contains device nodes and must be extracted as root.

## 6. Flash the boot image

Do this last, so a failed rootfs step leaves the old boot image in place.

```sh
# on the phone, in recovery
flash_image boot /sdcard/boot.img
PAGES=$(( $(busybox stat -c %s /sdcard/boot.img) / 2048 ))
sha256sum /sdcard/boot.img
busybox dd if=/dev/mtd/mtd2ro bs=2048 count=$PAGES 2>/dev/null | sha256sum
```

The two sums must be equal. If not, flash again; do not reboot with a
mismatching boot partition.

## 7. First boot

```sh
adb shell reboot
```

(`reboot` runs on the phone.) The stock kernel has no console on the screen:
it stays on the HTC splash. The BEAM starts and heart is armed (60 s timeout
in `bravo_hello`). Use USB networking to check.

## 8. Find the phone on USB and log in over SSH

`bravo-init` enables the RNDIS gadget; the phone shows up on the host as an
`rndis_host` network interface and runs a DHCP server (VintageNetDirect). The
host needs a DHCP client profile for that interface. With NetworkManager a
memory-only profile works:

```sh
nmcli connection add save no type ethernet con-name bravo-usb \
  match.driver rndis_host ipv4.method auto ipv6.method link-local
```

Other network managers: configure DHCP on the interface whose driver is
`rndis_host` (unverified for networkd and others). Then:

```sh
ip -br link                       # new enx... interface
ip -4 -br addr                    # host gets a /30 lease, e.g. 172.31.x.y/30
```

The phone is the other usable address in that /30. In the author's setup the
host got `.146/30` and the phone was `172.31.89.145`; yours will differ
(unverified how the subnet is chosen). `ip neigh show dev <iface>` also lists
it. Log in with the key configured in the firmware:

```sh
ssh <phone-ip>
ssh <phone-ip> 'IO.puts(File.read!("/proc/version"))'   # one-shot expression
```

NervesSSH here has no sftp, so `scp`/`sftp` will not work. Use
`scripts/bravo-deploy` instead ([UPDATING.md](UPDATING.md)). mDNS
(`nerves.local`) is configured in the example firmware but was not part of the
verification (unverified).

If nothing shows up, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## Rolling back to Android

You need your own `boot-mtd2.img` and `data-backup.tar` from step 2. In
recovery (`adb shell`):

```sh
cat /proc/mtd                      # re-check the layout
flash_image boot /sdcard/boot-mtd2.img
erase_image userdata
mkdir -p /nroot
busybox mount -t yaffs2 /dev/block/mtdblock5 /nroot
busybox tar -x -p -f /sdcard/data-backup.tar -C /nroot
sync
busybox umount /nroot
```

(`adb push` the two files to the phone first and compare sums. Restoring a
tar in this way is the author's procedure; exact tar flags and ownership
preservation are unverified for other backups.) `/system` and recovery were
never modified, so Android boots again. If you used a CWM nandroid backup,
restore it with CWM instead.

If the phone does not boot at all: Volume Down + Power for the bootloader,
then Recovery, and flash your boot image backup.
