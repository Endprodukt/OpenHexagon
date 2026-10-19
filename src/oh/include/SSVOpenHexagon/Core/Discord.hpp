// VectorPie port: no Discord on the cabinet; see Steam.hpp.
#pragma once

#include <string>

namespace hg::Discord {

class discord_manager
{
public:
    bool run_callbacks() { return false; }
    bool set_rich_presence_on_replay() { return false; }
    bool set_rich_presence_in_game(const std::string&, const std::string&, bool = false)
    {
        return false;
    }
};

} // namespace hg::Discord
