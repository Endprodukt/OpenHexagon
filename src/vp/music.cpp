// The made soundtrack in time with the level: see music.hpp.
#include "music.hpp"

#include <SSVUtils/Core/Log/Log.hpp>

#include <chrono>
#include <cstdio>
#include <cmath>
#include <future>
#include <map>
#include <mutex>

namespace vp {

BeatSync beatSync(float delayMax, float delayNow, float dmFactor)
{
    // HexagonGame::updateBeatPulse, tick by tick, in the game's float:
    //   if(delay <= 0) { pulse; delay = delayMax; } else delay -= step;
    const float step = 0.25f * dmFactor; // Config::TIME_STEP * factor

    BeatSync s;
    if(!(delayMax > 0.f) || !(step > 0.f))
    {
        s.pulseTicks = 240 / 2; // no pulse to follow: 120 bpm
        return s;
    }

    float d = delayNow;
    int first = 0;
    while(d > 0.f && first < 100000) { d -= step; ++first; }
    s.firstTicks = first;

    d = delayMax;
    int period = 1; // the tick that pulses
    while(d > 0.f && period < 100000) { d -= step; ++period; }
    s.pulseTicks = period;

    // The music between 120 and 190 beats a minute.
    double bpm = 240.0 * 60.0 / period;
    s.perPulse = 1.0;
    while(bpm * s.perPulse < 120.0) s.perPulse *= 2.0;
    while(bpm * s.perPulse > 190.0) s.perPulse *= 0.5;
    return s;
}

namespace {

struct Entry
{
    std::shared_future<std::shared_ptr<const Track>> future;
};

std::mutex g_lock;
std::map<std::string, Entry> g_cache;

std::uint32_t hash(const std::string& s)
{
    std::uint32_t h = 2166136261u;
    for(const char c : s) { h ^= static_cast<std::uint8_t>(c); h *= 16777619u; }
    return h;
}

} // namespace

std::shared_ptr<const Track> musicFor(const std::string& levelId, const BeatSync& sync, bool wait)
{
    // A beat as a whole number of samples: (pulse ticks / 240 s) per pulse.
    const double pulseFrames = sync.pulseTicks * (Track::RATE / 240.0); // 200 per tick
    TrackSpec spec;
    spec.seed = hash(levelId);
    spec.beatFrames = std::round(pulseFrames / sync.perPulse);
    spec.dubstep = sync.bpm() >= 150.0;

    const std::string key = levelId + "@" + std::to_string(static_cast<long long>(spec.beatFrames));

    std::shared_future<std::shared_ptr<const Track>> f;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        auto it = g_cache.find(key);
        if(it == g_cache.end())
        {
            // One piece is about 20 MB: keep the last few only.
            if(g_cache.size() >= 4) g_cache.erase(g_cache.begin());

            f = std::async(std::launch::async, [spec, key]
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    std::shared_ptr<const Track> t = renderTrack(spec);
                    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0).count();
                    char bpm[32];
                    std::snprintf(bpm, sizeof bpm, "%.1f", 60.0 * Track::RATE / spec.beatFrames);
                    ssvu::lo("vp::music") << "made " << key << " (" << (spec.dubstep ? "dubstep" : "techno")
                                          << ", " << bpm << " bpm) in " << ms << " ms\n";
                    return t;
                }).share();
            g_cache.emplace(key, Entry{f});
        }
        else
        {
            f = it->second.future;
        }
    }

    if(wait) return f.get();
    if(f.wait_for(std::chrono::seconds(0)) == std::future_status::ready) return f.get();
    return nullptr;
}

std::int64_t musicStartFrame(const Track& t, const BeatSync& sync, bool firstPlay, std::uint32_t random)
{
    static const int parts[] = {1, 17, 45};
    const int bar = firstPlay ? 1 : parts[random % 3];
    const std::int64_t lead = static_cast<std::int64_t>(sync.firstTicks) * (Track::RATE / 240);
    std::int64_t start = t.barStart(bar) - lead;
    if(start < 0) start = 0;
    return start;
}

} // namespace vp
