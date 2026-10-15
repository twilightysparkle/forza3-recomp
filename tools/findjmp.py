"""Find setjmp/longjmp candidates in a decrypted XEX image.

setjmp saves the non-volatile GPRs r14-r31 relative to r3 (stw/std rN, X(r3))
and returns 0; longjmp loads them back from r3 (lwz/ld rN, X(r3)). The CRT's
versions are hand-written leaves, often without .pdata, so this scans all of
.text for runs of such instructions and prints a listing of each hit.

usage: findjmp.py <default.xex>
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xexdis import load_image, pe_sections  # noqa: E402
import capstone

base, img = load_image(sys.argv[1])
tva, tsize = pe_sections(img)[".text"]
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)


def classify(w):
    op, rd, ra = w >> 26, (w >> 21) & 31, (w >> 16) & 31
    if ra != 3 or not (14 <= rd <= 31):
        return None
    if op == 36 or (op == 62 and (w & 3) == 0):
        return ("save", rd)
    if op == 32 or (op == 58 and (w & 3) == 0):
        return ("load", rd)
    return None


words = [struct.unpack_from(">I", img, o)[0] for o in range(tva, tva + tsize, 4)]
i = 0
while i < len(words):
    c = classify(words[i])
    if not c:
        i += 1
        continue
    kind, regs, j, gap = c[0], set(), i, 0
    while j < len(words) and gap <= 3:
        c2 = classify(words[j])
        if c2 and c2[0] == kind:
            regs.add(c2[1]); gap = 0
        else:
            gap += 1
        j += 1
    if len(regs) >= 16:
        a = base + tva + i * 4
        print(f"== {kind} run of r14-r31 ({len(regs)} regs) at {a:08X}")
        lo = max(0, i - 6)
        for k in range(lo, min(len(words), j + 4)):
            addr = base + tva + k * 4
            ins = next(md.disasm(struct.pack(">I", words[k]), addr), None)
            txt = f"{ins.mnemonic:8} {ins.op_str}" if ins else f".word 0x{words[k]:08X}"
            if not (ins and classify(words[k])):
                print(f"   {addr:08X}  {txt}")
        i = j
    else:
        i += 1
