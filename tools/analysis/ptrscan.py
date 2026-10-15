"""Find data-referenced code addresses that codegen did not register as functions.

Scans .rdata/.data for big-endian words pointing into .text, then reports
targets that are neither registered functions (generated/default/*_init.cpp)
nor inside a .pdata function. These are vtable/function-pointer-only entries
that crash at runtime with "Call to invalid or unregistered function".

usage: ptrscan.py <default.xex> <generated_init.cpp> [--toml]
"""
import bisect, os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xexdis import load_image, pe_sections, pdata  # noqa: E402

xex, init_cpp = sys.argv[1], sys.argv[2]
as_toml = "--toml" in sys.argv
base, img = load_image(xex)
secs = pe_sections(img)
funcs = pdata(img, base, *secs[".pdata"])
starts = [f[0] for f in funcs]
tva, tsize = secs[".text"]
text_lo, text_hi = base + tva, base + tva + tsize

# Any registered symbol counts: sub_*, rexcrt_* and __imp__* import thunks.
registered = {int(m, 16) for m in re.findall(r"\{\s*0x([0-9A-Fa-f]{8}),\s*[A-Za-z_]\w*\s*\}",
                                             open(init_cpp, encoding="utf-8").read())}


def in_pdata(a):
    i = bisect.bisect_right(starts, a) - 1
    return i >= 0 and funcs[i][0] <= a < funcs[i][1]


def word(a):
    return struct.unpack_from(">I", img, a - base)[0]


refs = {}
for name in (".rdata", ".data"):
    if name not in secs:
        continue
    va, size = secs[name]
    for o in range(va, va + size - 3, 4):
        v = struct.unpack_from(">I", img, o)[0]
        if text_lo <= v < text_hi and v % 4 == 0 and v not in registered and not in_pdata(v):
            if word(v) != 0:  # padding is never a target
                refs.setdefault(v, []).append(base + o)

# Code-built pointers: lis rX, hi ; ... ; addi/ori rY, rX, lo (within 8 insns).
for o in range(tva, tva + tsize - 4, 4):
    w = struct.unpack_from(">I", img, o)[0]
    if w >> 26 != 15 or (w >> 16) & 31 != 0:  # lis == addis rX, 0, hi
        continue
    rx, hi = (w >> 21) & 31, w & 0xFFFF
    for k in range(1, 9):
        w2 = struct.unpack_from(">I", img, o + k * 4)[0]
        op2 = w2 >> 26
        if op2 == 14 and (w2 >> 16) & 31 == rx:  # addi rY, rX, simm
            lo = w2 & 0xFFFF
            v = ((hi << 16) + (lo - 0x10000 if lo & 0x8000 else lo)) & 0xFFFFFFFF
        elif op2 == 24 and (w2 >> 21) & 31 == rx:  # ori rA, rX, uimm
            v = (hi << 16) | (w2 & 0xFFFF)
        else:
            if ((w2 >> 21) & 31) == rx and op2 not in (32, 34, 36, 38, 40, 44, 48, 50, 52, 54):
                break  # rX overwritten by something else
            continue
        if text_lo <= v < text_hi and v % 4 == 0 and v not in registered and not in_pdata(v):
            if word(v) != 0:
                refs.setdefault(v, []).append(base + o)
        break

# Targets codegen already emitted as an in-function label (e.g. the base of a
# switch branch table built with lis/addi) are not entries: registering them
# would split the function. Report them, but never emit them as entries.
gen_dir = os.path.dirname(os.path.abspath(init_cpp))
labels = set()
for fn in os.listdir(gen_dir):
    if fn.endswith(".cpp"):
        with open(os.path.join(gen_dir, fn), encoding="utf-8", errors="replace") as fh:
            labels.update(int(m, 16) for m in re.findall(r"^loc_([0-9A-F]{8}):", fh.read(), re.M))
labelled = sorted(a for a in refs if a in labels)
for a in labelled:
    del refs[a]

out = []
for a in sorted(refs):
    # Size: run until the first unconditional b/blr/bctr (inclusive), capped at 64 insns.
    n = 0
    while n < 64:
        w = word(a + n * 4)
        n += 1
        op = w >> 26
        if (op == 18 and not (w & 1)) or w in (0x4E800020, 0x4E800420):
            break
        if w == 0:
            n -= 1
            break
    out.append((a, n * 4, refs[a]))

if as_toml:
    for a, size, r in out:
        print(f"0x{a:08X} = {{}}  # ~0x{size:X} bytes, data ref 0x{r[0]:08X} (+{len(r) - 1})")
else:
    print(f"registered functions: {len(registered)}  pdata: {len(funcs)}")
    print(f"unregistered data-referenced code targets: {len(out)}")
    print(f"skipped (already an in-function label): {' '.join(f'{a:08X}' for a in labelled) or 'none'}")
    for a, size, r in out:
        print(f"  {a:08X} size 0x{size:X}  refs {len(r)} first {r[0]:08X}")
