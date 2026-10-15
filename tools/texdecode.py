"""Decode textures dumped by the SDK's --texture_dump_dir hook.

Reads the index line for a .bin (format, size, pitch, tiling, endianness),
un-tiles Xenos 2D tiled layout, decodes the format and writes one grayscale
PNG per channel (min-max normalised) plus value statistics.

usage: texdecode.py <index_N.txt> <tex_....bin> [<more .bin> ...]
"""
import os, struct, sys, zlib

FORMAT_NAMES = {2: "8", 6: "8_8_8_8", 7: "2_10_10_10", 18: "DXT1", 20: "DXT4_5", 25: "16_16",
                26: "16_16_16_16", 31: "16_16_FLOAT", 36: "32_FLOAT", 37: "32_32_FLOAT"}
BYTES = {2: 1, 6: 4, 7: 4, 22: 4, 23: 4, 25: 4, 26: 8, 31: 4, 36: 4, 37: 8}
FORMAT_NAMES.update({22: "24_8", 23: "24_8_FLOAT"})


def tiled_offset_2d(x, y, pitch, bpb_log2):
    """Xenia texture_address::Tiled2D (byte offset of block x,y)."""
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return (((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) +
            (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F))


def swap(data, endian):
    if endian == 0:
        return data
    b = bytearray(data)
    if endian == 1:  # 8in16
        b[0::2], b[1::2] = data[1::2], data[0::2]
    elif endian == 2:  # 8in32
        b[0::4], b[1::4], b[2::4], b[3::4] = data[3::4], data[2::4], data[1::4], data[0::4]
    elif endian == 3:  # 16in32
        b[0::4], b[1::4], b[2::4], b[3::4] = data[2::4], data[3::4], data[0::4], data[1::4]
    return bytes(b)


def write_png(path, w, h, gray):
    raw = b"".join(b"\0" + bytes(gray[y * w:(y + 1) * w]) for y in range(h))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def decode_dxt5a(meta, data):
    """k_DXT5A (BC4-style): 8-byte 4x4 blocks, two 8-bit endpoints + 3-bit indices."""
    w, h = meta["w"], meta["h"]
    bw, bh = (w + 3) // 4, (h + 3) // 4
    pitch = meta["pitch"] * 32 // 4 if meta["pitch"] else bw  # pitch in blocks
    data = swap(data, meta["endian"])
    out = [0] * (w * h)
    for by in range(bh):
        for bx in range(bw):
            off = tiled_offset_2d(bx, by, pitch, 3) if meta["tiled"] else (by * ((pitch * 8 + 255) & ~255) + bx * 8)
            blk = data[off:off + 8]
            if len(blk) < 8:
                continue
            a0, a1 = blk[0], blk[1]
            if a0 > a1:
                pal = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
            else:
                pal = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
            bits = int.from_bytes(blk[2:8], "little")
            for i in range(16):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x < w and y < h:
                    out[y * w + x] = pal[(bits >> (3 * i)) & 7]
    return [out]


def decode(meta, data):
    fmt, w, h = meta["format"], meta["w"], meta["h"]
    if fmt == 59:
        return decode_dxt5a(meta, data)
    bpb = BYTES[fmt]
    bpb_log2 = bpb.bit_length() - 1
    # Pitch from the fetch constant is in texels / 32; linear rows are 256-byte aligned.
    pitch = meta["pitch"] * 32 if meta["pitch"] else w
    data = swap(data, meta["endian"])
    chans = None
    for y in range(h):
        for x in range(w):
            if meta["tiled"]:
                off = tiled_offset_2d(x, y, pitch, bpb_log2)
            else:
                off = y * ((pitch * bpb + 255) & ~255) + x * bpb
            px = data[off:off + bpb]
            if len(px) < bpb:
                vals = [0.0]
            elif fmt == 25:
                vals = list(struct.unpack("<HH", px))
            elif fmt == 31:
                vals = list(struct.unpack("<ee", px))
            elif fmt == 36:
                vals = list(struct.unpack("<f", px))
            elif fmt in (22, 23):
                v = struct.unpack("<I", px)[0]
                vals = [v >> 8, v & 255]  # depth (24 bits), stencil
            elif fmt == 7:
                v = struct.unpack("<I", px)[0]
                vals = [v & 1023, (v >> 10) & 1023, (v >> 20) & 1023, v >> 30]
            else:
                vals = list(px)
            if chans is None:
                chans = [[] for _ in vals]
            for c, v in enumerate(vals):
                chans[c].append(v)
    return chans


def main():
    index = sys.argv[1]
    metas = {}
    for line in open(index):
        if line.startswith("#"):
            continue
        p = line.split()
        metas[p[-1]] = {"w": int(p[7]), "h": int(p[8]), "pitch": int(p[10]), "tiled": int(p[11]),
                        "format": int(p[13]), "endian": int(p[14]), "fetch": p[17:23]}
    for path in sys.argv[2:]:
        name = os.path.basename(path)
        meta = metas[name]
        if meta["format"] not in BYTES and meta["format"] != 59:
            print(f"{name}: format {meta['format']} not supported")
            continue
        chans = decode(meta, open(path, "rb").read())
        base = os.path.splitext(path)[0]
        print(f"{name}: {FORMAT_NAMES.get(meta['format'])} {meta['w']}x{meta['h']} tiled={meta['tiled']} "
              f"endian={meta['endian']} pitch={meta['pitch']} fetch={' '.join(meta['fetch'])}")
        for c, vals in enumerate(chans):
            lo, hi = min(vals), max(vals)
            uniq = len(set(vals))
            mean = sum(vals) / len(vals)
            print(f"  ch{c}: min {lo} max {hi} mean {mean:.1f} unique {uniq}")
            span = (hi - lo) or 1
            write_png(f"{base}_ch{c}.png", meta["w"], meta["h"],
                      [int(255 * (v - lo) / span) for v in vals])


main()
