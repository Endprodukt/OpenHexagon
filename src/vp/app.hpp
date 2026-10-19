// The VectorPie front end: the game loop around the original's HexagonGame,
// the two modes and their screens.
//
// HOME (the original): choose a level and a difficulty, play, restart or
// replay after a death, back to the level choice. Personal bests as in the
// original (HGAssets::setLocalScore), kept in save/scores.txt.
//
// ARCADE: coins and credits, the attract loop (title, high scores, a demo),
// a level choice against the clock, a credit's lives on one level (the
// times added up), high score tables per level, initials.
#pragma once

#include "controls.hpp"
#include "render.hpp"
#include "settings.hpp"

typedef struct vp_game vp_game;
typedef struct vp_dvg vp_dvg;

namespace vp {

int appInit(vp_game* g);
void appUpdate(vp_game* g, float dt);
void appDraw(vp_game* g, vp_dvg* d);
void appShutdown(vp_game* g);

// The settings at start (the frame rate is read before the loop opens).
const Settings& appBootSettings();

} // namespace vp
