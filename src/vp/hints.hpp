// The levels' words for the controls, put as the machine has them.
//
// The original's scripts name a PC keyboard ("by default, use left shift",
// "press space or middle mouse button to swap"). On the cabinet the player
// has arcade buttons. Spinner and buttons both work at once (2026-10-19), so
// the texts name both where the original names the turning keys. Which
// words fit is set in the service menu: SETUP (cabinet or PC). The level
// scripts stay as they are; a level that has to do something differently
// gets assets/VectorPie/<level>.lua, run after the level's own script.
#pragma once

#include "settings.hpp"

#include <optional>
#include <string>

namespace Lua {
class LuaContext;
}

namespace vp {

// HexagonGame::vpTextFilter: nullopt keeps the text, "" leaves it out.
std::optional<std::string> controlText(const std::string& original, const Settings& s);

// The line shown at the start of a level that can be swapped in.
std::string swapHint(const Settings& s);

// HexagonGame::vpAfterLevelScript.
void runLevelOverride(Lua::LuaContext& lua, const std::string& levelId, const Settings& s);

} // namespace vp
