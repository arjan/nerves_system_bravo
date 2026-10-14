# Troubleshooting

Symptoms we know of. If something is not listed, a bring-up image with
`diag/` can help, but it opens unauthenticated shells: see its warning.

## Phone stays on the HTC splash screen

Normal: the stock kernel has no fbcon. Check USB networking instead. If the
phone really does not run (no USB device appears, no heartbeat after about two
minutes), see the next entries.

## No new network interface on the host

* `dmesg -w` on the host while rebooting the phone: look for an RNDIS device
  (`rndis_host`).
* Try another cable/port; some hubs fail.
* `usb0` is enabled by `/usr/libexec/bravo-init`. If that script is not
  executable or does not end with `exec "$@"`, the VM never starts (boot
  loop) -- see below.

## Interface exists but no IP address

The host needs a DHCP client profile for the `rndis_host` driver. See
[INSTALL.md](INSTALL.md) step 8. Check with `ip -4 -br addr`.

## SSH: connection refused / permission denied

* Wait up to a couple of minutes after boot (slow CPU, BEAM start).
* The firmware only trusts the key configured at build time
  (`bravo_hello/config/target.exs`). Check `ssh -v`.
* There is no sftp: `scp` fails with a subsystem error. Use `ssh host
  '<elixir expression>'` or `scripts/bravo-deploy`.

## Boot loop

* Cause 1: `bravo-init` or another `-s` script does not `exec "$@"`.
* Cause 2: the BEAM crashes at start; heart (60 s timeout in the example)
  reboots.
* Cause 3: `/root` (cache partition) cannot be mounted.
* Diagnose: build with `BRAVO_DEBUG=1 scripts/bravo-image ...`, boot, then in
  recovery mount userdata and read `/boot-stdout.txt` and `/boot-dmesg.txt`.
  Then rebuild without `BRAVO_DEBUG`.
* Catch the loop with Volume Down + Power to reach HBOOT/Recovery.

## "Function not implemented" / Error 38 from a program

A glibc binary got into the image. Build against the musl toolchain.

## Rootfs does not mount after install

Check that you extracted a tar into a freshly erased, yaffs2-mounted
userdata, and did not flash a raw image ([LESSONS.md](LESSONS.md)).

## `flash_image`/`bravo-flash-boot` reports a mismatch

Do not reboot. Repeat the flash; if it keeps failing, boot recovery and flash
your own boot image backup.

## bravo-image: "stock boot image not found"

`board/boot-stock.img` was missing when the system was built, or run
`STOCK_BOOT=<path> scripts/bravo-image ...` explicitly.

## `mix deps.get` fails in `bravo_hello`

The Emerge path dependencies (work in progress) point to directories on the
author's machine. Remove them from `bravo_hello/mix.exs` to use the example
firmware.

## Nerves cannot find the toolchain

Create the artifact symlink described in [BUILDING.md](BUILDING.md) and make
sure `.build/bravo-toolchain.tar.gz` exists.

## Wi-Fi

Full background in [WIFI.md](WIFI.md). Quick map of symptoms:

| Symptom | Cause / fix |
|---|---|
| no `wlan0` | driver files missing or `insmod` failed; check `/lib/modules/bcm4329.ko`, `/lib/firmware/fw_bcm4329.bin`, parameters `firmware_path=... iface_name=wlan` |
| `CTRL-EVENT-SCAN-FAILED ret=-1` forever | wext scan is rejected by the driver; use `ap_scan=2` in `wpa.conf` |
| `WPA: Could not find AP from the scan results`, then `WRONG_KEY` / `4-Way Handshake failed` | usually **not** the key: the driver's scan list was empty; run a private-ioctl (`CSCAN`) scan right before associating |
| associated and handshake done, but no IP, `udhcpc` and a `default.script` process just sit there | the stock udhcpc script hangs; run `udhcpc ... -s /usr/libexec/bravo-udhcpc` |
| handshake still fails on a different network | `ap_scan=2` needs explicit `proto`/`pairwise`/`group`; the example is WPA2-AES, a TKIP or mixed-mode AP needs other values |
| `ifconfig wlan0` prints "no support for status display" | BusyBox build; read the address with `:inet.getifaddrs/0` |
| Buildroot rebuild after moving the project fails (`libfakeroot.so` not found, missing patch files) | absolute paths of the old location inside `.nerves/artifacts/*/host`; see below |

### Rebuilding the system after moving the project directory

A system artifact built in one directory has that directory's absolute path in
text files under `.nerves/artifacts/<system>/host/` (libtool `.la` files, linker
scripts, the `fakeroot` and Erlang launch scripts). After moving the project:
rewrite the old prefix in those text files (`grep -rIl OLD host | xargs sed -i
's#OLD#NEW#g'`), and if the legal-info step still fails on missing patch files,
temporarily symlink the old path to the new one for the build. Then repoint the
`~/.nerves/artifacts/` symlinks. Only needed when the system itself is rebuilt;
firmware builds were not affected.
