"""Rebuild the SDK's full resolve shaders with number format aware packing.

The SDK's resolve bytecode always packs fixed-point destinations as unsigned
fractions, ignoring RB_COPY_DEST_INFO.copy_dest_number. Forza Motorsport 3
resolves its car variance shadow map (k_16_16, 4x MSAA, exp bias +10) as
SignedInteger, so every texel saturated to 0xFFFF (-1), and the ground shader
drew a dark block around the car.

The sources are upstream Xenia's (its EDRAM sample layout matches the SDK's
render target cache; xenia-canary's differs), with xenia-canary's
number-format packing and -32 k_16_16 unpack clamp ported in. Upstream binds
the destination as a raw UAV and the SDK as a typed RWBuffer<uint2/uint4>, so
the destination declaration and stores are retargeted before compiling with
FXC.

usage: build_resolve_shaders.py <xenia upstream root> <xenia-canary root>
                                <output bytecode dir>
"""
import glob, os, re, shutil, subprocess, sys, tempfile

upstream, canary, out_dir = sys.argv[1], sys.argv[2], sys.argv[3]
work = tempfile.mkdtemp(prefix="resolve_shaders_")
gpu = os.path.join(work, "xenia", "gpu", "shaders")
ui = os.path.join(work, "xenia", "ui", "shaders")
os.makedirs(gpu)
os.makedirs(ui)
for f in glob.glob(os.path.join(upstream, "src/xenia/gpu/shaders/*.xesl*")):
    shutil.copy(f, gpu)


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def replace_once(text, old, new, what):
    if text.count(old) != 1:
        sys.exit(f"{what}: expected exactly one match")
    return text.replace(old, new)


# pixel_formats.xesli: take canary's packing functions (from the number format
# defines through XePack64bpp4Pixels) and its unpack clamps.
up_pf = read(os.path.join(gpu, "pixel_formats.xesli"))
ca_pf = read(os.path.join(canary, "src/xenia/gpu/shaders/pixel_formats.xesli"))
pack_start = "// ColorFormat packing, according to the Direct3D 11.3"
pack_end = "// EDRAM color format unpacking."
for name, text in (("upstream", up_pf), ("canary", ca_pf)):
    if text.count(pack_end) != 1:
        sys.exit(f"{name} pixel_formats.xesli: packing section end not found")
ca_start = ca_pf.index("#define kXenosSurfaceNumberFormat_UnsignedRepeatingFraction")
ca_pack = ca_pf[ca_start:ca_pf.index(pack_end)]
ca_pack = re.sub(r"\n *case kXenosFormat_16_16(_16_16)?_EDRAM:", "", ca_pack)
up_pf = up_pf[:up_pf.index(pack_start)] + ca_pack + up_pf[up_pf.index(pack_end):]
up_pf = up_pf.replace("return max(float_x4_xe(-1.0f),", "return max(float_x4_xe(-32.0f),")
up_pf = up_pf.replace("return max(float_x2_xe(-1.0f),", "return max(float_x2_xe(-32.0f),")
write(os.path.join(gpu, "pixel_formats.xesli"), up_pf)

# resolve.xesli: decode RB_COPY_DEST_INFO.copy_dest_number.
resolve = read(os.path.join(gpu, "resolve.xesli"))
resolve = replace_once(resolve, "    uint dest_format;\n",
                       "    uint dest_format;\n    uint dest_num_format;\n", "resolve struct")
resolve = replace_once(
    resolve,
    "    resolve_info.dest_format = (dest_info >> 7u) & ((1u << 6u) - 1u);\n",
    "    resolve_info.dest_format = (dest_info >> 7u) & ((1u << 6u) - 1u);\n"
    "    resolve_info.dest_num_format = (dest_info >> 13u) & ((1u << 3u) - 1u);\n",
    "resolve dest_format")
write(os.path.join(gpu, "resolve.xesli"), resolve)

# Pass the number format to the packing functions.
calls = {
    "resolve_full_16bpp.xesli": [("resolve_info.dest_format);",
                                  "resolve_info.dest_format,\n      resolve_info.dest_num_format);")],
    "resolve_full_32bpp.xesli": [("resolve_info.dest_format);",
                                  "resolve_info.dest_format,\n      resolve_info.dest_num_format);")],
    "resolve_full_64bpp.xesli": [("resolve_info.dest_format, packed_01",
                                  "resolve_info.dest_format,\n      resolve_info.dest_num_format, packed_01")],
    "resolve_full_8bpp.xesli": [("XePackR8G8B8A8UNorm(pixels_0123)",
                                 "XePackR8G8B8A8(pixels_0123, resolve_info.dest_num_format)"),
                                ("XePackR8G8B8A8UNorm(pixels_4567)",
                                 "XePackR8G8B8A8(pixels_4567, resolve_info.dest_num_format)")],
}
for name, edits in calls.items():
    path = os.path.join(gpu, name)
    text = read(path)
    for old, new in edits:
        text = replace_once(text, old, new, name)
    write(path, text)

# xesl.xesli: typed destination buffer for the SDK's descriptors.
xesl = read(os.path.join(upstream, "src/xenia/ui/shaders/xesl.xesli"))
hlsl_stores = r"""  #define byte_buffer_align8_store8_xe(name, byte_address, value) \
      ((name).Store2(uint(byte_address), value))
  #define byte_buffer_align16_store16_xe(name, byte_address, value) \
      ((name).Store4(uint(byte_address), value))
"""
typed_stores = r"""  // SDK binds the resolve destination as a typed RWBuffer.
  #define byte_buffer_align8_wo_declare_xe(name, glsl_set, glsl_binding, \
                                           hlsl_t, hlsl_t_space)         \
      RWBuffer<uint2> name : register(hlsl_t, hlsl_t_space);
  #define byte_buffer_align16_wo_declare_xe(name, glsl_set, glsl_binding, \
                                            hlsl_t, hlsl_t_space)         \
      RWBuffer<uint4> name : register(hlsl_t, hlsl_t_space);
  #define byte_buffer_align8_store8_xe(name, byte_address, value) \
      ((name)[uint(byte_address) >> 3u] = (value))
  #define byte_buffer_align16_store16_xe(name, byte_address, value) \
      ((name)[uint(byte_address) >> 4u] = (value))
"""
write(os.path.join(ui, "xesl.xesli"), replace_once(xesl, hlsl_stores, typed_stores, "xesl.xesli"))

fxc = sorted(glob.glob(r"C:\Program Files (x86)\Windows Kits\10\bin\*\x64\fxc.exe"))[-1]
os.makedirs(out_dir, exist_ok=True)
for bpp in (8, 16, 32, 64, 128):
    for suffix in ("", "_scaled"):
        name = f"resolve_full_{bpp}bpp{suffix}"
        src = os.path.join(gpu, name + ".cs.xesl")
        hdr = os.path.join(work, name + "_cs.h")
        cmd = [fxc, "/D", "SHADING_LANGUAGE_HLSL_XE=1", "/I", gpu, "/Fh", hdr,
               "/T", "cs_5_1", "/Vn", name + "_cs", "/O3", "/Qstrip_reflect",
               "/Qstrip_debug", "/Qstrip_priv", "/Gfp", "/nologo", src]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"{name}: {r.stderr}")
        uav = re.search(r"dcl_uav\w*[^\n]*", read(hdr)).group(0)
        if "typed_buffer" not in uav:
            sys.exit(f"{name}: unexpected destination declaration {uav}")
        shutil.copy(hdr, os.path.join(out_dir, name + "_cs.h"))
        print(f"{name}: {uav}")
shutil.rmtree(work)
