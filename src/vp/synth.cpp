// The soundtrack synthesizer: see synth.hpp.
#include "synth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace vp {

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float TAU = 2.f * PI;
constexpr float R = static_cast<float>(Track::RATE);

// --- small tools ------------------------------------------------------------

struct Rng
{
    std::uint32_t s;
    std::uint32_t next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float uni() { return (next() >> 8) * (1.f / 16777216.f); }     // 0..1
    float bi() { return uni() * 2.f - 1.f; }                       // -1..1
    int pick(int n) { return static_cast<int>(next() % static_cast<std::uint32_t>(n)); }
};

float midiHz(float n)
{
    return 440.f * std::pow(2.f, (n - 69.f) / 12.f);
}

// Zavalishin's state variable filter: stable at any cutoff.
struct Svf
{
    float ic1 = 0.f, ic2 = 0.f;
    float g = 0.f, k = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f;

    void set(float cutoff, float res) // res 0..~0.97
    {
        cutoff = std::clamp(cutoff, 20.f, R * 0.45f);
        g = std::tan(PI * cutoff / R);
        k = 2.f - 2.f * res;
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    // low, band, high
    void run(float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = v0 - k * v1 - v2;
    }

    float low(float x) { float l, b, h; run(x, l, b, h); return l; }
    float band(float x) { float l, b, h; run(x, l, b, h); return b; }
    float high(float x) { float l, b, h; run(x, l, b, h); return h; }
};

// A band-limited-ish saw (polyBLEP).
struct Saw
{
    float ph = 0.f;
    float step(float hz)
    {
        const float dt = hz / R;
        ph += dt;
        if(ph >= 1.f) ph -= 1.f;
        float v = 2.f * ph - 1.f;
        if(ph < dt) { const float t = ph / dt; v -= t + t - t * t - 1.f; }
        else if(ph > 1.f - dt) { const float t = (ph - 1.f) / dt; v -= t * t + t + t + 1.f; }
        return v;
    }
};

// sin(2 pi x) for x in 0..1, to about 0.1 %: Bhaskara-like parabola, refined.
float sinTurn(float x)
{
    const float y = x < 0.5f ? 1.f : -1.f;
    const float u = x < 0.5f ? x * 2.f : x * 2.f - 1.f; // 0..1 over a half
    const float p = 4.f * u * (1.f - u);
    return y * p * (0.775f + 0.225f * p);
}

struct Sine
{
    float ph = 0.f;
    float step(float hz)
    {
        ph += hz / R;
        if(ph >= 1.f) ph -= 1.f;
        return sinTurn(ph);
    }
};

// An exponential fall, by multiplying: e^(-rate * t) one sample at a time.
struct Fall
{
    float v = 0.f, k = 1.f;
    void start(float rate, float from = 1.f) { v = from; k = std::exp(-rate / R); }
    float next() { const float r = v; v *= k; return r; }
};

// A soft clip like tanh, at a fraction of its cost (Pade 3/2, clamped).
float soft(float x)
{
    if(x > 3.f) return 1.f;
    if(x < -3.f) return -1.f;
    const float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

// --- the song's plan ------------------------------------------------------------

enum Part
{
    LEAD_IN,
    INTRO,
    BUILD,
    DROP,
    BREAK,
    BUILD2,
    DROP2,
    OUTRO
};

Part partOf(int bar) // bar 0 = lead-in, 1..64
{
    if(bar <= 0) return LEAD_IN;
    if(bar <= 8) return INTRO;
    if(bar <= 16) return BUILD;
    if(bar <= 32) return DROP;
    if(bar <= 40) return BREAK;
    if(bar <= 44) return BUILD2;
    if(bar <= 60) return DROP2;
    return OUTRO;
}

int partBar(int bar) // the bar within its part, from 0
{
    static const int starts[] = {0, 1, 9, 17, 33, 41, 45, 61};
    return bar - starts[partOf(bar)];
}

int partBars(Part p)
{
    static const int len[] = {1, 8, 8, 16, 8, 4, 16, 4};
    return len[p];
}

bool isDrop(Part p) { return p == DROP || p == DROP2; }
bool isBuild(Part p) { return p == BUILD || p == BUILD2; }

// --- voices ------------------------------------------------------------------

struct Kick
{
    float t = 10.f;
    Sine osc;
    Fall pitch, amp;
    float punch = 1.f;
    bool hard = false;
    void hit(bool h, float p = 1.f)
    {
        t = 0.f; osc.ph = 0.f; punch = p; hard = h;
        pitch.start(hard ? 42.f : 30.f);
        amp.start(hard ? 7.f : 9.f);
    }
    float step()
    {
        if(t > 1.f) return 0.f;
        const float f = 46.f + 150.f * pitch.next();
        float v = osc.step(f) * amp.next();
        if(t < 0.004f) v += (1.f - t / 0.004f) * 0.6f; // the click
        t += 1.f / R;
        return soft(v * (hard ? 2.6f : 1.8f)) * 0.9f * punch;
    }
};

struct Snare
{
    float t = 10.f;
    Sine tone;
    Svf bp;
    float gain = 1.f;
    Snare() { bp.set(2100.f, 0.35f); }
    Fall fp, fb, fn;
    void hit(float g = 1.f) { t = 0.f; tone.ph = 0.f; gain = g; fp.start(40.f); fb.start(22.f); fn.start(13.f); }
    float step(Rng& rng)
    {
        if(t > 0.6f) return 0.f;
        const float body = tone.step(185.f + 60.f * fp.next()) * fb.next() * 0.55f;
        const float noise = bp.band(rng.bi()) * fn.next() * 1.6f;
        t += 1.f / R;
        return soft((body + noise) * 1.8f) * 0.55f * gain;
    }
};

struct Clap
{
    float t = 10.f;
    Svf bp;
    Clap() { bp.set(1300.f, 0.45f); }
    Fall burst, tail;
    int n = 0;
    void hit() { t = 0.f; n = 0; burst.start(90.f); tail.v = 0.f; }
    float step(Rng& rng)
    {
        if(t > 0.5f) return 0.f;
        // Three short bursts 9 ms apart, then the tail.
        if(n < 2 && t >= 0.009f * (n + 1)) { ++n; burst.start(90.f); }
        if(tail.v == 0.f && t >= 0.027f) tail.start(11.f, 0.8f);
        const float env = burst.next() + tail.next();
        t += 1.f / R;
        return bp.band(rng.bi()) * env * 0.9f;
    }
};

struct Hat
{
    float t = 10.f, decay = 40.f, gain = 1.f;
    Svf hp;
    Hat() { hp.set(7500.f, 0.2f); }
    Fall f;
    void hit(bool open, float g) { t = 0.f; decay = open ? 9.f : 45.f; gain = g; f.start(decay); }
    float step(Rng& rng)
    {
        if(t > 0.8f) return 0.f;
        const float v = hp.high(rng.bi()) * f.next();
        t += 1.f / R;
        return v * 0.32f * gain;
    }
};

struct Crash
{
    float t = 10.f;
    Svf hp;
    Crash() { hp.set(5000.f, 0.1f); }
    Fall f;
    void hit() { t = 0.f; f.start(1.7f); }
    float step(Rng& rng)
    {
        if(t > 3.f) return 0.f;
        const float v = hp.high(rng.bi()) * f.next();
        t += 1.f / R;
        return v * 0.28f;
    }
};

// The bass: two saws a little apart and a sine an octave under, through a
// resonant low pass. Dubstep moves the filter with an LFO (the wobble),
// techno with an envelope per note.
struct Bass
{
    Saw a, b;
    Sine sub;
    Svf lp, lp2;
    float hz = 55.f, t = 10.f, gate = 0.f, gateTarget = 0.f;
    float lfoPh = 0.f;
    int countdown = 0;
    Fall env;
    void note(float f) { hz = f; t = 0.f; gateTarget = 1.f; env.start(16.f); }
    void off() { gateTarget = 0.f; }

    // `wobble`: LFO cycles per second (0: envelope), `open` 0..1 how far the
    // filter goes, `grit` the distortion.
    float step(float wobble, float open, float grit)
    {
        gate += (gateTarget - gate) * 0.004f;
        if(gate < 1e-4f && gateTarget == 0.f) { t += 1.f / R; return 0.f; }

        lfoPh += wobble / R;
        if(lfoPh >= 1.f) lfoPh -= 1.f;
        const float e = env.next();

        if(--countdown <= 0)
        {
            float cutoff;
            if(wobble > 0.f)
            {
                const float l = 0.5f - 0.5f * sinTurn(std::fmod(lfoPh + 0.25f, 1.f));
                cutoff = 90.f + (300.f + 3200.f * open) * l * l;
            }
            else
            {
                cutoff = 110.f + (250.f + 2600.f * open) * e;
            }
            lp.set(cutoff, 0.72f);
            lp2.set(cutoff * 1.5f, 0.3f);
            countdown = 16;
        }

        const float osc = a.step(hz) * 0.5f + b.step(hz * 1.007f) * 0.5f;
        float v = lp2.low(lp.low(osc));
        v = soft(v * (1.f + grit * 5.f)) * (0.7f - grit * 0.15f);
        const float s = sub.step(hz * 0.5f) * 0.55f;
        t += 1.f / R;
        return (v + s) * gate;
    }
};

// Pad: the chord on three pairs of saws, slow in and out.
struct Pad
{
    Saw s[6];
    Svf lpL, lpR;
    float hz[3] = {110.f, 130.f, 165.f};
    float env = 0.f, target = 0.f;
    int countdown = 0;
    void chord(float a, float b, float c) { hz[0] = a; hz[1] = b; hz[2] = c; }
    void stepStereo(float open, float& l, float& r)
    {
        env += (target - env) * 0.00012f;
        if(env < 1e-4f) { l = r = 0.f; return; }
        if(--countdown <= 0) { lpL.set(500.f + 1800.f * open, 0.2f); lpR.set(520.f + 1850.f * open, 0.2f); countdown = 32; }
        float a = 0.f, b = 0.f;
        for(int i = 0; i < 3; ++i)
        {
            a += s[i * 2].step(hz[i] * 0.997f);
            b += s[i * 2 + 1].step(hz[i] * 1.004f);
        }
        l = lpL.low(a) * 0.12f * env;
        r = lpR.low(b) * 0.12f * env;
    }
};

// Lead: a square-ish saw, short notes, with a delay that goes left and right.
struct Lead
{
    Saw a, b;
    Svf lp;
    float hz = 440.f, t = 10.f, gain = 0.f;
    std::vector<float> dl, dr;
    std::size_t pos = 0, delayLen = 1;
    Fall fc, fa;
    int countdown = 0;
    void note(float f, float g) { hz = f; t = 0.f; gain = g; fc.start(12.f); fa.start(7.f); countdown = 0; }
    void setDelay(std::size_t frames)
    {
        delayLen = std::max<std::size_t>(1, frames);
        dl.assign(delayLen, 0.f);
        dr.assign(delayLen, 0.f);
    }
    void stepStereo(float& l, float& r)
    {
        float v = 0.f;
        if(t < 0.4f)
        {
            const float c = fc.next();
            if(--countdown <= 0) { lp.set(900.f + 4200.f * c, 0.35f); countdown = 16; }
            const float osc = a.step(hz) - b.step(hz * 1.5f) * 0.35f;
            v = lp.low(osc) * fa.next() * gain * 0.22f;
            t += 1.f / R;
        }
        const float el = dl[pos], er = dr[pos];
        dl[pos] = v + er * 0.42f;
        dr[pos] = el * 0.42f;
        pos = (pos + 1) % delayLen;
        l = v + el * 0.5f;
        r = v * 0.8f + er * 0.6f;
    }
};

// Riser: noise through a band pass that climbs, over a build.
struct Riser
{
    Svf bp;
    int countdown = 0;
    float step(Rng& rng, float progress) // 0..1 through the build, <0 off
    {
        if(progress < 0.f) return 0.f;
        if(--countdown <= 0) { bp.set(300.f + 7000.f * progress * progress, 0.6f); countdown = 32; }
        return bp.band(rng.bi()) * (0.05f + 0.35f * progress * progress);
    }
};

} // namespace

std::shared_ptr<Track> renderTrack(const TrackSpec& spec)
{
    auto track = std::make_shared<Track>();
    Rng rng{spec.seed * 2654435761u + 12345u};
    Rng noise{spec.seed ^ 0x9e3779b9u};
    if(noise.s == 0) noise.s = 1;
    if(rng.s == 0) rng.s = 7;

    const double beat = spec.beatFrames;
    const double step16 = beat / 4.0;
    const int BARS = 65; // lead-in + 64
    track->barFrames = static_cast<std::int64_t>(std::llround(beat * 4.0));
    track->leadIn = track->barFrames;
    track->loopStart = track->leadIn;

    const std::int64_t total = track->barFrames * BARS;
    track->pcm.assign(static_cast<std::size_t>(total) * 2, 0);

    // --- the piece's choices, by its seed
    const float tonic = 28.f + static_cast<float>(rng.pick(6));   // E1 .. A1
    static const int progs[][4] = {{0, 8, 3, 10}, {0, 5, 8, 7}, {0, 10, 8, 7}, {0, 3, 8, 10}, {0, 8, 10, 7}};
    const int* prog = progs[rng.pick(5)];
    static const int minorChord[3] = {0, 3, 7};
    static const int scale[] = {0, 3, 5, 7, 10, 12, 15}; // minor pentatonic and more

    // Dubstep: the bass's rhythm per bar (16 steps: 2 = note on, 1 = hold,
    // 0 = off) and its wobble per half bar (cycles per beat).
    static const char* dubRhythms[] = {"2111211120112111", "2111111121112011", "2112112111212111", "2011211121111211"};
    const char* dubRhythm = dubRhythms[rng.pick(4)];
    static const float wobbleSets[][4] = {{1.f, 2.f, 1.f, 4.f}, {2.f, 2.f, 3.f, 1.f}, {1.f, 4.f, 2.f, 0.5f}, {3.f, 2.f, 4.f, 2.f}};
    const float* wobbles = wobbleSets[rng.pick(4)];
    const float* wobbles2 = wobbleSets[rng.pick(4)];

    // Techno: the bass's 16ths (offbeat roll) and an acid line in drop 2.
    static const char* technoRhythms[] = {"0222022202220222", "0202022202020222", "0220022002200222"};
    const char* technoRhythm = technoRhythms[rng.pick(3)];
    int arpPattern[16];
    for(int& a : arpPattern) a = rng.pick(7);

    const bool dub = spec.dubstep;
    const float beatHz = static_cast<float>(R / beat); // beats per second

    Kick kick;
    Snare snare;
    Clap clap;
    Hat hat;
    Crash crash;
    Bass bass;
    Pad pad;
    Lead lead;
    Riser riser;
    lead.setDelay(static_cast<std::size_t>(beat * 0.75));

    float duckEnv = 0.f;                       // the sidechain: 1 at a kick, falling
    const float duckFall = std::exp(-9.f / R);
    float bassWobble = 0.f, bassOpen = 0.3f, bassGrit = 0.3f;
    float padOpen = 0.3f;

    const std::int64_t steps = static_cast<std::int64_t>(BARS) * 16;
    std::int64_t frame = 0;

    for(std::int64_t k = 0; k < steps; ++k)
    {
        const int bar = static_cast<int>(k / 16);     // 0 = lead-in
        const int st = static_cast<int>(k % 16);      // 16th in the bar
        const Part part = partOf(bar);
        const int pb = partBar(bar);
        const int chordIx = ((bar - 1) / 2) & 3;
        const float root = tonic + static_cast<float>(prog[chordIx < 0 ? 0 : chordIx]);
        const bool lastBarOfPart = pb == partBars(part) - 1;
        const float buildProgress = isBuild(part)
            ? (static_cast<float>(pb) + st / 16.f) / static_cast<float>(partBars(part)) : -1.f;
        const float leadInProgress = part == LEAD_IN ? st / 16.f : -1.f;

        // --- events of this 16th
        // Chords for the pad, every 2 bars.
        if(st == 0 && bar >= 1 && (bar - 1) % 2 == 0)
        {
            const float n = root + 24.f;
            pad.chord(midiHz(n + minorChord[0]), midiHz(n + minorChord[1]), midiHz(n + minorChord[2]));
        }

        pad.target = (part == INTRO || part == BREAK || part == OUTRO || isBuild(part)) ? 1.f
                     : isDrop(part) ? 0.45f : 0.f;
        padOpen = part == BREAK ? 0.25f + 0.6f * pb / 8.f : isBuild(part) ? 0.4f + 0.6f * buildProgress : 0.35f;

        if(st == 0 && pb == 0 && isDrop(part)) crash.hit();

        // Drums
        if(dub)
        {
            const bool drums = part != BREAK && !(part == INTRO && pb < 4) && part != LEAD_IN;
            if(drums && !(isBuild(part)))
            {
                const bool kickHere = st == 0 || (isDrop(part) && (st == 6 || (part == DROP2 && st == 11)))
                                      || (part == INTRO && st == 10);
                if(kickHere) { kick.hit(isDrop(part)); duckEnv = 1.f; }
                if(st == 8) snare.hit();
                if(st % 2 == 0) hat.hit(st % 4 == 2 && isDrop(part), st % 4 == 0 ? 0.7f : 1.f);
                else if(isDrop(part) && rng.uni() < 0.35f) hat.hit(false, 0.4f);
            }
        }
        else
        {
            const bool kicks = part != BREAK && part != LEAD_IN && !(isBuild(part) && lastBarOfPart);
            if(kicks && st % 4 == 0) { kick.hit(isDrop(part)); duckEnv = 1.f; }
            if((isDrop(part) || (part == INTRO && pb >= 4) || part == OUTRO) && (st == 4 || st == 12)) clap.hit();
            if(part != LEAD_IN && part != BREAK)
            {
                if(st % 4 == 2) hat.hit(true, 0.9f);
                else if(isDrop(part)) hat.hit(false, st % 2 ? 0.45f : 0.6f);
            }
        }

        // The build's snare roll: quarters, eighths, then sixteenths.
        if(isBuild(part))
        {
            const float pr = buildProgress;
            const int every = pr < 0.5f ? 4 : pr < 0.75f ? 2 : 1;
            if(st % every == 0) snare.hit(0.35f + 0.65f * pr);
        }
        if(part == LEAD_IN && st >= 8 && st % 2 == 0) snare.hit(0.25f + st / 32.f);

        // Bass
        if(part == BREAK || part == LEAD_IN || (isBuild(part) && lastBarOfPart))
        {
            bass.off();
        }
        else if(dub)
        {
            const char c = dubRhythm[st];
            const bool sounding = isDrop(part) || part == OUTRO || (part == INTRO && pb >= 4) || isBuild(part);
            if(!sounding || c == '0') bass.off();
            else if(c == '2')
            {
                // A note per bar on the chord's root; drop 2 jumps to the
                // fifth and the octave now and then.
                float n = root;
                if(part == DROP2 && (st == 8 || st == 12)) n += (st == 8 ? 7.f : 12.f);
                bass.note(midiHz(n + 12.f));
            }
            const float* w = part == DROP2 ? wobbles2 : wobbles;
            bassWobble = isDrop(part) ? w[(bar * 2 + st / 8) & 3] * beatHz : 0.5f * beatHz;
            bassOpen = isDrop(part) ? 1.f : isBuild(part) ? 0.2f + 0.6f * buildProgress : 0.25f;
            bassGrit = isDrop(part) ? 0.85f : 0.35f;
        }
        else
        {
            const char c = technoRhythm[st];
            const bool sounding = part != INTRO || pb >= 2;
            if(sounding && c == '2')
            {
                float n = root + 12.f;
                if(part == DROP2 && arpPattern[st] >= 5) n += 12.f; // the acid jumps
                bass.note(midiHz(n));
            }
            else if(c == '0') bass.off();
            bassWobble = 0.f;
            bassOpen = isDrop(part) ? (part == DROP2 ? 0.55f + 0.45f * std::sin(TAU * (bar - 45) / 16.f) : 0.45f)
                       : part == INTRO ? 0.08f + 0.04f * pb : isBuild(part) ? 0.15f + 0.5f * buildProgress : 0.12f;
            bassGrit = part == DROP2 ? 0.6f : 0.3f;
        }

        // Lead: arpeggio in the break and drop 2, on the chord's notes.
        if((part == BREAK || part == DROP2 || (part == DROP && pb >= 8)) && (st % 2 == 0 || part == BREAK))
        {
            const int deg = arpPattern[(st + bar) & 15];
            const float n = root + 36.f + static_cast<float>(scale[deg]);
            lead.note(midiHz(n), part == BREAK ? 0.8f : 1.f);
        }

        // --- the samples of this 16th
        const std::int64_t end = std::min<std::int64_t>(total, static_cast<std::int64_t>(std::llround((k + 1) * step16)));
        for(; frame < end; ++frame)
        {
            const float kk = kick.step();
            const float sn = snare.step(noise);
            const float cl = clap.step(noise);
            const float ht = hat.step(noise);
            const float cr = crash.step(noise);
            const float rs = riser.step(noise, std::max(buildProgress, leadInProgress));

            // Sidechain: the music ducks under the kick.
            const float duck = 1.f - 0.75f * duckEnv;
            duckEnv *= duckFall;

            const float bs = bass.step(bassWobble, bassOpen, bassGrit) * duck;
            float pl, pr, ll, lr;
            pad.stepStereo(padOpen, pl, pr);
            lead.stepStereo(ll, lr);

            const float drums = kk * 0.95f + sn * 0.7f + cl * 0.5f;
            float L = drums + ht * 0.8f + cr + rs + bs * 0.75f + (pl + ll) * duck;
            float Rr = drums + ht * 0.6f + cr * 0.9f + rs * 0.9f + bs * 0.75f + (pr + lr) * duck;

            // Master: a soft limiter.
            L = soft(L * 1.15f) * 0.92f;
            Rr = soft(Rr * 1.15f) * 0.92f;

            track->pcm[static_cast<std::size_t>(frame) * 2] = static_cast<short>(L * 32000.f);
            track->pcm[static_cast<std::size_t>(frame) * 2 + 1] = static_cast<short>(Rr * 32000.f);
        }
    }

    return track;
}

} // namespace vp
