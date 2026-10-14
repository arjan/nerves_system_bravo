#!/usr/bin/env python3
"""Build an Android boot.img (header v0) from a kernel and a ramdisk."""
import argparse, hashlib, struct

p = argparse.ArgumentParser()
p.add_argument("--kernel", required=True)
p.add_argument("--ramdisk", required=True)
p.add_argument("--cmdline", required=True)
p.add_argument("--base-from", help="existing boot.img to copy load addresses from")
p.add_argument("-o", "--output", required=True)
a = p.parse_args()

PAGE = 2048
kernel_addr, ramdisk_addr, tags_addr = 0x20008000, 0x21000000, 0x20000100
if a.base_from:
    hdr = open(a.base_from, "rb").read(40)
    _, _, kernel_addr, _, ramdisk_addr, _, _, tags_addr, PAGE = struct.unpack("<8s8I", hdr)

kernel = open(a.kernel, "rb").read()
ramdisk = open(a.ramdisk, "rb").read()
cmd = a.cmdline.encode()
assert len(cmd) < 512, "cmdline too long"

def pad(b):
    return b + b"\0" * (-len(b) % PAGE)

sha = hashlib.sha1(kernel + struct.pack("<I", len(kernel)) + ramdisk +
                   struct.pack("<I", len(ramdisk)) + struct.pack("<I", 0)).digest()
header = struct.pack("<8s10I16s512s32s", b"ANDROID!", len(kernel), kernel_addr,
                     len(ramdisk), ramdisk_addr, 0, 0x20f00000, tags_addr, PAGE, 0, 0,
                     b"", cmd, sha + b"\0" * 12)
img = pad(header) + pad(kernel) + pad(ramdisk)
open(a.output, "wb").write(img)
print(f"{a.output}: {len(img)} bytes, kernel {len(kernel)}, ramdisk {len(ramdisk)}, page {PAGE}")
