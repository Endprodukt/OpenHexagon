// The cabinet's settings: save/settings.ini, changed in the service menu
// (F2). Everything that is not the original's gameplay lives here.
#pragma once

#include <string>

namespace vp {

enum Bright
{
    BRIGHT_WALLS,
    BRIGHT_3D,
    BRIGHT_BACKGROUND,
    BRIGHT_PLAYER,
    BRIGHT_TEXT,
    BRIGHT_COUNT
};

enum Button
{
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_FOCUS,
    BUTTON_SWAP,
    BUTTON_BACK,
    BUTTON_COUNT
};

// What each button has when nothing says otherwise (Settings::button's).
inline constexpr const char* BUTTON_DEFAULT[BUTTON_COUNT] = {
    "key_Left", "key_Right", "key_Left_Ctrl", "key_Left_Alt", "key_Space"};

struct Settings
{
    // Game
    bool arcade{false};          // false: home (the original), true: arcade
    bool freePlay{false};        // arcade: START needs no credit
    int lives{3};                // arcade: lives per credit, 1..5
    bool attractSound{false};    // arcade: sound while nobody plays
    int selectSeconds{20};       // arcade: time to choose a level

    // Game: the original's gameplay options (its options menu, gameplay
    // and graphics), and how arcade picks a level's difficulty.
    int arcadeDifficulty{0};     // 0 the player chooses, 1 the easiest, 2 1x
    int gameSpeed{100};          // percent: the original's timescale
    bool rotation{true};         // off: the original's "no rotation"
    bool pulse{true};            // off: the original's "no pulse"
    bool rotateToStart{false};
    bool invincible{false};      // home only
    bool autoRestart{false};     // home only

    // Music: 0 off, 1 made by the game (synth.cpp, in time with the level),
    // 2 the original's files (the Steam version's .ogg; a level without its
    // file gets the made one). -1 until set: the files if there are any.
    int music{-1};

    // Sound, 0..100
    int volume{80};
    int musicVolume{80};
    int effectsVolume{80};

    // Controls
    bool cabinet{true};          // the words for the controls: cabinet or PC
    float sensitivity{1.f};      // spinner: 1 = one full turn of a
                                 // 1200-count spinner is one turn around
    bool reversed{false};
    // vpbind.h's controls, read directly; the standard: the keys AdvanceMAME
    // gives the first player (BUTTON_DEFAULT).
    std::string button[BUTTON_COUNT]{
        "key_Left", "key_Right", "key_Left_Ctrl", "key_Left_Alt", "key_Space"};

    // Video
    bool threeD{true};           // the original's 3D effect, with hidden lines
    int depth3D{100};            // percent of the original's 3D multiplier
    bool spokes{true};           // the background's sides as lines
    bool flash{true};
    bool trail{true};            // the player's trail and swap particles
    bool simplify{false};        // a level too busy for the board is drawn simpler (Renderer::_lod)
    int wallFill{1};             // lines across walls: 0 off, 1 low, 2 high
    bool tints{true};            // the 3D band and the fill in tones of
                                 // their own (else all in the wall's color)
    int frameHz{60};             // 60, 50 or 40 on the board
    int brightness[BRIGHT_COUNT]{90, 80, 45, 100, 90}; // the cabinet's, 2026-10-19
    int screenSize{96};          // percent of the board the picture fills
    int viewSize{100};           // percent of the original's view of the
                                 // field: 133 is as wide as its 16:9 picture
};

// Load from save/settings.ini (missing keys keep their defaults).
void loadSettings(Settings& s);
void saveSettings(const Settings& s);

// The settings the original's Config holds (3D, flash).
void applyToConfig(const Settings& s);

} // namespace vp
