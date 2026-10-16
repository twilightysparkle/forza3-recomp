// Forza3Launcher - game file verification against game_files.sha256.

#include "verify.h"

#include <windows.h>
#include <bcrypt.h>

#include <fstream>
#include <sstream>
#include <system_error>

namespace launcher {

std::vector<ManifestEntry> LoadManifest(const std::filesystem::path& file) {
  std::vector<ManifestEntry> entries;
  std::ifstream in(file);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    std::istringstream fields(line);
    ManifestEntry entry;
    fields >> entry.sha256 >> entry.size;
    std::getline(fields >> std::ws, entry.path);
    if (entry.sha256.size() != 64 || entry.path.empty()) continue;
    entry.bonus = entry.path.rfind("Disc 2/", 0) == 0;
    entries.push_back(std::move(entry));
  }
  return entries;
}

std::string Sha256File(const std::filesystem::path& file, TaskProgress* progress) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return {};
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return {};
  std::string result;
  if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
    std::vector<char> buffer(4 << 20);
    bool cancelled = false;
    while (in) {
      in.read(buffer.data(), std::streamsize(buffer.size()));
      std::streamsize got = in.gcount();
      if (got <= 0) break;
      BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(got), 0);
      if (progress) {
        progress->done += uint64_t(got);
        if (progress->cancel) {
          cancelled = true;
          break;
        }
      }
    }
    unsigned char digest[32];
    if (!cancelled && BCryptFinishHash(hash, digest, sizeof(digest), 0) == 0) {
      static const char kHex[] = "0123456789abcdef";
      for (unsigned char b : digest) {
        result += kHex[b >> 4];
        result += kHex[b & 15];
      }
    }
    BCryptDestroyHash(hash);
  }
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return result;
}

bool VerifyGame(const std::filesystem::path& game_dir, const std::vector<ManifestEntry>& manifest,
                TaskProgress& progress, VerifyReport& report) {
  std::error_code ec;
  report = {};
  uint64_t total = 0;
  for (const auto& entry : manifest) total += entry.size;
  progress.total = total;
  progress.done = 0;
  for (const auto& entry : manifest) {
    if (progress.cancel) return false;
    std::filesystem::path path = game_dir / std::filesystem::path(entry.path).make_preferred();
    progress.SetStatus("Checking " + entry.path);
    ++report.checked;
    if (!std::filesystem::is_regular_file(path, ec)) {
      report.missing.push_back(entry.path);
      progress.done += entry.size;
      continue;
    }
    if (std::filesystem::file_size(path, ec) != entry.size) {
      report.wrong_size.push_back(entry.path);
      progress.done += entry.size;
      continue;
    }
    std::string hash = Sha256File(path, &progress);
    if (progress.cancel) return false;
    if (hash != entry.sha256) report.wrong_hash.push_back(entry.path);
  }
  return true;
}

}  // namespace launcher
