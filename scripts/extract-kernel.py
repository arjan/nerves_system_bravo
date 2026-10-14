#!/usr/bin/env python3
"""Extract the kernel from an Android boot.img."""
import struct, sys
d = open(sys.argv[1], "rb").read()
magic, ksize, *_rest = struct.unpack("<8s8I", d[:40])
page = _rest[6]
assert magic == b"ANDROID!"
open(sys.argv[2], "wb").write(d[page:page + ksize])
print(f"kernel: {ksize} bytes")
