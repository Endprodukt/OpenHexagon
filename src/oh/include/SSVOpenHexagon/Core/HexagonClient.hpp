// VectorPie port: no online leaderboards on the cabinet. The game holds a
// null HexagonClient pointer, as the original does when offline; see
// Steam.hpp.
#pragma once

#include <cstdint>
#include <string>

namespace hg {

struct compressed_replay_file;

class HexagonClient
{
public:
    enum class State : std::uint8_t
    {
        Disconnected = 0,
        LoggedIn_Ready = 6,
    };

    void update() {}
    [[nodiscard]] State getState() const { return State::Disconnected; }
    bool trySendCompressedReplay(const std::string&, const compressed_replay_file&) { return false; }
    bool trySendStartedGame(const std::string&) { return false; }
    [[nodiscard]] bool isLevelSupportedByServer(const std::string&) const { return false; }
};

} // namespace hg
