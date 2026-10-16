"""Write launcher/data/game_files.sha256 from a game the launcher extracted.

usage: make_manifest.py <game dir> <out file>

Extract known-good disc images with the launcher's own importer first, so the
list matches what players get:
  Forza3Launcher.exe --extract-iso "<Disc 1>.iso" <dir>
  Forza3Launcher.exe --extract-iso "<Disc 2>.iso" <dir>
One line per file, `<sha256> <size> <path>`, path '/'-separated and relative to
the game folder; Disc 2 files are under "Disc 2/". Hashes only, no game data.
"""
import hashlib, os, sys

game, out = sys.argv[1], sys.argv[2]
lines = []
for dirpath, dirnames, filenames in os.walk(game):
    dirnames.sort()
    for name in sorted(filenames):
        full = os.path.join(dirpath, name)
        rel = os.path.relpath(full, game).replace(os.sep, "/")
        h = hashlib.sha256()
        with open(full, "rb") as f:
            for block in iter(lambda: f.read(1 << 22), b""):
                h.update(block)
        lines.append(f"{h.hexdigest()} {os.path.getsize(full)} {rel}")
lines.sort(key=lambda l: l.split(" ", 2)[2].lower())
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as f:
    f.write("# Forza Motorsport 3 Ultimate Collection (USA), title 4D53084D: files of the\n"
            "# Redump disc images the port was built from. <sha256> <size> <path>;\n"
            "# 'Disc 2/' is the Content Install Disc. Made by tools/make_manifest.py.\n")
    f.write("\n".join(lines) + "\n")
print(f"{len(lines)} files")
