// Copyright (c) 2013-2020 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: https://opensource.org/licenses/AFL-3.0

// VectorPie port of Core/Audio.cpp: see Audio.hpp.
//
// Sounds: ssvs::SoundPlayer's two modes. Override stops the same sound if it
// is playing and starts it again; Abort does nothing while it plays.
//
// Music: the original opens the .ogg once (_lastLoadedMusicPath) and seeks
// to the segment's time. Here the track is decoded once into memory with the
// SDK's stb_vorbis; each start makes a sound from the samples after the
// offset and loops the whole track after it (setLoop(true) in the original
// loops from the start: the part before the offset is played on the next
// pass, so the loop is the full track).

#include "SSVOpenHexagon/Global/Audio.hpp"

#include "SSVOpenHexagon/Global/Assert.hpp"
#include "SSVOpenHexagon/Utils/Concat.hpp"

#include <SSVUtils/Core/Log/Log.hpp>

#include <vectorpie/vp.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

// The SDK builds stb_vorbis into libvectorpie; only its declarations here.
#define STB_VORBIS_HEADER_ONLY
#include "../../../../vendor/VectorPie/sdk/src/third_party/stb/stb_vorbis.c"

namespace hg {

class Audio::AudioImpl
{
private:
    vp_audio* _dev;
    SoundPathGetter _soundPathGetter;
    MusicPathGetter _musicPathGetter;

    float _soundVolume{1.f};
    float _musicVolume{1.f};

    struct LoadedSound
    {
        vp_sound* sound{nullptr};
        vp_voice voice{0};
        bool failed{false};
    };

    std::unordered_map<std::string, LoadedSound> _sounds;

    // The decoded track (interleaved, _channels per frame).
    std::string _lastLoadedMusicPath;
    std::vector<short> _pcm;
    int _channels{2};
    int _rate{44100};

    // The playing part: from _offsetSeconds to the end, then the whole track
    // looped.
    vp_sound* _head{nullptr};
    vp_sound* _whole{nullptr};
    vp_voice _voice{0};
    bool _onWhole{false};
    bool _paused{true};
    bool _hasMusic{false};
    bool _quiet{false};
    bool _generated{false};      // VectorPie: the made soundtrack plays
    std::size_t _loopStart{0};   // frames: where the whole track loops from
    float _offsetSeconds{0.f};
    float _pitch{1.f};
    double _startedAt{0.0};

    [[nodiscard]] LoadedSound* getSound(const std::string& id)
    {
        auto it = _sounds.find(id);
        if(it != _sounds.end())
        {
            return it->second.failed ? nullptr : &it->second;
        }

        LoadedSound ls;
        const std::string* path = _soundPathGetter(id);

        if(path == nullptr || vp_sound_load(_dev, path->c_str(), &ls.sound) != VP_OK)
        {
            ssvu::lo("hg::AudioImpl::getSound")
                << "No sound for id '" << id << "'\n";
            ls.failed = true;
        }

        auto& slot = _sounds[id] = ls;
        return slot.failed ? nullptr : &slot;
    }

    enum class Mode
    {
        Override,
        Abort
    };

    void playSoundImpl(const std::string& id, const Mode mode, bool ui = false)
    {
        if(_dev == nullptr || _soundVolume <= 0.f || (_quiet && !ui))
        {
            return;
        }

        LoadedSound* ls = getSound(id);
        if(ls == nullptr)
        {
            return;
        }

        if(ls->voice != 0 && vp_voice_playing(_dev, ls->voice))
        {
            if(mode == Mode::Abort)
            {
                return;
            }

            vp_voice_stop(_dev, ls->voice, 0.f);
        }

        ls->voice = vp_sound_play(_dev, ls->sound, _soundVolume, 1.f, 0);
    }

    void freeMusicSounds()
    {
        if(_voice != 0)
        {
            vp_voice_stop(_dev, _voice, 0.f);
            _voice = 0;
        }

        vp_sound_free(_dev, _head);
        _head = nullptr;
    }

    [[nodiscard]] vp_sound* makeSound(std::size_t firstFrame)
    {
        const std::size_t frames = _pcm.size() / _channels;
        if(firstFrame >= frames)
        {
            firstFrame = 0;
        }

        const std::size_t n = frames - firstFrame;
        std::vector<float> f(n * _channels);
        const short* src = _pcm.data() + firstFrame * _channels;

        for(std::size_t i = 0; i < f.size(); ++i)
        {
            f[i] = src[i] * (1.f / 32768.f);
        }

        vp_sound* s = nullptr;
        if(vp_sound_from_samples(_dev, f.data(), static_cast<int>(n),
               _channels, _rate, &s) != VP_OK)
        {
            return nullptr;
        }

        return s;
    }

    void startVoice()
    {
        if(_dev == nullptr || !_hasMusic || _voice != 0)
        {
            return;
        }

        if(_head == nullptr)
        {
            _head = makeSound(static_cast<std::size_t>(_offsetSeconds * _rate + 0.5f));
        }

        _onWhole = false;
        _voice = vp_sound_play(_dev, _head, _musicVolume, _pitch, 0);
        _startedAt = (vp_ticks_ms() / 1000.0);
    }

public:
    explicit AudioImpl(vp_audio* device,
        const SoundPathGetter& soundPathGetter,
        const MusicPathGetter& musicPathGetter)
        : _dev{device},
          _soundPathGetter{soundPathGetter},
          _musicPathGetter{musicPathGetter}
    {}

    ~AudioImpl()
    {
        if(_dev == nullptr)
        {
            return;
        }

        freeMusicSounds();
        vp_sound_free(_dev, _whole);

        for(auto& [id, ls] : _sounds)
        {
            vp_sound_free(_dev, ls.sound);
        }
    }

    void setSoundVolume(const float volume)
    {
        _soundVolume = volume / 100.f;
    }

    void setMusicVolume(const float volume)
    {
        _musicVolume = volume / 100.f;

        if(_voice != 0)
        {
            vp_voice_set_volume(_dev, _voice, _musicVolume, 0.05f);
        }
    }

    // Called every step by the VectorPie loop (src/vp/app.cpp): after the
    // head part ends, the whole track loops.
    void update()
    {
        if(_dev == nullptr || _voice == 0 || _paused)
        {
            return;
        }

        if(!vp_voice_playing(_dev, _voice) && !_onWhole)
        {
            if(_whole == nullptr)
            {
                _whole = makeSound(_loopStart);
            }

            _onWhole = true;
            _voice = vp_sound_play(_dev, _whole, _musicVolume, _pitch, 1);
        }
    }

    void resumeMusic()
    {
        _paused = false;
        startVoice();
    }

    void pauseMusic()
    {
        // The original pauses a track it has just started (newGame) until
        // the level starts; here the start is simply held back.
        _paused = true;

        if(_voice != 0)
        {
            freeMusicSounds();
        }
    }

    void stopMusic()
    {
        _paused = true;
        freeMusicSounds();
    }

    void setMusicPlayingOffsetSeconds(const float seconds)
    {
        if(_offsetSeconds != seconds || _voice != 0)
        {
            freeMusicSounds();
        }

        _offsetSeconds = seconds;
    }

    [[nodiscard]] float getMusicPlayingOffsetSeconds() const
    {
        if(_voice == 0)
        {
            return _offsetSeconds;
        }

        return _offsetSeconds +
               static_cast<float>((vp_ticks_ms() / 1000.0) - _startedAt) * _pitch;
    }

    void stopSounds()
    {
        for(auto& [id, ls] : _sounds)
        {
            if(ls.voice != 0)
            {
                vp_voice_stop(_dev, ls.voice, 0.f);
                ls.voice = 0;
            }
        }
    }

    void playSoundOverride(const std::string& id)
    {
        playSoundImpl(id, Mode::Override);
    }

    void playPackSoundOverride(const std::string& packId, const std::string& id)
    {
        playSoundImpl(Utils::concat(packId, '_', id), Mode::Override);
    }

    void playSoundAbort(const std::string& id)
    {
        playSoundImpl(id, Mode::Abort);
    }

    void playPackSoundAbort(const std::string& packId, const std::string& id)
    {
        playSoundImpl(Utils::concat(packId, '_', id), Mode::Abort);
    }

    [[nodiscard]] bool hasMusic(
        const std::string& packId, const std::string& id) const
    {
        return _musicPathGetter(Utils::concat(packId, '_', id)) != nullptr;
    }

    [[nodiscard]] bool loadAndPlayMusic(const std::string& packId,
        const std::string& id, const float playingOffsetSeconds)
    {
        const std::string assetId = Utils::concat(packId, '_', id);
        const std::string* path = _musicPathGetter(assetId);

        if(path == nullptr || _dev == nullptr || _quiet)
        {
            _hasMusic = false;
            return false;
        }

        _generated = false;
        _loopStart = 0;

        if(_lastLoadedMusicPath != *path)
        {
            freeMusicSounds();
            vp_sound_free(_dev, _whole);
            _whole = nullptr;
            _pcm.clear();

            short* out = nullptr;
            int channels = 0;
            int rate = 0;
            const int frames = stb_vorbis_decode_filename(
                path->c_str(), &channels, &rate, &out);

            if(frames <= 0 || out == nullptr || channels < 1 || channels > 2)
            {
                ssvu::lo("hg::AudioImpl::playMusic")
                    << "Failed loading music file '" << *path << "'\n";

                std::free(out);
                _lastLoadedMusicPath.clear();
                _hasMusic = false;
                return false;
            }

            ssvu::lo("hg::AudioImpl::playMusic")
                << "decoded '" << *path << "' (" << frames / rate << " s)\n";
            _pcm.assign(out, out + static_cast<std::size_t>(frames) * channels);
            std::free(out);
            _channels = channels;
            _rate = rate;
            _lastLoadedMusicPath = *path;
        }

        _hasMusic = true;
        setMusicPlayingOffsetSeconds(playingOffsetSeconds);
        resumeMusic();

        return true;
    }

    void setQuiet(bool quiet)
    {
        _quiet = quiet;
        if(quiet)
        {
            stopMusic();
            stopSounds();
        }
    }

    void playUiSound(const std::string& id)
    {
        playSoundImpl(id, Mode::Override, true);
    }

    void playGenerated(const std::vector<short>& pcm, int rate,
        long long startFrame, long long loopStart, const std::string& key)
    {
        if(_dev == nullptr || _quiet)
        {
            return;
        }

        freeMusicSounds();
        if(_lastLoadedMusicPath != key)
        {
            vp_sound_free(_dev, _whole);
            _whole = nullptr;
            _pcm = pcm;
            _channels = 2;
            _rate = rate;
            _lastLoadedMusicPath = key;
        }

        _generated = true;
        _hasMusic = true;
        _pitch = 1.f;
        _loopStart = static_cast<std::size_t>(loopStart);
        _offsetSeconds = static_cast<float>(startFrame) / static_cast<float>(rate);
    }

    void setCurrentMusicPitch(const float pitch)
    {
        if(_generated)
        {
            return; // VectorPie: made at the level's tempo
        }

        _pitch = pitch;

        if(_voice != 0)
        {
            vp_voice_set_pitch(_dev, _voice, pitch);
        }
    }
};

[[nodiscard]] const Audio::AudioImpl& Audio::impl() const noexcept
{
    SSVOH_ASSERT(_impl != nullptr);
    return *_impl;
}

[[nodiscard]] Audio::AudioImpl& Audio::impl() noexcept
{
    SSVOH_ASSERT(_impl != nullptr);
    return *_impl;
}

Audio::Audio(vp_audio* device, const SoundPathGetter& soundPathGetter,
    const MusicPathGetter& musicPathGetter)
    : _impl{Utils::makeUnique<AudioImpl>(
          device, soundPathGetter, musicPathGetter)}
{}

Audio::~Audio() = default;

void Audio::setSoundVolume(const float volume)
{
    impl().setSoundVolume(volume);
}

void Audio::setMusicVolume(const float volume)
{
    impl().setMusicVolume(volume);
}

void Audio::resumeMusic()
{
    impl().resumeMusic();
}

void Audio::pauseMusic()
{
    impl().pauseMusic();
}

void Audio::stopMusic()
{
    impl().stopMusic();
}

void Audio::setMusicPlayingOffsetSeconds(const float seconds)
{
    impl().setMusicPlayingOffsetSeconds(seconds);
}

[[nodiscard]] float Audio::getMusicPlayingOffsetSeconds() const
{
    return impl().getMusicPlayingOffsetSeconds();
}

void Audio::stopSounds()
{
    impl().stopSounds();
}

void Audio::playSoundOverride(const std::string& id)
{
    impl().playSoundOverride(id);
}

void Audio::playPackSoundOverride(
    const std::string& packId, const std::string& id)
{
    impl().playPackSoundOverride(packId, id);
}

void Audio::playSoundAbort(const std::string& id)
{
    impl().playSoundAbort(id);
}

void Audio::playPackSoundAbort(const std::string& packId, const std::string& id)
{
    impl().playPackSoundAbort(packId, id);
}

[[nodiscard]] bool Audio::loadAndPlayMusic(const std::string& packId,
    const std::string& id, const float playingOffsetSeconds)
{
    return impl().loadAndPlayMusic(packId, id, playingOffsetSeconds);
}

void Audio::setCurrentMusicPitch(const float pitch)
{
    impl().setCurrentMusicPitch(pitch);
}

void Audio::playGenerated(const std::vector<short>& pcm, int rate,
    long long startFrame, long long loopStart, const std::string& key)
{
    impl().playGenerated(pcm, rate, startFrame, loopStart, key);
}

void Audio::setQuiet(bool quiet)
{
    impl().setQuiet(quiet);
}

void Audio::playUiSound(const std::string& id)
{
    impl().playUiSound(id);
}

bool Audio::hasMusic(const std::string& packId, const std::string& id) const
{
    return impl().hasMusic(packId, id);
}

void Audio::update()
{
    impl().update();
}

} // namespace hg
