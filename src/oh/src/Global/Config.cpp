// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port: see Config.hpp. Defaults as in the original's
// X_LINKEDVALUES list; the screen is the original's reference resolution
// 1024 x 768 with the auto zoom factor (recalculateSizes) of 1.

#include "SSVOpenHexagon/Global/Config.hpp"

#include <string>

namespace hg::Config {

namespace {

bool official{true};
bool debug{false};
bool noPulse{false};
bool noRotation{false};
bool noBackground{false};
bool blackAndWhite{false};
bool noSound{false};
bool noMusic{false};
bool _3DEnabled{true};
float _3DMultiplier{1.f};
bool invincible{false};
bool flashEnabled{true};
bool beatPulse{true};
float timescale{1.f};
bool showMessages{true};
float cameraShakeMultiplier{1.f};
bool rotateToStart{false};
bool autoRestart{false};

constexpr float defaultSpawnDistance{1600};
constexpr float playerSpeedDefault{9.45f};
constexpr float playerFocusSpeedDefault{4.625f};
constexpr float playerSizeDefault{7.3f};

std::string uneligibilityReason;

} // namespace

bool isEligibleForScore()
{
    // isEligibleForScore: the settings that make a score not comparable.
    if(getInvincible())
    {
        uneligibilityReason = "invincibility on";
        return false;
    }

    if(getNoPulse())
    {
        uneligibilityReason = "pulse off";
        return false;
    }

    if(getNoRotation())
    {
        uneligibilityReason = "rotation off";
        return false;
    }

    // VectorPie: the original's official mode allows no timescale either.
    if(getTimescale() != 1.f)
    {
        uneligibilityReason = "timescale changed";
        return false;
    }

    return true;
}

const std::string& getUneligibilityReason()
{
    return uneligibilityReason;
}

void setOfficial(bool x) { official = x; }
void setDebug(bool x) { debug = x; }
void setNoPulse(bool x) { noPulse = x; }
void setNoRotation(bool x) { noRotation = x; }
void setNoBackground(bool x) { noBackground = x; }
void setBlackAndWhite(bool x) { blackAndWhite = x; }
void setNoSound(bool x) { noSound = x; }
void setNoMusic(bool x) { noMusic = x; }
void set3D(bool x) { _3DEnabled = x; }
void set3DMultiplier(float x) { _3DMultiplier = x; }
void setInvincible(bool x) { invincible = x; }
void setFlash(bool x) { flashEnabled = x; }
void setBeatPulse(bool x) { beatPulse = x; }
void setTimescale(float x) { timescale = x; }
void setShowMessages(bool x) { showMessages = x; }
void setCameraShakeMultiplier(float x) { cameraShakeMultiplier = x; }
void setRotateToStart(bool x) { rotateToStart = x; }
void setAutoRestart(bool x) { autoRestart = x; }

bool getOfficial() { return official; }
bool getDebug() { return debug; }
bool getNoPulse() { return noPulse; }
bool getNoRotation() { return noRotation; }
bool getNoBackground() { return noBackground; }
bool getBlackAndWhite() { return blackAndWhite; }
bool getNoSound() { return noSound; }
bool getNoMusic() { return noMusic; }
bool get3D() { return _3DEnabled; }
float get3DMultiplier() { return _3DMultiplier; }
bool getShaders() { return false; } // VectorPie: no shaders on a vector board
bool getInvincible() { return invincible; }
bool getAutoRestart() { return autoRestart; }
bool getFlash() { return flashEnabled; }
bool getBeatPulse() { return beatPulse; }
bool getMusicSpeedDMSync() { return true; }
float getMusicSpeedMult() { return 1.f; }
bool getShowMessages() { return showMessages; }
bool getShowTrackedVariables() { return true; }
bool getDarkenUnevenBackgroundChunk() { return true; }
bool getRotateToStart() { return rotateToStart; }
float getTimescale() { return timescale; }
float getCameraShakeMultiplier() { return cameraShakeMultiplier; }
float getAngleTiltIntensity() { return 1.f; }
bool getShowPlayerTrail() { return true; }
unsigned int getPlayerTrailAlpha() { return 35; }
float getPlayerTrailScale() { return 0.9f; }
float getPlayerTrailDecay() { return 3.0f; }
bool getPlayerTrailHasSwapColor() { return true; }
bool getShowSwapParticles() { return true; }
bool getPlaySwapReadySound() { return true; }
bool getShowSwapBlinkingEffect() { return true; }
bool getUseLuaFileCache() { return false; }
bool getDisableGameRendering() { return false; }

float getSpawnDistance() { return defaultSpawnDistance; }
float getZoomFactor() { return 1.f; }
float getPlayerSpeed() { return playerSpeedDefault; }
float getPlayerFocusSpeed() { return playerFocusSpeedDefault; }
float getPlayerSize() { return playerSizeDefault; }
float getWidth() noexcept { return 1024.f; }
float getHeight() noexcept { return 768.f; }

} // namespace hg::Config
