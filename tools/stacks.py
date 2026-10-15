"""Condense an lldb 'thread backtrace all' dump to guest-relevant frames.

For each thread that runs recompiled code, prints the innermost host frames
(what it is blocked in) and the first guest sub_XXXXXXXX frames.

usage: stacks.py <lldb_output.txt> [max_guest_frames]
"""
import re, sys

text = open(sys.argv[1], encoding="utf-8", errors="replace").read()
max_guest = int(sys.argv[2]) if len(sys.argv) > 2 else 8
blocks = re.split(r"\n(?=[ *] thread #)", text)
frame_re = re.compile(r"frame #(\d+): 0x[0-9a-f]+ (\S+?)`([^\s(]+)")

for b in blocks:
    head = b.strip().splitlines()[0] if b.strip() else ""
    if "thread #" not in head:
        continue
    frames = frame_re.findall(b)
    guest = [(n, f) for n, mod, f in frames if re.match(r"(sub_|__imp__sub_|rexcrt_)", f)]
    if not guest:
        continue
    print(head.strip()[:110])
    for n, mod, f in frames:
        if re.match(r"(sub_|__imp__sub_|rexcrt_)", f):
            break
        print(f"    #{n:>2} {mod}`{f[:90]}")
    for n, f in guest[:max_guest]:
        print(f"    #{n:>2} {f}")
    print()
