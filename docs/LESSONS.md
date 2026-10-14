# Hard-won lessons

Each entry: symptom, cause, fix. All of these cost real time on the original
bring-up.

## adb shell cat > file corrupts binaries

* **Symptom:** a "backup" of the boot partition (and a kernel built from it)
  does not boot; checksums differ from the source; no error anywhere.
* **Cause:** `adb shell` allocates a pty on old adb; the line discipline turns
  every `\n` into `\r\n` in the stream.
* **Fix:** never redirect binary data through `adb shell`. Write the file on
  the phone and `adb pull` it. Compare checksums computed **on the phone**
  (`busybox dd if=FILE | busybox sha256sum`, or `sha256sum FILE`) with the host
  side. Verify a kernel by checking the Android header (`ANDROID!`) and
  decompressing its payload, not by file size.

## Raw YAFFS2 images do not mount

* **Symptom:** a `mkyaffs2` image flashed to userdata is not mountable or
  empty.
* **Cause:** Android's `flash_image` ignores OOB data, and mkyaffs2's OOB
  layout does not match this NAND anyway.
* **Fix:** let the phone's own YAFFS2 driver write the filesystem: in
  recovery `erase_image userdata`, mount it as yaffs2, `tar -x` the rootfs.
  `scripts/bravo-image` produces that tar (`userdata.tar`).

## erlinit -s is not "run before erlexec"

* **Symptom:** boot loop right after erlinit starts.
* **Cause:** `-s` is `--alternate-exec`. erlinit runs that program **instead
  of** erlexec and passes erlexec and its arguments as `$1...`.
* **Fix:** the script (`usr/libexec/bravo-init`) must finish with
  `exec "$@"`.

## erlinit only works as PID 1 and treats SIGTERM as reboot

* **Symptom:** running `erlinit` by hand refuses to start; sending it SIGTERM
  reboots the phone.
* **Cause:** by design.
* **Fix:** to test, boot with `init=/sbin/bravo-boot` (debug image,
  `BRAVO_DEBUG=1`), which does its own setup and `exec`s `/sbin/init`.
  `--ctty` redirects erlinit's stderr to the tty after startup, so logs may
  vanish from where you expect them.

## BusyBox dispatches on argv[0]

* **Symptom:** a copied BusyBox prints "applet not found" or behaves like the
  wrong tool.
* **Cause:** it picks the applet from the name it was invoked by.
* **Fix:** a renamed multi-call binary must be named `busybox` (or the applet
  name); see `bravo_hello/patch/files/dbgbb` being installed as
  `.../dbg/busybox`.

## glibc programs die with "Function not implemented" (Error 38)

* **Symptom:** ENOSYS from binaries built with the standard Nerves glibc
  toolchain.
* **Cause:** the 2.6.37 kernel lacks syscalls that a modern glibc uses.
* **Fix:** musl userland with a local Buildroot toolchain
  ([BUILDING.md](BUILDING.md)).

## No devtmpfs, no squashfs, no fbcon in the kernel

* **Symptom:** missing `/dev` nodes; squashfs rootfs cannot mount; no text
  on screen.
* **Cause:** the stock CyanogenMod 2.6.37 kernel config.
* **Fix:** tmpfs `/dev` populated by `mdev -s` in `bravo-init`, YAFFS2 rootfs,
  and USB networking instead of a console. A different kernel was tried and
  dropped: [KERNEL-EXPERIMENT.md](KERNEL-EXPERIMENT.md).

## Boot partition limit: 3 MB

* **Symptom:** `nandwrite`/`flash_image` fail or the image is truncated.
* **Cause:** mtd2 is 3 MB.
* **Fix:** the stock kernel plus an empty ramdisk is ~1.8 MB. Do not put a
  ramdisk with a full userland in it. `bravo-flash-boot` rejects larger
  images.

## flash_image ignores OOB; verify what you wrote

* **Symptom:** "successful" flashes that do not boot.
* **Fix:** always read the partition back and compare sha256
  (`bravo-flash-boot` does; for recovery see [INSTALL.md](INSTALL.md) step 6).

## `reboot` in `adb shell` reboots the phone, not the host

Obvious, but with several shells open it is easy to lose track of which one
you are in. Commands marked "on the phone" run there.

## Recovery has root adb; the running system has none

* Recovery: `adb shell` is root. Use it for installs and rollbacks.
* Running Nerves: no adbd; use SSH. NervesSSH on this system has **no
  sftp**; `scp`/`sftp` fail. `ssh host '<elixir expression>'` runs an
  expression, and the expression can read stdin with `IO.gets("")`, which is
  how `bravo-deploy`/`bravo-flash-boot` stream base64 data.
* To enter recovery/HBOOT: Volume Down + Power, or catch a reboot loop.

## USB networking needs a DHCP profile on the host

* **Symptom:** `usb0` appears on the host but there is no address, no route.
* **Cause:** the phone runs VintageNetDirect (a DHCP server) and expects the
  host to be a DHCP client.
* **Fix:** [INSTALL.md](INSTALL.md) step 8.

## Wi-Fi on the Android bcm4329 driver

* **Symptoms and causes in order of appearance:** `SCAN-FAILED ret=-1` (the
  driver rejects the standard scan: use `ap_scan=2`), then
  `Could not find AP from the scan results` followed by a **misleading**
  `WRONG_KEY` (the driver's scan list must be filled by a private `CSCAN` scan
  before associating; the key was fine, as the verified MIC of message 3 showed),
  then no IP address (the stock udhcpc `default.script` hangs).
* **Method that found it:** run `wpa_supplicant -dd` by hand for 25 s and read
  only the lines after `RX EAPOL`. Message 1 arriving, message 2 sent, message 3
  decrypted means the radio path and the key are fine and the failure is in
  wpa_supplicant's own checks.
* **Do not trust `WRONG_KEY`** on drivers like this one; look at where the
  handshake actually stops.
* Details and the working recipe: [WIFI.md](WIFI.md).

## Environment quirks

* The system's `mix.exs` sets `TARGET_ARCH=armv7`, `TARGET_ABI=musleabihf`;
  Rust targets then resolve to `armv7-unknown-linux-musleabihf`.
* The Nerves `build_path_link` of the toolchain stub is a symlink you create
  by hand ([BUILDING.md](BUILDING.md)).
* This BusyBox has no `tr` or `head`; `ifconfig` cannot show status. Do not rely on
  them in scripts you run on the phone.
* Moving the project directory breaks a built system artifact (absolute paths);
  see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).
