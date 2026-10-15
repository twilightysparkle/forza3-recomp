import struct, sys

KEYS = {
    0x00010001: "resource_info", 0x000002FF: "base_file_desc?", 0x00010100: "entry_point",
    0x00010201: "image_base", 0x000103FF: "import_libraries", 0x00018002: "checksum_timestamp",
    0x000183FF: "original_pe_name", 0x00040006: "execution_info", 0x00020200: "default_stack_size",
    0x00020301: "default_heap_size", 0x00030000: "system_flags", 0x00040404: "game_ratings",
    0x00008105: "title_workspace_size", 0x000003FF: "file_format_info", 0x00020104: "tls_info",
    0x000200FF: "static_libraries? (0x200FF)", 0x000300FF: "?", 0x00E10402: "?",
}

d = open(sys.argv[1], "rb").read()
magic, modflags, pe_off, _, sec_off, nhdr = struct.unpack(">4sIIIII", d[:24])
print("magic", magic, "module_flags", hex(modflags), "pe_data_offset", hex(pe_off), "headers", nhdr)
for i in range(nhdr):
    key, val = struct.unpack(">II", d[24 + i * 8: 32 + i * 8])
    name = KEYS.get(key, "?")
    print(f"  {key:08X} {name:22} {val:08X}")
    if key == 0x00040006:
        media_id, ver, base_ver, title_id, plat, etype, disc, ndisc = struct.unpack(">IIIIBBBB", d[val:val + 20])
        print(f"    title_id={title_id:08X} media_id={media_id:08X} version={ver:08X} disc {disc}/{ndisc}")
    if key == 0x000003FF:
        size, enc, comp = struct.unpack(">IHH", d[val:val + 8])
        print(f"    encryption={enc} (1=encrypted) compression={comp} (1=basic,2=normal/LZX)")
    if key == 0x000183FF:
        size = struct.unpack(">I", d[val:val + 4])[0]
        print("    ", d[val + 4: val + size].split(b"\0")[0].decode(errors="replace"))
    if key == 0x000103FF:
        size, strsize, nstr = struct.unpack(">III", d[val:val + 12])
        names = d[val + 12: val + 12 + strsize].split(b"\0")
        print("    imports:", [n.decode() for n in names if n])
    if key == 0x000200FF:
        size = struct.unpack(">I", d[val:val + 4])[0]
        for j in range((size - 4) // 16):
            e = d[val + 4 + j * 16: val + 20 + j * 16]
            nm = e[:8].split(b"\0")[0].decode(errors="replace")
            a, b, c, q = struct.unpack(">HHHH", e[8:])
            print(f"    lib {nm:8} {a}.{b}.{c}.{q & 0x7FFF}")
