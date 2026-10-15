"""Word-by-word PPC listing of a decrypted image range, skipping undecodable words.

usage: scan.py <image.bin> <start_hex> <end_hex> [all]
"""
import sys
import capstone

BASE = 0x82000000
img = open(sys.argv[1], "rb").read()
start, end = int(sys.argv[2], 16), int(sys.argv[3], 16)
show_all = len(sys.argv) > 4
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
KEEP = ("b", "bl", "blr", "bctr", "bctrl", "mtctr", "mtlr", "mflr", "mfcr", "mtcrf", "mtfsf", "mffs",
        "lwz", "stw", "sync", "isync", "twi", "tw", "li", "lis", "addi", "mr", "cmpwi", "cmplwi")
skipped = 0
for a in range(start, end, 4):
    w = img[a - BASE:a - BASE + 4]
    ins = next(md.disasm(w, a), None)
    if ins is None:
        skipped += 1
        if show_all:
            print(f"   {a:08X}  .word    0x{w.hex().upper()}")
        continue
    m = ins.mnemonic.rstrip("+-")
    if show_all or m in KEEP or m.startswith("b"):
        print(f"   {a:08X}  {ins.mnemonic:8} {ins.op_str}")
print(f"({skipped} undecodable words, VMX128 or data)")
