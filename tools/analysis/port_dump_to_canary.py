"""Port the SDK's texture-dump/resolve-log debug hook into a xenia-canary checkout.

Copies OnGuestSwap/DumpDrawTextures from the SDK's texture cache and the
resolve log line from its draw util, translating names to canary's
(REXCVAR_GET -> cvars::, REXGPU_INFO -> XELOGI, rex:: -> xe::), so dumps
from both emulators are directly comparable.

usage: port_dump_to_canary.py <rexglue-sdk dir> <xenia-canary dir>
"""
import re, sys
from pathlib import Path

sdk, canary = Path(sys.argv[1]), Path(sys.argv[2])
gpu = canary / "src" / "xenia" / "gpu"


def rd(p):
    return p.read_text(encoding="utf-8")


def wr(p, s):
    p.write_text(s, encoding="utf-8", newline="\n")


def extract(src, start_marker, end_marker):
    i = src.index(start_marker)
    j = src.index(end_marker, i)
    return src[i:j]


def translate(code):
    code = code.replace("REXCVAR_GET(texture_dump_dir)", "cvars::texture_dump_dir")
    code = code.replace("REXCVAR_GET(debug_resolve_16_16_raw)", "false")
    code = code.replace("REXGPU_INFO(", "XELOGI(")
    code = code.replace("rex::bit_scan_forward", "xe::bit_scan_forward")
    code = code.replace("rex::", "xe::")
    return code


def once(s, old, new, what):
    if old not in s:
        raise SystemExit(f"anchor not found: {what}")
    return s.replace(old, new, 1)


# --- texture_cache.h ---------------------------------------------------------
h = gpu / "texture_cache.h"
s = rd(h)
if "OnGuestSwap" not in s:
    s = once(s, "#include <cstdint>\n", "#include <atomic>\n#include <cstdint>\n#include <cstdio>\n"
             "#include <unordered_set>\n", "texture_cache.h includes")
    s = once(s, "class TextureCache {",
             "// True while --texture_dump_dir is capturing a guest frame (resolves get logged).\n"
             "extern std::atomic<bool> g_texture_dump_frame_active;\n\nclass TextureCache {",
             "class TextureCache")
    s = once(s, "  virtual void RequestTextures(uint32_t used_texture_mask);\n",
             "  virtual void RequestTextures(uint32_t used_texture_mask);\n\n"
             "  // Debug texture dump (--texture_dump_dir), ported from rexglue-sdk.\n"
             "  void SetDumpDrawShaders(uint64_t vertex_shader_hash, uint64_t pixel_shader_hash) {\n"
             "    dump_vertex_shader_hash_ = vertex_shader_hash;\n"
             "    dump_pixel_shader_hash_ = pixel_shader_hash;\n  }\n  void OnGuestSwap();\n",
             "RequestTextures decl")
    # Last 'private:' section of the class.
    k = s.rindex(" private:\n")
    s = s[:k] + (" private:\n  void DumpDrawTextures(uint32_t used_texture_mask);\n\n"
                 "  FILE* dump_index_ = nullptr;\n  uint32_t dump_draw_ = 0;\n  uint32_t dump_frame_ = 0;\n"
                 "  uint64_t dump_vertex_shader_hash_ = 0;\n  uint64_t dump_pixel_shader_hash_ = 0;\n"
                 "  std::unordered_set<uint64_t> dump_written_;\n") + s[k + len(" private:\n"):]
    wr(h, s)

# --- shared_memory.h: memory() public -----------------------------------------
h = gpu / "shared_memory.h"
s = rd(h)
if "public for debug dumps" not in s:
    s = s.replace("  Memory& memory() const { return memory_; }\n", "", 1)
    s = once(s, " protected:\n", "  // Guest memory (public for debug dumps such as --texture_dump_dir).\n"
             "  Memory& memory() const { return memory_; }\n\n protected:\n", "shared_memory protected")
    wr(h, s)

# --- texture_cache.cc ----------------------------------------------------------
sdk_cache = rd(sdk / "src" / "graphics" / "pipeline" / "texture" / "cache.cpp")
hook = extract(sdk_cache, "std::atomic<bool> g_texture_dump_frame_active", "void TextureCache::RequestTextures")
hook = translate(hook)
c = gpu / "texture_cache.cc"
s = rd(c)
if "OnGuestSwap" not in s:
    s = once(s, '#include "xenia/gpu/texture_cache.h"\n',
             '#include "xenia/gpu/texture_cache.h"\n\n#include <filesystem>\n#include <system_error>\n',
             "texture_cache.cc include")
    s = once(s, "DEFINE_bool(tiled_shared_memory",
             'DEFINE_string(texture_dump_dir, "",\n'
             '              "Debug: folder watched for a TRIGGER file; when it appears, every texture "\n'
             '              "sampled during the next guest frame is logged and dumped there",\n'
             '              "GPU");\n\nDEFINE_bool(tiled_shared_memory', "cvar")
    s = once(s, "void TextureCache::RequestTextures(uint32_t used_texture_mask) {\n",
             hook + "void TextureCache::RequestTextures(uint32_t used_texture_mask) {\n"
             "  if (dump_index_) {\n    DumpDrawTextures(used_texture_mask);\n  }\n",
             "RequestTextures def")
    wr(c, s)

# --- d3d12_command_processor.cc ---------------------------------------------------
c = gpu / "d3d12" / "d3d12_command_processor.cc"
s = rd(c)
if "SetDumpDrawShaders" not in s:
    s = once(s, "  texture_cache_->RequestTextures(used_texture_mask);\n",
             "  texture_cache_->SetDumpDrawShaders(\n      vertex_shader->ucode_data_hash(),\n"
             "      pixel_shader != nullptr ? pixel_shader->ucode_data_hash() : 0);\n"
             "  texture_cache_->RequestTextures(used_texture_mask);\n", "RequestTextures call")
    m = re.search(r"void D3D12CommandProcessor::IssueSwap\([^)]*\)\s*\{\n", s)
    if not m:
        raise SystemExit("anchor not found: IssueSwap")
    s = s[:m.end()] + "  texture_cache_->OnGuestSwap();\n" + s[m.end():]
    wr(c, s)

# --- draw_util.cc: resolve log -----------------------------------------------------
sdk_draw = rd(sdk / "src" / "graphics" / "util" / "draw.cpp")
log = extract(sdk_draw, "  if (g_texture_dump_frame_active.load", "  REXGPU_TRACE(")
log = translate(log)
c = gpu / "draw_util.cc"
s = rd(c)
if "Texture dump resolve" not in s:
    s = once(s, "namespace draw_util {",
             "extern std::atomic<bool> g_texture_dump_frame_active;\n\nnamespace draw_util {",
             "draw_util namespace")
    s = once(s, '#if 0\n  XELOGD(\n      "Resolve: ', log + '#if 0\n  XELOGD(\n      "Resolve: ',
             "resolve trace")
    s = once(s, '#include "xenia/gpu/draw_util.h"\n', '#include "xenia/gpu/draw_util.h"\n\n#include <atomic>\n',
             "draw_util include")
    wr(c, s)

print("ported")
