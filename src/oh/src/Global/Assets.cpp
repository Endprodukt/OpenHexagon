// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port of Assets.cpp: see Assets.hpp. The loading functions are
// the original's; textures, fonts, shaders, profiles and the workshop are
// left out.

#include "SSVOpenHexagon/Global/Assets.hpp"

#include "SSVOpenHexagon/Data/LevelData.hpp"
#include "SSVOpenHexagon/Data/LoadInfo.hpp"
#include "SSVOpenHexagon/Data/MusicData.hpp"
#include "SSVOpenHexagon/Data/PackData.hpp"
#include "SSVOpenHexagon/Data/PackInfo.hpp"
#include "SSVOpenHexagon/Data/StyleData.hpp"

#include "SSVOpenHexagon/Global/Assert.hpp"
#include "SSVOpenHexagon/Global/Macros.hpp"
#include "SSVOpenHexagon/Global/UtilsJson.hpp"
#include "SSVOpenHexagon/Global/Version.hpp"

#include "SSVOpenHexagon/SSVUtilsJson/SSVUtilsJson.hpp"

#include "SSVOpenHexagon/Utils/BuildPackId.hpp"
#include "SSVOpenHexagon/Utils/Clock.hpp"
#include "SSVOpenHexagon/Utils/Concat.hpp"
#include "SSVOpenHexagon/Utils/EraseIf.hpp"
#include "SSVOpenHexagon/Utils/LoadFromJson.hpp"
#include "SSVOpenHexagon/Utils/UniquePtr.hpp"

#include <SSVUtils/Core/FileSystem/FileSystem.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>

namespace hg {

class HGAssets::HGAssetsImpl
{
private:
    std::unordered_map<std::string, LevelData> levelDatas;
    std::unordered_map<std::string, std::vector<std::string>>
        levelDataIdsByPack;

    std::unordered_map<std::string, PackData> packDatas;

    std::vector<PackInfo> packInfos;
    std::vector<PackInfo> selectablePackInfos;

    std::unordered_map<std::string, std::string> soundPathMap;
    std::unordered_map<std::string, std::string> musicPathMap;
    std::map<std::string, MusicData> musicDataMap;
    std::map<std::string, StyleData> styleDataMap;

    // VectorPie: the local best times (ProfileData's scores).
    std::string scoresPath;
    std::string extraPacksPath; // VectorPie: the player's packs
    std::unordered_map<std::string, float> scores;

    std::unordered_set<std::string> packIdsWithMissingDependencies;

    std::string buf;

    std::unordered_map<std::string, std::string> luaFileCache;
    LoadInfo loadInfo;

    template <typename... Ts>
    [[nodiscard]] std::string& concatIntoBuf(const Ts&...);

    [[nodiscard]] bool loadAllPackDatas();
    [[nodiscard]] bool loadAllPackAssets();
    [[nodiscard]] bool verifyAllPackDependencies();

    [[nodiscard]] bool loadPackData(const ssvufs::Path& packPath);

    [[nodiscard]] bool loadPackAssets(const PackData& packData);

    void loadPackAssets_loadMusic(
        const std::string& mPackId, const ssvufs::Path& mPath);
    void loadPackAssets_loadMusicData(
        const std::string& mPackId, const ssvufs::Path& mPath);
    void loadPackAssets_loadStyleData(
        const std::string& mPackId, const ssvufs::Path& mPath);
    void loadPackAssets_loadLevelData(
        const std::string& mPackId, const ssvufs::Path& mPath);
    void loadPackAssets_loadCustomSounds(
        const std::string& mPackId, const ssvufs::Path& mPath);

    void loadBaseSounds();
    void loadScores();
    void saveScores();

public:
    explicit HGAssetsImpl(const std::string& mScoresPath, const std::string& mExtraPacksPath);

    [[nodiscard]] LoadInfo& getLoadResults()
    {
        return loadInfo;
    }

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

    [[nodiscard]] float getLocalScore(const std::string& mId)
    {
        const auto it = scores.find(mId);
        return it == scores.end() ? 0.f : it->second;
    }

    void setLocalScore(const std::string& mId, float mScore)
    {
        scores[mId] = mScore;
        saveScores();
    }

    [[nodiscard]] const std::string* getSoundPath(
        const std::string& assetId) const
    {
        const auto it = soundPathMap.find(assetId);
        return it == soundPathMap.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::string* getMusicPath(
        const std::string& assetId) const
    {
        const auto it = musicPathMap.find(assetId);
        return it == musicPathMap.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const std::unordered_map<std::string, LevelData>&
    getLevelDatas() const noexcept
    {
        return levelDatas;
    }

    [[nodiscard]] const std::unordered_set<std::string>&
    getPackIdsWithMissingDependencies() const noexcept
    {
        return packIdsWithMissingDependencies;
    }

    [[nodiscard]] std::unordered_map<std::string, std::string>&
    getLuaFileCache()
    {
        return luaFileCache;
    }
};

[[nodiscard]] static std::vector<ssvufs::Path>& getScanBuffer()
{
    static std::vector<ssvufs::Path> buffer;
    return buffer;
}

[[nodiscard]] static const std::vector<ssvufs::Path>& scanSingleByExt(
    const ssvufs::Path& path, const std::string& extension)
{
    std::vector<ssvufs::Path>& buffer = getScanBuffer();
    buffer.clear();

    ssvufs::scan<ssvufs::Mode::Single, ssvufs::Type::File, ssvufs::Pick::ByExt>(
        buffer, path, extension);

    return buffer;
}

[[nodiscard]] static const std::vector<ssvufs::Path>& scanSingleByName(
    const ssvufs::Path& path, const std::string& name)
{
    std::vector<ssvufs::Path>& buffer = getScanBuffer();
    buffer.clear();

    ssvufs::scan<ssvufs::Mode::Single, ssvufs::Type::File,
        ssvufs::Pick::ByName>(buffer, path, name);

    return buffer;
}

[[nodiscard]] static const std::vector<ssvufs::Path>& scanSingleFolderName(
    const ssvufs::Path& path)
{
    std::vector<ssvufs::Path>& buffer = getScanBuffer();
    buffer.clear();

    ssvufs::scan<ssvufs::Mode::Single, ssvufs::Type::Folder>(buffer, path);

    return buffer;
}

template <typename... Ts>
[[nodiscard]] std::string& HGAssets::HGAssetsImpl::concatIntoBuf(
    const Ts&... xs)
{
    buf.clear();
    Utils::concatInto(buf, xs...);
    return buf;
}

HGAssets::HGAssetsImpl::HGAssetsImpl(const std::string& mScoresPath, const std::string& mExtraPacksPath)
    : scoresPath{mScoresPath}, extraPacksPath{mExtraPacksPath}
{
    const HRTimePoint tpBeforeLoad = HRClock::now();

    loadBaseSounds();

    if(!loadAllPackDatas())
    {
        ssvu::lo("HGAssets::HGAssets") << "Error loading all pack datas\n";
        std::terminate();
        return;
    }

    if(!loadAllPackAssets())
    {
        ssvu::lo("HGAssets::HGAssets") << "Error loading all pack assets\n";
        std::terminate();
        return;
    }

    if(!verifyAllPackDependencies())
    {
        ssvu::lo("HGAssets::HGAssets") << "Error verifying pack dependencies\n";
        std::terminate();
        return;
    }

    loadScores();

    for(auto& v : levelDataIdsByPack)
    {
        std::sort(v.second.begin(), v.second.end(),
            [&](const std::string& mA, const std::string& mB) {
                return levelDatas.at(mA).menuPriority <
                       levelDatas.at(mB).menuPriority;
            });
    }

    std::sort(packInfos.begin(), packInfos.end(),
        [&](const PackInfo& mA, const PackInfo& mB)
        { return getPackData(mA.id).priority < getPackData(mB.id).priority; });

    std::sort(selectablePackInfos.begin(), selectablePackInfos.end(),
        [&](const PackInfo& mA, const PackInfo& mB)
        { return getPackData(mA.id).priority < getPackData(mB.id).priority; });

    loadInfo.errorMessages.shrink_to_fit();

    const std::chrono::duration durElapsed = HRClock::now() - tpBeforeLoad;

    ssvu::lo("HGAssets::HGAssets")
        << "Loaded all assets in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(durElapsed)
               .count()
        << "ms\n";
}

// VectorPie: the "soundBuffers" of Assets/assets.json (loadAssetsFromJson),
// by their file names.
void HGAssets::HGAssetsImpl::loadBaseSounds()
{
    if(!ssvufs::Path{"Assets/assets.json"}.isFile())
    {
        ssvu::lo("HGAssets") << "Assets/assets.json not found\n";
        return;
    }

    auto [object, error] = ssvuj::getFromFileWithErrors("Assets/assets.json");
    loadInfo.addFormattedError(error);

    for(const auto& f :
        ssvuj::getExtr<std::vector<std::string>>(object, "soundBuffers"))
    {
        soundPathMap.emplace(f, "Assets/" + f);
    }
}

// VectorPie: one "<validator> <seconds>" per line.
void HGAssets::HGAssetsImpl::loadScores()
{
    std::ifstream in(scoresPath);
    std::string line;

    while(std::getline(in, line))
    {
        const auto sp = line.rfind(' ');
        if(sp == std::string::npos)
        {
            continue;
        }

        scores[line.substr(0, sp)] =
            std::strtof(line.c_str() + sp + 1, nullptr);
    }
}

void HGAssets::HGAssetsImpl::saveScores()
{
    const std::string tmp = scoresPath + ".new";

    {
        std::ofstream out(tmp, std::ios::trunc);
        for(const auto& [id, score] : scores)
        {
            out << id << ' ' << score << '\n';
        }
    }

    std::remove(scoresPath.c_str());
    std::rename(tmp.c_str(), scoresPath.c_str());
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::loadPackData(
    const ssvufs::Path& packPath)
{
    if(!ssvufs::Path{packPath + "/pack.json"}.isFile())
    {
        return false;
    }

    auto p = ssvuj::getFromFileWithErrors(packPath + "/pack.json");

    // Workaround of lambda capture of structured binding.
    auto& packRoot = p.first;
    auto& error = p.second;

    loadInfo.addFormattedError(error);

    auto packDisambiguator = ssvuj::getExtr<std::string>(
        packRoot, "disambiguator", "no disambiguator");

    auto packName =
        ssvuj::getExtr<std::string>(packRoot, "name", "unknown name");

    auto packAuthor =
        ssvuj::getExtr<std::string>(packRoot, "author", "unknown author");

    auto packDescription =
        ssvuj::getExtr<std::string>(packRoot, "description", "no description");

    const auto packVersion = ssvuj::getExtr<int>(packRoot, "version", 0);

    const auto packPriority = ssvuj::getExtr<float>(packRoot, "priority", 100);

    const std::string packId = Utils::buildPackId(
        packDisambiguator, packAuthor, packName, packVersion);

    const auto getPackDependencies = [&]
    {
        std::vector<PackDependency> result;

        if(!ssvuj::hasObj(packRoot, "dependencies"))
        {
            return result;
        }

        const ssvuj::Obj& objDependencies =
            ssvuj::getObj(packRoot, "dependencies");

        const auto dependencyCount = ssvuj::getObjSize(objDependencies);
        result.reserve(dependencyCount);

        for(std::size_t i = 0; i < dependencyCount; ++i)
        {
            const ssvuj::Obj& pdRoot = ssvuj::getObj(objDependencies, i);

            result.emplace_back(PackDependency{
                ssvuj::getExtr<std::string>(pdRoot, "disambiguator"),
                ssvuj::getExtr<std::string>(pdRoot, "name"),
                ssvuj::getExtr<std::string>(pdRoot, "author"),
                ssvuj::getExtr<int>(pdRoot, "min_version"),
            });
        }

        return result;
    };

    packInfos.emplace_back(PackInfo{packId, packPath});

    packDatas.emplace(packId,                              //
        PackData{
            .folderPath{packPath.getStr()},                //
            .id{packId},                                   //
            .disambiguator{SSVOH_MOVE(packDisambiguator)}, //
            .name{SSVOH_MOVE(packName)},                   //
            .author{SSVOH_MOVE(packAuthor)},               //
            .description{SSVOH_MOVE(packDescription)},     //
            .version{packVersion},                         //
            .priority{packPriority},                       //
            .dependencies{getPackDependencies()}           //
        });

    return true;
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::loadPackAssets(
    const PackData& packData)
{
    const std::string& packPath{packData.folderPath};
    const std::string& packId{packData.id};

    ssvu::lo("::loadAssets") << "loading '" << packId << "' assets\n";

    try
    {
        // VectorPie: no shaders.
        if(ssvufs::Path{packPath + "Sounds/"}.isFolder())
        {
            loadPackAssets_loadCustomSounds(packId, packPath);
        }

        if(ssvufs::Path{packPath + "Music/"}.isFolder())
        {
            loadPackAssets_loadMusic(packId, packPath);
            loadPackAssets_loadMusicData(packId, packPath);
        }

        if(ssvufs::Path{packPath + "Styles/"}.isFolder())
        {
            loadPackAssets_loadStyleData(packId, packPath);
        }

        if(ssvufs::Path{packPath + "Levels/"}.isFolder())
        {
            loadPackAssets_loadLevelData(packId, packPath);
        }
    }
    catch(const std::runtime_error& mEx)
    {
        const std::string& errorMessage =
            concatIntoBuf("Exception during asset loading: ", mEx.what(), '\n');

        loadInfo.errorMessages.emplace_back("FATAL ERROR, " + errorMessage);
        ssvu::lo("FATAL ERROR") << errorMessage;
        return false;
    }
    catch(...)
    {
        const std::string errorMessage =
            "Exception during asset loading: unknown.\n";

        loadInfo.errorMessages.emplace_back("FATAL ERROR, " + errorMessage);
        ssvu::lo("FATAL ERROR") << errorMessage;
        return false;
    }

    if(packHasLevels(packId))
    {
        selectablePackInfos.emplace_back(PackInfo{packId, packPath});
    }

    return true;
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::isValidLevelId(
    const std::string& mLevelId) const noexcept
{
    return levelDatas.find(mLevelId) != levelDatas.end();
}

[[nodiscard]] const LevelData& HGAssets::HGAssetsImpl::getLevelData(
    const std::string& mAssetId) const
{
    SSVOH_ASSERT(isValidLevelId(mAssetId));
    return levelDatas.at(mAssetId);
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::packHasLevels(
    const std::string& mPackId)
{
    return levelDataIdsByPack.count(mPackId) > 0;
}

[[nodiscard]] const std::vector<std::string>&
HGAssets::HGAssetsImpl::getLevelIdsByPack(const std::string& mPackId)
{
    SSVOH_ASSERT(levelDataIdsByPack.count(mPackId) > 0);
    return levelDataIdsByPack.at(mPackId);
}

[[nodiscard]] const std::unordered_map<std::string, PackData>&
HGAssets::HGAssetsImpl::getPackDatas()
{
    return packDatas;
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::isValidPackId(
    const std::string& mPackId) const noexcept
{
    return packDatas.find(mPackId) != packDatas.end();
}

[[nodiscard]] const PackData& HGAssets::HGAssetsImpl::getPackData(
    const std::string& mPackId)
{
    SSVOH_ASSERT(isValidPackId(mPackId));
    return packDatas.at(mPackId);
}

[[nodiscard]] const std::vector<PackInfo>&
HGAssets::HGAssetsImpl::getSelectablePackInfos() const noexcept
{
    return selectablePackInfos;
}

[[nodiscard]] const PackData* HGAssets::HGAssetsImpl::findPackData(
    const std::string& mPackDisambiguator, const std::string& mPackName,
    const std::string& mPackAuthor) const noexcept
{
    for(const auto& [packId, packData] : packDatas)
    {
        if(packData.disambiguator == mPackDisambiguator && //
            packData.name == mPackName &&                  //
            packData.author == mPackAuthor)
        {
            return &packData;
        }
    }

    return nullptr;
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::loadAllPackDatas()
{
    if(!ssvufs::Path{"Packs/"}.isFolder())
    {
        ssvu::lo("::loadAssets") << "Folder Packs/ does not exist" << std::endl;
        return false;
    }

    // ------------------------------------------------------------------------
    const auto tryLoadPackFromPath = [&](const auto& packPath)
    {
        if(!loadPackData(packPath))
        {
            const std::string& errorMessage =
                concatIntoBuf("Error loading pack data '",
                    static_cast<const std::string&>(packPath), '\n');

            loadInfo.errorMessages.emplace_back(errorMessage);
            ssvu::lo("::loadAssets") << errorMessage;
        }
        else
        {
            ++loadInfo.packs;
        }
    };

    // ------------------------------------------------------------------------
    // Load pack datas from `Packs/` folder.
    for(const auto& packPath : scanSingleFolderName("Packs/"))
    {
        tryLoadPackFromPath(packPath);
    }

    // VectorPie: the player's own packs (save/packs), in place of the Steam
    // workshop. A pack that is there twice (same id) keeps the first.
    if(!extraPacksPath.empty() && ssvufs::Path{extraPacksPath}.isFolder())
    {
        const std::vector<ssvufs::Path> extra = scanSingleFolderName(extraPacksPath);
        for(const auto& packPath : extra)
        {
            tryLoadPackFromPath(packPath);
        }
    }

    // VectorPie: no Steam workshop.

    return true;
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::loadAllPackAssets()
{
    for(const auto& [packId, packData] : packDatas)
    {
        if(loadPackAssets(packData))
        {
            continue;
        }

        const std::string& errorMessage =
            concatIntoBuf("Error loading pack info '", packId, '\n');

        loadInfo.errorMessages.emplace_back(errorMessage);
        ssvu::lo("::loadAssets") << errorMessage;

        return false;
    }

    return true;
}

[[nodiscard]] bool HGAssets::HGAssetsImpl::verifyAllPackDependencies()
{
    // ------------------------------------------------------------------------
    // Verify pack dependencies.
    const auto dependencyExists = [this](const PackDependency& pd)
    {
        for(const auto& [packId, packData] : packDatas)
        {
            if(                                                 //
                (packData.disambiguator == pd.disambiguator) && //
                (packData.name == pd.name) &&                   //
                (packData.author == pd.author) &&               //
                (packData.version >= pd.minVersion))
            {
                return true;
            }
        }

        return false;
    };

    for(const auto& [packId, packData] : packDatas)
    {
        for(const PackDependency& pd : packData.dependencies)
        {
            if(dependencyExists(pd))
            {
                continue;
            }

            const std::string& errorMessage =
                concatIntoBuf("Missing pack dependency '", pd.name,
                    "' for pack '", packData.name, "'\n");

            loadInfo.errorMessages.emplace_back(errorMessage);
            ssvu::lo("::loadAssets") << errorMessage;

            packIdsWithMissingDependencies.emplace(packId);
        }
    }

    ssvu::eraseRemoveIf(selectablePackInfos, [&](const PackInfo& pi)
        { return packIdsWithMissingDependencies.contains(pi.id); });

    return true;
}

void HGAssets::HGAssetsImpl::loadPackAssets_loadCustomSounds(
    const std::string& mPackId, const ssvufs::Path& mPath)
{
    for(const auto& p : scanSingleByExt(mPath + "Sounds/", ".ogg"))
    {
        // VectorPie: the path; src/vp/audio.cpp loads the sound on first use.
        soundPathMap.emplace(
            concatIntoBuf(mPackId, '_', p.getFileName()), p.getStr());

        ++loadInfo.assets;
    }
}

void HGAssets::HGAssetsImpl::loadPackAssets_loadMusic(
    const std::string& mPackId, const ssvufs::Path& mPath)
{
    for(const auto& p : scanSingleByExt(mPath + "Music/", ".ogg"))
    {
        musicPathMap.emplace(
            concatIntoBuf(mPackId, '_', p.getFileNameNoExtensions()), p.getStr());

        ++loadInfo.assets;
    }
}

void HGAssets::HGAssetsImpl::loadPackAssets_loadMusicData(
    const std::string& mPackId, const ssvufs::Path& mPath)
{
    for(const auto& p : scanSingleByExt(mPath + "Music/", ".json"))
    {
        auto [object, error] = ssvuj::getFromFileWithErrors(p);
        loadInfo.addFormattedError(error);

        MusicData musicData{Utils::loadMusicFromJson(object)};
        musicDataMap.emplace(
            concatIntoBuf(mPackId, '_', musicData.id), SSVOH_MOVE(musicData));

        ++loadInfo.assets;
    }
}

void HGAssets::HGAssetsImpl::loadPackAssets_loadStyleData(
    const std::string& mPackId, const ssvufs::Path& mPath)
{
    for(const auto& p : scanSingleByExt(mPath + "Styles/", ".json"))
    {
        auto [object, error] = ssvuj::getFromFileWithErrors(p);
        loadInfo.addFormattedError(error);

        StyleData styleData{object};
        styleDataMap.emplace(
            concatIntoBuf(mPackId, '_', styleData.id), SSVOH_MOVE(styleData));

        ++loadInfo.assets;
    }
}

void HGAssets::HGAssetsImpl::loadPackAssets_loadLevelData(
    const std::string& mPackId, const ssvufs::Path& mPath)
{
    for(const auto& p : scanSingleByExt(mPath + "Levels/", ".json"))
    {
        auto [object, error] = ssvuj::getFromFileWithErrors(p);
        loadInfo.addFormattedError(error);

        LevelData levelData{object, mPath, mPackId};
        const std::string& assetId = concatIntoBuf(mPackId, '_', levelData.id);

        levelDataIdsByPack[mPackId].emplace_back(assetId);
        levelDatas.emplace(assetId, SSVOH_MOVE(levelData));

        ++loadInfo.levels;
    }
}

[[nodiscard]] const MusicData& HGAssets::HGAssetsImpl::getMusicData(
    const std::string& mPackId, const std::string& mId)
{
    const std::string& assetId = concatIntoBuf(mPackId, '_', mId);

    const auto it = musicDataMap.find(assetId);
    if(it == musicDataMap.end())
    {
        ssvu::lo("getMusicData") << "Asset '" << assetId << "' not found\n";

        SSVOH_ASSERT(!musicDataMap.empty());
        return musicDataMap.begin()->second;
    }

    return it->second;
}

[[nodiscard]] const StyleData& HGAssets::HGAssetsImpl::getStyleData(
    const std::string& mPackId, const std::string& mId)
{
    const std::string& assetId = concatIntoBuf(mPackId, '_', mId);

    const auto it = styleDataMap.find(assetId);
    if(it == styleDataMap.end())
    {
        ssvu::lo("getStyleData") << "Asset '" << assetId << "' not found\n";

        SSVOH_ASSERT(!styleDataMap.empty());
        return styleDataMap.begin()->second;
    }

    return it->second;
}

HGAssets::HGAssets(const std::string& mScoresPath, const std::string& mExtraPacksPath)
    : _impl(Utils::makeUnique<HGAssetsImpl>(mScoresPath, mExtraPacksPath))
{}

HGAssets::~HGAssets() = default;

LoadInfo& HGAssets::getLoadResults()
{
    return _impl->getLoadResults();
}

bool HGAssets::isValidLevelId(const std::string& mLevelId) const noexcept
{
    return _impl->isValidLevelId(mLevelId);
}

const LevelData& HGAssets::getLevelData(const std::string& mAssetId) const
{
    return _impl->getLevelData(mAssetId);
}

bool HGAssets::packHasLevels(const std::string& mPackId)
{
    return _impl->packHasLevels(mPackId);
}

const std::vector<std::string>& HGAssets::getLevelIdsByPack(
    const std::string& mPackId)
{
    return _impl->getLevelIdsByPack(mPackId);
}

const std::unordered_map<std::string, PackData>& HGAssets::getPackDatas()
{
    return _impl->getPackDatas();
}

bool HGAssets::isValidPackId(const std::string& mPackId) const noexcept
{
    return _impl->isValidPackId(mPackId);
}

const PackData& HGAssets::getPackData(const std::string& mPackId)
{
    return _impl->getPackData(mPackId);
}

const std::vector<PackInfo>& HGAssets::getSelectablePackInfos() const noexcept
{
    return _impl->getSelectablePackInfos();
}

const PackData* HGAssets::findPackData(const std::string& mPackDisambiguator,
    const std::string& mPackName, const std::string& mPackAuthor) const noexcept
{
    return _impl->findPackData(mPackDisambiguator, mPackName, mPackAuthor);
}

const MusicData& HGAssets::getMusicData(
    const std::string& mPackId, const std::string& mId)
{
    return _impl->getMusicData(mPackId, mId);
}

const StyleData& HGAssets::getStyleData(
    const std::string& mPackId, const std::string& mId)
{
    return _impl->getStyleData(mPackId, mId);
}

float HGAssets::getLocalScore(const std::string& mId)
{
    return _impl->getLocalScore(mId);
}

void HGAssets::setLocalScore(const std::string& mId, float mScore)
{
    _impl->setLocalScore(mId, mScore);
}

bool HGAssets::anyLocalProfileActive() const
{
    return true; // VectorPie: the cabinet is one local profile.
}

const std::string* HGAssets::getSoundPath(const std::string& assetId) const
{
    return _impl->getSoundPath(assetId);
}

const std::string* HGAssets::getMusicPath(const std::string& assetId) const
{
    return _impl->getMusicPath(assetId);
}

const std::unordered_map<std::string, LevelData>&
HGAssets::getLevelDatas() const noexcept
{
    return _impl->getLevelDatas();
}

const std::unordered_set<std::string>&
HGAssets::getPackIdsWithMissingDependencies() const noexcept
{
    return _impl->getPackIdsWithMissingDependencies();
}

std::unordered_map<std::string, std::string>& HGAssets::getLuaFileCache()
{
    return _impl->getLuaFileCache();
}

const std::unordered_map<std::string, std::string>&
HGAssets::getLuaFileCache() const
{
    return const_cast<HGAssetsImpl&>(*_impl).getLuaFileCache();
}

} // namespace hg
