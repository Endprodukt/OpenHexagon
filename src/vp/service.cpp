// The service menu: see service.hpp. The structure is the Tempest 2000
// port's service.c, with Open Hexagon's settings.
#include "service.hpp"

#include "controls.hpp"
#include "render.hpp"
#include "vpbind.h"

#include "SSVOpenHexagon/Global/Audio.hpp"

#include <SDL.h>
#include <vectorpie/vp.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace vp {

namespace {

enum Row
{
    R_CONTROLS, R_VIDEO, R_GAME, R_MODE, R_FREE, R_LIVES, R_SELECT, R_ATTRACT, R_MUSICSRC,
    R_VOLUME, R_MUSIC, R_EFFECTS, R_SAVE, R_CANCEL,
    R_SETUP, R_TEST, R_SENS, R_DIRECTION,
    R_LEFT, R_RIGHT, R_FOCUS, R_SWAP, R_BACKBTN,
    R_3D, R_DEPTH, R_FILL, R_TINTS, R_SPOKES, R_FLASH, R_TRAIL, R_SIMPLIFY, R_HZ, R_SIZE, R_VIEW,
    R_BRIGHT, R_BRIGHT_LAST = R_BRIGHT + BRIGHT_COUNT - 1,
    R_ADIFF, R_SPEED, R_ROTATION, R_PULSE, R_ROTSTART, R_INVINCIBLE, R_AUTORESTART,
    R_BACK, ROWS
};

enum
{
    PAGE_MAIN,
    PAGE_CONTROLS,
    PAGE_VIDEO,
    PAGE_GAME
};

const int main_rows[] = {R_CONTROLS, R_VIDEO, R_GAME, R_MODE, R_FREE, R_LIVES,
    R_SELECT, R_ATTRACT, R_MUSICSRC, R_VOLUME, R_MUSIC, R_EFFECTS, R_CANCEL, R_SAVE};
const int control_rows[] = {R_SETUP, R_TEST, R_SENS, R_DIRECTION, R_LEFT, R_RIGHT,
    R_FOCUS, R_SWAP, R_BACKBTN, R_BACK};
const int video_rows[] = {R_3D, R_DEPTH, R_FILL, R_TINTS, R_SPOKES, R_FLASH, R_TRAIL, R_SIMPLIFY, R_HZ,
    R_SIZE, R_VIEW, R_BRIGHT, R_BRIGHT + 1, R_BRIGHT + 2, R_BRIGHT + 3, R_BRIGHT + 4,
    R_BACK};

const int game_rows[] = {R_ADIFF, R_SPEED, R_ROTATION, R_PULSE, R_ROTSTART,
    R_INVINCIBLE, R_AUTORESTART, R_BACK};

template <typename T, int N>
constexpr int count(const T (&)[N])
{
    return N;
}

constexpr int SENS_MIN = 5; // hundredths
constexpr int SENS_MAX = 400;
constexpr float ROW_COUNTS = DEG_PER_COUNT / MENU_STEP_DEG;  // rows a spinner count moves, at sensitivity 1
constexpr float VALUE_COUNTS = 0.05f;  // steps a count moves a value
constexpr int REPEAT_DELAY = 24;
constexpr int REPEAT_EVERY = 3;

const char* const row_names[ROWS] = {
    "CONTROLS", "VIDEO", "GAME", "GAME MODE", "FREE PLAY", "LIVES PER CREDIT",
    "LEVEL SELECT TIME", "ATTRACT SOUND", "MUSIC",
    "MASTER VOLUME", "MUSIC VOLUME", "EFFECTS VOLUME", "SAVE", "CANCEL",
    "SETUP", "SPINNER TEST", "SPINNER SENSITIVITY", "SPINNER DIRECTION",
    "LEFT BUTTON", "RIGHT BUTTON", "FOCUS BUTTON", "SWAP BUTTON", "BACK BUTTON",
    "3D EFFECT", "3D DEPTH", "WALL FILL", "COLOR TONES", "BACKGROUND SIDES", "DEATH FLASH", "PLAYER TRAIL",
    "SIMPLER WHEN BUSY",
    "FRAME RATE", "SCREEN SIZE", "VIEW SIZE",
    "WALL BRIGHTNESS", "3D BRIGHTNESS", "BACKGROUND BRIGHTNESS",
    "PLAYER BRIGHTNESS", "TEXT BRIGHTNESS",
    "ARCADE DIFFICULTY", "GAME SPEED", "ROTATION", "PULSE", "ROTATE TO START",
    "INVINCIBLE (HOME)", "AUTO RESTART (HOME)",
    "BACK"};

struct State
{
    Settings now;
    hg::Audio* audio{nullptr};
    int sens{100};
    int page{PAGE_MAIN}, cursor{0}, row{R_CONTROLS};
    bool editing{false};
    vpb_layout lay{};               // laying out a button (vpbind.h)
    bool testing{false};
    float spin{0.f};
    int held{0};
    float testAngle{0.f};       // the spinner test: degrees the player went
    float testTurn{0.f};        // and this turn's part of it
    int testRest{0};
    bool key{false};
    unsigned frames{0};
    int music{0};
} S;

constexpr int TEST_REST = 90;

void beep(const char* id)
{
    if(S.audio != nullptr)
    {
        S.audio->playSoundOverride(id);
    }
}

const int* page_rows(int* n)
{
    if(S.page == PAGE_CONTROLS) { *n = count(control_rows); return control_rows; }
    if(S.page == PAGE_VIDEO) { *n = count(video_rows); return video_rows; }
    if(S.page == PAGE_GAME) { *n = count(game_rows); return game_rows; }
    *n = count(main_rows);
    return main_rows;
}

int* row_number(int row, int* lo, int* hi)
{
    *lo = 0;
    *hi = 100;
    switch(row)
    {
        case R_SENS: *lo = SENS_MIN; *hi = SENS_MAX; return &S.sens;
        case R_LIVES: *lo = 1; *hi = 5; return &S.now.lives;
        case R_SELECT: *lo = 5; *hi = 60; return &S.now.selectSeconds;
        case R_VOLUME: return &S.now.volume;
        case R_MUSIC: return &S.now.musicVolume;
        case R_EFFECTS: return &S.now.effectsVolume;
        case R_DEPTH: *hi = 200; return &S.now.depth3D;
        case R_SIZE: *lo = 70; *hi = 100; return &S.now.screenSize;
        case R_VIEW: *lo = 100; *hi = 135; return &S.now.viewSize;
        case R_SPEED: *lo = 50; *hi = 150; return &S.now.gameSpeed;
    }
    if(row >= R_BRIGHT && row <= R_BRIGHT_LAST) return &S.now.brightness[row - R_BRIGHT];
    return nullptr;
}

bool* row_switch(int row)
{
    switch(row)
    {
        case R_MODE: return &S.now.arcade;
        case R_FREE: return &S.now.freePlay;
        case R_ATTRACT: return &S.now.attractSound;
        case R_DIRECTION: return &S.now.reversed;
        case R_SETUP: return &S.now.cabinet;
        case R_3D: return &S.now.threeD;
        case R_SPOKES: return &S.now.spokes;
        case R_FLASH: return &S.now.flash;
        case R_TRAIL: return &S.now.trail;
        case R_SIMPLIFY: return &S.now.simplify;
        case R_TINTS: return &S.now.tints;
        case R_ROTATION: return &S.now.rotation;
        case R_PULSE: return &S.now.pulse;
        case R_ROTSTART: return &S.now.rotateToStart;
        case R_INVINCIBLE: return &S.now.invincible;
        case R_AUTORESTART: return &S.now.autoRestart;
    }
    return nullptr;
}

bool is_button(int row)
{
    return row >= R_LEFT && row <= R_BACKBTN;
}

void hear()
{
    if(S.audio == nullptr) return;
    const float master = S.now.volume / 100.f;
    S.audio->setSoundVolume(master * S.now.effectsVolume);
    S.audio->setMusicVolume(master * S.now.musicVolume);
}

void change(int row, int steps)
{
    int lo, hi;
    int* number = row_number(row, &lo, &hi);
    bool* onoff = row_switch(row);

    if(!steps) return;
    if(is_button(row))                  // none, or its standard
    {
        std::string& has = S.now.button[row - R_LEFT];
        char b[VPB_LENGTH];
        std::snprintf(b, sizeof b, "%s", has.c_str());
        vpb_layout_toggle(&S.lay, b, sizeof b, BUTTON_DEFAULT[row - R_LEFT]);
        has = b;
        beep("beep.ogg");
        return;
    }
    if(onoff)
    {
        *onoff = !*onoff;
        beep("beep.ogg");
        return;
    }
    if(row == R_ADIFF)
    {
        S.now.arcadeDifficulty = (S.now.arcadeDifficulty + (steps > 0 ? 1 : 2)) % 3;
        beep("beep.ogg");
        return;
    }
    if(row == R_MUSICSRC)
    {
        S.now.music = (S.now.music + (steps > 0 ? 1 : 2)) % 3;
        beep("beep.ogg");
        return;
    }
    if(row == R_FILL)
    {
        S.now.wallFill = (S.now.wallFill + (steps > 0 ? 1 : 2)) % 3;
        beep("beep.ogg");
        return;
    }
    if(row == R_HZ)
    {
        static const int rates[] = {40, 50, 60};
        int i = S.now.frameHz >= 60 ? 2 : S.now.frameHz >= 50 ? 1 : 0;
        i = (i + (steps > 0 ? 1 : 2)) % 3;
        S.now.frameHz = rates[i];
        beep("beep.ogg");
        return;
    }
    if(!number) return;
    if(row == R_DEPTH || row == R_SELECT || row == R_SPEED) steps *= 5;
    *number += steps;
    if(*number < lo) *number = lo;
    if(*number > hi) *number = hi;
    if(row >= R_VOLUME && row <= R_EFFECTS) hear();
}

// The menu is silent but for the volume rows: the effects' row beeps again
// and again, the master's and music's play what the music is.
void sound_for_row()
{
    static int preview = 0;
    if(preview > 0) { --preview; return; }
    if(S.row == R_EFFECTS || S.row == R_VOLUME)
    {
        beep("beep.ogg");
        preview = 45;
    }
}

void turn_to(int page)
{
    int n;
    vpb_layout_moved(&S.lay);
    S.page = page;
    S.cursor = 0;
    S.row = page_rows(&n)[0];
}

bool repeated(bool held)
{
    if(!held) return false;
    if(S.held == 1) return true;
    return S.held > REPEAT_DELAY && (S.held - REPEAT_DELAY) % REPEAT_EVERY == 0;
}

int spin_steps(vp_input* in, float per_count, float sign)
{
    S.spin += static_cast<float>(vp_input_delta(in, "p1_dialx")) * per_count *
              sign * (S.now.reversed ? -1.f : 1.f);
    const int steps = static_cast<int>(S.spin);
    S.spin -= static_cast<float>(steps);
    return steps;
}

// Laying out a button: vpbind.h.
void capture(vp_input* in)
{
    std::string& has = S.now.button[S.row - R_LEFT];
    char b[VPB_LENGTH];
    std::snprintf(b, sizeof b, "%s", has.c_str());
    const int got = vpb_layout_step(&S.lay, in, b, sizeof b, BUTTON_DEFAULT[S.row - R_LEFT]);
    has = b;
    if(got == VPB_CHANGED) beep("select.ogg");
    else if(got == VPB_LEFT) beep("beep.ogg");
}

// The spinner test: the player round the center, moved by the spinner as
// in the game. Right when one full turn of the spinner takes the player
// once around.
void test(vp_input* in)
{
    const int spin = vp_input_delta(in, "p1_dialx");
    const bool more = vpb_menu_right() != 0;     // the menu's own: laid out buttons may be anything
    const bool less = vpb_menu_left() != 0;

    if(vpb_menu_confirm(in)) { S.testing = false; return; }
    if(vpb_menu_up() || vpb_menu_down()) S.now.reversed = !S.now.reversed;

    S.held = more != less ? S.held + 1 : 0;
    if(repeated(more != less)) change(R_SENS, more ? 1 : -1);

    if(spin)
    {
        const float deg = static_cast<float>(spin) * DEG_PER_COUNT * S.sens / 100.f *
                          (S.now.reversed ? -1.f : 1.f);
        if(S.testRest >= TEST_REST) S.testTurn = 0.f;
        S.testAngle += deg;
        S.testTurn += deg;
        S.testRest = 0;
    }
    else if(S.testRest < TEST_REST)
    {
        ++S.testRest;
    }
}

std::string spelled(const std::string& controls)
{
    char b[VPB_LENGTH * 2];
    return vpb_spelled(controls.c_str(), b, sizeof b);
}

const Rgb YELLOW{255, 255, 0};
const Rgb WHITE{255, 255, 255};
const Rgb CYAN{0, 200, 255};
const Rgb GREEN{0, 255, 0};

void slider(vp_dvg* d, float x, float y, float width, float part, bool lit)
{
    const float knob = x + width * part;
    const std::uint8_t bright = lit ? 255 : 150;

    vp_color(d, 0, bright, bright);
    ui::line(d, x, y - 2.5f, x, y + 2.5f);
    ui::line(d, x, y, knob, y);
    vp_color(d, 40, 60, 110);
    ui::line(d, knob, y, x + width, y);
    ui::line(d, x + width, y - 2.5f, x + width, y + 2.5f);
    if(lit) vp_color(d, 255, 255, 0); else vp_color(d, 200, 200, 200);
    ui::line(d, knob - 3.f, y, knob, y - 3.f);
    ui::line(d, knob, y - 3.f, knob + 3.f, y);
    ui::line(d, knob + 3.f, y, knob, y + 3.f);
    ui::line(d, knob, y + 3.f, knob - 3.f, y);
}

void draw_test(vp_dvg* d)
{
    char value[64];
    float turn = std::fabs(S.testTurn) / 360.f;

    ui::text(d, "SPINNER TEST", 192.f, 6.f, 11.f, VP_TEXT_CENTER, YELLOW);
    ui::text(d, "ONE FULL TURN OF THE SPINNER", 192.f, 26.f, 6.f, VP_TEXT_CENTER, CYAN);
    ui::text(d, "SHOULD TAKE THE PLAYER ONCE AROUND", 192.f, 36.f, 6.f, VP_TEXT_CENTER, CYAN);

    // The center hexagon and the player, as small as the game's.
    const float cx = 192.f, cy = 140.f, r = 34.f;
    vp_color(d, 255, 255, 255);
    for(int i = 0; i < 6; ++i)
    {
        const float a0 = 6.2831853f * i / 6.f, a1 = 6.2831853f * (i + 1) / 6.f;
        ui::line(d, cx + std::cos(a0) * r, cy + std::sin(a0) * r,
            cx + std::cos(a1) * r, cy + std::sin(a1) * r);
    }
    const float a = S.testAngle * 3.14159265f / 180.f;
    const float pr = r + 14.f;
    vp_color(d, 255, 255, 0);
    const float tx = cx + std::cos(a) * (pr + 6.f), ty = cy + std::sin(a) * (pr + 6.f);
    const float lx = cx + std::cos(a - 0.12f) * pr, ly = cy + std::sin(a - 0.12f) * pr;
    const float rx = cx + std::cos(a + 0.12f) * pr, ry = cy + std::sin(a + 0.12f) * pr;
    ui::line(d, tx, ty, lx, ly);
    ui::line(d, lx, ly, rx, ry);
    ui::line(d, rx, ry, tx, ty);

    std::snprintf(value, sizeof value, "THIS TURN: %d.%02d TIMES AROUND", static_cast<int>(turn),
        static_cast<int>(turn * 100.f) % 100);
    ui::text(d, value, 192.f, 214.f, 7.f, VP_TEXT_CENTER, WHITE);
    std::snprintf(value, sizeof value, "SENSITIVITY %d.%02d  %s", S.sens / 100, S.sens % 100,
        S.now.reversed ? "REVERSED" : "NORMAL");
    ui::text(d, value, 192.f, 228.f, 5.f, VP_TEXT_CENTER, WHITE);
    ui::text(d, "LEFT - LESS    RIGHT - MORE", 192.f, 240.f, 4.5f, VP_TEXT_CENTER, GREEN);
    ui::text(d, "WRONG WAY ROUND - UP OR DOWN TURNS IT", 192.f, 250.f, 4.5f, VP_TEXT_CENTER, GREEN);
    ui::text(d, "START 1 - BACK", 192.f, 264.f, 5.5f, VP_TEXT_CENTER, YELLOW);
}

} // namespace

bool serviceHotkey()
{
    vpb_update();                       // every control, once a step (vpbind.h)
    const bool key = vpb_key_held(SDL_SCANCODE_F2) != 0;
    const bool hit = key && !S.key;
    S.key = key;
    return hit;
}

void serviceOpen(const Settings& now, hg::Audio* audio)
{
    const bool key = S.key;
    S = State{};
    S.key = key;
    S.now = now;
    for(auto& b : S.now.button)         // advmame.rc names of an older file
    {
        char c[VPB_LENGTH];
        std::snprintf(c, sizeof c, "%s", b.c_str());
        vpb_normalize(c, sizeof c);
        b = c;
    }
    S.audio = audio;
    S.sens = static_cast<int>(now.sensitivity * 100.f + 0.5f);
    if(S.sens < SENS_MIN) S.sens = SENS_MIN;
    if(S.sens > SENS_MAX) S.sens = SENS_MAX;
    S.row = main_rows[0];
    S.testRest = TEST_REST;
    if(audio != nullptr)
    {
        audio->stopMusic();
        audio->stopSounds();
    }
}

const Settings& serviceGet()
{
    S.now.sensitivity = static_cast<float>(S.sens) / 100.f;
    return S.now;
}

int serviceUpdate(vp_input* in, bool hotkey)
{
    int n;
    const int* rows = page_rows(&n);

    ++S.frames;
    sound_for_row();
    if(hotkey) return SERVICE_CANCELLED;
    if(S.lay.capturing) { capture(in); return SERVICE_OPEN; }
    if(S.testing) { test(in); return SERVICE_OPEN; }

    // The menu's own controls only: what is laid out never counts here.
    const bool accept = vpb_menu_confirm(in) != 0;
    int l = vpb_menu_left(), r = vpb_menu_right();
    vpb_layout_filter(&S.lay, &l, &r);
    const bool left = l != 0, right = r != 0;
    if(is_button(S.row) && !S.editing &&
        (vpb_key_pressed(SDL_SCANCODE_DELETE) || vpb_key_pressed(SDL_SCANCODE_KP_PERIOD)))
    {
        std::string& has = S.now.button[S.row - R_LEFT];
        char b[VPB_LENGTH];
        std::snprintf(b, sizeof b, "%s", has.c_str());
        vpb_layout_standard(&S.lay, b, sizeof b, BUTTON_DEFAULT[S.row - R_LEFT]);
        has = b;
        beep("select.ogg");
        return SERVICE_OPEN;
    }
    S.held = left != right ? S.held + 1 : 0;
    if(repeated(left != right)) change(S.row, right ? 1 : -1);

    if(S.editing)
    {
        change(S.row, spin_steps(in, VALUE_COUNTS * static_cast<float>(S.sens) / 100.f, 1.f));
        if(accept) { S.editing = false; S.spin = 0.f; beep("beep.ogg"); }
        return SERVICE_OPEN;
    }

    int steps = spin_steps(in, ROW_COUNTS * static_cast<float>(S.sens) / 100.f, 1.f);
    if(vpb_menu_down()) ++steps;
    if(vpb_menu_up()) --steps;
    if(steps)
    {
        vpb_layout_moved(&S.lay);
        S.cursor += steps;
        if(S.cursor < 0) S.cursor = 0;
        if(S.cursor >= n) S.cursor = n - 1;
        S.row = rows[S.cursor];
    }
    if(!accept) return SERVICE_OPEN;

    S.spin = 0.f;
    if(S.row == R_SAVE) return SERVICE_SAVED;
    if(S.row == R_CANCEL) return SERVICE_CANCELLED;
    beep("beep.ogg");
    if(S.row == R_CONTROLS) turn_to(PAGE_CONTROLS);
    else if(S.row == R_VIDEO) turn_to(PAGE_VIDEO);
    else if(S.row == R_GAME) turn_to(PAGE_GAME);
    else if(S.row == R_BACK) turn_to(PAGE_MAIN);
    else if(S.row == R_TEST)
    {
        S.testing = true;
        S.held = 0;
        S.testAngle = S.testTurn = 0.f;
        S.testRest = TEST_REST;
    }
    else if(is_button(S.row)) vpb_layout_begin(&S.lay, S.now.button[S.row - R_LEFT].c_str());
    else if(row_switch(S.row) || S.row == R_HZ || S.row == R_FILL || S.row == R_MUSICSRC || S.row == R_ADIFF) change(S.row, 1);
    else S.editing = true;
    return SERVICE_OPEN;
}

void serviceDraw(vp_dvg* d)
{
    int n;
    const int* rows = page_rows(&n);
    char value[64];

    if(S.testing) { draw_test(d); return; }

    ui::text(d, S.page == PAGE_CONTROLS ? "CONTROLS" : S.page == PAGE_VIDEO ? "VIDEO" : S.page == PAGE_GAME ? "GAME" : "SERVICE MENU",
        192.f, 6.f, 11.f, VP_TEXT_CENTER, YELLOW);

    for(int i = 0; i < n; ++i)
    {
        const int row = rows[i];
        const bool here = row == S.row, set = here && (S.editing || S.lay.capturing);
        int lo, hi;
        int* number = row_number(row, &lo, &hi);
        bool* onoff = row_switch(row);
        // Rows 17 apart, closer when a page has more than fit (video: 16).
        const float rowStep = n > 13 ? 210.f / static_cast<float>(n) : 17.f;
        const float y = 32.f + rowStep * static_cast<float>(i);
        Rgb c = here ? WHITE : Rgb{60, 110, 255};

        if(row == R_SAVE || row == R_CANCEL) continue; // side by side, below
        if(here) ui::text(d, ">", 12.f, y, 6.f, VP_TEXT_RIGHT, YELLOW);
        ui::text(d, row_names[row], 16.f, y, 6.f, VP_TEXT_LEFT, c);
        if(set) c = YELLOW;

        const float vx = 226.f;
        if(row == R_CONTROLS || row == R_VIDEO || row == R_GAME || row == R_TEST)
        {
            ui::text(d, "OPEN", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_BACK)
        {
        }
        else if(row == R_SETUP)
        {
            ui::text(d, S.now.cabinet ? "CABINET" : "PC", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_MODE)
        {
            ui::text(d, S.now.arcade ? "ARCADE" : "HOME", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_SENS)
        {
            std::snprintf(value, sizeof value, "%d.%02d", *number / 100, *number % 100);
            ui::text(d, value, vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_DIRECTION)
        {
            ui::text(d, S.now.reversed ? "REVERSED" : "NORMAL", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_LIVES)
        {
            std::snprintf(value, sizeof value, "%d", *number);
            ui::text(d, value, vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_SELECT)
        {
            std::snprintf(value, sizeof value, "%d SECONDS", *number);
            ui::text(d, value, vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_MUSICSRC)
        {
            ui::text(d, S.now.music == 0 ? "OFF" : S.now.music == 2 ? "ORIGINAL FILES" : "MADE BY THE GAME", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_ADIFF)
        {
            ui::text(d, S.now.arcadeDifficulty == 1 ? "EASIEST" : S.now.arcadeDifficulty == 2 ? "NORMAL (1X)" : "PLAYER CHOOSES", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_SPEED)
        {
            std::snprintf(value, sizeof value, "%d%%", *number);
            ui::text(d, value, vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_FILL)
        {
            ui::text(d, S.now.wallFill == 0 ? "OFF" : S.now.wallFill == 2 ? "HIGH" : "LOW", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_HZ)
        {
            std::snprintf(value, sizeof value, "%d HZ", S.now.frameHz);
            ui::text(d, value, vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(row == R_DEPTH || row == R_SIZE || row == R_VIEW)
        {
            std::snprintf(value, sizeof value, "%d%%", *number);
            ui::text(d, value, vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(number)
        {
            slider(d, vx, y + 3.f, 100.f, static_cast<float>(*number - lo) / static_cast<float>(hi - lo), here);
            std::snprintf(value, sizeof value, "%d", *number);
            ui::text(d, value, 372.f, y, 6.f, VP_TEXT_RIGHT, c);
        }
        else if(onoff)
        {
            ui::text(d, *onoff ? "YES" : "NO", vx, y, 6.f, VP_TEXT_LEFT, c);
        }
        else if(S.lay.capturing && here && ((S.frames / 15) & 1))
        {
            ui::text(d, "PRESS A BUTTON", vx, y, 6.f, VP_TEXT_LEFT, YELLOW);
        }
        else if(is_button(row))
        {
            // All its controls; smaller when they would not fit.
            const std::string v = spelled(S.now.button[row - R_LEFT]);
            float size = 6.f;
            const float room = 380.f - vx;
            if(static_cast<float>(v.size()) * size * 1.6f > room) size = room / (static_cast<float>(v.size()) * 1.6f);
            ui::text(d, v, vx, y + (6.f - size), size, VP_TEXT_LEFT, c);
        }
    }

    if(S.page == PAGE_MAIN)
    {
        if(S.row == R_CANCEL) ui::text(d, "CANCEL", 140.f, 236.f, 10.f, VP_TEXT_CENTER, WHITE);
        else ui::text(d, "CANCEL", 140.f, 238.f, 7.f, VP_TEXT_CENTER, Rgb{200, 60, 60});
        if(S.row == R_SAVE) ui::text(d, "SAVE", 244.f, 236.f, 10.f, VP_TEXT_CENTER, WHITE);
        else ui::text(d, "SAVE", 244.f, 238.f, 7.f, VP_TEXT_CENTER, Rgb{0, 200, 0});
    }
    // The row's hint, above the help lines: wrapped to lines no wider than
    // HINT_WIDTH, the last one ending above the help (2026-10-19: some ran
    // to the screen's edges and into the help).
    std::vector<std::pair<std::string, Rgb>> hints;
    if(const char* said = vpb_layout_said(&S.lay)) hints.push_back({said, Rgb{255, 80, 80}});
    if(S.page == PAGE_VIDEO && S.row == R_HZ) hints.push_back({"THE FRAME RATE CHANGES AT THE NEXT START", CYAN});
    if(S.page == PAGE_CONTROLS && S.row == R_SETUP) hints.push_back({"HOW THE LEVELS AND THE TUTORIAL NAME THE CONTROLS", CYAN});
    if(S.page == PAGE_GAME)
    {
        const char* hint = S.row == R_ADIFF ? "ARCADE: WHICH OF THE LEVEL'S OWN DIFFICULTIES IS PLAYED. THE LEVEL GETS FASTER AND HARDER, ITS SCRIPT TOO"
                         : S.row == R_SPEED ? "THE ORIGINAL'S TIMESCALE: THE WHOLE GAME RUNS SLOWER OR FASTER, THE CLOCK TOO. SLOWER IS EASIER"
                         : S.row == R_INVINCIBLE ? "HOME ONLY: NO DEATH, TO PRACTISE"
                         : S.row == R_AUTORESTART ? "HOME ONLY: A NEW TRY AT ONCE AFTER A DEATH"
                         : S.row == R_BACK ? "" : "THE ORIGINAL'S GAMEPLAY OPTIONS";
        if(*hint) hints.push_back({hint, CYAN});
        hints.push_back({"WITH HELP ON, SCORES GO TO TABLES OF THEIR OWN", CYAN});
    }
    if(S.page == PAGE_VIDEO && S.row == R_FILL) hints.push_back({"LINES ACROSS THE WALLS: THEY READ AS SOLID. MORE LINES, MORE BEAM TIME", CYAN});
    if(S.page == PAGE_VIDEO && S.row == R_TINTS) hints.push_back({"THE 3D SIDES AND THE FILL IN TONES OF THEIR OWN", CYAN});
    if(S.page == PAGE_VIDEO && S.row == R_VIEW) hints.push_back({"MORE OF THE FIELD, ALL OF IT SMALLER. 133 - AS WIDE AS 16:9", CYAN});
    if(S.page == PAGE_VIDEO && S.row == R_SIMPLIFY) hints.push_back({"A LEVEL TOO BUSY FOR THE BOARD IS DRAWN WITH FEWER LINES", CYAN});
    {
        // Lines of one hint HINT_STEP apart, hints HINT_GAP more.
        constexpr float HINT_WIDTH = 300.f, HINT_STEP = 7.5f, HINT_GAP = 3.f;
        struct Line { std::string text; Rgb color; float before; };
        std::vector<Line> lines;
        float height = 0.f;
        for(const auto& h : hints)
        {
            bool first = true;
            for(const std::string& l : ui::wrap(h.first, 4.5f, HINT_WIDTH))
            {
                const float before = first && !lines.empty() ? HINT_GAP : 0.f;
                lines.push_back({l, h.second, before});
                height += HINT_STEP + before;
                first = false;
            }
        }
        float y = 254.f - height;
        for(const Line& l : lines)
        {
            y += l.before;
            ui::text(d, l.text, 192.f, y, 4.5f, VP_TEXT_CENTER, l.color);
            y += HINT_STEP;
        }
    }
    const char* one = "SPINNER, UP, DOWN - ROW   LEFT, RIGHT - SET";
    const char* two = "ENTER, START 1 - TAKE";
    if(S.editing) { one = "SPINNER, LEFT, RIGHT - SET"; two = "ENTER, START 1 - DONE"; }
    if(S.lay.capturing || (is_button(S.row) && !S.editing)) vpb_layout_help(&S.lay, &one, &two);
    ui::text(d, one, 192.f, 262.f, 4.5f, VP_TEXT_CENTER, Rgb{0, 150, 0});
    ui::text(d, two, 192.f, 270.f, 4.5f, VP_TEXT_CENTER, Rgb{0, 150, 0});
}

} // namespace vp
