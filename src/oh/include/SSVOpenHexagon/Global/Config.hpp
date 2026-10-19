// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port: the original Config is a JSON-backed table of linked
// values with windows, keyboard and joystick binds. Here it keeps only the
// getters the game core calls, with the original defaults (see
// reference/SSVOpenHexagon/src/SSVOpenHexagon/Global/Config.cpp, the
// X_LINKEDVALUES list). The cabinet's settings live in the service menu
// (src/vp/service.cpp), which writes them through the setters below.

#pragma once

#include "SSVOpenHexagon/Global/Version.hpp"
#include "SSVOpenHexagon/Utils/Casts.hpp"

#include <string>

namespace hg::Config {

inline constexpr float TICKS_PER_SECOND = 240.f;
inline constexpr float TIME_STEP = 60.f / TICKS_PER_SECOND;
inline constexpr float TIME_SLICE = 60.f / TICKS_PER_SECOND;

[[nodiscard]] bool isEligibleForScore();
[[nodiscard]] const std::string& getUneligibilityReason();

void setOfficial(bool mOfficial);
void setDebug(bool mDebug);
void setNoPulse(bool mNoPulse);
void setNoRotation(bool mNoRotation);
void setNoBackground(bool mNoBackground);
void setBlackAndWhite(bool mBlackAndWhite);
void setNoSound(bool mNoSound);
void setNoMusic(bool mNoMusic);
void set3D(bool m3D);
void set3DMultiplier(float x);
void setInvincible(bool mInvincible);
void setFlash(bool mFlash);
void setBeatPulse(bool x);
void setTimescale(float mX);
void setShowMessages(bool x);
void setCameraShakeMultiplier(float x);
void setRotateToStart(bool x);
void setAutoRestart(bool x);

[[nodiscard]] bool getOfficial();
[[nodiscard]] bool getDebug();
[[nodiscard]] bool getNoPulse();
[[nodiscard]] bool getNoRotation();
[[nodiscard]] bool getNoBackground();
[[nodiscard]] bool getBlackAndWhite();
[[nodiscard]] bool getNoSound();
[[nodiscard]] bool getNoMusic();
[[nodiscard]] bool get3D();
[[nodiscard]] float get3DMultiplier();
[[nodiscard]] bool getShaders();
[[nodiscard]] bool getInvincible();
[[nodiscard]] bool getAutoRestart();
[[nodiscard]] bool getFlash();
[[nodiscard]] bool getBeatPulse();
[[nodiscard]] bool getMusicSpeedDMSync();
[[nodiscard]] float getMusicSpeedMult();
[[nodiscard]] bool getShowMessages();
[[nodiscard]] bool getShowTrackedVariables();
[[nodiscard]] bool getDarkenUnevenBackgroundChunk();
[[nodiscard]] bool getRotateToStart();
[[nodiscard]] float getTimescale();
[[nodiscard]] float getCameraShakeMultiplier();
[[nodiscard]] float getAngleTiltIntensity();
[[nodiscard]] bool getShowPlayerTrail();
[[nodiscard]] unsigned int getPlayerTrailAlpha();
[[nodiscard]] float getPlayerTrailScale();
[[nodiscard]] float getPlayerTrailDecay();
[[nodiscard]] bool getPlayerTrailHasSwapColor();
[[nodiscard]] bool getShowSwapParticles();
[[nodiscard]] bool getPlaySwapReadySound();
[[nodiscard]] bool getShowSwapBlinkingEffect();
[[nodiscard]] bool getUseLuaFileCache();
[[nodiscard]] bool getDisableGameRendering();

[[nodiscard]] float getSpawnDistance();
[[nodiscard]] float getZoomFactor();
[[nodiscard]] float getPlayerSpeed();
[[nodiscard]] float getPlayerFocusSpeed();
[[nodiscard]] float getPlayerSize();
[[nodiscard]] float getWidth() noexcept;
[[nodiscard]] float getHeight() noexcept;

} // namespace hg::Config
