// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port of HGGraphics.cpp.
//
// draw() fills the same buffers as the original (background, wall quads,
// pivot quads, cap and player triangles) in the same way. What the original
// then hands to the graphics card goes to src/vp/render.cpp instead, which
// turns the filled shapes into outlines on the vector board. The 3D effect
// is computed as in the original (skew of the background camera, offset and
// color of every layer), but the layers are kept as a list (vpLayers3D) and
// not as `depth` copies of every vertex: the renderer extrudes each shape
// along them and hides what lies behind.
//
// updateText is the original's. The drawing of text, key icons, level info,
// particles and the flash is src/vp/render.cpp.

#include "SSVOpenHexagon/Core/HexagonGame.hpp"

#include "SSVOpenHexagon/Components/CWall.hpp"

#include "SSVOpenHexagon/Global/Assert.hpp"
#include "SSVOpenHexagon/Global/Assets.hpp"
#include "SSVOpenHexagon/Global/Config.hpp"

#include "SSVOpenHexagon/Utils/Color.hpp"
#include "SSVOpenHexagon/Utils/String.hpp"

#include <SSVStart/Utils/Vector2.hpp>

#include <SSVUtils/Core/Log/Log.hpp>
#include <SSVUtils/Core/Utils/Rnd.hpp>

#include <cmath>
#include <cstdint>

namespace hg {

[[nodiscard]] static std::string formatTime(const double x)
{
    return ssvu::toStr(std::floor(x * 1000) / 1000.f);
}

void HexagonGame::draw()
{
    if(window == nullptr || Config::getDisableGameRendering())
    {
        return;
    }

    SSVOH_ASSERT(backgroundCamera.has_value());
    SSVOH_ASSERT(overlayCamera.has_value());

    if(!status.hasDied)
    {
        if(levelStatus.cameraShake > 0.f)
        {
            const sf::Vector2f shake(ssvu::getRndR(-levelStatus.cameraShake,
                                         levelStatus.cameraShake),
                ssvu::getRndR(
                    -levelStatus.cameraShake, levelStatus.cameraShake));

            backgroundCamera->setCenter(shake);
            overlayCamera->setCenter(
                shake + sf::Vector2f{Config::getWidth() / 2.f,
                            Config::getHeight() / 2.f});
        }
        else
        {
            backgroundCamera->setCenter(ssvs::zeroVec2f);
            overlayCamera->setCenter(sf::Vector2f{
                Config::getWidth() / 2.f, Config::getHeight() / 2.f});
        }
    }

    backgroundTris.clear();

    if(!Config::getNoBackground())
    {
        styleData.drawBackground(backgroundTris, ssvs::zeroVec2f,
            levelStatus.sides,
            Config::getDarkenUnevenBackgroundChunk() &&
                levelStatus.darkenUnevenBackgroundChunk,
            Config::getBlackAndWhite());
    }

    wallQuads3D.clear();
    pivotQuads3D.clear();
    playerTris3D.clear();
    wallQuads.clear();
    pivotQuads.clear();
    playerTris.clear();
    capTris.clear();
    vpLayers3D.clear();

    // Reserve right amount of memory for all walls and custom walls
    wallQuads.reserve_more_quad(walls.size() + cwManager.count());

    for(CWall& w : walls)
    {
        w.draw(getColorWall(), wallQuads);
    }

    cwManager.draw(wallQuads);

    if(status.started)
    {
        player.draw(getSides(), getColorMain(), getColorPlayer(), pivotQuads,
            capTris, playerTris, getColorCap(), Config::getAngleTiltIntensity(),
            Config::getShowSwapBlinkingEffect());
    }

    if(Config::get3D())
    {
        const float depth(styleData._3dDepth);

        const float pulse3D{Config::getNoPulse() ? 1.f : status.pulse3D};
        const float effect{
            styleData._3dSkew * Config::get3DMultiplier() * pulse3D};

        const sf::Vector2f skew{1.f, 1.f + effect};
        backgroundCamera->setSkew(skew);

        const float radRot(
            ssvu::toRad(backgroundCamera->getRotation()) + (ssvu::pi / 2.f));
        const float sinRot(std::sin(radRot));
        const float cosRot(std::cos(radRot));

        const auto adjustAlpha = [&](sf::Color& c, const float i)
        {
            if(styleData._3dAlphaMult == 0.f)
            {
                return;
            }

            const float newAlpha =
                (static_cast<float>(c.a) / styleData._3dAlphaMult) -
                i * styleData._3dAlphaFalloff;

            c.a = Utils::componentClamp(newAlpha);
        };

        for(int j(0); j < static_cast<int>(depth); ++j)
        {
            const float i(depth - j - 1);

            const float offset(styleData._3dSpacing *
                               (float(i + 1.f) * styleData._3dPerspectiveMult) *
                               (effect * 3.6f) * 1.4f);

            const sf::Vector2f newPos(offset * cosRot, offset * sinRot);

            Layer3D layer;
            layer.offset = newPos;

            sf::Color overrideColor;

            if(!Config::getBlackAndWhite())
            {
                overrideColor = Utils::getColorDarkened(
                    styleData.get3DOverrideColor(), styleData._3dDarkenMult);
            }
            else
            {
                overrideColor = Utils::getColorDarkened(
                    sf::Color(255, 255, 255, styleData.getMainColor().a),
                    styleData._3dDarkenMult);
            }
            adjustAlpha(overrideColor, i);

            // Pivot layers
            layer.pivotColor = overrideColor;

            if(styleData.get3DOverrideColor() == styleData.getMainColor())
            {
                overrideColor = Utils::getColorDarkened(
                    getColorWall(), styleData._3dDarkenMult);

                adjustAlpha(overrideColor, i);
            }

            // Wall layers
            layer.wallColor = overrideColor;

            // Apply player color if no 3D override is present.
            if(styleData.get3DOverrideColor() == styleData.getMainColor())
            {
                overrideColor = Utils::getColorDarkened(
                    getColorPlayer(), styleData._3dDarkenMult);

                adjustAlpha(overrideColor, i);
            }

            // Player layers
            layer.playerColor = overrideColor;

            vpLayers3D.push_back(layer);
        }
    }
    else
    {
        // VectorPie: the original sets the skew only with 3D on; a setting
        // switched off in the service menu must not leave the last one.
        backgroundCamera->setSkew(sf::Vector2f{1.f, 1.f});
    }

    if(mustTakeScreenshot)
    {
        mustTakeScreenshot = false; // VectorPie: no screenshots
    }
}

void HexagonGame::initFlashEffect(int r, int g, int b)
{
    flashPolygon.clear();
    flashPolygon.reserve(6);

    const sf::Color color{static_cast<std::uint8_t>(r),
        static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b), 0};

    const auto width = static_cast<float>(Config::getWidth());
    const auto height = static_cast<float>(Config::getHeight());
    const float offset = 100.f;

    const sf::Vector2f nw{-offset, -offset};
    const sf::Vector2f sw{-offset, height + offset};
    const sf::Vector2f se{width + offset, height + offset};
    const sf::Vector2f ne{width + offset, -offset};

    flashPolygon.batch_unsafe_emplace_back_quad(color, nw, sw, se, ne);
}

void HexagonGame::updateText(ssvu::FT mFT)
{
    if(window == nullptr)
    {
        return;
    }

    // ------------------------------------------------------------------------
    // Update "personal best" text animation.
    pbTextGrowth += 0.08f * mFT;
    if(pbTextGrowth > ssvu::pi * 2.f)
    {
        pbTextGrowth = 0;
    }

    // ------------------------------------------------------------------------
    os.str("");

    if(debugPause)
    {
        os << "(!) PAUSED (!)\n";
    }

    if(levelStatus.tutorialMode)
    {
        os << "TUTORIAL MODE\n";
    }
    else if(Config::getOfficial())
    {
        // VectorPie: "OFFICIAL MODE" means online scores in the original;
        // there are none on the cabinet, so it is not shown.
    }

    if(Config::getDebug())
    {
        os << "DEBUG MODE\n";

        os << "CUSTOM WALLS: " << cwManager.count() << " / "
           << cwManager.maxHandles() << '\n';
    }

    if(status.started)
    {
        if(levelStatus.swapEnabled)
        {
            os << "SWAP ENABLED\n";
        }

        if(Config::getInvincible())
        {
            os << "INVINCIBILITY ON\n";
        }

        if(const float timescale = Config::getTimescale(); timescale != 1.f)
        {
            os << "TIMESCALE " << timescale << '\n';
        }

        if(status.scoreInvalid)
        {
            os << "SCORE INVALIDATED (" << status.invalidReason << ")\n";
        }

        if(status.hasDied)
        {
            os << status.restartInput;
            os << status.replayInput;
        }

        // VectorPie: the deprecation warnings go to the log only.

        const auto& trackedVariables(levelStatus.trackedVariables);
        if(Config::getShowTrackedVariables() && !trackedVariables.empty())
        {
            os << '\n';
            for(const auto& [variableName, display] : trackedVariables)
            {
                if(!lua.doesVariableExist(variableName))
                {
                    continue;
                }

                const std::string value{
                    lua.readVariable<std::string>(variableName)};

                // VectorPie: the cabinet's words for the controls.
                std::string shown = display;
                if(vpTextFilter)
                {
                    if(const auto r = vpTextFilter(display); r.has_value())
                    {
                        if(r->empty())
                        {
                            continue;
                        }
                        shown = *r;
                    }
                }

                os << Utils::toUppercase(shown) << ": "
                   << Utils::toUppercase(value) << '\n';
            }
        }
    }
    else if(Config::getRotateToStart())
    {
        os << "ROTATE TO START\n";
        messageText.setString("ROTATE TO START");
    }

    os.flush();

    // Set in game timer text
    if(!levelStatus.scoreOverridden)
    {
        // By default, use the timer for scoring
        if(status.started)
        {
            timeText.setString(formatTime(status.getTimeSeconds()));
        }
        else
        {
            timeText.setString("0");
        }
    }
    else
    {
        // Alternative scoring
        timeText.setString(
            lua.readVariable<std::string>(levelStatus.scoreOverride));
    }

    // Set information text
    text.setString(os.str());

    const float growth = std::sin(pbTextGrowth);
    pbText.setCharacterSize(64.f + growth * 10.f);

    // ------------------------------------------------------------------------
    if(mustShowReplayUI())
    {
        const replay_file& rf = activeReplay->replayFile;

        os.str("");

        if(!levelStatus.scoreOverridden)
        {
            os << formatTime(rf.played_seconds()) << "s";
        }
        else
        {
            os << formatTime(rf._played_score);
        }

        os << " BY " << rf._player_name;

        os.flush();

        replayText.setString(os.str());
    }
    else
    {
        replayText.setString("");
    }
}

} // namespace hg
