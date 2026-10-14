# Release checklist and audit

State of the audit: 2026-10-14, repos `nerves_system_bravo`,
`nerves_toolchain_bravo`, `bravo_hello` (no git history was present in any of
the three directories, so this is a file-tree scan; scan git history too
before the first push if one exists). `bravo-kernel/` and `bravo-backup/`
must **not** be in the release.

**This file is an internal audit: remove or sanitise it before publishing** (it names author-specific details).

Items marked (unsure) are my reading, not legal or verified advice.

## A. Blockers: do not publish

| # | Item | Where | Action |
|---|------|-------|--------|
| A1 | The phone's own kernel image (vendor binary, GPL kernel) | `nerves_system_bravo/board/boot-stock.img` (3 MB). Copies also embedded in `bravo_hello/diag-boot.img`, `bravo_hello/out*/boot.img`, and in the built artifact `nerves_system_bravo/.nerves/artifacts/*/images/boot-stock.img` | Do not commit. `.gitignore` now covers `board/*.img`, `*.img`, `.nerves/`. Users dump their own (A1a). Delete or ignore the `bravo_hello` copies. |
| A2 | Private backups | `bravo-backup/` (outside the three repos) and any `boot-mtd*.img`, `data-backup*.tar` | Never commit; ignore patterns added. Docs only tell users to make their own. |
| A3 | Rootfs/tar images with the author's key and absolute build paths | `bravo_hello/out/`, `out-clean/`, `out-debug/`, `out-key/` (`userdata.tar`, `boot.img`): the tars contain absolute `/home/<user>/...` build paths (checked) and an SSH public key (`ssh-laptop-2026`) in `sys.config` | Do not commit `out*/`. |
| A4 | Debug patch kit with binaries and an author-specific key | `bravo_hello/patch/` (`files/dbgbb` = static BusyBox, `files/fbdmesg` = binary, `key/sys.config` + `key/paste-into-iex.txt` base64 copy of it with the author's public key `ssh-laptop-2026`, `key/apply.sh`, `files/bravo-boot` = unauthenticated `nc -e sh` on :2323) | Remove `patch/` from the release, or keep only the shell scripts with the key and binaries removed. BusyBox binary is GPL: shipping it needs source offer. |
| A5 | Absolute local paths in `bravo_hello/mix.exs` | `emerge` and `video_interop` path deps under the author's home directory | Remove the Emerge/video_interop deps (WIP) before release; otherwise `mix deps.get` fails for everyone. |
| A6 | No LICENSE file anywhere | all three repos | Add (see C). Until then legally nobody may reuse the code. |
| A7 | Built tool binary | `nerves_system_bravo/diag/fbdmesg` (ARM static ELF) | Ignored now; ship the source `fbdmesg.c` and build instructions only. |
| A8 | Logs with absolute paths | `nerves_system_bravo/build.log`, `.build/*.log`, `bravo_hello/.logs/`, `.nerves/`, `deps/`, `_build/` | Ignored (`*.log`, `.build/`, `.nerves/`). Add the same to `bravo_hello/.gitignore` (none exists yet). |

### Scan results (text and binary, excluding `deps/`, `_build/`, `.build/`, `.nerves/`)

* Phone serial (given by the owner, redacted here): **not found** in text files nor in the
  binaries `board/boot-stock.img`, `diag/fbdmesg`, `patch/files/*`,
  `out-clean/boot.img`, `diag-boot.img`, the four `userdata.tar`. I did not
  open every artifact; re-run `grep -a` over the final tree before pushing.
* Broadcom firmware or `bcm4329.ko`: **not found** anywhere in the three repos
  (searched by name `*.ko`, `*bcm4329*`, `fw_*`, `nvram*`; the only `bcm` hits
  are unrelated toolchain sources in `.build/`).
* SSH private keys: none in the tree. `bravo_hello/config/target.exs` reads a
  public key from `~/.ssh/ssh-laptop-2026.pub` at build time.
* Emails: none found in the source files (`arjan.scherpenisse`, `enreach`
  searched). The user name `arjan` appears only in absolute paths (A3, A5, A8).
* Author-specific network details:
  * `172.31.89.145` default host in `scripts/bravo-deploy` and
    `scripts/bravo-flash-boot` (docstrings and `--host` default).
  * `172.31.7.2` in `diag/init` and `bravo_hello/patch/files/bravo-boot`
    (a debug address, not secret).
  * `enx<author's MAC>` (the author's USB NIC) hard-coded in `diag/phone-sh` and
    `diag/flash-boot-usb`; `fe80::63de` hint in `diag/phone-sh`.
  * `bravo-backup/boot-mtd2.img` referenced in the docstring of
    `scripts/bravo-flash-boot` and in a comment in `diag/flash-boot-usb`.
* Hostnames: `nerves-bravo` (`rootfs_overlay/etc/erlinit.config`), `nerves`
  (mDNS) are generic.

### A1a. Proposed way to get the kernel without shipping it

Documented in [docs/INSTALL.md](docs/INSTALL.md) step 2: in recovery, `dd` the
boot partition to the phone's scratch space, `adb pull`, compare sha256 on
phone and host, check the `ANDROID!` magic and 3145728 bytes, copy to
`board/boot-stock.img`. Missing, worth adding:

* A script `scripts/bravo-dump-boot` automating that (dd on the phone, pull,
  checksum both sides, magic check). It must never use `adb shell cat >`.
* A check in `bravo-image` that the image has the Android magic, a header
  page size of 2048 and a kernel that decompresses (the author checked by
  decompressing the payload manually).
* Decide which kernels are supported (only CyanogenMod 2.6.37.6 was used) and
  say so in the check.

## B. Hard-coded local paths that break for other users

Not changed (by instruction); each with the portable fix.

| File | Problem | Fix |
|------|---------|-----|
| `nerves_system_bravo/nerves_defconfig` (`BR2_TOOLCHAIN_EXTERNAL_URL=file://${NERVES_DEFCONFIG_DIR}/.build/bravo-toolchain.tar.gz`) | Not an absolute path, but needs a locally built tarball in `.build/`; the pack step is undocumented in code | Add `scripts/build-toolchain.sh` that downloads a pinned Buildroot (2026.05.2 was used) with checksum, builds, packs the tarball, creates the Nerves artifact link. Do not host the tarball (GPL source obligations, 100 MB). |
| `nerves_toolchain_bravo/lib/nerves_toolchain_bravo.ex` | Error text refers to `nerves_system_bravo/toolchain/build.sh`, which does not exist | Create that script (above) or fix the message. |
| `~/.nerves/artifacts/nerves_toolchain_bravo-linux_x86_64-0.1.0` | Manual absolute symlink into `.build/toolchain/host` | Create it from the script. |
| `nerves_system_bravo/mix.exs`: `{:nerves_toolchain_bravo, path: "../nerves_toolchain_bravo"}` | Requires sibling checkout; blocks Hex publication | Move the stub into the system repo, or use a git dependency with `sparse:`. |
| `bravo_hello/mix.exs`: `path: "../nerves_system_bravo"` | Same | Fine for an example; document or use a git dep. |
| `bravo_hello/mix.exs`: Emerge/video_interop paths | Absolute paths under the author's home (A5) | Remove (WIP). |
| `bravo_hello/config/target.exs` | `~/.ssh/ssh-laptop-2026.pub`, build raises if missing | Read an env var (e.g. `BRAVO_SSH_KEY`) with a documented default such as `~/.ssh/id_ed25519.pub`; also make `regulatory_domain` configurable. |
| `scripts/bravo-deploy`, `scripts/bravo-flash-boot` | Default `--host 172.31.89.145`; `StrictHostKeyChecking=no` | Make `--host` required or auto-detect; offer `accept-new` with a known-hosts file. |
| `scripts/bravo-image` | `STOCK_BOOT` default `$SYS/images/boot-stock.img` is only valid inside the built artifact; from the repo you must set `STOCK_BOOT` | Default to `board/boot-stock.img` when `images/` has none. |
| `diag/phone-sh`, `diag/flash-boot-usb` | Author's NIC name `enx<author's MAC>`, link-local guess; `../../bravo-backup` in text | Take interface/host from arguments or env; remove path mention. |
| `diag/mkdiag-image`, `diag/fbdmesg` | Needs a static ARM BusyBox you supply and a prebuilt `fbdmesg`; no build recipe | Document how to build both with the Buildroot toolchain, or drop `diag/` from the release. |
| `bravo_hello/patch/*/apply.sh` | Assume recovery's `/sdcard` and `/dev/block/mtdblock5` | Same as A4: drop or generalise. |
| `nerves_system_bravo/mix.exs` `checksum:` lists `board` | If `board/` does not exist in a fresh clone (all `*.img` ignored), Nerves checksum may fail (unsure) | Commit a `board/.gitkeep` or `board/README`. |
| `post-createfs.sh` | Silently skips copying `boot-stock.img` (`|| true`) | Fail loudly or print a clear message. |
| `.tool-versions` | Pins `erlang 29.0.2`, `elixir 1.20.2-otp-29`; very new | State as tested versions; other versions untested. |

## C. License situation (unsure, please review)

Recommendation: **Apache-2.0** for the original files (matches other Nerves
systems and `nerves_system_br`), with REUSE-style metadata: `LICENSES/`
directory with the SPDX texts, `REUSE.toml` (like `nerves_system_br`'s) or SPDX
headers, and a `NOTICE`. Nothing found forces a different license on the repo
itself, since no GPL code is copied into it, as far as I can tell.

| Component | License (as far as known) | Needed |
|-----------|---------------------------|--------|
| Original scripts, defconfigs, overlay, docs | Apache-2.0 (proposed); copyright holder to be set | `LICENSE`, SPDX headers |
| `scripts/mkbootimg.py` | Written for this repo (assumed; it reimplements the Android boot header v0 format). If any code was copied from AOSP mkbootimg: Apache-2.0 notice required | confirm origin (unsure) |
| `diag/fbdmesg.c` | Original (assumed) | header |
| `diag/font8x16.h` | Generated from `Lat15-Fixed16.psf` (console font). Console fonts are usually derived from the public-domain misc-fixed fonts, but I have not verified | check origin, or replace with your own font; keep the note |
| Nerves, `nerves_system_br` | Apache-2.0, with CC0/CC-BY/GPL-2.0-or-later parts (see its `REUSE.toml`) | credit; no copy in this repo |
| Buildroot and built packages (BusyBox GPL-2.0, mtd-utils GPL-2.0, musl MIT, GCC/binutils GPL-3.0 with runtime exceptions, Linux headers GPL-2.0 w/ syscall note, erlinit MIT, nerves_heart Apache-2.0, nbtty, ...) | various | The build runs `legal-info` and writes `.nerves/artifacts/*/legal-info/`. Anyone redistributing an image must ship it plus sources. This repo does not distribute binaries. Note the build log warns "Buildroot source code has not been saved". |
| Stock kernel (CyanogenMod 2.6.37.6, GPL-2.0) | GPL-2.0 | Not distributed. If you ever distribute a boot image you must offer the corresponding kernel source and config. |
| Broadcom firmware, `bcm4329.ko` | Proprietary / vendor; redistribution not permitted without a license | Never distribute; docs say to copy from your own phone. |
| Hex deps in `bravo_hello` (nerves_ssh, vintage_net, ...) | mostly Apache-2.0 (unsure per package) | check `mix hex.licenses`-style tooling before shipping a firmware |
| `bravo-kernel/` experiment (GPL kernel patches/builds) | GPL-2.0 | Not in the release |
| `bravo_hello` Emerge/video_interop | unknown | WIP, excluded |

## D. Missing pieces for a public release

* **Wi-Fi:** documented in `docs/WIFI.md` and verified on one phone and one access
  point (WPA2-AES). Before a release: say that TKIP/mixed-mode APs are untested;
  make sure no `wpa.conf`, PSK, SSID or passphrase appears in any repo, image or
  `userdata.tar` (the example app writes them to the phone at runtime with
  `scripts/bravo-config`); the Broadcom driver and firmware stay out of every
  image that is shared. The udhcpc script is new in the system overlay, so a
  system built before it was added lacks it.
* **Artifacts:** `nerves_package` has `artifact_sites: []`; the system is
  never published and **always built locally** (and so is the toolchain). No
  prebuilt images exist. Say this at the top of every doc (done in README).
  Distributing prebuilt images would raise the GPL, Broadcom and kernel
  issues above; recommended not to.
* **Hex:** do not publish `nerves_system_bravo`/`nerves_toolchain_bravo` to
  Hex: a Hex package cannot have path deps, and the toolchain stub cannot
  build anything. Recommend a GitHub-only release with git dependencies.
* **Versioning:** `VERSION` = 0.1.0 (system), 0.1.0 (stub toolchain, its own
  `VERSION`), `bravo_hello` 0.1.0. Tag `v0.1.0`, keep `CHANGELOG.md`. The
  toolchain artifact link name includes the stub version; keep them in sync.
* **CI:** none. A full build in CI is heavy (Buildroot toolchain + system)
  and needs the user's kernel, so CI cannot do the real build. Suggest:
  `shellcheck` on scripts, `python3 -m py_compile` on `scripts/*.py`,
  `mix format --check-formatted`, markdown link check, and optionally a
  scheduled/manual toolchain build job.
* **Tests:** none. At least unit-test `mkbootimg.py` (header round trip with a
  synthetic kernel) and `bravo-image` argument handling.
* **Untested:** other phones/revisions/ROMs/kernels; non-Linux hosts;
  rebuilding from a clean checkout (the path to a fresh clone has not been
  exercised: do a clean-room build following `docs/BUILDING.md` and fix the
  docs where they are wrong); every command marked "(unverified)" in `docs/`.
* **Security:** `diag/` and the debug PID 1 offer unauthenticated root
  shells; `bravo-deploy`/`bravo-flash-boot` disable host key checks. NervesSSH
  is key-only (good). Say it in the release notes; consider dropping `diag/`.
* **Docs to confirm on hardware:** the `dd if=/dev/mtd/mtd2ro` dump command,
  `tar` flags for the install and rollback, the Wi-Fi module parameter names
  and `/system` file paths, DHCP profile for non-NetworkManager hosts.
* **Repository hygiene:** the three directories are not git repositories;
  decide on one repo (recommended: one monorepo with the system, stub
  toolchain and example, which fixes the path deps) or three; add
  `.gitignore` to `bravo_hello` and `nerves_toolchain_bravo`; add
  `CONTRIBUTING`/issue templates if desired.
* **Stale files:** `bravo_hello/mix.lock` still contains `nerves_pack` and
  other deps not in `mix.exs`; run `mix deps.clean --unlock --unused`.
  `scripts/bravo-boot`, `diag/init` and the docs mention the abandoned 3.0
  kernel in places; fine, but keep the story consistent.

## E. Final pre-push checklist

- [ ] `LICENSE` + SPDX/REUSE + `NOTICE` added
- [ ] `board/boot-stock.img`, all `*.img`, `.nerves/`, `.build/`, `deps/`,
      `_build/`, `out*/`, `*.log` not in the tree (`git status --ignored`)
- [ ] `bravo_hello/patch/` removed or sanitised; Emerge deps removed
- [ ] `grep -rIa -e /home/ -e <serial> -e ssh-laptop .` over the final tree is empty
- [ ] default host IP and NIC name removed from scripts
- [ ] clean-room build from a fresh clone works, docs corrected
- [ ] `bravo-kernel/` and `bravo-backup/` not included
- [ ] release notes state: replaces Android, no prebuilt images, supplied by
      the user: kernel, Broadcom files
