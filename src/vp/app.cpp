// The VectorPie front end: see app.hpp.
#include "app.hpp"

#include "hints.hpp"
#include "music.hpp"
#include "service.hpp"

#include "SSVOpenHexagon/Core/HexagonGame.hpp"
#include "SSVOpenHexagon/Core/Replay.hpp"
#include "SSVOpenHexagon/Data/LevelData.hpp"
#include "SSVOpenHexagon/Data/LoadInfo.hpp"
#include "SSVOpenHexagon/Data/MusicData.hpp"
#include "SSVOpenHexagon/Data/PackData.hpp"
#include "SSVOpenHexagon/Data/PackInfo.hpp"
#include "SSVOpenHexagon/Data/StyleData.hpp"
#include "SSVOpenHexagon/Global/Assets.hpp"
#include "SSVOpenHexagon/Global/Audio.hpp"
#include "SSVOpenHexagon/Global/Config.hpp"

#include <SSVUtils/Core/Log/Log.hpp>

#include <vectorpie/vp.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace vp {

namespace {

// ---------------------------------------------------------------------------
// Timings of the screens that are not the original's (seconds).
constexpr double TITLE_SECONDS = 8.0;
constexpr double SCORES_SECONDS = 8.0;
constexpr double DEMO_SECONDS = 40.0;
constexpr double DEATH_PAUSE = 2.0;      // arcade: after a death, before the next life
constexpr double GAME_OVER_SECONDS = 4.0;
constexpr double CONTINUE_SECONDS = 10.0; // arcade: the continue countdown
constexpr double CONTINUE_SKIP = 1.0;     // ... each button press takes this off it
constexpr double WARNING_SECONDS = 6.0;   // attract: the epilepsy warning
constexpr float INITIALS_SECONDS = 30.f;
constexpr double PREVIEW_SETTLE = 0.1;   // the level choice: rest before the preview starts
constexpr double PREVIEW_AFTER_DEATH = 1.5;
constexpr int PREVIEW_FORWARD = 150;     // steps (2.5 s) run unseen at its start: walls already in view

enum class Mode
{
    HomeTitle,
    HomeMenu,
    HomeGame,
    Title,
    Warning,
    Scores,
    Demo,
    Select,
    Play,
    Continue,
    GameOver,
    Initials,
    Error
};

struct LevelEntry
{
    std::string assetId;
    std::string packId;
    const hg::LevelData* data;
};

// An arcade table: save/arcade/<level>.txt, "NAME CENTISECONDS" best first,
// the format of the SDK's highscores.txt.
struct Table
{
    struct Row
    {
        std::string name;
        int score;
    };

    std::vector<Row> rows;
};

const Rgb YELLOW{255, 255, 0};
const Rgb WHITE{255, 255, 255};
const Rgb CYAN{0, 200, 255};
const Rgb GREEN{0, 220, 0};
const Rgb RED{230, 60, 60};

struct App
{
    Settings s;
    std::unique_ptr<hg::HGAssets> assets;
    std::unique_ptr<hg::Audio> audio;
    ssvs::GameWindow window;
    std::unique_ptr<hg::HexagonGame> game;
    Renderer renderer;
    Controls ctl;
    vp_credits credits{};
    vp_audio* dev{nullptr};

    Mode mode{Mode::Title};
    double modeTime{0.0};
    bool service{false};

    std::vector<LevelEntry> levels;
    int sel{0};
    int diff{0};
    float spinMenu{0.f};    // menus: spinner degrees towards the next step

    hg::StyleData menuStyle;
    std::string menuStyleLevel;
    float menuRotation{0.f};

    bool menuRequested{false};
    bool menuError{false};
    std::string errorText;

    // Arcade
    int lives{0};
    // The best single try of the credit, and of the credits continued from
    // it: each life plays the level from its start, the best try is the
    // score (2026-10-18).
    double best{0.0};
    double continueSkip{0.0}; // the continue countdown: seconds taken off by presses
    int continueShown{-1};
    bool deathSeen{false};
    double deathAt{0.0};
    int scoresLevel{0};
    int newRank{-1};
    char initials[4]{'A', 'A', 'A', 0};
    int initialsPos{0};
    float initialsSpin{0.f};
    bool demoPilot{false};
    double lastLifeTime{0.0};
    double titleMusicAt{0.0};
    float playDm{1.f};      // arcade: the difficulty of the credit being played
    bool tutorialDone{false}; // arcade: the credit ended with a tutorial's end
    bool swapHint{false};   // the swap line still to be shown at this level's start
    std::vector<float> pendingSpin; // the next replay's spinner log

    // The level choice's preview: the chosen level playing behind the
    // lettering, silent, from its best replay or by the pilot. Not the
    // original's (its menu shows only the level's style).
    struct Preview
    {
        std::string key;     // level and difficulty it was started for
        double since{0.0};   // when the choice settled there
        bool running{false};
        bool dead{false};
        double deadAt{0.0};
    } preview;

    std::unordered_map<std::string, std::string> musicFallback;
    std::mt19937 rnd{std::random_device{}()};
} A;

Settings g_boot;
BeatSync g_lastSync;
bool g_musicDemo = false;

std::string saveDir()
{
    return vp_save_dir();
}

// ---------------------------------------------------------------------------
// Sound

void applyVolumes(bool quiet)
{
    const float master = quiet ? 0.f : A.s.volume / 100.f;
    if(A.audio)
    {
        A.audio->setSoundVolume(master * A.s.effectsVolume);
        A.audio->setMusicVolume(master * A.s.musicVolume);
    }
}

bool attractQuiet()
{
    return A.s.arcade && !A.s.attractSound;
}

// ---------------------------------------------------------------------------
// Levels

// The levels to choose from, the tutorials too: a tutorial ends a credit
// when its script ends it (updatePlay).
void collectLevels()
{
    A.levels.clear();

    for(const hg::PackInfo& pi : A.assets->getSelectablePackInfos())
    {
        if(!A.assets->packHasLevels(pi.id))
        {
            continue;
        }

        for(const std::string& id : A.assets->getLevelIdsByPack(pi.id))
        {
            const hg::LevelData& ld = A.assets->getLevelData(id);
            if(!ld.selectable)
            {
                continue;
            }

            A.levels.push_back(LevelEntry{id, pi.id, &ld});
        }
    }
}

const LevelEntry* current()
{
    if(A.levels.empty()) return nullptr;
    A.sel = ((A.sel % static_cast<int>(A.levels.size())) + static_cast<int>(A.levels.size())) %
            static_cast<int>(A.levels.size());
    return &A.levels[A.sel];
}

std::string diffText(float d);

float currentDiff()
{
    const LevelEntry* e = current();
    if(e == nullptr || e->data->difficultyMults.empty()) return 1.f;
    const auto& m = e->data->difficultyMults;
    A.diff = std::clamp(A.diff, 0, static_cast<int>(m.size()) - 1);
    return m[A.diff];
}

void diffToOne()
{
    const LevelEntry* e = current();
    if(e == nullptr) return;
    const auto& m = e->data->difficultyMults;
    for(int i = 0; i < static_cast<int>(m.size()); ++i)
        if(m[i] == 1.f) A.diff = i;

    // Arcade, unless set to 1x: the easiest of the level's difficulties
    // first (the player may still choose a harder one, ARCADE DIFFICULTY).
    if(A.s.arcade && A.s.arcadeDifficulty != 2) A.diff = 0;
}

// The difficulty arcade plays a level at: the player's choice, the easiest,
// or 1x, as the service menu says (ARCADE DIFFICULTY).
float arcadeDiff(const LevelEntry& e)
{
    const auto& m = e.data->difficultyMults;
    if(m.empty()) return 1.f;
    if(A.s.arcadeDifficulty == 1) return m.front();
    if(A.s.arcadeDifficulty == 2) return 1.f;
    return m[std::clamp(A.diff, 0, static_cast<int>(m.size()) - 1)];
}

// The table a run goes into: a level at a difficulty, with the help that
// was on (GAME SPEED, ROTATION, PULSE). 1x without help keeps the level's
// own name, as the tables were before.
std::string tableId(const std::string& levelId, float dm)
{
    std::string id = levelId;
    if(dm != 1.f) id += "_x" + diffText(dm);
    if(A.s.gameSpeed != 100) id += "_s" + std::to_string(A.s.gameSpeed);
    if(!A.s.rotation) id += "_norot";
    if(!A.s.pulse) id += "_nopulse";
    return id;
}

std::string diffText(float d);

void prepareMenuStyle()
{
    const LevelEntry* e = current();
    if(e == nullptr || A.menuStyleLevel == e->assetId) return;
    A.menuStyle = A.assets->getStyleData(e->packId, e->data->styleId);
    A.menuStyle.computeColors();
    A.menuStyleLevel = e->assetId;
}

std::string upper(std::string t)
{
    for(char& c : t) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return t;
}

std::string timeText(double seconds)
{
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", std::floor(seconds * 100.0) / 100.0);
    return b;
}

std::string diffText(float d)
{
    char b[32];
    std::snprintf(b, sizeof b, "%g", d);
    return b;
}

// ---------------------------------------------------------------------------
// Files: replays (for the attract demo) and the arcade tables.

std::string safe(std::string t)
{
    for(char& c : t)
        if(!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_' && c != '.') c = '_';
    return t;
}

fs::path replayPath(const std::string& levelId, float dm)
{
    return fs::path(saveDir()) / "replays" / (safe(levelId) + "_" + diffText(dm) + "x.ohr");
}

// Keep the replay of the level's best run, for the attract mode.
void keepReplayIfBest(const hg::replay_file& rf)
{
    // A run with help (slower, no rotation...) plays back wrong at the
    // normal settings: the demo and the preview show only runs without.
    if(!hg::Config::isEligibleForScore()) return;

    std::error_code ec;
    const fs::path p = replayPath(rf._level_id, rf._difficulty_mult);
    fs::create_directories(p.parent_path(), ec);

    hg::replay_file old;
    if(fs::exists(p) && old.deserialize_from_file(p) && old.played_seconds() >= rf.played_seconds())
    {
        return;
    }

    if(!rf.serialize_to_file(p))
    {
        ssvu::lo("vp::keepReplayIfBest") << "could not write " << p.string() << '\n';
        return;
    }

    // The spinner's turn per tick, beside it (.spin: floats).
    const std::vector<float>& spin = A.game->vpGetSpinLog();
    std::ofstream out(p.string() + ".spin", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(spin.data()), static_cast<std::streamsize>(spin.size() * sizeof(float)));
}

std::vector<float> loadSpin(const fs::path& replay)
{
    std::vector<float> v;
    std::ifstream in(replay.string() + ".spin", std::ios::binary);
    if(!in) return v;
    in.seekg(0, std::ios::end);
    const auto n = static_cast<std::size_t>(in.tellg()) / sizeof(float);
    in.seekg(0);
    v.resize(n);
    in.read(reinterpret_cast<char*>(v.data()), static_cast<std::streamsize>(n * sizeof(float)));
    return v;
}

// The attract loop shows no tutorial (2026-10-18): no table, no demo.
bool isTutorial(const std::string& levelId)
{
    return levelId.find("_tutorial_") != std::string::npos;
}

// The next level after `from` that is not a tutorial (`from` itself when
// all are).
int nextShown(int from)
{
    const int n = static_cast<int>(A.levels.size());
    for(int i = 1; i <= n; ++i)
    {
        const int k = (from + i) % n;
        if(!isTutorial(A.levels[k].assetId)) return k;
    }
    return from;
}

bool loadRandomReplay(hg::replay_file& out)
{
    std::error_code ec;
    std::vector<fs::path> files;
    for(const auto& e : fs::directory_iterator(fs::path(saveDir()) / "replays", ec))
        if(e.path().extension() == ".ohr") files.push_back(e.path());

    std::shuffle(files.begin(), files.end(), A.rnd);
    for(const fs::path& p : files)
    {
        hg::replay_file rf;
        if(rf.deserialize_from_file(p) && A.assets->isValidLevelId(rf._level_id) && !isTutorial(rf._level_id) &&
            A.assets->isValidPackId(rf._pack_id) && rf.played_seconds() > 5.0)
        {
            out = rf;
            A.pendingSpin = loadSpin(p);
            return true;
        }
    }
    return false;
}

fs::path tablePath(const std::string& levelId)
{
    return fs::path(saveDir()) / "arcade" / (safe(levelId) + ".txt");
}

Table loadTable(const std::string& levelId)
{
    Table t;
    std::ifstream in(tablePath(levelId));
    std::string name;
    int score;
    while(in >> name >> score && t.rows.size() < 10) t.rows.push_back({name, score});
    return t;
}

void saveTable(const std::string& levelId, const Table& t)
{
    std::error_code ec;
    const fs::path p = tablePath(levelId);
    fs::create_directories(p.parent_path(), ec);
    const fs::path tmp = p.string() + ".new";
    {
        std::ofstream out(tmp, std::ios::trunc);
        for(const auto& r : t.rows) out << r.name << ' ' << r.score << '\n';
    }
    fs::rename(tmp, p, ec);
}

int tableRank(const Table& t, int score)
{
    if(score <= 0) return -1;
    for(int i = 0; i < static_cast<int>(t.rows.size()); ++i)
        if(score > t.rows[i].score) return i;
    return t.rows.size() < 10 ? static_cast<int>(t.rows.size()) : -1;
}

// ---------------------------------------------------------------------------
// Modes

void enter(Mode m)
{
    A.mode = m;
    A.modeTime = 0.0;
    A.spinMenu = 0.f;
    A.ctl.reset();
}

void stopGameSound()
{
    if(A.audio)
    {
        A.audio->stopMusic();
        A.audio->stopSounds();
    }
}

bool startLevel(const LevelEntry& e, float dm, const hg::replay_file* replay,
    bool pilot = false)
{
    A.menuRequested = false;
    A.deathSeen = false;
    A.ctl.reset();
    A.game->vpDisableScores(pilot);
    A.swapHint = !pilot && replay == nullptr;

    try
    {
        if(replay != nullptr)
        {
            A.game->vpSetSpinReplay(std::move(A.pendingSpin));
            A.pendingSpin.clear();
            A.game->setLastReplay(*replay);
            A.game->newGame(replay->_pack_id, replay->_level_id,
                replay->_first_play, replay->_difficulty_mult, true);
        }
        else
        {
            A.game->newGame(e.packId, e.assetId, true, dm, false);
        }
    }
    catch(const std::exception& ex)
    {
        A.errorText = ex.what();
        if(!pilot) enter(Mode::Error);
        return false;
    }

    return true;
}

bool loadReplayFor(const std::string& levelId, float dm, hg::replay_file& out)
{
    std::error_code ec;
    const fs::path p = replayPath(levelId, dm);
    if(!(fs::exists(p, ec) && out.deserialize_from_file(p) && A.assets->isValidLevelId(out._level_id)))
        return false;
    A.pendingSpin = loadSpin(p);
    return true;
}

void runTicks(bool pilot);

void previewStop()
{
    A.preview = {};
    if(A.audio) A.audio->setQuiet(false);
}

// Once per step on the level choice: start the chosen level once the choice
// has rested a moment, run it, start it again a while after a death.
// The preview's steps: the pilot plays, and cannot die (the preview runs on
// instead of starting again); the player's own setting comes back after.
void previewTicks(int steps)
{
    const bool inv = hg::Config::getInvincible();
    hg::Config::setInvincible(true);
    for(int i = 0; i < steps && !A.menuRequested; ++i) runTicks(true);
    hg::Config::setInvincible(inv);
}

void updatePreview()
{
    auto& P = A.preview;
    const LevelEntry* e = current();
    if(e == nullptr) return;

    const float dm = A.mode == Mode::Select ? arcadeDiff(*e) : currentDiff();
    const std::string key = e->assetId + "|" + diffText(dm);

    if(key != P.key)
    {
        P.key = key;
        P.since = A.modeTime;
        P.running = false;
        return;
    }

    if(!P.running)
    {
        if(A.modeTime - P.since < PREVIEW_SETTLE) return;

        if(A.audio) A.audio->setQuiet(true);
        hg::replay_file rf;
        const bool replay = loadReplayFor(e->assetId, dm, rf);
        if(!startLevel(*e, dm, replay ? &rf : nullptr, true))
        {
            P.since = A.modeTime + 5.0; // a script that fails: try later
            return;
        }
        P.running = true;
        P.dead = false;

        // Ahead to where the level is under way, unseen.
        const auto t0 = std::chrono::steady_clock::now();
        previewTicks(PREVIEW_FORWARD);
        if(std::getenv("OH_PREVIEW_TIMING"))
            std::fprintf(stderr, "preview: %s ready in %lld ms\n", e->assetId.c_str(),
                static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - t0).count()));
        return;
    }

    previewTicks(1);

    if(A.menuRequested)
    {
        A.menuRequested = A.menuError = false;
        P.running = false;
        P.since = A.modeTime + 5.0;
        return;
    }

    if(A.game->hasDied())
    {
        if(!P.dead) { P.dead = true; P.deadAt = A.modeTime; }
        if(A.modeTime - P.deadAt > PREVIEW_AFTER_DEATH)
        {
            P.running = false;
            P.since = A.modeTime - PREVIEW_SETTLE;
        }
    }
}

// The title's music: a made piece of its own, 103 ticks a beat (139.8 bpm);
// the logo pulses with it.
const BeatSync& titleSync()
{
    static BeatSync s = []
    {
        BeatSync b;
        b.pulseTicks = 103;
        b.firstTicks = 0;
        b.perPulse = 1.0;
        return b;
    }();
    return s;
}

void startTitleMusic()
{
    if(!A.audio || A.s.music == 0) return;
    if(A.s.arcade && !A.s.attractSound) return;
    const std::shared_ptr<const Track> t = musicFor("@title", titleSync(), true);
    if(!t) return;
    A.audio->playGenerated(t->pcm, Track::RATE, t->barStart(17) , t->loopStart, "@title");
    A.audio->resumeMusic();
    A.titleMusicAt = A.modeTime;
}

void toHomeTitle();

void toAttract()
{
    previewStop();
    stopGameSound();
    applyVolumes(attractQuiet());
    enter(Mode::Title);
    startTitleMusic();
}

void toHomeMenu()
{
    previewStop();
    stopGameSound();
    applyVolumes(false);
    enter(Mode::HomeMenu);
    prepareMenuStyle();
}

void toHomeTitle()
{
    previewStop();
    stopGameSound();
    applyVolumes(false);
    enter(Mode::HomeTitle);
    startTitleMusic();
}

void toModeStart()
{
    collectLevels();
    A.sel = 0;
    diffToOne();
    if(A.s.arcade) toAttract();
    else toHomeTitle();
}

void startDemo()
{
    hg::replay_file rf;
    A.demoPilot = !loadRandomReplay(rf);

    if(A.demoPilot)
    {
        if(A.levels.empty()) { enter(Mode::Title); return; }
        std::uniform_int_distribution<int> pick(0, static_cast<int>(A.levels.size()) - 1);
        A.sel = pick(A.rnd);
        if(isTutorial(A.levels[A.sel].assetId)) A.sel = nextShown(A.sel);
        diffToOne();
        if(!startLevel(A.levels[A.sel], 1.f, nullptr, true)) return;
    }
    else
    {
        if(!startLevel(A.levels.front(), 1.f, &rf, true)) return;
    }

    enter(Mode::Demo);
}

// Run the original's ticks for one 60 Hz step: Config::TICKS_PER_SECOND.
void runTicks(bool pilot)
{
    const int ticks = static_cast<int>(hg::Config::TICKS_PER_SECOND / 60.f + 0.5f);

    for(int i = 0; i < ticks && !A.menuRequested; ++i)
    {
        bool ccw = false, cw = false;
        float spin = 0.f;
        if(pilot) A.game->vpAutopilot(ccw, cw);
        else A.ctl.tick(ticks, ccw, cw, spin);

        A.game->setInputs(ccw, cw, !pilot && A.ctl.focus, !pilot && A.ctl.swap);
        A.game->vpSetSpin(spin);

        try
        {
            A.game->tick(hg::Config::getTimescale());
        }
        catch(const std::exception& ex)
        {
            A.errorText = ex.what();
            A.menuRequested = true;
            A.menuError = true;
        }
    }

    if(A.audio) A.audio->update();
}

// ---------------------------------------------------------------------------
// Updates per mode

// The spinner in the menus: a step per MENU_STEP_DEG of turn.
int menuSteps(float& acc)
{
    acc += A.ctl.spinDeg;
    int steps = 0;
    while(acc >= MENU_STEP_DEG) { acc -= MENU_STEP_DEG; ++steps; }
    while(acc <= -MENU_STEP_DEG) { acc += MENU_STEP_DEG; --steps; }
    return steps;
}

void menuSpin(float& acc)
{
    const int steps = menuSteps(acc);
    if(steps == 0) return;
    A.sel += steps;
    A.diff = 0;
    diffToOne();
    if(A.audio) A.audio->playUiSound("beep.ogg");
}

void levelChoiceInput()
{
    menuSpin(A.spinMenu);
    if(A.ctl.menuRight || A.ctl.down) { ++A.sel; diffToOne(); if(A.audio) A.audio->playUiSound("beep.ogg"); }
    if(A.ctl.menuLeft || A.ctl.up) { --A.sel; diffToOne(); if(A.audio) A.audio->playUiSound("beep.ogg"); }
    prepareMenuStyle();
}

void updateMenuBackground(float dt)
{
    A.menuRotation += 6.f * dt * 60.f / 10.f; // a slow turn, as the original's menu
    A.menuStyle.update(dt * 60.f);
    A.menuStyle.computeColors();
}

void updateHomeTitle()
{
    if(A.audio) A.audio->update();
    if(A.ctl.ok || A.ctl.alt)
    {
        if(A.audio) A.audio->playUiSound("select.ogg");
        toHomeMenu();
    }
}

void updateHomeMenu(float dt)
{
    levelChoiceInput();
    updateMenuBackground(dt);
    updatePreview();

    const LevelEntry* e = current();
    if(e == nullptr) return;

    if(A.ctl.alt && e->data->difficultyMults.size() > 1)
    {
        A.diff = (A.diff + 1) % static_cast<int>(e->data->difficultyMults.size());
        if(A.audio) A.audio->playUiSound("difficultyMultUp.ogg");
    }

    if(A.ctl.ok)
    {
        previewStop();
        stopGameSound();
        if(startLevel(*e, currentDiff(), nullptr)) enter(Mode::HomeGame);
    }
    else if(A.ctl.menuBack)
    {
        toHomeTitle();
    }
}

void updateHomeGame()
{
    runTicks(false);

    if(A.game->hasDied())
    {
        if(!A.deathSeen)
        {
            A.deathSeen = true;
            if(!A.game->inReplay())
            {
                (void)A.game->takeNewPersonalBest();
                keepReplayIfBest(A.game->makeLastReplayFile());
            }
        }

        if(A.ctl.ok) { A.game->requestRestart(); A.deathSeen = false; }
        else if(A.ctl.alt) { A.game->requestReplay(); A.deathSeen = false; }
    }
    else
    {
        A.deathSeen = false;
    }

    if(A.ctl.backPressed || A.ctl.menuBack) A.game->requestMenu();

    if(A.menuRequested)
    {
        A.menuRequested = false;
        if(A.menuError) { A.menuError = false; enter(Mode::Error); stopGameSound(); return; }
        toHomeMenu();
    }
}

bool tryStart()
{
    if(!A.ctl.start1) return false;
    if(!vp_credits_take(&A.credits, 1)) return false;
    vp_lamps_set(0);
    applyVolumes(false);
    stopGameSound();
    diffToOne();
    enter(Mode::Select);
    A.preview = {};
    prepareMenuStyle();
    if(A.audio) A.audio->playUiSound("select.ogg");
    return true;
}

void updateAttract(float dt)
{
    vp_credits_blink(&A.credits, A.modeTime);
    if(tryStart()) return;

    if(A.mode == Mode::Title)
    {
        updateMenuBackground(dt);
        if(A.modeTime > TITLE_SECONDS) enter(Mode::Warning);
    }
    else if(A.mode == Mode::Warning)
    {
        updateMenuBackground(dt);
        if(A.modeTime > WARNING_SECONDS)
        {
            A.scoresLevel = A.levels.empty() ? 0 : nextShown(A.scoresLevel);
            enter(Mode::Scores);
        }
    }
    else if(A.mode == Mode::Scores)
    {
        updateMenuBackground(dt);
        if(A.modeTime > SCORES_SECONDS) { stopGameSound(); startDemo(); }
    }
    else if(A.mode == Mode::Demo)
    {
        runTicks(true);
        if(A.game->hasDied() && !A.deathSeen) { A.deathSeen = true; A.deathAt = A.modeTime; }
        if(A.menuRequested || A.modeTime > DEMO_SECONDS || (A.deathSeen && A.modeTime - A.deathAt > DEATH_PAUSE))
        {
            A.menuRequested = A.menuError = false;
            stopGameSound();
            enter(Mode::Title);
            startTitleMusic();
        }
    }
}

void updateSelect(float dt)
{
    levelChoiceInput();
    updateMenuBackground(dt);
    updatePreview();

    const LevelEntry* e = current();
    if(e == nullptr) { toAttract(); return; }

    if(A.s.arcadeDifficulty == 0 && A.ctl.alt && e->data->difficultyMults.size() > 1)
    {
        A.diff = (A.diff + 1) % static_cast<int>(e->data->difficultyMults.size());
        if(A.audio) A.audio->playUiSound("difficultyMultUp.ogg");
    }

    if(A.ctl.ok || A.modeTime > A.s.selectSeconds)
    {
        previewStop();
        stopGameSound();
        A.lives = A.s.lives;
        A.tutorialDone = false;
        A.best = 0.0;
        A.lastLifeTime = 0.0;
        A.playDm = arcadeDiff(*e);
        if(startLevel(*e, A.playDm, nullptr)) enter(Mode::Play);
    }
}

// Arcade: the next life, or a continue, plays the level as at its first
// start: the music from its beginning too. The original's restart takes a
// random part of the music (2026-10-19, the user: not in the middle).
bool restartFromStart()
{
    const LevelEntry* e = current();
    const bool ok = e != nullptr && startLevel(*e, A.playDm, nullptr);
    A.swapHint = false; // once, at the credit's first start
    return ok;
}

void gameOver()
{
    const LevelEntry* e = current();
    const int score = static_cast<int>(A.best * 100.0);
    const Table t = loadTable(tableId(e->assetId, A.playDm));
    A.newRank = tableRank(t, score);
    enter(Mode::GameOver);
}

void updatePlay()
{
    runTicks(false);

    if(A.game->hasDied())
    {
        if(!A.deathSeen)
        {
            A.deathSeen = true;
            A.deathAt = A.modeTime;
            A.lastLifeTime = A.game->getTimeSeconds();
            if(A.game->vpTutorial())
            {
                // The tutorial's script ended it (it cannot be lost): the
                // credit is done, no life lost, no score.
                A.tutorialDone = true;
                A.lives = 0;
            }
            else
            {
                A.best = std::max(A.best, A.lastLifeTime);
                --A.lives;
                keepReplayIfBest(A.game->makeLastReplayFile());
            }
        }

        if(A.modeTime - A.deathAt > DEATH_PAUSE && A.game->canRestartAfterDeath())
        {
            if(A.tutorialDone)
            {
                A.newRank = -1;
                enter(Mode::GameOver);
            }
            else if(A.lives > 0)
            {
                restartFromStart();
            }
            else
            {
                A.continueSkip = 0.0;
                A.continueShown = -1;
                enter(Mode::Continue);
            }
        }
    }

    if(A.menuRequested)
    {
        // A level's script failed: the credit is not lost for nothing.
        A.menuRequested = false;
        if(A.menuError) { A.menuError = false; ++A.credits.credits; enter(Mode::Error); stopGameSound(); }
    }
}

double continueLeft()
{
    return std::max(0.0, CONTINUE_SECONDS - A.modeTime - A.continueSkip);
}

// After the last life: START (with a credit) gives the lives again, the
// level from its start; the best try so far stays the score to beat. Any
// other button runs the countdown down faster.
void updateContinue()
{
    runTicks(false);
    vp_credits_blink(&A.credits, A.modeTime);

    if(A.ctl.start1 && vp_credits_take(&A.credits, 1))
    {
        vp_lamps_set(0);
        if(A.audio) A.audio->playUiSound("select.ogg");
        A.lives = A.s.lives;
        if(restartFromStart()) enter(Mode::Play);
        return;
    }

    const Controls& c = A.ctl;
    if(c.focusPressed || c.swapPressed || c.backPressed || c.leftPressed || c.rightPressed || c.up || c.down)
        A.continueSkip += CONTINUE_SKIP;

    const int shown = static_cast<int>(std::ceil(continueLeft()));
    if(shown != A.continueShown)
    {
        if(A.continueShown >= 0 && A.audio) A.audio->playUiSound("beep.ogg");
        A.continueShown = shown;
    }
    if(continueLeft() <= 0.0)
    {
        vp_lamps_set(0);
        gameOver();
    }
}

void updateGameOver()
{
    runTicks(false);
    if(A.modeTime < GAME_OVER_SECONDS) return;

    if(A.tutorialDone)
    {
        toAttract();
    }
    else if(A.newRank >= 0)
    {
        A.initials[0] = A.initials[1] = A.initials[2] = 'A';
        A.initialsPos = 0;
        A.initialsSpin = 0.f;
        enter(Mode::Initials);
    }
    else
    {
        A.scoresLevel = A.sel;
        stopGameSound();
        applyVolumes(attractQuiet());
        enter(Mode::Scores);
    }
}

// BACK_ARROW: the last "letter", drawn as an arrow: taking it goes back one
// letter (2026-10-19, the user: not with START 2).
constexpr char BACK_ARROW = '<';
const char LETTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-<";
constexpr int NLETTERS = static_cast<int>(sizeof LETTERS) - 1;

void stepLetter(int by)
{
    char& c = A.initials[A.initialsPos];
    const char* p = std::strchr(LETTERS, c);
    int i = p ? static_cast<int>(p - LETTERS) : 0;
    i = ((i + by) % NLETTERS + NLETTERS) % NLETTERS;
    c = LETTERS[i];
    if(A.audio) A.audio->playUiSound("beep.ogg");
}

void finishInitials()
{
    for(int i = 0; i < 3; ++i)
        if(A.initials[i] == BACK_ARROW) A.initials[i] = '-'; // the time ran out on the arrow
    const LevelEntry* e = current();
    const int score = static_cast<int>(A.best * 100.0);
    Table t = loadTable(tableId(e->assetId, A.playDm));
    const int r = tableRank(t, score);
    if(r >= 0)
    {
        t.rows.insert(t.rows.begin() + r, Table::Row{A.initials, score});
        if(t.rows.size() > 10) t.rows.resize(10);
        saveTable(tableId(e->assetId, A.playDm), t);
    }
    (void)vp_hiscore_insert(A.initials, score); // the menu's hint bar
    A.scoresLevel = A.sel;
    stopGameSound();
    applyVolumes(attractQuiet());
    enter(Mode::Scores);
}

void updateInitials()
{
    runTicks(false);

    for(int st = menuSteps(A.initialsSpin); st != 0; st += st > 0 ? -1 : 1) stepLetter(st > 0 ? 1 : -1);
    if(A.ctl.menuRight || A.ctl.up) stepLetter(1);
    if(A.ctl.menuLeft || A.ctl.down) stepLetter(-1);

    // Any of the game's buttons takes the letter (2026-10-19).
    const Controls& c = A.ctl;
    if(c.ok || c.focusPressed || c.swapPressed || c.backPressed)
    {
        char& now = A.initials[A.initialsPos];
        if(now == BACK_ARROW)
        {
            if(A.initialsPos == 0) return;
            now = A.initials[A.initialsPos - 1];
            --A.initialsPos;
            if(A.audio) A.audio->playUiSound("beep.ogg");
            return;
        }
        if(A.audio) A.audio->playUiSound("select.ogg");
        if(++A.initialsPos >= 3) { finishInitials(); return; }
        A.initials[A.initialsPos] = A.initials[A.initialsPos - 1];
    }
    if(A.modeTime > INITIALS_SECONDS) finishInitials();
}

// ---------------------------------------------------------------------------
// Drawing

void drawCredits(vp_dvg* d)
{
    const Rgb c = ui::dim(WHITE, A.s.brightness[BRIGHT_TEXT]);
    if(A.s.freePlay)
    {
        ui::text(d, "FREE PLAY", 192.f, 274.f, 6.f, VP_TEXT_CENTER, c);
        return;
    }
    char b[32];
    std::snprintf(b, sizeof b, "CREDITS %d", A.credits.credits);
    ui::text(d, b, 192.f, 274.f, 6.f, VP_TEXT_CENTER, c);
}

void drawPrompt(vp_dvg* d, float y)
{
    const bool blink = std::fmod(A.modeTime, 1.0) < 0.6;
    if(!blink) return;
    const Rgb c = ui::dim(YELLOW, A.s.brightness[BRIGHT_TEXT]);
    if(vp_credits_enough(&A.credits, 1)) ui::text(d, "PRESS START", 192.f, y, 9.f, VP_TEXT_CENTER, c);
    else ui::text(d, "INSERT COIN", 192.f, y, 9.f, VP_TEXT_CENTER, c);
}

// Words to lines of at most `width` units at `height`.
using ui::wrap;

// A level's name as one line at `height`, or as two at `small` when it is
// too wide (it was cut at 24 letters), each line no wider than `width`.
float nameLines(const std::string& name, float height, float small, float width, std::vector<std::string>& out)
{
    out = {name};
    if(ui::textWidth(name, height) <= width) return height;
    out = wrap(name, small, width);
    if(out.size() > 2) out.resize(2);
    float h = small;
    for(const std::string& l : out) h = std::min(h, small * width / std::max(width, ui::textWidth(l, small)));
    return h;
}

void drawLevelInfo(vp_dvg* d, bool arcade)
{
    const LevelEntry* e = current();
    if(e == nullptr) return;
    const int tb = A.s.brightness[BRIGHT_TEXT];
    const hg::PackData& pack = A.assets->getPackData(e->packId);

    std::vector<std::string> name;
    const float nh = nameLines(upper(e->data->name), 13.f, 9.f, 350.f, name);

    ui::text(d, upper(pack.name), 192.f, 18.f, 6.f, VP_TEXT_CENTER, ui::dim(CYAN, tb));
    float ny = name.size() > 1 ? 29.f : 32.f;
    for(const std::string& l : name)
    {
        ui::text(d, l, 192.f, ny, nh, VP_TEXT_CENTER, ui::dim(WHITE, tb));
        ny += nh + 4.f;
    }
    ui::text(d, "BY " + upper(e->data->author), 192.f, std::max(52.f, ny + 3.f), 5.f, VP_TEXT_CENTER, ui::dim(CYAN, tb));

    float y = 196.f;
    for(const std::string& l : wrap(upper(e->data->description), 5.f, 340.f))
    {
        if(y > 226.f) break;
        ui::text(d, l, 192.f, y, 5.f, VP_TEXT_CENTER, ui::dim(Rgb{180, 180, 180}, tb));
        y += 9.f;
    }

    char b[64];
    if(!arcade)
    {
        const float dm = currentDiff();
        if(e->data->difficultyMults.size() > 1)
        {
            std::snprintf(b, sizeof b, "DIFFICULTY %sX", diffText(dm).c_str());
            ui::text(d, b, 16.f, 240.f, 6.f, VP_TEXT_LEFT, ui::dim(YELLOW, tb));
        }
        const float best = A.assets->getLocalScore(e->data->getValidatorWithoutPackId(dm));
        std::snprintf(b, sizeof b, "BEST %s", timeText(best).c_str());
        ui::text(d, b, 368.f, 240.f, 6.f, VP_TEXT_RIGHT, ui::dim(YELLOW, tb));
        ui::text(d, e->data->difficultyMults.size() > 1
                        ? "SPINNER - LEVEL   START 2 - DIFFICULTY   START 1 - PLAY"
                        : "SPINNER - LEVEL   START 1 - PLAY",
            192.f, 262.f, 4.5f, VP_TEXT_CENTER, ui::dim(GREEN, tb));
    }
    else
    {
        const float dm = arcadeDiff(*e);
        const Table t = loadTable(tableId(e->assetId, dm));
        if(e->data->difficultyMults.size() > 1 || dm != 1.f)
        {
            std::snprintf(b, sizeof b, "DIFFICULTY %sX", diffText(dm).c_str());
            ui::text(d, b, 16.f, 226.f, 6.f, VP_TEXT_LEFT, ui::dim(YELLOW, tb));
        }
        if(!t.rows.empty())
        {
            std::snprintf(b, sizeof b, "HIGH SCORE %s %s", t.rows[0].name.c_str(),
                timeText(t.rows[0].score / 100.0).c_str());
            ui::text(d, b, 192.f, 240.f, 6.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
        }
        const int left = std::max(0, A.s.selectSeconds - static_cast<int>(A.modeTime));
        std::snprintf(b, sizeof b, "%d", left);
        ui::text(d, b, 368.f, 6.f, 10.f, VP_TEXT_RIGHT, ui::dim(YELLOW, tb));
        ui::text(d, A.s.arcadeDifficulty == 0 && e->data->difficultyMults.size() > 1
                        ? "SPINNER - LEVEL   START 2 - DIFFICULTY   START 1 - PLAY"
                        : "SPINNER - LEVEL   START 1 - PLAY",
            192.f, 262.f, 4.5f, VP_TEXT_CENTER, ui::dim(GREEN, tb));
    }
}

void drawMenuBackground(vp_dvg* d)
{
    if((A.mode == Mode::HomeMenu || A.mode == Mode::Select) && A.preview.running)
    {
        A.game->draw();
        A.renderer.drawGame(*A.game, d, A.s, false);
        return;
    }

    prepareMenuStyle();
    const LevelEntry* e = current();
    if(e == nullptr) return;
    A.renderer.drawMenuBackground(A.menuStyle, 6, A.menuRotation, d, A.s);
}

void drawTitle(vp_dvg* d)
{
    // In time with the title's music when it plays (from its start).
    const float beatHz = static_cast<float>(titleSync().bpm() / 60.0);
    A.renderer.drawTitle(A.modeTime - A.titleMusicAt, beatHz, d, A.s);
    const int tb = A.s.brightness[BRIGHT_TEXT];
    ui::text(d, "BY VITTORIO ROMEO", 192.f, 186.f, 5.f, VP_TEXT_CENTER, ui::dim(Rgb{255, 150, 190}, tb));
    if(A.mode == Mode::HomeTitle)
    {
        if(std::fmod(A.modeTime, 1.0) < 0.6)
            ui::text(d, "PRESS START", 192.f, 228.f, 9.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
        return;
    }
    drawPrompt(d, 228.f);
    drawCredits(d);
}

void drawScores(vp_dvg* d)
{
    drawMenuBackground(d);
    const int tb = A.s.brightness[BRIGHT_TEXT];
    if(A.levels.empty()) return;
    const LevelEntry& e = A.levels[A.scoresLevel % A.levels.size()];
    const Table t = loadTable(tableId(e.assetId, A.s.arcadeDifficulty == 2 || e.data->difficultyMults.empty()
                                                       ? 1.f : e.data->difficultyMults.front()));

    ui::text(d, "HIGH SCORES", 192.f, 14.f, 11.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
    std::vector<std::string> name;
    const float nh = nameLines(upper(e.data->name), 7.f, 5.f, 350.f, name);
    float ny = name.size() > 1 ? 30.f : 34.f;
    for(const std::string& l : name)
    {
        ui::text(d, l, 192.f, ny, nh, VP_TEXT_CENTER, ui::dim(CYAN, tb));
        ny += nh + 3.f;
    }

    for(int i = 0; i < 10; ++i)
    {
        const float y = 56.f + 17.f * i;
        char b[64];
        const bool mine = A.mode == Mode::Scores && i == A.newRank && A.sel == A.scoresLevel &&
                          std::fmod(A.modeTime, 0.5) < 0.25;
        const Rgb c = ui::dim(mine ? YELLOW : WHITE, tb);
        if(i < static_cast<int>(t.rows.size()))
        {
            std::snprintf(b, sizeof b, "%2d. %-3s", i + 1, t.rows[i].name.c_str());
            ui::text(d, b, 110.f, y, 7.f, VP_TEXT_LEFT, c);
            ui::text(d, timeText(t.rows[i].score / 100.0), 280.f, y, 7.f, VP_TEXT_RIGHT, c);
        }
        else
        {
            std::snprintf(b, sizeof b, "%2d. ---", i + 1);
            ui::text(d, b, 110.f, y, 7.f, VP_TEXT_LEFT, ui::dim(Rgb{90, 90, 90}, tb));
        }
    }
    drawPrompt(d, 236.f);
    drawCredits(d);
}

void drawArcadeHud(vp_dvg* d)
{
    if(A.game->vpTutorial())
    {
        drawCredits(d); // a tutorial: no time to add up, no lives
        return;
    }

    const int tb = A.s.brightness[BRIGHT_TEXT];
    char b[64];
    if(A.best > 0.0)
    {
        std::snprintf(b, sizeof b, "BEST %s", timeText(A.best).c_str());
        ui::text(d, b, 372.f, 8.f, 7.f, VP_TEXT_RIGHT, ui::dim(WHITE, tb));
    }

    // The lives left, as the player's triangle.
    vp_color(d, static_cast<std::uint8_t>(255 * tb / 100), static_cast<std::uint8_t>(255 * tb / 100), 0);
    const int shownLives = A.deathSeen ? A.lives : A.lives - 1;
    for(int i = 0; i < shownLives; ++i)
    {
        const float x = 366.f - i * 12.f, y = 26.f;
        ui::line(d, x, y - 4.f, x + 4.f, y + 4.f);
        ui::line(d, x + 4.f, y + 4.f, x - 4.f, y + 4.f);
        ui::line(d, x - 4.f, y + 4.f, x, y - 4.f);
    }
    drawCredits(d);
}

// A level that can be swapped in says so at its start, for its first
// SWAP_HINT_SECONDS (2026-10-19, the user). Not in the tutorials: they
// teach it themselves.
constexpr double SWAP_HINT_SECONDS = 4.0;

void drawSwapHint(vp_dvg* d)
{
    if(!A.swapHint) return;
    if(A.game->hasDied() || A.game->getTimeSeconds() > SWAP_HINT_SECONDS || A.game->vpTutorial())
    {
        A.swapHint = false;
        return;
    }
    if(!A.game->vpSwapEnabled()) return;
    float y = 244.f;
    for(const std::string& l : ui::wrap(swapHint(A.s), 6.f, 350.f))
    {
        ui::text(d, l, 192.f, y, 6.f, VP_TEXT_CENTER, ui::dim(YELLOW, A.s.brightness[BRIGHT_TEXT]));
        y += 10.f;
    }
}

// `dimPercent`: the game behind a screen of lettering (game over, initials)
// at that brightness, so the lettering stands out.
void drawGame(vp_dvg* d, bool hud = true, int dimPercent = 100)
{
    A.game->draw();
    Settings s = A.s;
    for(int& b : s.brightness) b = b * dimPercent / 100;
    A.renderer.drawGame(*A.game, d, s, hud);
}

// The original's epilepsyWarning.png, as lettering.
void drawWarning(vp_dvg* d)
{
    drawMenuBackground(d);
    const int tb = A.s.brightness[BRIGHT_TEXT];
    ui::text(d, "WARNING", 192.f, 46.f, 16.f, VP_TEXT_CENTER, ui::dim(RED, tb));
    const char* lines[] = {
        "THIS GAME CONTAINS FLASHING LIGHTS,",
        "FAST MOVING PATTERNS AND QUICK",
        "CHANGES OF COLOR THAT MAY TRIGGER",
        "SEIZURES IN PEOPLE WITH",
        "PHOTOSENSITIVE EPILEPSY.",
        "",
        "IF YOU FEEL DIZZY, YOUR VISION",
        "CHANGES OR YOUR MUSCLES TWITCH,",
        "STOP PLAYING AT ONCE.",
    };
    float y = 92.f;
    for(const char* l : lines)
    {
        ui::text(d, l, 192.f, y, 6.f, VP_TEXT_CENTER, ui::dim(WHITE, tb));
        y += 14.f;
    }
    drawPrompt(d, 236.f);
    drawCredits(d);
}

void drawContinue(vp_dvg* d)
{
    const int tb = A.s.brightness[BRIGHT_TEXT];
    drawGame(d, false, 35);
    ui::text(d, "CONTINUE?", 192.f, 56.f, 14.f, VP_TEXT_CENTER, ui::dim(WHITE, tb));
    char b[16];
    std::snprintf(b, sizeof b, "%d", std::max(0, A.continueShown));
    ui::text(d, b, 192.f, 92.f, 30.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
    if(A.best > 0.0) ui::text(d, "BEST " + timeText(A.best), 192.f, 160.f, 9.f, VP_TEXT_CENTER, ui::dim(CYAN, tb));
    drawPrompt(d, 190.f);
    drawCredits(d);
}

void drawError(vp_dvg* d)
{
    const int tb = A.s.brightness[BRIGHT_TEXT];
    ui::text(d, "THIS LEVEL HAS AN ERROR", 192.f, 60.f, 9.f, VP_TEXT_CENTER, ui::dim(RED, tb));
    float y = 90.f;
    for(const std::string& l : wrap(upper(A.errorText), 4.5f, 350.f))
    {
        if(y > 230.f) break;
        ui::text(d, l, 192.f, y, 4.5f, VP_TEXT_CENTER, ui::dim(WHITE, tb));
        y += 9.f;
    }
    ui::text(d, "START 1 - BACK", 192.f, 262.f, 5.f, VP_TEXT_CENTER, ui::dim(GREEN, tb));
}

// OH_MUSIC_DEMO=<seconds>: for every level the made piece as the level
// starts it (difficulty 1, first play), that many seconds of it, written to
// save/music_demo/<level>.wav, with how long making it took.
void musicDemo(vp_game* g, double seconds)
{
    g_musicDemo = true;
    std::error_code ec;
    const fs::path dir = fs::path(saveDir()) / "music_demo";
    fs::create_directories(dir, ec);

    for(const auto& [id, ld] : A.assets->getLevelDatas())
    {
        const LevelEntry e{id, ld.packId, &ld};
        if(!startLevel(e, 1.f, nullptr, true)) continue;
        g_lastSync = {};
        runTicks(true); // start(): the beat pulse as the level sets it
        if(g_lastSync.pulseTicks == 0) { std::fprintf(stderr, "music-demo: %s: no start\n", id.c_str()); continue; }

        const auto t0 = std::chrono::steady_clock::now();
        const std::shared_ptr<const Track> t = musicFor(id, g_lastSync, true);
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        const std::int64_t start = musicStartFrame(*t, g_lastSync, true, 0);
        const std::int64_t frames = std::min<std::int64_t>(static_cast<std::int64_t>(seconds * Track::RATE),
            static_cast<std::int64_t>(t->pcm.size() / 2) - start);

        std::ofstream out(dir / (safe(ld.id) + ".wav"), std::ios::binary);
        auto u32 = [&](std::uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
        auto u16 = [&](std::uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
        out.write("RIFF", 4); u32(static_cast<std::uint32_t>(36 + frames * 4)); out.write("WAVEfmt ", 8);
        u32(16); u16(1); u16(2); u32(Track::RATE); u32(Track::RATE * 4); u16(4); u16(16);
        out.write("data", 4); u32(static_cast<std::uint32_t>(frames * 4));
        out.write(reinterpret_cast<const char*>(t->pcm.data() + start * 2), frames * 4);

        int peak = 0;
        for(std::int64_t i = start * 2; i < (start + frames) * 2; ++i) peak = std::max(peak, std::abs(static_cast<int>(t->pcm[static_cast<std::size_t>(i)])));
        std::fprintf(stderr, "music-demo: %s: %.1f bpm (pulse every %d ticks, %g beats), first pulse after %d ticks, made in %lld ms, peak %d\n",
            ld.id.c_str(), g_lastSync.bpm(), g_lastSync.pulseTicks, g_lastSync.perPulse, g_lastSync.firstTicks, ms, peak);
    }
    g_musicDemo = false;
    vp_game_quit(g);
}

// OH_SELFTEST=<seconds>: every level, every difficulty, played by the pilot
// for up to that many seconds of game time, as fast as it goes. One line per
// level on stderr (survived time, died or not, Lua errors), then the end.
void selfTest(vp_game* g, double seconds)
{
    hg::Config::setNoMusic(true);
    int errors = 0, runs = 0;
    for(const auto& [id, ld] : A.assets->getLevelDatas())
    {
        for(const float dm : ld.difficultyMults)
        {
            const LevelEntry e{id, ld.packId, &ld};
            if(!startLevel(e, dm, nullptr, true))
            {
                std::fprintf(stderr, "selftest: %s x%g: ERROR at start: %s\n", id.c_str(), dm, A.errorText.c_str());
                ++errors;
                continue;
            }

            const int steps = static_cast<int>(seconds * 60.0);
            int i = 0;
            int darkest = 999, darkFrames = 0, wallFrames = 0, flips = 0, hidden = 0;
            bool lastInv = false, first = true;
            for(; i < steps && !A.game->hasDied() && !A.menuRequested; ++i)
            {
                runTicks(true);
                A.game->draw();
                A.renderer.drawGame(*A.game, vp_game_output(g), A.s);
                hidden += A.renderer.mostlyHiddenWalls() > 0 ? 1 : 0;
                const Renderer::Probe pr = Renderer::probe(*A.game, A.s);
                if(!first && pr.inverted != lastInv) ++flips;
                lastInv = pr.inverted;
                first = false;
                if(pr.walls > 0)
                {
                    ++wallFrames;
                    darkest = std::min(darkest, pr.darkestWall);
                    if(pr.darkestWall < 60) ++darkFrames;
                }
            }
            std::fprintf(stderr, "selftest-color: %s x%g: darkest wall %d, dark frames %d/%d, inversion flips %d, frames with a mostly hidden wall %d%s\n",
                id.c_str(), dm, darkest == 999 ? -1 : darkest, darkFrames, wallFrames, flips, hidden, lastInv ? " (inverted)" : "");

            ++runs;
            if(A.menuRequested && A.menuError) ++errors;
            std::fprintf(stderr, "selftest: %s x%g: %.2f s%s%s %s\n", id.c_str(), dm,
                A.game->getTimeSeconds(), A.game->hasDied() ? " died" : "",
                A.menuError ? " LUA ERROR" : "", A.menuError ? A.errorText.c_str() : "");
            A.menuRequested = A.menuError = false;
        }
    }
    std::fprintf(stderr, "selftest: %d runs, %d with errors\n", runs, errors);
    vp_game_quit(g);
}

} // namespace

const Settings& appBootSettings()
{
    loadSettings(g_boot);
    return g_boot;
}

int appInit(vp_game* g)
{
    // The original reads Packs/ and Assets/ from its working folder.
    std::error_code ec;
    fs::current_path(fs::path(vp_package_dir()) / "assets", ec);
    if(ec)
    {
        std::fprintf(stderr, "openhexagon: no assets folder next to the program\n");
        return VP_ERR_IO;
    }

    A.s = g_boot;
    applyToConfig(A.s);
    ui::setScreenSize(A.s.screenSize);

    // More packs: save/packs/<pack>/ (pack.json, Levels, Scripts, Styles,
    // Music), kept over updates of the game.
    std::string extra = saveDir() + "/packs/";
    for(char& c : extra) if(c == '\\') c = '/';
    fs::create_directories(extra, ec);
    A.assets = std::make_unique<hg::HGAssets>(saveDir() + "/scores.txt", extra);
    for(const std::string& m : A.assets->getLoadResults().errorMessages)
        std::fprintf(stderr, "openhexagon: %s", m.c_str());

    collectLevels();

    // Music: a pack's Music/ folder, else save/music/<file> (the Steam
    // version's .ogg files can be put there).
    for(const auto& [id, ld] : A.assets->getLevelDatas())
    {
        const hg::MusicData& md = A.assets->getMusicData(ld.packId, ld.musicId);
        A.musicFallback.emplace(ld.packId + "_" + ld.musicId, saveDir() + "/music/" + md.fileName);
    }

    A.dev = vp_game_audio(g);
    A.audio = std::make_unique<hg::Audio>(
        A.dev,
        [](const std::string& id) { return A.assets->getSoundPath(id); },
        [](const std::string& id) -> const std::string*
        {
            if(const std::string* p = A.assets->getMusicPath(id)) return p;
            const auto it = A.musicFallback.find(id);
            if(it == A.musicFallback.end()) return nullptr;
            std::error_code e2;
            return fs::exists(it->second, e2) ? &it->second : nullptr;
        });

    // Music not set yet: the original's files where there are some.
    if(A.s.music < 0)
    {
        A.s.music = 1;
        for(const auto& [id, ld] : A.assets->getLevelDatas())
        {
            if(A.audio->hasMusic(ld.packId, ld.musicId)) { A.s.music = 2; break; }
        }
        applyToConfig(A.s);
    }

    A.game = std::make_unique<hg::HexagonGame>(
        nullptr, nullptr, *A.assets, A.audio.get(), &A.window, nullptr);
    A.game->vpTextFilter = [](const std::string& t) { return controlText(t, A.s); };
    A.game->vpAfterLevelScript = [](Lua::LuaContext& lua, const std::string& id)
    { runLevelOverride(lua, id, A.s); };
    // The made soundtrack (music.cpp): in time with the beat pulse.
    A.game->vpUseMadeMusic = []
    {
        if(A.s.music == 1) return true;
        if(A.s.music != 2) return false;
        const hg::LevelData* ld = A.game->getLevelData();
        return ld == nullptr || !A.audio->hasMusic(ld->packId, ld->musicId);
    };
    A.game->vpOnStart = [](float delayMax, float delayNow, float dmFactor, bool firstPlay)
    {
        const BeatSync sync = beatSync(delayMax, delayNow, dmFactor);
        const std::string& id = A.game->getLevelId();
        g_lastSync = sync;
        if(g_musicDemo) return;
        if(A.mode == Mode::HomeMenu || A.mode == Mode::Select)
        {
            (void)musicFor(id, sync, false); // the preview: make it while one looks
            return;
        }
        const std::shared_ptr<const Track> t = musicFor(id, sync, true);
        if(!t || !A.audio) return;
        const std::int64_t start = musicStartFrame(*t, sync, firstPlay, A.rnd());
        A.audio->playGenerated(t->pcm, Track::RATE, start, t->loopStart,
            id + "@" + std::to_string(sync.pulseTicks) + "/" + std::to_string(sync.perPulse));
    };
    A.game->fnGoToMenu = [](const bool error)
    {
        A.menuRequested = true;
        A.menuError = error;
    };

    vp_credits_init(&A.credits);
    A.credits.free_play = A.s.freePlay ? 1 : 0;
    vp_input_grab_mouse(vp_game_input(g), 1);

    A.sel = 0;
    diffToOne();
    toModeStart();

    // OH_LEVEL=<part of a level id>: a testing aid, straight into that level.
    if(const char* l = std::getenv("OH_LEVEL"))
    {
        for(int i = 0; i < static_cast<int>(A.levels.size()); ++i)
        {
            if(A.levels[i].assetId.find(l) == std::string::npos) continue;
            A.sel = i;
            diffToOne();
            if(startLevel(A.levels[i], currentDiff(), nullptr)) enter(A.s.arcade ? Mode::Play : Mode::HomeGame);
            A.lives = A.s.lives;
            break;
        }
    }

    if(const char* t = std::getenv("OH_MUSIC_DEMO"))
    {
        musicDemo(g, std::atof(t) > 0.0 ? std::atof(t) : 60.0);
    }

    if(const char* t = std::getenv("OH_SELFTEST"))
    {
        selfTest(g, std::atof(t) > 0.0 ? std::atof(t) : 30.0);
    }

    return A.levels.empty() ? VP_ERR_IO : VP_OK;
}

void appUpdate(vp_game* g, float dt)
{
    vp_input* in = vp_game_input(g);
    const bool hotkey = serviceHotkey();

    A.modeTime += dt;

    if(A.service)
    {
        const int r = serviceUpdate(in, hotkey);
        if(r == SERVICE_OPEN) return;

        A.service = false;
        if(r == SERVICE_SAVED)
        {
            const bool modeChanged = serviceGet().arcade != A.s.arcade;
            A.s = serviceGet();
            saveSettings(A.s);
            applyToConfig(A.s);
            ui::setScreenSize(A.s.screenSize);
            A.credits.free_play = A.s.freePlay ? 1 : 0;
            if(modeChanged) { toModeStart(); return; }
        }
        applyVolumes(A.s.arcade && (A.mode == Mode::Title || A.mode == Mode::Warning || A.mode == Mode::Scores || A.mode == Mode::Demo) && !A.s.attractSound);
        if(A.mode == Mode::HomeGame || A.mode == Mode::Play || A.mode == Mode::Demo || A.mode == Mode::Continue || A.mode == Mode::GameOver || A.mode == Mode::Initials)
            toModeStart(); // the music was stopped for the menu: start over
        return;
    }

    if(hotkey)
    {
        A.service = true;
        vp_lamps_set(0);
        serviceOpen(A.s, A.audio.get());
        return;
    }

    A.ctl.step(in, A.s);

    if(A.s.arcade)
    {
        if(vp_credits_update(&A.credits, in) && A.audio)
        {
            A.audio->playUiSound("increment.ogg");
        }
    }

    switch(A.mode)
    {
        case Mode::HomeTitle: updateHomeTitle(); break;
        case Mode::HomeMenu: updateHomeMenu(dt); break;
        case Mode::HomeGame: updateHomeGame(); break;
        case Mode::Title:
        case Mode::Warning:
        case Mode::Scores:
        case Mode::Demo: updateAttract(dt); break;
        case Mode::Select: updateSelect(dt); break;
        case Mode::Play: updatePlay(); break;
        case Mode::Continue: updateContinue(); break;
        case Mode::GameOver: updateGameOver(); break;
        case Mode::Initials: updateInitials(); break;
        case Mode::Error:
            if(A.ctl.ok || A.modeTime > 20.0) toModeStart();
            break;
    }

    if(A.mode != Mode::Title && A.mode != Mode::Warning && A.mode != Mode::Scores && A.mode != Mode::Demo &&
        A.mode != Mode::Select && A.mode != Mode::Continue)
    {
        vp_lamps_set(0);
    }
}

void appDraw(vp_game* g, vp_dvg* d)
{
    (void)g;

    if(A.service)
    {
        serviceDraw(d);
        return;
    }

    const int tb = A.s.brightness[BRIGHT_TEXT];

    switch(A.mode)
    {
        case Mode::HomeMenu:
            drawMenuBackground(d);
            drawLevelInfo(d, false);
            break;

        case Mode::HomeGame:
            drawGame(d);
            drawSwapHint(d);
            break;

        case Mode::Title:
        case Mode::HomeTitle: drawTitle(d); break;
        case Mode::Warning: drawWarning(d); break;
        case Mode::Scores: drawScores(d); break;

        case Mode::Demo:
            drawGame(d);
            ui::text(d, "DEMO", 192.f, 236.f, 7.f, VP_TEXT_CENTER, ui::dim(CYAN, tb));
            drawPrompt(d, 250.f);
            drawCredits(d);
            break;

        case Mode::Select:
            drawMenuBackground(d);
            drawLevelInfo(d, true);
            drawCredits(d);
            break;

        case Mode::Play:
            drawGame(d);
            drawSwapHint(d);
            drawArcadeHud(d);
            break;

        case Mode::Continue: drawContinue(d); break;

        case Mode::GameOver:
        {
            drawGame(d, false, 35);
            if(A.tutorialDone)
            {
                ui::text(d, "TUTORIAL COMPLETE", 192.f, 110.f, 14.f, VP_TEXT_CENTER, ui::dim(WHITE, tb));
                ui::text(d, "NOW PLAY SOME REAL LEVELS!", 192.f, 140.f, 7.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
            }
            else
            {
                ui::text(d, "GAME OVER", 192.f, 110.f, 16.f, VP_TEXT_CENTER, ui::dim(WHITE, tb));
                ui::text(d, "BEST " + timeText(A.best), 192.f, 140.f, 9.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
            }
            drawCredits(d);
            break;
        }

        case Mode::Initials:
        {
            drawGame(d, false, 35);
            const LevelEntry* e = current();
            ui::text(d, "NEW HIGH SCORE", 192.f, 70.f, 11.f, VP_TEXT_CENTER, ui::dim(YELLOW, tb));
            if(e)
            {
                std::vector<std::string> name;
                const float nh = nameLines(upper(e->data->name), 6.f, 4.5f, 350.f, name);
                ui::text(d, name[0], 192.f, name.size() > 1 ? 86.f : 92.f, nh, VP_TEXT_CENTER, ui::dim(CYAN, tb));
                if(name.size() > 1) ui::text(d, name[1], 192.f, 94.f, nh, VP_TEXT_CENTER, ui::dim(CYAN, tb));
            }
            ui::text(d, timeText(A.best), 192.f, 108.f, 9.f, VP_TEXT_CENTER, ui::dim(WHITE, tb));
            for(int i = 0; i < 3; ++i)
            {
                const bool now = i == A.initialsPos;
                if(now && std::fmod(A.modeTime, 0.4) > 0.25) continue;
                const char letter[2] = {A.initials[i], 0};
                const float x = 168.f + 24.f * i;
                const Rgb lc = ui::dim(now ? YELLOW : WHITE, tb);
                if(letter[0] == BACK_ARROW)
                {
                    vp_color(d, lc.r, lc.g, lc.b);
                    ui::line(d, x + 7.f, 148.f, x - 7.f, 148.f);
                    ui::line(d, x - 7.f, 148.f, x - 1.f, 142.f);
                    ui::line(d, x - 7.f, 148.f, x - 1.f, 154.f);
                }
                else ui::text(d, letter, x, 140.f, 16.f, VP_TEXT_CENTER, lc);
                vp_color(d, 0, static_cast<std::uint8_t>(200 * tb / 100), 0);
                ui::line(d, x - 8.f, 162.f, x + 8.f, 162.f);
            }
            ui::text(d, "SPINNER - LETTER   ANY BUTTON - TAKE   ARROW - BACK", 192.f, 200.f, 4.5f, VP_TEXT_CENTER, ui::dim(GREEN, tb));
            break;
        }

        case Mode::Error: drawError(d); break;
    }
}

void appShutdown(vp_game* g)
{
    (void)g;
    A.game.reset();
    A.audio.reset();
    A.assets.reset();
}

} // namespace vp
