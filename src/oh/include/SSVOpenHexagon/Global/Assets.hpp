// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port: HGAssets without textures, fonts, shaders, profiles and
// the Steam workshop. Packs, levels, styles and music data load as in the
// original (Assets.cpp, loadPackData and loadPackAssets_*). Sounds and music
// are file paths; src/vp/audio.cpp plays them. The local best times are a
// table in the game's save folder (setLocalScore), in place of a profile.

#pragma once

#include "SSVOpenHexagon/Utils/UniquePtr.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sf {
class Shader;
}

namespace hg {

struct LoadInfo;
class MusicData;
struct LevelData;
struct PackData;
struct PackInfo;
class StyleData;

class HGAssets
{
private:
    class HGAssetsImpl;
    Utils::UniquePtr<HGAssetsImpl> _impl;

public:
    // mScoresPath: where the local best times are kept. mExtraPacksPath:
    // a folder of more packs, each like those in Packs/ (the player's own
    // and the Steam workshop's); empty for none.
    explicit HGAssets(const std::string& mScoresPath,
        const std::string& mExtraPacksPath = {});

    ~HGAssets();

    [[nodiscard]] LoadInfo& getLoadResults();

    [[nodiscard]] bool isValidLevelId(
        const std::string& mLevelId) const noexcept;

    [[nodiscard]] const LevelData& getLevelData(
        const std::string& mAssetId) const;

    [[nodiscard]] bool packHasLevels(const std::string& mPackId);

    [[nodiscard]] const std::vector<std::string>& getLevelIdsByPack(
        const std::string& mPackId);

    [[nodiscard]] const std::unordered_map<std::string, PackData>&
    getPackDatas();

    [[nodiscard]] bool isValidPackId(const std::string& mPackId) const noexcept;

    [[nodiscard]] const PackData& getPackData(const std::string& mPackId);

    [[nodiscard]] const std::vector<PackInfo>&
    getSelectablePackInfos() const noexcept;

    [[nodiscard]] const PackData* findPackData(
        const std::string& mPackDisambiguator, const std::string& mPackName,
        const std::string& mPackAuthor) const noexcept;

    [[nodiscard]] const MusicData& getMusicData(
        const std::string& mPackId, const std::string& mId);
    [[nodiscard]] const StyleData& getStyleData(
        const std::string& mPackId, const std::string& mId);

    // VectorPie: no shaders. Every id is invalid.
    [[nodiscard]] std::optional<std::size_t> getShaderIdByPath(
        const std::string&)
    {
        return std::nullopt;
    }
    [[nodiscard]] sf::Shader* getShaderByShaderId(const std::size_t)
    {
        return nullptr;
    }
    [[nodiscard]] bool isValidShaderId(const std::size_t) const
    {
        return false;
    }

    [[nodiscard]] float getLocalScore(const std::string& mId);
    void setLocalScore(const std::string& mId, float mScore);

    [[nodiscard]] bool anyLocalProfileActive() const;

    // Sound ids as the original's: "beep.ogg" for Assets/, and
    // "<packId>_<file>" for a pack's Sounds/.
    [[nodiscard]] const std::string* getSoundPath(
        const std::string& assetId) const;

    // "<packId>_<music id>", as musicPathMap.
    [[nodiscard]] const std::string* getMusicPath(
        const std::string& assetId) const;

    [[nodiscard]] const std::unordered_map<std::string, LevelData>&
    getLevelDatas() const noexcept;

    [[nodiscard]] const std::unordered_set<std::string>&
    getPackIdsWithMissingDependencies() const noexcept;

    [[nodiscard]] std::unordered_map<std::string, std::string>&
    getLuaFileCache();

    [[nodiscard]] const std::unordered_map<std::string, std::string>&
    getLuaFileCache() const;
};

} // namespace hg
