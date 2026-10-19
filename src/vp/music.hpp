// The made soundtrack in time with the level (synth.hpp makes the sound).
//
// The original's beat pulse (HexagonGame::updateBeatPulse) counts
// beatPulseDelay down by TIME_STEP * getMusicDMSyncFactor() each tick and
// pulses when it reaches zero, then starts again from beatPulseDelayMax.
// So it pulses every whole number of ticks (240 a second), and the first
// time a whole number of ticks after the level starts. This works both out
// as the game does, in float, and makes the music's beat exactly that
// period (or a power of two of it: a level that pulses on every second
// beat, or on eighths), with a downbeat on the first pulse.
#pragma once

#include "synth.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace vp {

struct BeatSync
{
    int pulseTicks{0};      // ticks between pulses
    int firstTicks{0};      // ticks from the level's start to the first pulse
    double perPulse{1.0};   // beats of music per pulse (2, 1, 1/2, 1/4...)
    [[nodiscard]] double bpm() const { return 240.0 * 60.0 / pulseTicks * perPulse; }
};

BeatSync beatSync(float delayMax, float delayNow, float dmFactor);

// A piece for a level at a sync, made on a thread of its own and kept.
// `wait`: block until it is made (else nullptr while it is being made).
std::shared_ptr<const Track> musicFor(const std::string& levelId, const BeatSync& sync, bool wait);

// Where to start in it: the downbeat of a part (first play: bar 1, else the
// intro, the drop or drop 2, chosen at random) less the ticks to the first
// pulse, so the first pulse lands on it.
std::int64_t musicStartFrame(const Track& t, const BeatSync& sync, bool firstPlay, std::uint32_t random);

} // namespace vp
