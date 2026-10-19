// The soundtrack, made by the game: hard dubstep and techno, one piece per
// level, in time with the level's beat pulse.
//
// Not the original's music (that is licensed to the Steam version only).
// Everything here is synthesized: kick, snare, clap, hats, a wobble or a
// rolling bass, pads, an arpeggio lead, risers and a sidechain. A piece is
// 1 bar of lead-in and 64 bars of song that loop:
//
//   bars  1- 8 intro      drums thin, pad, filtered bass
//   bars  9-16 build      snare roll, riser, the filter opening
//   bars 17-32 drop       everything
//   bars 33-40 break      pad and arpeggio, no kick
//   bars 41-44 build
//   bars 45-60 drop 2     the bass and the lead varied
//   bars 61-64 outro      back into the intro
//
// The tempo is exact to the sample: a beat is a whole number of samples,
// worked out from the beat pulse's period in game ticks (see music.cpp).
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace vp {

struct TrackSpec
{
    std::uint32_t seed{1};     // key, patterns, sounds
    double beatFrames{0.0};    // samples (at RATE) per beat
    bool dubstep{false};       // halftime dubstep, else four to the floor techno
};

struct Track
{
    static constexpr int RATE = 48000;

    std::vector<short> pcm;    // stereo, interleaved
    std::int64_t barFrames{0};
    std::int64_t leadIn{0};    // the song's bar 1 starts here
    std::int64_t loopStart{0}; // where it loops back to (bar 1)

    // Where a bar of the song starts (1-based, as the plan above).
    [[nodiscard]] std::int64_t barStart(int bar) const
    {
        return leadIn + static_cast<std::int64_t>(bar - 1) * barFrames;
    }
};

std::shared_ptr<Track> renderTrack(const TrackSpec& spec);

} // namespace vp
