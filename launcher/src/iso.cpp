// Forza3Launcher - Xbox 360 disc image (XDVDFS) extraction.
//
// XDVDFS: at sector 32 of the game partition sits the volume descriptor,
// "MICROSOFT*XBOX*MEDIA" followed by the root directory's sector and size.
// A directory is a binary search tree of entries, each:
//   u16 left, u16 right     (subtree offsets in 4-byte units, 0xFFFF = none)
//   u32 sector, u32 size    (file data, or the subdirectory's tree)
//   u8 attributes (0x10 = directory), u8 name length, name
// Sectors are 2048 bytes, relative to the partition. Redump images of XGD2/3
// discs carry a video partition first, so the game partition starts at one of
// a few fixed offsets; trimmed XISOs start at 0.

#include "iso.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <system_error>
#include <vector>

namespace launcher {

namespace {

constexpr uint64_t kSectorSize = 2048;
constexpr char kMagic[] = "MICROSOFT*XBOX*MEDIA";
constexpr size_t kMagicSize = sizeof(kMagic) - 1;
// Trimmed XISO, XGD3, XGD2, XGD1 game partition offsets.
constexpr uint64_t kPartitionOffsets[] = {0, 0x2080000, 0xFD90000, 0x18300000};

struct Entry {
  std::string name;
  bool directory = false;
  uint32_t sector = 0;
  uint32_t size = 0;
};

struct FileItem {
  std::filesystem::path rel;
  uint32_t sector = 0;
  uint32_t size = 0;
};

uint16_t Le16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
uint32_t Le32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

bool ReadAt(std::ifstream& file, uint64_t offset, void* data, size_t size) {
  file.clear();
  file.seekg(std::streamoff(offset));
  file.read(static_cast<char*>(data), std::streamsize(size));
  return file.gcount() == std::streamsize(size);
}

bool EqualsNoCase(const std::string& a, const char* b) {
  size_t n = std::strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

class Volume {
 public:
  explicit Volume(std::ifstream& file) : file_(file) {}

  bool Open() {
    for (uint64_t offset : kPartitionOffsets) {
      uint8_t descriptor[kSectorSize];
      if (!ReadAt(file_, offset + 32 * kSectorSize, descriptor, sizeof(descriptor))) continue;
      if (std::memcmp(descriptor, kMagic, kMagicSize) != 0) continue;
      base_ = offset;
      root_sector_ = Le32(descriptor + 20);
      root_size_ = Le32(descriptor + 24);
      return true;
    }
    return false;
  }

  bool ReadRoot(std::vector<Entry>& out) { return ReadDirectory(root_sector_, root_size_, out); }

  bool ReadDirectory(uint32_t sector, uint32_t size, std::vector<Entry>& out) {
    out.clear();
    if (size == 0) return true;  // Empty directory.
    if (size > 64u * 1024 * 1024) return false;
    std::vector<uint8_t> data(size);
    if (!ReadAt(file_, base_ + sector * kSectorSize, data.data(), size)) return false;
    // Walk the tree; `visited` guards against loops in a damaged image.
    std::vector<bool> visited(size / 4 + 1, false);
    std::vector<uint32_t> stack = {0};
    while (!stack.empty()) {
      uint32_t at = stack.back();
      stack.pop_back();
      if (at + 14 > size || visited[at / 4]) continue;
      visited[at / 4] = true;
      const uint8_t* e = data.data() + at;
      uint16_t left = Le16(e), right = Le16(e + 2);
      if (left == 0xFFFF && right == 0xFFFF && Le32(e + 4) == 0xFFFFFFFF) continue;  // Padding.
      uint8_t name_length = e[13];
      if (at + 14 + name_length > size) continue;
      Entry entry;
      entry.sector = Le32(e + 4);
      entry.size = Le32(e + 8);
      entry.directory = (e[12] & 0x10) != 0;
      entry.name.assign(reinterpret_cast<const char*>(e + 14), name_length);
      if (!entry.name.empty() && entry.name != "." && entry.name != "..") {
        out.push_back(std::move(entry));
      }
      if (left != 0 && left != 0xFFFF) stack.push_back(uint32_t(left) * 4);
      if (right != 0 && right != 0xFFFF) stack.push_back(uint32_t(right) * 4);
    }
    return true;
  }

  uint64_t FileOffset(uint32_t sector) const { return base_ + sector * kSectorSize; }

 private:
  std::ifstream& file_;
  uint64_t base_ = 0;
  uint32_t root_sector_ = 0;
  uint32_t root_size_ = 0;
};

// Collects every file under a directory, depth first, skipping $SystemUpdate.
bool CollectFiles(Volume& volume, const std::vector<Entry>& entries,
                  const std::filesystem::path& prefix, std::vector<FileItem>& files,
                  std::vector<std::filesystem::path>& dirs, int depth) {
  if (depth > 32) return false;
  for (const Entry& entry : entries) {
    if (EqualsNoCase(entry.name, "$SystemUpdate")) continue;
    // Names on disc are ASCII; reject anything that could escape the folder.
    if (entry.name.find_first_of("/\\:") != std::string::npos) return false;
    std::filesystem::path rel = prefix / entry.name;
    if (entry.directory) {
      dirs.push_back(rel);
      std::vector<Entry> children;
      if (!volume.ReadDirectory(entry.sector, entry.size, children)) return false;
      if (!CollectFiles(volume, children, rel, files, dirs, depth + 1)) return false;
    } else {
      files.push_back({rel, entry.sector, entry.size});
    }
  }
  return true;
}

void RemoveQuietly(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::remove_all(path, ec);
}

}  // namespace

DiscKind ExtractIso(const std::filesystem::path& iso, const std::filesystem::path& game_dir,
                    TaskProgress& progress, std::string& error) {
  std::ifstream file(iso, std::ios::binary);
  if (!file) {
    error = "Could not open the disc image.";
    return DiscKind::kFailed;
  }
  progress.SetStatus("Reading the disc image...");
  Volume volume(file);
  std::vector<Entry> root;
  if (!volume.Open() || !volume.ReadRoot(root)) {
    error = "This is not an Xbox 360 disc image (no XDVDFS file system found).";
    return DiscKind::kFailed;
  }

  bool bonus = std::any_of(root.begin(), root.end(), [](const Entry& e) {
    return e.directory && EqualsNoCase(e.name, "Content");
  });
  bool game = std::any_of(root.begin(), root.end(), [](const Entry& e) {
    return !e.directory && EqualsNoCase(e.name, "default.xex");
  });
  if (!bonus && !game) {
    error = "This disc has no default.xex or Content folder: not a Forza Motorsport 3 disc.";
    return DiscKind::kFailed;
  }

  std::vector<FileItem> files;
  std::vector<std::filesystem::path> dirs;
  if (!CollectFiles(volume, root, {}, files, dirs, 0)) {
    error = "The disc image's file system is damaged.";
    return DiscKind::kFailed;
  }
  uint64_t total = 0;
  for (const FileItem& f : files) total += f.size;
  progress.total = total;
  progress.done = 0;

  // Extract next to the destination, then swap it in, so a cancelled or
  // failed import never leaves a half-written game folder behind.
  std::filesystem::path dest = bonus ? game_dir / "Disc 2" : game_dir;
  std::filesystem::path temp = dest;
  temp += ".importing";
  RemoveQuietly(temp);
  std::error_code ec;
  std::filesystem::create_directories(temp, ec);
  for (const auto& dir : dirs) std::filesystem::create_directories(temp / dir, ec);

  std::vector<char> buffer(4 << 20);
  for (const FileItem& f : files) {
    progress.SetStatus("Extracting " + f.rel.generic_string());
    std::ofstream out(temp / f.rel, std::ios::binary | std::ios::trunc);
    if (!out) {
      error = "Could not write " + (temp / f.rel).string();
      RemoveQuietly(temp);
      return DiscKind::kFailed;
    }
    uint64_t offset = volume.FileOffset(f.sector);
    uint64_t left = f.size;
    while (left > 0) {
      if (progress.cancel) {
        out.close();
        RemoveQuietly(temp);
        error = "Cancelled.";
        return DiscKind::kFailed;
      }
      size_t chunk = size_t(std::min<uint64_t>(left, buffer.size()));
      if (!ReadAt(file, offset, buffer.data(), chunk)) {
        out.close();
        RemoveQuietly(temp);
        error = "The disc image ends early (incomplete or damaged): " + f.rel.generic_string();
        return DiscKind::kFailed;
      }
      out.write(buffer.data(), std::streamsize(chunk));
      if (!out) {
        out.close();
        RemoveQuietly(temp);
        error = "Write failed (is the disk full?): " + f.rel.generic_string();
        return DiscKind::kFailed;
      }
      offset += chunk;
      left -= chunk;
      progress.done += chunk;
    }
  }

  // Swap in. A game folder keeps an already imported Disc 2.
  progress.SetStatus("Finishing...");
  if (std::filesystem::exists(dest, ec)) {
    if (!bonus && std::filesystem::exists(dest / "Disc 2", ec) &&
        !std::filesystem::exists(temp / "Disc 2", ec)) {
      std::filesystem::rename(dest / "Disc 2", temp / "Disc 2", ec);
    }
    std::filesystem::remove_all(dest, ec);
    if (ec) {
      error = "Could not replace " + dest.string() + ": " + ec.message();
      return DiscKind::kFailed;
    }
  }
  std::filesystem::rename(temp, dest, ec);
  if (ec) {
    error = "Could not move the extracted files into " + dest.string() + ": " + ec.message();
    return DiscKind::kFailed;
  }
  return bonus ? DiscKind::kBonus : DiscKind::kGame;
}

}  // namespace launcher
