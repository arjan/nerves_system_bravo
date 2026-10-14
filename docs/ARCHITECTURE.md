# Architecture

## Boot flow

```
HBOOT (HTC bootloader, untouched)
  |  loads the boot partition (mtd2): Android boot image header v0
  v
stock Linux 2.6.37.6 zImage (taken from the phone) + empty ramdisk
  cmdline: root=/dev/mtdblock5 rootfstype=yaffs2 rw init=/sbin/init
           no_console_suspend=1 msmsdcc_sdioirq=1 wire.search_count=5
  |  mounts YAFFS2 userdata (mtd5) read-write as /
  v
/sbin/init = erlinit (PID 1)           config: /etc/erlinit.config
  |  mounts /dev/mtdblock4 (cache) on /root as yaffs2, cgroups
  |  runs  -s /usr/libexec/bravo-init  INSTEAD of erlexec
  v
usr/libexec/bravo-init (sh)
  |  tmpfs on /dev, devpts, /dev/shm
  |  mdev as hotplug helper, mdev -s populates /dev
  |  echo 1 > /sys/class/usb_composite/rndis/enable   -> usb0
  |  exec "$@"   (= erlexec + args)
  v
erlexec -> BEAM (OTP 29), release in /srv/erlang, --boot shoehorn
  v
shoehorn -> nerves_runtime, vintage_net (VintageNetDirect on usb0), mdns_lite,
            NervesSSH, application
            (the application loads the Wi-Fi driver and supervises wpa_supplicant
             + udhcpc on wlan0; the system only ships the binaries, see WIFI.md)
```

Debug variant (`BRAVO_DEBUG=1`): `init=/sbin/bravo-boot` runs first, mounts
proc/sys, logs to `/boot-stdout.txt` and `/boot-dmesg.txt` on the rootfs,
enables RNDIS, then `exec /sbin/init`.

## Partition map

| mtd | name | content | written by |
|-----|------|---------|-----------|
| 0 | (not examined) | | nobody |
| 1 | recovery | CWM recovery | untouched |
| 2 | boot | stock kernel + empty ramdisk, max 3 MB | `flash_image` (recovery) or `bravo-flash-boot` |
| 3 | system | Android `/system` | untouched |
| 4 | cache | `/root` app data, YAFFS2 (40 MB) | the running system (formatted by yaffs2 on first mount) |
| 5 | userdata | rootfs, YAFFS2 | recovery `tar -x`; `bravo-deploy` for single files |

mtd0 was not examined; never write it.

## Image tooling

```
mix firmware (bravo_hello)           -> bravo_hello.fw  (squashfs rootfs only)
scripts/bravo-image fw outdir
   unzip data/rootfs.img
   unsquashfs + fakeroot: add /dev nodes, [bravo-boot], tar  -> userdata.tar
   extract-kernel.py  boot-stock.img  -> zImage
   empty cpio.gz as ramdisk
   mkbootimg.py (copies addresses + page size from stock) -> boot.img
recovery: erase_image userdata; mount yaffs2; tar -x userdata.tar
          flash_image boot boot.img
running:  bravo-deploy (files), bravo-flash-boot (boot partition)
```

`fwup.conf` exists only so Nerves can produce a `.fw`; its task writes the
squashfs to `/dev/mtdblock5` but is never used on the phone.

`post-createfs.sh` copies `board/boot-stock.img` and `scripts/` into the
system artifact's `images/`, so `bravo-image` run from there needs nothing
else.

## Components

* Buildroot (via `nerves_system_br` 1.34.4) builds the userland with an
  external musl toolchain from `toolchain/bravo_toolchain_defconfig`.
* BusyBox (`busybox.fragment`) adds mdev, insmod/lsmod/modprobe, mknod, chroot,
  expr, etc. `mtd-utils` provides `flash_erase`, `nanddump`, `nandwrite`.
* `diag/` is a separate ramdisk-only boot (`mkdiag-image`) for bring-up with an
  on-screen log (`fbdmesg`) and unauthenticated shells on TCP 2323/2324. It is
  dangerous to leave installed.
