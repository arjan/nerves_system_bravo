# Kernel experiment (abandoned)

Status: **dead end, documented for the lessons.** Not supported, not part of
the release. The supported configuration is the stock 2.6.37.6 kernel.

## Goal

Replace the stock kernel with a newer downstream one to get a kernel text
console on the screen (fbcon) and devtmpfs, instead of working blind over USB.

## What was tried

* Kernel: `spezi77/android_kernel_htc_qsd8k_3.0` (3.0.16, branch `cm-13.0`).
  There is no mainline support for the QSD8250, so 3.0.16 was the newest
  option found.
* Build: GCC 7 in a Docker container (Ubuntu 18.04 with
  `gcc-arm-linux-gnueabihf`). GCC 14 fails on the kernel's inline asm
  register checks.
* Config: devtmpfs, VT, framebuffer console and `FB_MSM` enabled on top of the
  upstream defconfig. A local patch to `drivers/video/msm/msm_fb.c`.
* Diagnostic ramdisk images (`diag/init` supports both kernels) to see what
  the kernel does without touching the NAND rootfs.

It built and booted.

## Findings

| Finding | Detail |
|---------|--------|
| Silent kernel | The upstream defconfig strips `CONFIG_PRINTK` ("remove debug & logs"): no log, and `dmesg`/`klogctl` return ENOSYS. It also lacked VT, fbcon and devtmpfs. |
| Colour bands | `msm_fb.c` defaults to 32 bpp, but stride (`xres * 2`) and bitfields are RGB565 (16 bpp). Android reprograms the mode on start; fbcon does not. A one-line patch (`BITS_PER_PIXEL_DEF 16`) fixed the bands. |
| Slow, glitchy console | Every glyph goes through `msmfb_update`; fbcon switching triggered `BUG: scheduling while atomic`. |
| No USB RNDIS | This HTC gadget stubs `functions_store`. Functions are switched via `/sys/class/android_usb/f_<name>/on`, but writing `f_rndis/on` never completed, so there was no USB network. |

## Why it was dropped

Even with patches the console was too slow and unstable to be useful, USB
networking (the only other way in) did not work, and the stock kernel with
userland-drawn output is a smaller, working configuration.

## If someone tries again

Suggestions, none tested:

* Keep `CONFIG_PRINTK=y` from the first build on, so failures are visible.
* Start with the diagnostic ramdisk only; touch the NAND rootfs last.
* Fix the msm_fb 16/32 bpp mismatch from the start. Prefer drawing from
  userspace to the framebuffer over fbcon; it avoids the per-glyph update
  path.
* Investigate the HTC gadget code path behind `f_rndis/on` before building
  anything else on top; USB is the debug channel.
* Use `/proc/config.gz` of the working stock kernel as a base if it exists
  (unverified).
* Keep the stock-kernel path working at all times: flash tools in this repo
  (`bravo-flash-boot`, diag `flash-boot-usb`) allow switching back quickly.
* Mind the GPL: if you distribute a kernel image, you must provide the source
  and your config.

The experiment tree (`bravo-kernel/`: patches, `build.sh`, Docker file) is not
published with this repository.
