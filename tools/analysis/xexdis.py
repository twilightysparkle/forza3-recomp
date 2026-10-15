"""Decrypt a retail XEX2 (basic compression), then inspect addresses.

usage: xexdis.py <default.xex> <out_image.bin> addr[:before[:after]] ...
"""
import bisect, struct, sys
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
import capstone

RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")


def aes(key, data, mode):
    c = Cipher(algorithms.AES(key), mode).decryptor()
    return c.update(data) + c.finalize()


def load_image(path):
    d = open(path, "rb").read()
    _, _, pe_off, _, sec_off, nhdr = struct.unpack(">4sIIIII", d[:24])
    hdrs = dict(struct.unpack(">II", d[24 + i * 8:32 + i * 8]) for i in range(nhdr))
    base = hdrs[0x00010201]
    ffi = hdrs[0x000003FF]
    info_size, enc, comp = struct.unpack(">IHH", d[ffi:ffi + 8])
    assert comp == 1, f"only basic compression supported (got {comp})"
    session = aes(RETAIL_KEY, d[sec_off + 0x150:sec_off + 0x160], modes.ECB())
    raw = d[pe_off:]
    raw = raw[:len(raw) // 16 * 16]
    if enc:
        raw = aes(session, raw, modes.CBC(b"\0" * 16))
    img, pos = bytearray(), 0
    for i in range((info_size - 8) // 8):
        dsz, zsz = struct.unpack(">II", d[ffi + 8 + i * 8:ffi + 16 + i * 8])
        img += raw[pos:pos + dsz] + b"\0" * zsz
        pos += dsz
    assert img[:2] == b"MZ", "decryption failed (no MZ header)"
    return base, bytes(img)


def pe_sections(img):
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    nsec = struct.unpack_from("<H", img, pe + 6)[0]
    opt = struct.unpack_from("<H", img, pe + 20)[0]
    out = {}
    for i in range(nsec):
        o = pe + 24 + opt + i * 40
        name = img[o:o + 8].split(b"\0")[0].decode()
        vsize, va = struct.unpack_from("<II", img, o + 8)
        out[name] = (va, vsize)
    return out


def pdata(img, base, va, size):
    funcs = []
    for o in range(va, va + size, 8):
        begin, packed = struct.unpack_from(">II", img, o)
        if begin == 0:
            break
        funcs.append((begin, begin + ((packed >> 8) & 0x3FFFFF) * 4))
    return sorted(funcs)


def main():
    base, img = load_image(sys.argv[1])
    open(sys.argv[2], "wb").write(img)
    secs = pe_sections(img)
    print("sections:", {k: (hex(base + v[0]), hex(v[1])) for k, v in secs.items()})
    funcs = pdata(img, base, *secs[".pdata"]) if ".pdata" in secs else []
    starts = [f[0] for f in funcs]
    print(f"pdata functions: {len(funcs)}")
    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    for arg in sys.argv[3:]:
        parts = arg.split(":")
        addr = int(parts[0], 16)
        before = int(parts[1]) if len(parts) > 1 else 6
        after = int(parts[2]) if len(parts) > 2 else 6
        i = bisect.bisect_right(starts, addr) - 1
        if i >= 0 and funcs[i][0] <= addr < funcs[i][1]:
            where = f"inside pdata func {funcs[i][0]:08X}-{funcs[i][1]:08X}"
        else:
            nxt = funcs[i + 1][0] if i + 1 < len(funcs) else 0
            prv = funcs[i] if i >= 0 else (0, 0)
            where = f"NOT in pdata (prev {prv[0]:08X}-{prv[1]:08X}, next {nxt:08X})"
        print(f"\n== {addr:08X}: {where}")
        start = addr - before * 4
        code = img[start - base:addr - base + (after + 1) * 4]
        for ins in md.disasm(code, start):
            mark = "=>" if ins.address == addr else "  "
            print(f"{mark} {ins.address:08X}  {ins.mnemonic:8} {ins.op_str}")


if __name__ == "__main__":
    main()