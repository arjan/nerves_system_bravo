# Updating a running device

Normal Nerves updates (fwup over SSH, `mix upload`) do not work here: fwup
cannot write this NAND, and the rootfs is YAFFS2, not squashfs. Two scripts
replace them. Both talk to the phone over the USB network using
`ssh host '<elixir expression>'`, and both need the SSH key from the firmware
to be loaded in your agent or default identity (`BatchMode=yes`).

Requirements on the host: `python3`, `ssh`. On the phone: the running system
from this repo (NervesSSH exec, `flash_erase`/`nandwrite` from mtd-utils).

Security note: both scripts use `StrictHostKeyChecking=no` and
`UserKnownHostsFile=/dev/null`. That is tolerable on a point-to-point USB link
and wrong anywhere else. Do not point them at a routed network.

## Typical flow

```sh
# rebuild firmware, then
STOCK_BOOT=nerves_system_bravo/board/boot-stock.img \
  nerves_system_bravo/scripts/bravo-image bravo_hello.fw out/

nerves_system_bravo/scripts/bravo-deploy out/userdata.tar --host <phone-ip>          # dry run
nerves_system_bravo/scripts/bravo-deploy out/userdata.tar --host <phone-ip> --apply  # write
```

## scripts/bravo-deploy: sync files of the rootfs

```
bravo-deploy <userdata.tar> [--host IP] [--apply] [--reboot] [--remove PATH...]
```

* Compares the sha256 of every regular file in the tar with the phone and
  lists new/changed files. **Dry run unless `--apply`.**
* Sends only differing files as base64 chunks (stdin read with `IO.gets` in
  the remote expression), writes to `<path>.bravo-new`, then renames into
  place, then re-reads and verifies every sent file. Exit code 1 on mismatch.
* Never touched: `dev/`, `root/`, `tmp/`, `run/`, `proc/`, `sys/`, `boot-*`,
  `diag.log`. Files that exist only on the phone are never removed unless you
  list them in `--remove` (paths without leading `/`).
* Only regular files are synced. Symlinks, directories, device nodes and
  permission changes on unchanged files are not handled. File names with
  non-ASCII, `"` or `\` are skipped with a message.
* `--host` defaults to `172.31.89.145`, the author's phone. **Always pass
  your own address.**
* `--reboot` calls `Nerves.Runtime.reboot()` after a successful verification.

Applying changes to the Erlang release (`/srv/erlang`) while the VM runs is
safe to write but only takes effect after a reboot.

## scripts/bravo-flash-boot: new boot image

```
bravo-flash-boot <boot.img> [--host IP] [--reboot]
```

1. Refuses images larger than 3 MB (the partition).
2. Stages the image to `/tmp/boot.img` on the phone and compares its sha256
   on the phone. Aborts before touching flash if it differs.
3. `flash_erase /dev/mtd2 0 0`, then `nandwrite -p /dev/mtd2 /tmp/boot.img`.
4. Reads `/dev/mtd2ro` back and compares sha256 again.
5. Reboots only with `--reboot`, never after a failed verification.

Between steps 3 and 4 the boot partition is empty or partial. A power loss
or crash there leaves the phone unable to boot Linux; HBOOT and recovery are
in other places and still work, so recover from there (below).

`--host` defaults to the author's address here too.

## Recovery fallback

When the running system is unreachable or the boot partition is wrong:

1. Volume Down + Power, choose Recovery (or catch a reboot loop).
2. Follow [INSTALL.md](INSTALL.md) steps 4 to 6 with a new `userdata.tar` /
   `boot.img`, or flash your original boot image backup to go back to Android.

Do not use the `diag/flash-boot-usb` script for normal updates. It needs the
diagnostic ramdisk with its unauthenticated shell and a hard-coded host
interface name, and is for bring-up only.
