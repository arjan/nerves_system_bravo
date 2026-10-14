# Building

Everything is built locally. There are no prebuilt artifacts and no Docker is
needed for the system itself. Paths below are relative to the directory that
holds the three sibling repos:

```
nerves_system_bravo/
nerves_toolchain_bravo/
bravo_hello/
```

Build steps were run on Linux x86_64 only. Build times and disk usage were not
recorded (the Buildroot toolchain build alone produces a ~100 MB tarball).

## 1. Host prerequisites

* Erlang/OTP 29 and Elixir 1.20. The repos pin them in `.tool-versions`:
  `erlang 29.0.2`, `elixir 1.20.2-otp-29` (asdf).
* Nerves bootstrap archive: `mix archive.install hex nerves_bootstrap`.
* `fwup` (needed by `mix firmware`).
* Buildroot host dependencies: a C/C++ toolchain, `make`, `git`, `wget`/`curl`,
  `cpio`, `unzip`, `rsync`, `bc`, `file`, `perl`, `python3`, ncurses headers
  (exact package list: see the Buildroot manual; unverified for your distro).
* For `scripts/bravo-image`: `unzip`, `unsquashfs` (squashfs-tools),
  `fakeroot`, `tar`, `cpio`, `gzip`, `python3`.
* The phone's own kernel image at `nerves_system_bravo/board/boot-stock.img`
  ([INSTALL.md](INSTALL.md) step 2). It is a build input and is not in the repo.

```sh
cd nerves_system_bravo
asdf install
mix archive.install hex nerves_bootstrap
```

## 2. Toolchain (once)

glibc binaries from the standard Nerves toolchain do not run on the phone's
2.6.37 kernel (`ENOSYS`, Error 38), so a musl toolchain with GCC 14 and
kernel headers 5.10 is built with Buildroot. Config:
`toolchain/bravo_toolchain_defconfig` (cortex-a8, EABIHF, NEON, musl, C++).

The toolchain was built with Buildroot 2026.05.2. Download it from
buildroot.org and unpack it somewhere outside this repo:

```sh
cd nerves_system_bravo
BR_SRC=/path/to/buildroot-2026.05.2          # your unpacked Buildroot
cp toolchain/bravo_toolchain_defconfig "$BR_SRC/configs/"
make -C "$BR_SRC" O="$PWD/.build/toolchain" bravo_toolchain_defconfig
make -C "$PWD/.build/toolchain" toolchain
```

Pack it as the tarball that `nerves_defconfig` expects
(`BR2_TOOLCHAIN_EXTERNAL_URL=file://${NERVES_DEFCONFIG_DIR}/.build/bravo-toolchain.tar.gz`).
The top-level directory in the tarball must be `bravo-toolchain/` and the
tool prefix is `arm-buildroot-linux-musleabihf`. The original pack command was
not recorded; this reproduces the layout (unverified command):

```sh
tar -C .build/toolchain -czf .build/bravo-toolchain.tar.gz \
    --transform 's,^host,bravo-toolchain,' host
```

Then point Nerves at the toolchain. The stub package `nerves_toolchain_bravo`
cannot build anything, so its artifact is a symlink to the Buildroot host tree:

```sh
mkdir -p ~/.nerves/artifacts
ln -sfn "$PWD/.build/toolchain/host" \
    ~/.nerves/artifacts/nerves_toolchain_bravo-linux_x86_64-0.1.0
```

Note: `nerves_toolchain_bravo`'s error messages mention a
`nerves_system_bravo/toolchain/build.sh` that does not exist; the steps above
replace it.

## 3. System

```sh
cd nerves_system_bravo
ls board/boot-stock.img          # must exist, see INSTALL.md
mix deps.get
MIX_TARGET=target mix compile
```

`nerves_defconfig` includes `wpa_supplicant` (wext driver only, no nl80211), see
[WIFI.md](WIFI.md). `mix.exs` configures Buildroot via `nerves_system_br` 1.34.4 with
`make source all legal-info` (downloads all package sources; the backup site
is `https://dl.nerves-project.org`). The artifact is created locally in
`.nerves/artifacts/nerves_system_bravo-portable-0.1.0/` and contains
`images/` (including a copy of `boot-stock.img` and `scripts/`) and
`legal-info/`.

The system `mix.exs` sets `TARGET_ARCH=armv7`, `TARGET_CPU=cortex_a8`,
`TARGET_OS=linux`, `TARGET_ABI=musleabihf`. Rust crates therefore resolve to
`armv7-unknown-linux-musleabihf`.

If `board/boot-stock.img` is missing, `post-createfs.sh` silently skips the
copy and `bravo-image` later fails with "stock boot image not found".

## 4. Firmware

The example project is `bravo_hello`:

```sh
cd bravo_hello
asdf install
export MIX_TARGET=bravo
mix deps.get
mix firmware
```

The result is `_build/bravo_dev/nerves/images/bravo_hello.fw`. It only
carries the squashfs rootfs; it cannot be burned with fwup on this phone.

`bravo_hello/config/target.exs` currently requires an SSH public key; see
[bravo_hello/README.md](../../bravo_hello/README.md). The Emerge path
dependencies in `bravo_hello/mix.exs` are work in progress and must be removed
or replaced before `mix deps.get` works on another machine.

## 5. Images for the phone

```sh
STOCK_BOOT=nerves_system_bravo/board/boot-stock.img \
  nerves_system_bravo/scripts/bravo-image \
  bravo_hello/_build/bravo_dev/nerves/images/bravo_hello.fw out/
```

Outputs:

* `out/boot.img`: the phone's kernel + empty ramdisk + command line
  `root=/dev/mtdblock5 rootfstype=yaffs2 rw init=/sbin/init no_console_suspend=1 msmsdcc_sdioirq=1 wire.search_count=5`
  (`init=/sbin/bravo-boot` with `BRAVO_DEBUG=1`, a debug PID 1 that logs to
  the rootfs).
* `out/userdata.tar`: the rootfs tree, with device nodes created under
  `fakeroot`, to be extracted onto a freshly erased yaffs2 mount.

`bravo-image` extracts the kernel with `scripts/extract-kernel.py` and builds a
v0 Android boot header with `scripts/mkbootimg.py`, copying load addresses and
page size from the stock image. Verify `boot.img` is at most 3 MB (3145728
bytes); the partition limit.

Do not use `BRAVO_DEBUG=1` images for anything but bring-up.
