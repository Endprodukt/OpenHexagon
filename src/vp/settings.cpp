// save/settings.ini: see settings.hpp.
#include "settings.hpp"

#include "vpbind.h"

#include "SSVOpenHexagon/Global/Config.hpp"

#include <vectorpie/vp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace vp {

namespace {

const char* const brightNames[BRIGHT_COUNT] = {
    "bright_walls", "bright_3d", "bright_background", "bright_player",
    "bright_text"};

const char* const buttonNames[BUTTON_COUNT] = {
    "button_left", "button_right", "button_focus", "button_swap",
    "button_back"};

std::string path()
{
    return std::string(vp_save_dir()) + "/settings.ini";
}

bool yes(const std::string& v)
{
    return v == "yes" || v == "1" || v == "on" || v == "true";
}

int clampi(int v, int lo, int hi)
{
    return std::max(lo, std::min(hi, v));
}

} // namespace

void loadSettings(Settings& s)
{
    std::ifstream in(path());
    std::string line;

    while(std::getline(in, line))
    {
        const auto hash = line.find_first_of(";#");
        if(hash != std::string::npos)
        {
            line.erase(hash);
        }

        const auto eq = line.find('=');
        if(eq == std::string::npos)
        {
            continue;
        }

        auto trim = [](std::string t)
        {
            const auto a = t.find_first_not_of(" \t\r");
            const auto b = t.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string{} : t.substr(a, b - a + 1);
        };

        const std::string k = trim(line.substr(0, eq));
        const std::string v = trim(line.substr(eq + 1));
        const int n = std::atoi(v.c_str());

        if(k == "mode") s.arcade = v == "arcade";
        else if(k == "free_play") s.freePlay = yes(v);
        else if(k == "lives") s.lives = clampi(n, 1, 5);
        else if(k == "attract_sound") s.attractSound = yes(v);
        else if(k == "select_seconds") s.selectSeconds = clampi(n, 5, 60);
        else if(k == "arcade_difficulty") s.arcadeDifficulty = v == "easiest" ? 1 : v == "normal" ? 2 : 0;
        else if(k == "game_speed") s.gameSpeed = clampi(n, 50, 150);
        else if(k == "rotation") s.rotation = yes(v);
        else if(k == "pulse") s.pulse = yes(v);
        else if(k == "rotate_to_start") s.rotateToStart = yes(v);
        else if(k == "invincible") s.invincible = yes(v);
        else if(k == "auto_restart") s.autoRestart = yes(v);
        else if(k == "volume") s.volume = clampi(n, 0, 100);
        else if(k == "music") s.music = v == "off" ? 0 : v == "files" ? 2 : 1;
        else if(k == "music_volume") s.musicVolume = clampi(n, 0, 100);
        else if(k == "effects_volume") s.effectsVolume = clampi(n, 0, 100);
        else if(k == "sensitivity") s.sensitivity = std::max(0.05f, std::min(4.f, static_cast<float>(std::atof(v.c_str()))));
        else if(k == "spinner_reversed") s.reversed = yes(v);
        else if(k == "setup") s.cabinet = v != "pc";
        else if(k == "3d") s.threeD = yes(v);
        else if(k == "3d_depth") s.depth3D = clampi(n, 0, 200);
        else if(k == "spokes") s.spokes = yes(v);
        else if(k == "flash") s.flash = yes(v);
        else if(k == "trail") s.trail = yes(v);
        else if(k == "simplify") s.simplify = yes(v);
        else if(k == "wall_fill") s.wallFill = v == "off" ? 0 : v == "high" ? 2 : 1;
        else if(k == "tints") s.tints = yes(v);
        else if(k == "frame_hz") s.frameHz = n >= 55 ? 60 : n >= 45 ? 50 : 40;
        else if(k == "screen_size") s.screenSize = clampi(n, 70, 100);
        else if(k == "view_size") s.viewSize = clampi(n, 100, 135);
        else
        {
            for(int i = 0; i < BRIGHT_COUNT; ++i)
                if(k == brightNames[i]) s.brightness[i] = clampi(n, 0, 100);
            for(int i = 0; i < BUTTON_COUNT; ++i)
                if(k == buttonNames[i] && !v.empty())
                {
                    char b[VPB_LENGTH];
                    std::snprintf(b, sizeof b, "%s", v.c_str());
                    vpb_normalize(b, sizeof b);     // advmame.rc names of an older file
                    s.button[i] = b;
                }
        }
    }
}

void saveSettings(const Settings& s)
{
    const std::string p = path();
    const std::string tmp = p + ".new";

    {
        std::ofstream out(tmp, std::ios::trunc);
        out << "; Open Hexagon for VectorPie: the service menu (F2) writes this file.\n";
        out << "mode = " << (s.arcade ? "arcade" : "home") << '\n';
        out << "free_play = " << (s.freePlay ? "yes" : "no") << '\n';
        out << "lives = " << s.lives << '\n';
        out << "attract_sound = " << (s.attractSound ? "yes" : "no") << '\n';
        out << "select_seconds = " << s.selectSeconds << '\n';
        out << "arcade_difficulty = " << (s.arcadeDifficulty == 1 ? "easiest" : s.arcadeDifficulty == 2 ? "normal" : "choose") << '\n';
        out << "game_speed = " << s.gameSpeed << '\n';
        out << "rotation = " << (s.rotation ? "yes" : "no") << '\n';
        out << "pulse = " << (s.pulse ? "yes" : "no") << '\n';
        out << "rotate_to_start = " << (s.rotateToStart ? "yes" : "no") << '\n';
        out << "invincible = " << (s.invincible ? "yes" : "no") << '\n';
        out << "auto_restart = " << (s.autoRestart ? "yes" : "no") << '\n';
        if(s.music >= 0) out << "music = " << (s.music == 0 ? "off" : s.music == 2 ? "files" : "made") << '\n';
        out << "volume = " << s.volume << '\n';
        out << "music_volume = " << s.musicVolume << '\n';
        out << "effects_volume = " << s.effectsVolume << '\n';
        char sens[32];
        std::snprintf(sens, sizeof sens, "%.2f", s.sensitivity);
        out << "sensitivity = " << sens << '\n';
        out << "spinner_reversed = " << (s.reversed ? "yes" : "no") << '\n';
        out << "setup = " << (s.cabinet ? "cabinet" : "pc") << '\n';
        for(int i = 0; i < BUTTON_COUNT; ++i)
            out << buttonNames[i] << " = " << s.button[i] << '\n';
        out << "3d = " << (s.threeD ? "yes" : "no") << '\n';
        out << "3d_depth = " << s.depth3D << '\n';
        out << "spokes = " << (s.spokes ? "yes" : "no") << '\n';
        out << "flash = " << (s.flash ? "yes" : "no") << '\n';
        out << "trail = " << (s.trail ? "yes" : "no") << '\n';
        out << "simplify = " << (s.simplify ? "yes" : "no") << '\n';
        out << "wall_fill = " << (s.wallFill == 0 ? "off" : s.wallFill == 2 ? "high" : "low") << '\n';
        out << "tints = " << (s.tints ? "yes" : "no") << '\n';
        out << "frame_hz = " << s.frameHz << '\n';
        out << "screen_size = " << s.screenSize << '\n';
        out << "view_size = " << s.viewSize << '\n';
        for(int i = 0; i < BRIGHT_COUNT; ++i)
            out << brightNames[i] << " = " << s.brightness[i] << '\n';
    }

    std::remove(p.c_str());
    std::rename(tmp.c_str(), p.c_str());
}

void applyToConfig(const Settings& s)
{
    hg::Config::set3D(s.threeD);
    hg::Config::set3DMultiplier(static_cast<float>(s.depth3D) / 100.f);
    hg::Config::setFlash(s.flash);
    hg::Config::setNoMusic(s.music == 0);
    hg::Config::setTimescale(static_cast<float>(s.gameSpeed) / 100.f);
    hg::Config::setNoRotation(!s.rotation);
    hg::Config::setNoPulse(!s.pulse);
    hg::Config::setRotateToStart(s.rotateToStart);
    // Arcade: a credit always ends, and nothing restarts by itself.
    hg::Config::setInvincible(s.invincible && !s.arcade);
    hg::Config::setAutoRestart(s.autoRestart && !s.arcade);
}

} // namespace vp
