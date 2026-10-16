// Forza3Launcher - local profiles, each with its own save data.

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace launcher {

// userdata/
//   profiles\<name>\        one profile's --user_data_root (saves, settings)
//     0000000000000000\     junction to shared\0000000000000000
//   shared\0000000000000000\  installed Disc 2 content, shared by all profiles
//   cache\                  shader cache, shared (--cache_root)
//   deleted-profiles\       deleted profiles, kept in case of a mistake
class Profiles {
 public:
  explicit Profiles(std::filesystem::path userdata);

  const std::filesystem::path& userdata() const { return userdata_; }
  std::filesystem::path ProfileDir(const std::string& name) const;
  std::filesystem::path CacheDir() const { return userdata_ / "cache"; }

  std::vector<std::string> List() const;
  bool Create(const std::string& name, std::string& error);
  bool Delete(const std::string& name, std::string& error);

  // Turns a pre-launcher userdata\ (one save straight inside it, as forza3.exe
  // run on its own creates) into a profile named `name`. No-op otherwise.
  void MigrateLegacy(const std::string& name);

  // Makes sure the profile shares the installed Disc 2 content.
  bool LinkSharedContent(const std::string& name, std::string& error) const;

 private:
  std::filesystem::path userdata_;
};

// 1-15 characters: letters, digits and single spaces (an Xbox gamertag).
bool IsValidProfileName(const std::string& name);

}  // namespace launcher
