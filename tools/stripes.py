"""Stripe score for forza3 window captures: vertical vs horizontal neighbour contrast.

Horizontal banding on the road (the "flashing" seen while driving) makes
adjacent-row differences much larger than adjacent-column ones. Score =
mean |row diff| / mean |column diff| over the lower road region; ~1 is normal.
Pure Python + PNG decode via System.Drawing is slow, so this uses zlib directly.

usage: stripes.py <png> [<png> ...]
"""
import struct, sys, zlib


def read_png_rgb(path):
    data = open(path, "rb").read()
    pos, chunks, w = 8, [], 0
    while pos < len(data):
        n, typ = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if typ == b"IHDR":
            w, h, depth, ctype = struct.unpack(">IIBB", body[:10])
            bpp = {2: 3, 6: 4}[ctype]
        elif typ == b"IDAT":
            chunks.append(body)
        pos += 12 + n
    raw = zlib.decompress(b"".join(chunks))
    stride, rows, prev = w * bpp, [], bytearray(w * bpp)
    for y in range(h):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, h, bpp, rows


def score(path, x0=60, x1=580, y0=560, y1=716):
    w, h, bpp, rows = read_png_rgb(path)
    lum = lambda r, x: (r[x * bpp] * 3 + r[x * bpp + 1] * 6 + r[x * bpp + 2]) / 10
    vd = hd = n = 0
    for y in range(y0, min(y1, h - 1), 2):
        r0, r1 = rows[y], rows[y + 1]
        for x in range(x0, x1, 3):
            v = lum(r0, x)
            vd += abs(v - lum(r1, x)); hd += abs(v - lum(r0, x + 1)); n += 1
    return vd / max(hd, 1e-6)


def speckle(path, x0=40, x1=1240, y0=430, y1=716, r=4):
    """Per mille of shadowed pixels (local mean < 70) lit > 30 above that mean.

    Catches the lit dashes inside shadows (failed shadow-map tests)."""
    w, h, bpp, rows = read_png_rgb(path)
    lum = lambda r_, x: (r_[x * bpp] * 3 + r_[x * bpp + 1] * 6 + r_[x * bpp + 2]) / 10
    dark = hits = 0
    for y in range(y0 + r, min(y1, h - r), 2):
        for x in range(x0 + r, x1 - r, 2):
            m = sum(lum(rows[y + dy], x + dx) for dy in (-r, 0, r) for dx in (-r, 0, r)) / 9
            if m < 70:
                dark += 1
                if lum(rows[y], x) > m + 30:
                    hits += 1
    return 1000 * hits / max(dark, 1), dark


for p in sys.argv[1:]:
    sp, dark = speckle(p)
    print(f"{p.rsplit(chr(92), 1)[-1]}: stripes {score(p):.2f}  speckle {sp:.1f} (dark px {dark})")
