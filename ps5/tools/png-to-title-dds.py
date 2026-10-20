#!/usr/bin/env python3
# Eternal Darkness for the PlayStation 5 - a picture as home-screen art (sce_sys/pic0.dds, pic1.dds).
#
#   python3 ps5/tools/png-to-title-dds.py PICTURE OUTPUT.dds
#
# pic0.dds is the background behind the title on the home screen; pic1.dds is the
# splash while it starts (seen on the console, 2026-10-10). The picture is scaled
# to 3840x2160 and written in the one form the console is known to show: DDS, DX10
# header, BC7_UNORM, one level, straight alpha.
#
# The BC7 mode 6 encoder and the DDS header are PS5 RetroArch's
# (tools/make-title-art.py), unchanged:
# Copyright (C) 2026 Mihawk-99
# SPDX-License-Identifier: GPL-3.0-or-later
import struct
import sys
from pathlib import Path

import numpy as np

WIDTH, HEIGHT = 3840, 2160

WEIGHTS = np.array([0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64], dtype=np.int64)


def quantise(e: np.ndarray) -> np.ndarray:
    """Endpoint colours to the 7 stored bits (the p-bit is 1, for alpha 255)."""
    return np.clip(np.round((e - 1.0) / 2.0), 0, 127).astype(np.int64)


def palette(q0: np.ndarray, q1: np.ndarray) -> np.ndarray:
    """The 16 colours two quantised endpoints give, (blocks, 16, 3)."""
    e0, e1 = (q0 * 2 + 1)[:, None, :], (q1 * 2 + 1)[:, None, :]
    w = WEIGHTS[None, :, None]
    return ((64 - w) * e0 + w * e1 + 32) >> 6


def choose(px: np.ndarray, q0: np.ndarray, q1: np.ndarray):
    """Each pixel's best index for these endpoints, and the block's error."""
    pal = palette(q0, q1)
    err = ((px[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)
    idx = err.argmin(-1)
    return idx, np.take_along_axis(err, idx[..., None], -1)[..., 0].sum(-1)


def encode_blocks(px: np.ndarray) -> bytes:
    """BC7 mode 6 for blocks of 16 RGB pixels (blocks, 16, 3), alpha 255."""
    pf = px.astype(np.float64)
    mean = pf.mean(1)
    cen = pf - mean[:, None, :]
    cov = np.einsum('nki,nkj->nij', cen, cen)
    axis = np.ones((len(px), 3)) / np.sqrt(3.0)
    for _ in range(8):
        axis = np.einsum('nij,nj->ni', cov, axis)
        norm = np.sqrt((axis * axis).sum(1, keepdims=True))
        axis = np.where(norm > 1e-9, axis / np.maximum(norm, 1e-9), 1.0 / np.sqrt(3.0))
    proj = np.einsum('nki,ni->nk', cen, axis)
    q0 = quantise(mean + proj.min(1, keepdims=True) * axis)
    q1 = quantise(mean + proj.max(1, keepdims=True) * axis)
    idx, err = choose(px, q0, q1)
    # One least-squares refit of the endpoints to the chosen indices
    w = WEIGHTS[idx].astype(np.float64) / 64.0
    a, b, c = ((1 - w) ** 2).sum(1), ((1 - w) * w).sum(1), (w * w).sum(1)
    d0 = ((1 - w)[..., None] * pf).sum(1)
    d1 = (w[..., None] * pf).sum(1)
    det = a * c - b * b
    ok = det > 1e-9
    safe = np.where(ok, det, 1.0)[:, None]
    r0 = quantise(np.where(ok[:, None], (c[:, None] * d0 - b[:, None] * d1) / safe, mean))
    r1 = quantise(np.where(ok[:, None], (a[:, None] * d1 - b[:, None] * d0) / safe, mean))
    idx2, err2 = choose(px, r0, r1)
    better = (err2 < err)[:, None]
    q0, q1 = np.where(better, r0, q0), np.where(better, r1, q1)
    idx = np.where(better, idx2, idx)
    # The first pixel's index is stored in 3 bits: swap the ends if it needs 4
    swap = idx[:, 0] >= 8
    q0, q1 = np.where(swap[:, None], q1, q0), np.where(swap[:, None], q0, q1)
    idx = np.where(swap[:, None], 15 - idx, idx)

    lo = np.full(len(px), 1 << 6, dtype=np.uint64)   # mode 6
    hi = np.zeros(len(px), dtype=np.uint64)
    pos = 7

    def put(value: np.ndarray, bits: int):
        nonlocal pos
        for bit in range(bits):
            v = ((value >> bit) & 1).astype(np.uint64)
            if pos < 64:
                lo[:] |= v << np.uint64(pos)
            else:
                hi[:] |= v << np.uint64(pos - 64)
            pos += 1

    for ch in range(3):
        put(q0[:, ch], 7)
        put(q1[:, ch], 7)
    alpha = np.full(len(px), 127, dtype=np.int64)
    put(alpha, 7)
    put(alpha, 7)
    one = np.ones(len(px), dtype=np.int64)
    put(one, 1)   # P0
    put(one, 1)   # P1
    put(idx[:, 0], 3)
    for i in range(1, 16):
        put(idx[:, i], 4)
    out = np.empty(len(px) * 2, dtype="<u8")
    out[0::2], out[1::2] = lo, hi
    return out.tobytes()


def bc7(img: np.ndarray) -> bytes:
    """The picture as BC7 blocks, row of blocks by row of blocks."""
    rgb = np.clip(np.round(img), 0, 255).astype(np.int64)
    by, bx = HEIGHT // 4, WIDTH // 4
    blocks = rgb.reshape(by, 4, bx, 4, 3).transpose(0, 2, 1, 3, 4).reshape(by * bx, 16, 3)
    step = bx * 16
    return b"".join(encode_blocks(blocks[i:i + step]) for i in range(0, len(blocks), step))


def dds_header() -> bytes:
    """The DDS header of the launcher art the console has shown: DX10, BC7_UNORM
    (98), 2D, one level, straight alpha."""
    DDSD_CAPS, DDSD_HEIGHT, DDSD_WIDTH, DDSD_PIXELFORMAT = 0x1, 0x2, 0x4, 0x1000
    DDSD_MIPMAPCOUNT, DDSD_LINEARSIZE = 0x20000, 0x80000
    flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_MIPMAPCOUNT | DDSD_LINEARSIZE
    linear = WIDTH * HEIGHT               # BC7: 16 bytes a 4x4 block, one byte a pixel
    header = struct.pack("<4sIIIIIII", b"DDS ", 124, flags, HEIGHT, WIDTH, linear, 1, 1)
    header += b"\0" * 44                  # reserved
    header += struct.pack("<II4sIIIII", 32, 0x4, b"DX10", 0, 0, 0, 0, 0)
    header += struct.pack("<IIIII", 0x1000, 0, 0, 0, 0)   # caps: texture
    header += struct.pack("<IIIII", 98, 3, 0, 1, 1)       # BC7_UNORM, TEXTURE2D, array 1, straight alpha
    assert len(header) == 148
    return header


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: png-to-title-dds.py PICTURE OUTPUT.dds", file=sys.stderr)
        return 2
    from PIL import Image
    picture = Image.open(sys.argv[1]).convert("RGB").resize((WIDTH, HEIGHT), Image.LANCZOS)
    data = dds_header() + bc7(np.asarray(picture, dtype=np.float64))
    Path(sys.argv[2]).write_bytes(data)
    print(f"{sys.argv[2]}: {len(data)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
