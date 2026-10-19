// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

#pragma once

#include "SSVOpenHexagon/Core/CustomTimelineManager.hpp"
#include "SSVOpenHexagon/Core/HGStatus.hpp"
#include "SSVOpenHexagon/Core/RandomNumberGenerator.hpp"
#include "SSVOpenHexagon/Core/Replay.hpp"

#include "SSVOpenHexagon/Data/LevelStatus.hpp"
#include "SSVOpenHexagon/Data/MusicData.hpp"
#include "SSVOpenHexagon/Data/StyleData.hpp"

#include "SSVOpenHexagon/Components/CPlayer.hpp"

#include "SSVOpenHexagon/Utils/Utils.hpp"
#include "SSVOpenHexagon/Utils/LuaWrapper.hpp"
#include "SSVOpenHexagon/Utils/FastVertexVector.hpp"
#include "SSVOpenHexagon/Utils/Timeline2.hpp"

#include "SSVOpenHexagon/Components/CCustomWallManager.hpp"

// VectorPie: SSVStart's GameSystem and VertexVector are not used; Camera
// and GameWindow are the small versions in src/compat.
#include <SSVStart/GameSystem/GameWindow.hpp>
#include <SSVStart/Camera/Camera.hpp>

#include <SSVUtils/Core/Common/Frametime.hpp>
#include <SSVUtils/Timeline/Timeline.hpp>

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Text.hpp>

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <sstream>
#include <unordered_set>
#include <functional>
#include <optional>
#include <vector>
#include <string>
#include <string_view>

struct ImGuiInputTextCallbackData;

namespace Json {
class Value;
}

namespace ssvuj {
using Obj = Json::Value;
}

// VectorPie: the vector renderer reads the game's draw buffers.
namespace vp {
class Renderer;
}

namespace hg {

class Audio;
class HGAssets;
class HexagonClient;
struct LevelData;
struct SpeedData;
struct PackData;

namespace Steam {
class steam_manager;
}

namespace Discord {
class discord_manager;
}

class HexagonGame
{
    friend class vp::Renderer; // VectorPie

private:
    Steam::steam_manager* steamManager;
    Discord::discord_manager* discordManager;
    bool discordHung{false};
    bool steamHung{false};
    std::int8_t discordAttempt{1};
    std::int8_t steamAttempt{1};

    HGAssets& assets;
    sf::Font font;     // VectorPie: no fonts, the SDK's stroke font
    sf::Font fontBold;

    Audio* audio;

    const LevelData* levelData;

    // VectorPie: no ssvs::GameState; src/vp/app.cpp calls update() and
    // draw(). `window` is null when headless.
    ssvs::GameWindow* window;

    HexagonClient* hexagonClient;

    // IMGUI Lua Console
    std::vector<std::string> ilcCmdLog;
    std::vector<std::string> ilcHistory;
    int ilcHistoryPos{-1};
    std::string ilcCmdBuffer;
    std::string ilcTrackBuffer;
    bool ilcShowConsole{false};
    bool ilcShowConsoleNext{false};
    std::vector<std::string> ilcLuaTracked;
    std::vector<std::string> ilcLuaTrackedNames;
    std::vector<std::string> ilcLuaTrackedResults;
    bool debugPause{false};

    std::vector<std::string> execScriptPackPathContext;

public:
    int ilcTextEditCallback(ImGuiInputTextCallbackData* data);

public:
    CPlayer player;
    std::vector<CWall> walls;
    CCustomWallManager cwManager;
    float timeUntilRichPresenceUpdate = 0.f;

private:
    std::optional<ssvs::Camera> backgroundCamera;
    std::optional<ssvs::Camera> overlayCamera;

    struct PreShakeCenters
    {
        sf::Vector2f background;
        sf::Vector2f overlay;
    };

    std::optional<PreShakeCenters> preShakeCenters;


    ssvu::TimelineManager effectTimelineManager;

    const sf::Vector2f centerPos{0.f, 0.f};

    Lua::LuaContext lua;
    std::unordered_set<std::string> calledDeprecatedFunctions;

    LevelStatus levelStatus;
    MusicData musicData;
    StyleData styleData;

    Utils::timeline2 timeline;
    Utils::timeline2_runner timelineRunner;

    Utils::timeline2 eventTimeline;
    Utils::timeline2_runner eventTimelineRunner;

    Utils::timeline2 messageTimeline;
    Utils::timeline2_runner messageTimelineRunner;

    CustomTimelineManager _customTimelineManager;

    sf::Text messageText;
    sf::Text pbText;

    Utils::FastVertexVectorTris flashPolygon;

    struct Particle
    {
        sf::Sprite sprite;
        sf::Vector2f velocity;
        float angularVelocity;
    };

    struct TrailParticle
    {
        sf::Sprite sprite;
        float angle;
    };

    struct SwapParticle
    {
        sf::Sprite sprite;
        sf::Vector2f velocity;
    };

    sf::Texture* txStarParticle;
    sf::Texture* txSmallCircle;
    sf::Texture vpStarParticle;   // VectorPie: named stand-ins for the
    sf::Texture vpSmallCircle;    // textures, drawn as vector shapes

    std::vector<Particle> particles;
    std::vector<TrailParticle> trailParticles;
    std::vector<SwapParticle> swapParticles;
    bool mustSpawnPBParticles{false};
    bool vpNewPersonalBest{false}; // VectorPie: for the front end
    bool vpScoresDisabled{false};  // VectorPie: a pilot is playing

    // VectorPie: the spinner, 1:1. vpSpinDeg is how far the player turns in
    // this tick (degrees, + clockwise), set before the tick; it becomes the
    // tick's speed in CPlayer::updateInputMovement, so the original's wall
    // pushing works as for keys. vpSpinLog keeps it per recorded tick (with
    // the replay's input bits), vpSpinReplay plays it back.
    float vpSpinDeg{0.f};
    std::vector<float> vpSpinLog;
    std::vector<float> vpSpinReplay;
    std::size_t vpSpinIndex{0};

    struct SwapParticleSpawnInfo
    {
        bool ready;
        sf::Vector2f position;
        float angle;
    };

    std::optional<SwapParticleSpawnInfo> swapParticlesSpawnInfo;
    float nextPBParticleSpawn{0.f};
    float pbTextGrowth{0.f};

    // VectorPie: no key icons and level info box (textures and filled
    // rectangles); src/vp/render.cpp draws its own lettering.

    bool firstPlay{true};
    bool restartFirstTime{true};
    bool inputFocused{false};
    bool inputSwap{false};
    bool mustTakeScreenshot{false};
    bool mustChangeSides{false};
    bool mustStart{false};

    random_number_generator rng;
    HexagonGameStatus status;

    float deathInputIgnore{0.f};

    struct ActiveReplay
    {
        replay_file replayFile;
        replay_player replayPlayer;
        std::string replayPackName;
        std::string replayLevelName;

        explicit ActiveReplay(const replay_file& mReplayFile);
    };

    std::optional<ActiveReplay> activeReplay;

    random_number_generator::seed_type lastSeed{};
    replay_data lastReplayData{};
    bool lastFirstPlay{};
    double lastPlayedScore{};

    std::string restartId;
    float difficultyMult{1};
    int inputImplLastMovement{0};
    int inputMovement{0};
    bool inputImplCW{false};
    bool inputImplCCW{false};
    bool playerNowReadyToSwap{false};

    std::ostringstream os;

    sf::Text fpsText;
    sf::Text timeText;
    sf::Text text;
    sf::Text replayText;

    // Color of the polygon in the center.
    CapColor capColor;

    std::string packId;
    std::string levelId;

    // Lua related methods
    void initLua_Utils();
    void initLua_AudioControl();
    void initLua_MainTimeline();
    void initLua_EventTimeline();
    void initLua_CustomTimelines();
    void initLua_LevelControl();
    void initLua_StyleControl();
    void initLua_WallCreation();
    void initLua_Steam();
    void initLua_CustomWalls();
    void initLua_Deprecated();

    void initLua();
    void runLuaFile(const std::string& mFileName);

    // Wall creation
    void createWall(int mSide, float mThickness, const SpeedData& mSpeed,
        const SpeedData& mCurve, float mHueMod);

public:
    // ------------------------------------------------------------------------
    // Testing-related utilities
    std::function<void(const replay_file&)> onDeathReplayCreated;

    void setMustStart(const bool x);

    bool executeRandomInputs{false};
    bool alwaysSpinRight{false};

    // ------------------------------------------------------------------------
    // Lua stuff

    void initLuaAndPrintDocs();

    void luaExceptionLippincottHandler(std::string_view mName);

    template <typename T, typename... TArgs>
    auto runLuaFunctionIfExists(std::string_view mName, const TArgs&... mArgs)
    try
    {
        return Utils::runLuaFunctionIfExists<T, TArgs...>(lua, mName, mArgs...);
    }
    catch(...)
    {
        luaExceptionLippincottHandler(mName);
        return decltype(Utils::runLuaFunctionIfExists<T, TArgs...>(
            lua, mName, mArgs...)){};
    }

    void raiseWarning(
        const std::string& mFunctionName, const std::string& mAdditionalInfo);

    void setLastReplay(const replay_file& mReplayFile);

private:
    void start();

    void initFlashEffect(int r, int g, int b);

    // Fast-forward
    std::optional<double> fastForwardTarget;
    void fastForwardTo(const double target);

    // Advance by ticks
    std::optional<int> advanceTickCount;
    void advanceByTicks(const int nTicks);

    // Update methods
    void update(ssvu::FT mFT, const float timescale);
    void updateInput();
    void updateInput_UpdateJoystickControls();
    void updateInput_UpdateTouchControls();
    void updateInput_ResolveInputImplToInputMovement();
    void updateInput_RecordCurrentInputToLastReplayData();
    void updateWalls(ssvu::FT mFT);
    void updateIncrement();
    void updateEvents(ssvu::FT mFT);
    void updateLevel(ssvu::FT mFT);
    void updateCustomTimelines();
    void updateCustomWalls(ssvu::FT mFT);
    void updatePulse(ssvu::FT mFT);
    void refreshPulse();
    void updateBeatPulse(ssvu::FT mFT);
    void refreshBeatPulse();
    void updateRotation(ssvu::FT mFT);
    void updateCameraShake(ssvu::FT mFT);
    void updateFlash(ssvu::FT mFT);
    void updatePulse3D(ssvu::FT mFT);
    void updateText(ssvu::FT mFT);
    void updateParticles(ssvu::FT mFT);
    void updateTrailParticles(ssvu::FT mFT);
    void updateSwapParticles(ssvu::FT mFT);

    // Post update methods
    void postUpdate();
    void postUpdate_ImguiLuaConsole();

public:
    // Draw methods
    void draw(); // VectorPie: fills the draw buffers, see HGGraphics.cpp

private:

    // Gameplay methods
    void incrementDifficulty();
    void sideChange(unsigned int mSideNumber);

    // VectorPie: the drawText_* / drawKeyIcons / drawLevelInfo /
    // draw*Particles functions are src/vp/render.cpp.

    // Data-related methods
    void setLevelData(const LevelData& mLevelData, bool mMusicFirstPlay);
    void playLevelMusic();
    void playLevelMusicAtTime(float mSeconds);
    void stopLevelMusic();

    // Message-related methods
    void addMessage(std::string mMessage, double mDuration, bool mSoundToggle);
    void clearMessages();

    enum class CheckScore
    {
        NoLocalProfile,
        Ineligible,
        Invalid,
        Valid,
    };

    // Level/menu loading/unloading/changing
    [[nodiscard]] bool shouldSaveScore();
    void goToMenu(bool mSendScores = true, bool mError = false);

    void invalidateScore(const std::string& mReason);

    [[nodiscard]] bool imguiLuaConsoleHasInput();

    template <typename T>
    auto makeLuaAccessor(T& obj, const std::string& prefix);

    static void nameFormat(std::string& name);
    [[nodiscard]] static std::string diffFormat(float diff);
    [[nodiscard]] static std::string timeFormat(float time);

private:
    void performPlayerSwap(const bool mPlaySound);
    void performPlayerKill();
    void saveReplay();

    // VectorPie: the 3D layers as draw() computes them (offset of each
    // layer from the top one, and its colors), deepest first. The vector
    // renderer extrudes the top shapes along them instead of drawing
    // `depth` copies.
    struct Layer3D
    {
        sf::Vector2f offset;
        sf::Color wallColor;
        sf::Color pivotColor;
        sf::Color playerColor;
    };

    std::vector<Layer3D> vpLayers3D;

    Utils::FastVertexVectorTris backgroundTris;
    Utils::FastVertexVectorTris wallQuads;
    Utils::FastVertexVectorTris pivotQuads;
    Utils::FastVertexVectorTris playerTris;
    Utils::FastVertexVectorTris capTris;
    Utils::FastVertexVectorTris wallQuads3D;
    Utils::FastVertexVectorTris pivotQuads3D;
    Utils::FastVertexVectorTris playerTris3D;

public:
    std::function<void(const bool)> fnGoToMenu;

    // VectorPie: the levels' words for the controls ("left shift", "space")
    // put as the cabinet has them. Called with each message (e_messageAdd*)
    // and tracked variable's name as the script wrote it: nullopt keeps it,
    // an empty string leaves it out, anything else replaces it. Also called
    // with "@restart" and "@replay" for the hints after a death.
    std::function<std::optional<std::string>(const std::string&)> vpTextFilter;

    // VectorPie: the made soundtrack. vpUseMadeMusic: true skips the
    // level's own music (playLevelMusic). vpOnStart: called in start(),
    // before the music resumes, with the beat pulse as it stands (the
    // music is put in time with it there).
    std::function<bool()> vpUseMadeMusic;
    std::function<void(float mDelayMax, float mDelayNow, float mDMFactor,
        bool mFirstPlay)>
        vpOnStart;

    // VectorPie: run after the level's script (newGame), with its Lua.
    std::function<void(Lua::LuaContext&, const std::string& mLevelId)>
        vpAfterLevelScript;

    HexagonGame(Steam::steam_manager* mSteamManager,
        Discord::discord_manager* mDiscordManager, HGAssets& mAssets,
        Audio* mAudio, ssvs::GameWindow* mGameWindow,
        HexagonClient* mHexagonClient);

    // VectorPie: one simulation tick (Config::TIME_STEP), as GameState's
    // onUpdate and onPostUpdate. The input fields below are set before it.
    void tick(const float timescale);

    // VectorPie: the controls, set by src/vp/controls.cpp before each tick
    // (the original's add2StateInput bindings and Joystick::pressed).
    void setInputs(bool mCCW, bool mCW, bool mFocus, bool mSwap);
    void requestRestart();
    void requestReplay();
    void requestMenu();

    // VectorPie: what the front end needs to know.
    [[nodiscard]] bool hasDied() const noexcept;
    [[nodiscard]] bool isStarted() const noexcept;
    [[nodiscard]] bool canRestartAfterDeath() const noexcept;
    [[nodiscard]] double getTimeSeconds() const noexcept;
    [[nodiscard]] const LevelData* getLevelData() const noexcept;
    [[nodiscard]] float getDifficultyMult() const noexcept;
    [[nodiscard]] const std::string& getLevelId() const noexcept;
    [[nodiscard]] const replay_file* getActiveReplayFile() const noexcept;
    [[nodiscard]] bool takeNewPersonalBest() noexcept;
    [[nodiscard]] replay_file makeLastReplayFile();

    // VectorPie: how far the player turns in one tick now (the spinner's
    // unit, CPlayer::updateInputMovement), and the attract mode's pilot.
    [[nodiscard]] float vpDegPerTick() const;
    void vpDisableScores(bool x) noexcept { vpScoresDisabled = x; }
    void vpSetSpin(float deg) noexcept { vpSpinDeg = deg; }
    // VectorPie: the level is a tutorial (l_setTutorialMode): it cannot be
    // lost, its script ends it with a forced death (e_kill).
    [[nodiscard]] bool vpTutorial() const noexcept { return levelStatus.tutorialMode; }
    // VectorPie: the level lets the player swap (l_setSwapEnabled).
    [[nodiscard]] bool vpSwapEnabled() const noexcept { return levelStatus.swapEnabled; }
    [[nodiscard]] const std::vector<float>& vpGetSpinLog() const noexcept { return vpSpinLog; }
    void vpSetSpinReplay(std::vector<float> v) { vpSpinReplay = std::move(v); }
    void vpAutopilot(bool& ccw, bool& cw) const;

    ~HexagonGame();


    // Gameplay methods
    void newGame(const std::string& mPackId, const std::string& mId,
        bool mFirstPlay, float mDifficultyMult, bool executeLastReplay);

    enum class SaveScoreIfNeededResult
    {
        NoWindow = 0,
        ShouldNotSave = 1,
        NotPersonalBest = 2,
        PersonalBest = 3,
    };

    void death(bool mForce = false);
    void death_shakeCamera();
    void death_flashEffect();
    [[nodiscard]] replay_file death_createReplayFile();
    void death_updateRichPresence();
    [[nodiscard]] SaveScoreIfNeededResult death_saveScoreIfNeeded();
    void death_saveScoreIfNeededAndShowPBEffects();
    void death_sendAndSaveReplay(const replay_file& rf);
    // VectorPie: no server, no compressed replay files; the front end keeps
    // the replays it wants (src/vp/app.cpp, attract mode).

    struct GameExecutionResult
    {
        double playedTimeSeconds;
        double pausedTimeSeconds;
        double totalTimeSeconds;
        float customScore;
    };

    [[nodiscard]] std::optional<GameExecutionResult> executeGameUntilDeath(
        const int maxProcessingSeconds, const float timescale);

    [[nodiscard]] std::optional<GameExecutionResult>
    runReplayUntilDeathAndGetScore(const replay_file& mReplayFile,
        const int maxProcessingSeconds, const float timescale);

    // Other methods
    void executeEvents(ssvuj::Obj& mRoot, float mTime);
    void updateRichPresenceCallbacks();

    [[nodiscard]] bool shouldPlaySounds() const;
    [[nodiscard]] bool shouldPlayMusic() const;
    void playSoundOverride(const std::string& mId);
    void playSoundAbort(const std::string& mId);
    void playPackSoundOverride(
        const std::string& mPackId, const std::string& mId);


    // Setters
    void setSides(unsigned int mSides);

    // Getters
    [[nodiscard]] float getRadius() const noexcept;
    [[nodiscard]] const sf::Color& getColor(int mIdx) const noexcept;
    [[nodiscard]] float getSpeedMultDM() const noexcept;
    [[nodiscard]] float getDelayMultDM() const noexcept;
    [[nodiscard]] float getRotationSpeed() const noexcept;
    [[nodiscard]] unsigned int getSides() const noexcept;
    [[nodiscard]] float getWallSkewLeft() const noexcept;
    [[nodiscard]] float getWallSkewRight() const noexcept;
    [[nodiscard]] float getWallAngleLeft() const noexcept;
    [[nodiscard]] float getWallAngleRight() const noexcept;
    [[nodiscard]] HexagonGameStatus& getStatus() noexcept;
    [[nodiscard]] const HexagonGameStatus& getStatus() const noexcept;
    [[nodiscard]] LevelStatus& getLevelStatus();
    [[nodiscard]] HGAssets& getAssets();
    [[nodiscard]] sf::Color getColorMain() const;
    [[nodiscard]] sf::Color getColorPlayer() const;
    [[nodiscard]] sf::Color getColorPlayerAdjustedForSwap() const;
    [[nodiscard]] sf::Color getColorPlayerTrail() const;
    [[nodiscard]] sf::Color getColorText() const;
    [[nodiscard]] sf::Color getColorCap() const;
    [[nodiscard]] sf::Color getColorWall() const;
    [[nodiscard]] float getMusicDMSyncFactor() const;
    [[nodiscard]] float getOptionalMusicDMSyncFactor() const;

    void refreshMusicPitch();

    // Input
    [[nodiscard]] bool getInputFocused() const;
    [[nodiscard]] float getPlayerSpeedMult() const;
    [[nodiscard]] bool getInputSwap() const;
    [[nodiscard]] int getInputMovement() const;

    // Pack information
    [[nodiscard]] const PackData& getPackData() const noexcept;
    [[nodiscard]] const std::string& getPackId() const noexcept;
    [[nodiscard]] const std::string& getPackDisambiguator() const noexcept;
    [[nodiscard]] const std::string& getPackAuthor() const noexcept;
    [[nodiscard]] const std::string& getPackName() const noexcept;
    [[nodiscard]] int getPackVersion() const noexcept;

    [[nodiscard]] bool inReplay() const noexcept;
    [[nodiscard]] bool mustReplayInput() const noexcept;
    [[nodiscard]] bool mustShowReplayUI() const noexcept;

    [[nodiscard]] float getSwapCooldown() const noexcept;
};

} // namespace hg
