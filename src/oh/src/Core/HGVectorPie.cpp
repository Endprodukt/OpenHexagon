// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port: what the original's GameState and input callbacks did for
// HexagonGame, as plain calls for the VectorPie front end (src/vp).

#include "SSVOpenHexagon/Core/HexagonGame.hpp"

#include "SSVOpenHexagon/Data/LevelData.hpp"

#include "SSVOpenHexagon/Global/Config.hpp"

namespace hg {

void HexagonGame::tick(const float timescale)
{
    // GameState::onUpdate, then onPostUpdate, at Config::TIME_STEP.
    update(Config::TIME_STEP, timescale);
    postUpdate();
}

void HexagonGame::setInputs(bool mCCW, bool mCW, bool mFocus, bool mSwap)
{
    // add2StateInput(Tid::RotateCCW / RotateCW / Focus / Swap).
    inputImplCCW = mCCW;
    inputImplCW = mCW;
    inputFocused = mFocus;
    inputSwap = mSwap;
}

void HexagonGame::requestRestart()
{
    // Tid::Restart: only after a death, once the input leniency is over.
    if(deathInputIgnore <= 0.f && status.hasDied)
    {
        status.mustStateChange = StateChange::MustRestart;
    }
}

void HexagonGame::requestReplay()
{
    // Tid::Replay.
    if((deathInputIgnore <= 0.f && status.hasDied) || inReplay())
    {
        status.mustStateChange = StateChange::MustReplay;
    }
}

void HexagonGame::requestMenu()
{
    // Tid::Exit.
    goToMenu();
}

bool HexagonGame::hasDied() const noexcept
{
    return status.hasDied;
}

bool HexagonGame::isStarted() const noexcept
{
    return status.started;
}

bool HexagonGame::canRestartAfterDeath() const noexcept
{
    return status.hasDied && deathInputIgnore <= 0.f;
}

double HexagonGame::getTimeSeconds() const noexcept
{
    return status.getTimeSeconds();
}

const LevelData* HexagonGame::getLevelData() const noexcept
{
    return levelData;
}

float HexagonGame::getDifficultyMult() const noexcept
{
    return difficultyMult;
}

const std::string& HexagonGame::getLevelId() const noexcept
{
    return levelId;
}

const replay_file* HexagonGame::getActiveReplayFile() const noexcept
{
    return activeReplay.has_value() ? &activeReplay->replayFile : nullptr;
}

bool HexagonGame::takeNewPersonalBest() noexcept
{
    const bool r = vpNewPersonalBest;
    vpNewPersonalBest = false;
    return r;
}

replay_file HexagonGame::makeLastReplayFile()
{
    return death_createReplayFile();
}

} // namespace hg

// ----------------------------------------------------------------------------
// VectorPie additions that are not the original's.

#include "SSVOpenHexagon/Components/CWall.hpp"

#include <cmath>

namespace hg {

float HexagonGame::vpDegPerTick() const
{
    // CPlayer::updateInputMovement: _currentSpeed degrees per tick.
    return getPlayerSpeedMult() *
           (inputFocused ? Config::getPlayerFocusSpeed()
                         : Config::getPlayerSpeed()) *
           Config::TIME_STEP;
}

// The attract mode's demo player when no replay has been saved yet: not
// the original's (it has none). It looks along 72 rays from the center for
// the nearest wall, and turns towards the ray with the most room that it can
// reach without passing a wall close to the player.
void HexagonGame::vpAutopilot(bool& ccw, bool& cw) const
{
    constexpr int RAYS = 72;
    constexpr float TAU = 6.2831853f;

    float room[RAYS];

    for(int i = 0; i < RAYS; ++i)
    {
        const float a = TAU * i / RAYS;
        const sf::Vector2f dir{std::cos(a), std::sin(a)};
        float best = 1e9f;

        for(const CWall& w : walls)
        {
            const auto& v = w.getVertexPositions();
            for(int k = 0; k < 4; ++k)
            {
                const sf::Vector2f p = v[k], q = v[(k + 1) % 4];
                const sf::Vector2f e = q - p;
                const float den = dir.x * e.y - dir.y * e.x;
                if(std::fabs(den) < 1e-6f) continue;
                const float t = (p.x * e.y - p.y * e.x) / den;      // along the ray
                const float u = (p.x * dir.y - p.y * dir.x) / den;  // along the edge
                if(t > 0.f && u >= 0.f && u <= 1.f && t < best) best = t;
            }
        }

        room[i] = best;
    }

    float pa = player.getPlayerAngle();
    pa = std::fmod(pa, TAU);
    if(pa < 0.f) pa += TAU;
    const int here = static_cast<int>(pa / TAU * RAYS + 0.5f) % RAYS;

    const float near = status.radius + 45.f;
    int target = here;
    float bestScore = room[here];

    for(const int dirStep : {1, -1})
    {
        for(int k = 1; k < RAYS / 2; ++k)
        {
            const int i = ((here + dirStep * k) % RAYS + RAYS) % RAYS;
            if(room[i] < near) break; // a wall beside the player: no way past
            const float score = room[i] - k * 6.f;
            if(score > bestScore + 20.f)
            {
                bestScore = score;
                target = i;
            }
        }
    }

    int delta = target - here;
    if(delta > RAYS / 2) delta -= RAYS;
    if(delta < -RAYS / 2) delta += RAYS;

    ccw = delta < 0;
    cw = delta > 0;
}

} // namespace hg
