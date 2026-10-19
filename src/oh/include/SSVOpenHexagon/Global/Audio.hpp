// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port: the original's Audio interface (Audio.hpp), played through
// vp_audio. Sounds are the same ids ("beep.ogg", "<packId>_<file>"); music
// is "<packId>_<music id>". The SDK streams music only from its start, so a
// track is decoded once into memory and each segment start plays from there.

#pragma once

#include "SSVOpenHexagon/Utils/UniquePtr.hpp"

#include <string>
#include <functional>
#include <vector>

typedef struct vp_audio vp_audio;

namespace hg {

class Audio
{
public:
    using SoundPathGetter =
        std::function<const std::string*(const std::string&)>;

    using MusicPathGetter =
        std::function<const std::string*(const std::string&)>;

private:
    class AudioImpl;

    Utils::UniquePtr<AudioImpl> _impl;

    [[nodiscard]] const AudioImpl& impl() const noexcept;
    [[nodiscard]] AudioImpl& impl() noexcept;

public:
    explicit Audio(vp_audio* device, const SoundPathGetter& soundPathGetter,
        const MusicPathGetter& musicPathGetter);

    ~Audio();

    // 0..100, as the original's config values.
    void setSoundVolume(const float volume);
    void setMusicVolume(const float volume);

    void resumeMusic();
    void pauseMusic();
    void stopMusic();

    void setMusicPlayingOffsetSeconds(const float seconds);

    [[nodiscard]] float getMusicPlayingOffsetSeconds() const;

    void stopSounds();

    void playSoundOverride(const std::string& id);
    void playPackSoundOverride(
        const std::string& packId, const std::string& id);

    void playSoundAbort(const std::string& id);
    void playPackSoundAbort(const std::string& packId, const std::string& id);

    [[nodiscard]] bool loadAndPlayMusic(const std::string& packId,
        const std::string& id, const float playingOffsetSeconds);

    void setCurrentMusicPitch(const float pitch);

    // VectorPie: the made soundtrack (src/vp/music.cpp): play `pcm` (stereo,
    // 16 bit, `rate`) from `startFrame`, then loop from `loopStart`. `key`
    // names it, so the same piece is not copied again. The pitch stays 1:
    // the piece is made at the level's tempo.
    void playGenerated(const std::vector<short>& pcm, int rate,
        long long startFrame, long long loopStart, const std::string& key);

    // VectorPie: while quiet the game is not heard (sounds and music do
    // nothing); playUiSound is heard regardless (the menus' beeps).
    void setQuiet(bool quiet);
    void playUiSound(const std::string& id);

    // VectorPie: once per step, so a track loops after its first pass.
    void update();

    // VectorPie: whether a track exists for the id (music is optional).
    [[nodiscard]] bool hasMusic(
        const std::string& packId, const std::string& id) const;
};

} // namespace hg
