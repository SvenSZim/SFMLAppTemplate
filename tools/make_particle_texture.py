#!/usr/bin/env python3
"""Writes resources/textures/particle.png: a white disc with a soft edge on transparency.

The showcase draws its particles with it, tinted by the theme. The image is made here, not
taken from elsewhere, so it needs no licence of its own. Standard library only.

    tools/make_particle_texture.py
"""

import math
import pathlib
import struct
import zlib

SIZE = 64      # pixels on each side
EDGE = 2.0     # width of the soft edge, in pixels


def alpha(x: int, y: int) -> int:
    """How opaque the pixel is: full inside the disc, fading over EDGE pixels at its rim."""
    centre = SIZE / 2.0
    distance = math.hypot(x + 0.5 - centre, y + 0.5 - centre)
    radius = centre - 1.0
    coverage = min(max((radius - distance) / EDGE + 0.5, 0.0), 1.0)
    return round(255 * coverage)


def chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def main() -> None:
    rows = b"".join(
        b"\x00" + b"".join(bytes((255, 255, 255, alpha(x, y))) for x in range(SIZE)) for y in range(SIZE)
    )
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))  # 8-bit RGBA
        + chunk(b"IDAT", zlib.compress(rows, 9))
        + chunk(b"IEND", b"")
    )
    target = pathlib.Path(__file__).resolve().parent.parent / "resources" / "textures" / "particle.png"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(png)
    print(f"wrote {target} ({len(png)} bytes)")


if __name__ == "__main__":
    main()
