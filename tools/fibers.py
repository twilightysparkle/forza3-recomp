"""Find code touching the current thread's fiber slot (thread->0x164) and callers of given addresses.

usage: fibers.py <default.xex> <image.bin> target_hex ...
"""
import bisect, struct, sys
sys.path.insert(0, __file__.rsplit("\\", 1)[0])
from xexdis import load_image, pe_sections, pdata  # noqa: E402

xex, imgpath = sys.argv[1], sys.argv[2]
targets = {int(t, 16) for t in sys.argv[3:]}
base, img = load_image(xex)
secs = pe_sections(img)
funcs = pdata(img, base, *secs[".pdata"])
starts = [f[0] for f in funcs]
tva, tsize = secs[".text"]


def owner(a):
    i = bisect.bisect_right(starts, a) - 1
    return f"{funcs[i][0]:08X}" if i >= 0 and funcs[i][0] <= a < funcs[i][1] else "no-pdata"


words = [(base + o, struct.unpack_from(">I", img, o)[0]) for o in range(tva, tva + tsize, 4)]
callers = {t: [] for t in targets}
hits = []
for idx, (a, w) in enumerate(words):
    op = w >> 26
    if op == 18:  # b / bl
        off = w & 0x03FFFFFC
        if off & 0x02000000:
            off -= 0x04000000
        dest = a + off if not (w & 2) else off
        if dest in targets:
            callers[dest].append((a, "bl" if w & 1 else "b"))
    # lwz rD, 0x100(r13)
    if op == 32 and ((w >> 16) & 31) == 13 and (w & 0xFFFF) == 0x100:
        rd = (w >> 21) & 31
        for a2, w2 in words[idx + 1: idx + 10]:
            if (w2 >> 26) in (32, 36) and ((w2 >> 16) & 31) == rd and (w2 & 0xFFFF) == 0x164:
                hits.append((a, a2, "stw" if (w2 >> 26) == 36 else "lwz", owner(a)))
                break

print("thread->0x164 accesses:")
for a, a2, kind, own in hits:
    print(f"  {a:08X} {kind} at {a2:08X}  func {own}")
for t, cs in callers.items():
    print(f"callers of {t:08X}: {len(cs)}")
    for a, k in cs[:12]:
        print(f"  {k:2} from {a:08X}  func {owner(a)}")
