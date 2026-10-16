// Forza3Launcher - the player's settings, shared with the game.
//
// The game reads <exe dir>\forza3.toml (shipped) and then forza3.user.toml
// (the player's changes). The launcher and the in-game F2 menu both write
// forza3.user.toml with the same keys, listing only values that differ from
// the shipped ones.

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace launcher {

struct BindDef {
  const char* cvar;   // keybind_<...>
  const char* label;  // Action shown in the Controls tab.
  const char* group;
};

// Key bindings the Controls tab offers, in display order.
const std::vector<BindDef>& BindDefs();

class Settings {
 public:
  // Reads forza3.toml and forza3.user.toml from `exe_dir`.
  void Load(const std::filesystem::path& exe_dir);
  // Writes forza3.user.toml. Returns false on a write error.
  bool Save() const;

  std::string Get(const std::string& key) const;
  void Set(const std::string& key, const std::string& value);
  bool GetBool(const std::string& key) const { return Get(key) == "true"; }
  void SetBool(const std::string& key, bool value) { Set(key, value ? "true" : "false"); }
  int GetInt(const std::string& key) const;
  double GetDouble(const std::string& key) const;

  // Back to the shipped value.
  void Reset(const std::string& key);
  const std::string& Shipped(const std::string& key) const;
  bool dirty() const { return dirty_; }
  void ClearDirty() { dirty_ = false; }

 private:
  std::filesystem::path user_file_;
  std::map<std::string, std::string> shipped_;  // forza3.toml over SDK defaults.
  std::map<std::string, std::string> values_;
  bool dirty_ = false;
};

}  // namespace launcher
