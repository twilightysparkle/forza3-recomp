// forza3 - default game and user data locations

#pragma once

#include <rex/rex_app.h>

namespace forza3 {

// Lets forza3.exe start with no arguments. Without --game_data_root, finds the
// extracted disc as a "game" or "assets" folder (holding default.xex) next to
// the exe or in a parent folder. Without --user_data_root, keeps saves and
// installed content in a "userdata" folder beside it (portable, instead of
// Documents\forza3).
void ApplyDefaultPaths(rex::PathConfig& paths);

}  // namespace forza3
