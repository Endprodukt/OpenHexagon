// VectorPie port: no Steam on the cabinet. The game holds a null
// steam_manager pointer, as the original does when Steam is not running;
// these declarations only let the guarded calls compile.
#pragma once

#include <string_view>

namespace hg::Steam {

class steam_manager
{
public:
    bool run_callbacks() { return false; }
    bool unlock_achievement(std::string_view) { return false; }
    bool set_rich_presence_in_game(std::string_view, std::string_view, std::string_view)
    {
        return false;
    }
};

} // namespace hg::Steam
