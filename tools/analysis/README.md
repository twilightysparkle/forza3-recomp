# Analysis scripts

Scripts used to find what `rexglue codegen` missed and to debug runtime
problems. Run them with `python -E -P` (needs
`pip install --user cryptography capstone`). They work on your own
`default.xex`; nothing here contains game data.

| Script | What it does |
|---|---|
| `xexinfo.py <xex>` | Header, title ID, media ID, version, statically linked libraries |
| `xexdis.py <xex> <image.bin> addr[:before[:after]] ...` | Decrypt the image to `image.bin`, check `.pdata`, disassemble around addresses |
| `scan.py <image.bin> <start> <end> [all]` | Word-by-word PPC listing that skips undecodable (VMX128) words |
| `ptrscan.py <xex> <generated/default/forza3_init.cpp> [--toml]` | Code addresses referenced from data or built in code (vtables, callbacks) that codegen didn't register; `--toml` prints manifest entries |
| `fibers.py <xex> <image.bin> addr ...` | Code touching the thread's fiber slot (`KTHREAD+0x164`) and callers of addresses: finds the XAPI fiber functions for `[entrypoint.rexcrt]` |
| `findjmp.py <xex>` | `setjmp` / `longjmp` candidates for `setjmp_address` / `longjmp_address` |
| `stacks.py <lldb_backtrace.txt>` | Condense `thread backtrace all` from a crash dump to the guest frames |
| `stripes.py <png> ...` | Score horizontal banding in window captures (the old road-flickering bug) |
| `port_dump_to_canary.py <rexglue-sdk> <xenia-canary>` | Port a local SDK texture-dump debug hook into xenia-canary, to compare frame dumps between the two (the hook itself is not part of `patches/`) |
