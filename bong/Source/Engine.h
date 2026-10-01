#pragma once

// bong DSP. Plain C++ (no JUCE) so it can be stress-tested on its own.
//   input -> RHYTHM (step effects driven by the sequencer) -> ATMOS (texture chain) -> output
//   KAOS modulators are evaluated by the processor and folded into the parameter values.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <array>

namespace bong
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clamp01 (float v) { return std::min (1.0f, std::max (0.0f, v)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float expMap (float v, float lo, float hi) { return lo * std::pow (hi / lo, clamp01 (v)); }

// Deterministic hash -> [0,1)
inline float hash01 (uint32_t a, uint32_t b, uint32_t c)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (float) (h & 0xFFFFFF) / 16777216.0f;
}

struct Rng
{
    uint32_t s = 0x1234567u;
    void seed (uint32_t v) { s = v * 2654435761u + 1u; if (s == 0) s = 1; }
    float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 16777216.0f; }
    float bi() { return uni() * 2.0f - 1.0f; }
};

struct SVF
{
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void reset() { ic1 = ic2 = 0; }
    void set (float cutoff, float q, float sr)
    {
        const float g = std::tan (kPi * std::min (std::max (cutoff, 20.0f), 0.45f * sr) / sr);
        k = 1.0f / std::max (0.5f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline float lp (float v0)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v2;
    }
};

struct OnePole
{
    float z = 0, a = 0;
    void setCutoff (float hz, float sr) { a = std::exp (-kTwoPi * std::min (hz, 0.49f * sr) / sr); }
    inline float lp (float x) { z = x + a * (z - x); return z; }
};

// Circular history with absolute (64-bit) sample addressing
struct History
{
    std::vector<float> buf; int64_t mask = 0;
    void prepare (int minSize) { int s = 1; while (s < minSize) s <<= 1; buf.assign ((size_t) s, 0.0f); mask = s - 1; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (int64_t abs, float x) { buf[(size_t) (abs & mask)] = x; }
    inline float read (double absPos) const
    {
        const double fl = std::floor (absPos);
        const int64_t i0 = (int64_t) fl;
        const float fr = (float) (absPos - fl);
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + fr * (b - a);
    }
    int64_t size() const { return mask + 1; }
};

//==============================================================================
// Parameter snapshot (all knob values 0..1 unless noted). Filled by the processor every sub-block.
struct Params
{
    // rhythm
    float g_depth, g_rate, g_smooth, r_len, r_count, r_tone, v_len, v_chance, v_fade;
    float s_pieces, s_blend, s_chance, f_cut, f_res, f_depth, b_speed, b_curve, b_tone;
    float amt[6];
    float rhythmMix, swing;
    int sync, seqLen, seqDir;
    uint16_t pattern[6];
    // atmos
    float k_size, k_dens, k_spread, z_hold, z_fade, z_tone, y_size, y_tail, y_color;
    float h_oct, h_amt, h_glit, t_drift, t_cut, t_speed, w_wow, w_flut, w_hiss;
    bool atmosOn[6];
    float atmosMix;
    bool freeze;
    float outputGain;
    uint32_t seed;
};

// Transport info for a block of samples
struct Clock
{
    double ppqStart = 0;     // position in quarter notes at first sample
    double ppqPerSample = 0; // bpm / 60 / sr
    double bpm = 120;
};

//==============================================================================
class Rhythm
{
public:
    enum { GATE, GENTAG, BAGLAENS, SKAER, FILTER, BREMS };

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& h : hist) h.prepare ((int) (sr * 12.0f));
        for (auto& f : filt) f.reset();
        for (auto& t : toneLp) t.z = 0;
        gateGain = 1; lastStepCounter = INT64_MIN; absPos = 0; curStep = -1;
        stepActive.fill (false);
    }

    int currentSeqStep() const { return curStep; }

    // in/out are processed in place; numCh 1 or 2
    void process (float* const* ch, int numCh, int n, const Params& p, const Clock& clk)
    {
        const double stepsPerBeat = p.sync == 0 ? 2.0 : p.sync == 2 ? 8.0 : 4.0;
        const double swingAmt = clamp01 (p.swing) * 0.5;     // boundary moves from 1.0 up to 1.5 inside a pair
        const double samplesPerStep = (60.0 / std::max (20.0, clk.bpm)) / stepsPerBeat * sr;

        for (int i = 0; i < n; ++i)
        {
            // --- sequencer position ---
            const double ppq = clk.ppqStart + clk.ppqPerSample * i;
            const double stepPos = ppq * stepsPerBeat;
            const double pairPos = stepPos * 0.5;
            const double pairIdx = std::floor (pairPos);
            const double inPair = (pairPos - pairIdx) * 2.0;    // 0..2
            const double boundary = 1.0 + swingAmt;
            const int half = inPair < boundary ? 0 : 1;
            const int64_t counter = (int64_t) pairIdx * 2 + half;

            if (counter != lastStepCounter)
            {
                const double lenSteps = half == 0 ? boundary : (2.0 - boundary);
                const double elapsed = half == 0 ? inPair : (inPair - boundary);
                startStep (counter, p, samplesPerStep * lenSteps, samplesPerStep * elapsed, samplesPerStep);
            }

            const double t = (double) (absPos - stepStartAbs);
            processSample (ch, numCh, i, p, t);
            ++absPos;
        }
    }

private:
    void startStep (int64_t counter, const Params& p, double lenSamples, double alreadyElapsed, double samplesPerStep)
    {
        lastStepCounter = counter;
        stepLen = std::max (32.0, lenSamples);
        stepStartAbs = absPos - (int64_t) alreadyElapsed;
        stepSamples = samplesPerStep;

        const int L = std::max (1, std::min (16, p.seqLen));
        const int64_t k = counter < 0 ? 0 : counter;
        int s = 0;
        switch (p.seqDir)
        {
            case 1: s = L - 1 - (int) (k % L); break;
            case 2: { const int period = std::max (1, 2 * L - 2); const int m = (int) (k % period); s = m < L ? m : period - m; break; }
            case 3: s = (int) (hash01 (p.seed, (uint32_t) k, 99) * L); break;
            default: s = (int) (k % L);
        }
        curStep = s;

        for (int f = 0; f < 6; ++f)
            stepActive[(size_t) f] = ((p.pattern[f] >> s) & 1) != 0 && p.amt[f] > 0.001f;

        auto roll = [&] (int fx, float chance) { return hash01 (p.seed, (uint32_t) k, (uint32_t) fx) < chance; };
        if (stepActive[BAGLAENS]) stepActive[BAGLAENS] = roll (BAGLAENS, 0.1f + 0.9f * p.v_chance);
        if (stepActive[SKAER])    stepActive[SKAER]    = roll (SKAER, 0.15f + 0.85f * p.s_chance);

        // slice choice for SKÆR
        const int pieces = 2 + (int) std::round (p.s_pieces * 14.0f);
        sliceIndex = (int) (hash01 (p.seed, (uint32_t) k, 777) * pieces);
        brakePos = 0; brakeRate = 1;
    }

    inline float fadeEnv (double t, double len, double fade) const
    {
        if (fade <= 1) return 1.0f;
        const double a = std::min (t / fade, (len - t) / fade);
        return (float) std::max (0.0, std::min (1.0, a));
    }

    void processSample (float* const* ch, int numCh, int i, const Params& p, double t)
    {
        const double fade = 0.003 * sr;
        const float stepEnv = fadeEnv (t, stepLen, fade);

        // Which time effect owns this step (priority)
        int timeFx = -1;
        if (stepActive[BREMS]) timeFx = BREMS;
        else if (stepActive[BAGLAENS]) timeFx = BAGLAENS;
        else if (stepActive[SKAER]) timeFx = SKAER;
        else if (stepActive[GENTAG]) timeFx = GENTAG;

        // Pre-compute shared read position for the time effect
        double readAbs = 0; float wetEnv = 0; float toneHz = 20000;
        if (timeFx == GENTAG)
        {
            const int div = 1 + (int) std::round (p.r_count * 7.0f);
            const double slice = std::max (64.0, stepLen / div);
            const double within = std::fmod (t, slice);
            readAbs = (double) stepStartAbs + within;
            const double gateLen = slice * (0.15 + 0.85 * (1.0 - p.r_len * 0.9));
            wetEnv = fadeEnv (within, std::min (gateLen, slice), 0.002 * sr);
            toneHz = expMap (p.r_tone, 400.0f, 20000.0f);
        }
        else if (timeFx == BAGLAENS)
        {
            const double W = stepSamples * (1 + std::round (p.v_len * 3.0f));
            const double w = std::fmod (t, W);
            readAbs = (double) stepStartAbs - w - 1;
            wetEnv = fadeEnv (w, W, 0.002 * sr + p.v_fade * 0.35 * W);
        }
        else if (timeFx == SKAER)
        {
            const int pieces = 2 + (int) std::round (p.s_pieces * 14.0f);
            const double bar = std::min (stepSamples * 16.0, (double) sr * 10.0);
            const double sliceLen = bar / pieces;
            readAbs = (double) stepStartAbs - bar + sliceIndex * sliceLen + std::fmod (t, sliceLen);
            wetEnv = fadeEnv (std::fmod (t, sliceLen), sliceLen, 0.002 * sr) * (0.3f + 0.7f * p.s_blend);
        }
        else if (timeFx == BREMS)
        {
            const float endSpeed = p.b_speed * 0.9f;          // 0 = full stop, ~0.5 = half speed
            const float u = (float) std::min (1.0, t / stepLen);
            const float shape = std::pow (u, 0.25f + (1.0f - p.b_curve) * 2.5f);
            brakeRate = 1.0f - (1.0f - endSpeed) * shape;
            readAbs = (double) stepStartAbs + brakePos;
            brakePos += brakeRate;
            wetEnv = 1.0f;
            toneHz = lerp (20000.0f, 300.0f + 15000.0f * brakeRate * brakeRate, p.b_tone);
        }

        const float timeAmt = timeFx >= 0 ? p.amt[timeFx] * stepEnv : 0.0f;

        // Gate
        float gTarget = 1.0f;
        if (stepActive[GATE])
        {
            static const int divs[] = { 1, 2, 3, 4, 6, 8 };
            const int div = divs[std::min (5, (int) (p.g_rate * 6.0f))];
            const double ph = std::fmod (t * div / stepLen, 1.0);
            const float depth = p.g_depth * p.amt[GATE];
            gTarget = ph < 0.5 ? 1.0f : 1.0f - depth;
        }
        const float gCoef = std::exp (-1.0f / (sr * (0.0008f + p.g_smooth * 0.03f)));
        gateGain = gTarget + gCoef * (gateGain - gTarget);

        // Step filter
        const bool filtOn = stepActive[FILTER];
        if (filtOn || (i & 15) == 0)
        {
            const float u = (float) std::min (1.0, t / stepLen);
            const float cut = expMap (p.f_cut, 80.0f, 16000.0f) * std::pow (2.0f, p.f_depth * 5.0f * (1.0f - u));
            const float q = 0.6f + p.f_res * p.f_res * 9.0f;
            for (int c = 0; c < 2; ++c) filt[c].set (cut, q, sr);
        }
        const float filtAmt = filtOn ? p.amt[FILTER] * stepEnv : 0.0f;
        for (int c = 0; c < 2; ++c) toneLp[c].setCutoff (toneHz, sr);

        for (int c = 0; c < numCh; ++c)
        {
            const float x = ch[c][i];
            hist[c].write (absPos, x);
            float y = x;

            if (timeAmt > 0.0f)
            {
                float w = hist[c].read (readAbs);
                w = toneLp[c].lp (w);
                y = lerp (x, w * wetEnv, timeAmt);
            }
            else toneLp[c].z = x;

            const float fl = filt[c].lp (y);
            y = lerp (y, fl, filtAmt);
            y *= gateGain;

            ch[c][i] = lerp (x, y, p.rhythmMix);
        }
    }

    float sr = 44100;
    History hist[2];
    SVF filt[2];
    OnePole toneLp[2];
    std::array<bool, 6> stepActive {};
    int64_t lastStepCounter = INT64_MIN, absPos = 0, stepStartAbs = 0;
    double stepLen = 1000, stepSamples = 1000, brakePos = 0;
    float brakeRate = 1, gateGain = 1;
    int sliceIndex = 0, curStep = -1;
};

//==============================================================================
// Freeverb-style stereo reverb
class Reverb
{
public:
    void prepare (double sampleRate)
    {
        const float scale = (float) sampleRate / 44100.0f;
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4] = { 556, 441, 341, 225 };
        for (int c = 0; c < 2; ++c)
        {
            const int spread = c == 0 ? 0 : 23;
            for (int j = 0; j < 8; ++j) { comb[c][j].buf.assign ((size_t) ((combT[j] + spread) * scale), 0.0f); comb[c][j].idx = 0; comb[c][j].store = 0; }
            for (int j = 0; j < 4; ++j) { ap[c][j].buf.assign ((size_t) ((apT[j] + spread) * scale), 0.0f); ap[c][j].idx = 0; }
        }
        avgDelaySec = 1400.0f / 44100.0f;
    }
    void set (float feedback, float damp) { fb = feedback; dampA = damp; }
    // Approximate steady-state gain from input to output (used to keep shimmer feedback below 1)
    float loopGain() const { return 0.12f / std::max (0.005f, 1.0f - fb); }
    float rt60() const { return fb <= 0.0f ? 0.0f : -3.0f * avgDelaySec / std::log10 (fb); }

    inline void process (float inL, float inR, float& outL, float& outR)
    {
        const float in[2] = { inL, inR };
        float out[2] = { 0, 0 };
        for (int c = 0; c < 2; ++c)
        {
            const float x = in[c] * 0.015f;
            float acc = 0;
            for (auto& cb : comb[c])
            {
                float y = cb.buf[(size_t) cb.idx];
                cb.store = y * (1 - dampA) + cb.store * dampA;
                cb.buf[(size_t) cb.idx] = x + cb.store * fb;
                if (++cb.idx >= (int) cb.buf.size()) cb.idx = 0;
                acc += y;
            }
            for (auto& a : ap[c])
            {
                float b = a.buf[(size_t) a.idx];
                float y = -acc + b;
                a.buf[(size_t) a.idx] = acc + b * 0.5f;
                if (++a.idx >= (int) a.buf.size()) a.idx = 0;
                acc = y;
            }
            out[c] = acc;
        }
        outL = out[0]; outR = out[1];
    }

private:
    struct Comb { std::vector<float> buf; int idx = 0; float store = 0; };
    struct AP { std::vector<float> buf; int idx = 0; };
    Comb comb[2][8]; AP ap[2][4];
    float fb = 0.8f, dampA = 0.3f, avgDelaySec = 0.03f;
};

// Two-tap delay-line pitch shifter
struct PitchShift
{
    std::vector<float> buf; int w = 0; float phase = 0; int size = 4096;
    void prepare (double sr) { size = 1; while (size < (int) (sr * 0.09)) size <<= 1; buf.assign ((size_t) size, 0.0f); w = 0; phase = 0; }
    inline float process (float x, float ratio)
    {
        buf[(size_t) w] = x;
        const float win = (float) size * 0.8f;
        phase += (1.0f - ratio);
        while (phase < 0) phase += win;
        while (phase >= win) phase -= win;
        auto rd = [&] (float d) {
            float pos = (float) w - d - 1.0f; while (pos < 0) pos += (float) size;
            const int i0 = (int) pos; const float fr = pos - (float) i0;
            return buf[(size_t) (i0 & (size - 1))] * (1 - fr) + buf[(size_t) ((i0 + 1) & (size - 1))] * fr;
        };
        const float d1 = phase, d2 = std::fmod (phase + win * 0.5f, win);
        const float g1 = std::sin (kPi * d1 / win), g2 = std::sin (kPi * d2 / win);
        w = (w + 1) & (size - 1);
        return rd (d1) * g1 + rd (d2) * g2;
    }
};

//==============================================================================
class Atmos
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            hist[c].prepare ((int) (sr * 3.0f));
            tape[c].prepare ((int) (sr * 0.05f));
            fog[c].reset();
            shift[c].prepare (sampleRate);
            hissLp[c].z = 0; freezeLp[c].z = 0; glitLp[c].z = 0;
            loop[c].assign ((size_t) (sr * 1.6f), 0.0f);
        }
        rev.prepare (sampleRate);
        for (auto& g : grains) g.active = false;
        abs = 0; grainClock = 0; freezeLevel = 0; wasFrozen = false; loopLen = 1; loopPos = 0;
        wowPh = flutPh = fogPh = 0; fogRand = fogRandT = 0; shimL = shimR = 0;
        rng.seed (77);
        // predelay line
        pre[0].prepare ((int) (sr * 0.2f)); pre[1].prepare ((int) (sr * 0.2f));
    }

    float tailSeconds() const { return rev.rt60(); }
    bool isFrozen() const { return wasFrozen; }

    void process (float* const* ch, int numCh, int n, const Params& p)
    {
        const bool onKorn = p.atmosOn[0], onFrys = p.atmosOn[1], onSky = p.atmosOn[2];
        const bool onShim = p.atmosOn[3], onFog = p.atmosOn[4], onTape = p.atmosOn[5];

        // block-rate settings
        rev.set (std::min (0.988f, 0.70f + p.y_tail * 0.288f), 0.05f + (1.0f - p.y_color) * 0.6f);
        const float preDelay = p.y_size * 0.12f * sr;
        const float fogBase = expMap (p.t_cut, 150.0f, 16000.0f);
        const float fogRate = expMap (p.t_speed, 0.02f, 3.0f);
        static const float semis[] = { 0, 5, 7, 12, 19, 24 };
        const float ratio = std::pow (2.0f, semis[std::min (5, (int) (p.h_oct * 6.0f))] / 12.0f);
        for (int c = 0; c < 2; ++c)
        {
            freezeLp[c].setCutoff (expMap (1.0f - p.z_tone, 500.0f, 18000.0f), sr);
            hissLp[c].setCutoff (7000.0f, sr);
            glitLp[c].setCutoff (expMap (p.h_glit, 2000.0f, 16000.0f), sr);
        }
        const float grainRate = p.k_dens * p.k_dens * 40.0f;
        const float grainLen = expMap (p.k_size, 0.02f, 0.5f) * sr;
        const float fadeSec = expMap (p.z_fade, 0.05f, 10.0f);
        const float freezeRelease = std::exp (-1.0f / (fadeSec * sr));
        const float freezeAttack = std::exp (-1.0f / (0.02f * sr));

        const int nAdd = (onKorn && grainRate > 0.05f ? 1 : 0) + (onFrys ? 1 : 0) + (onSky || onShim ? 1 : 0);
        const float addNorm = 0.8f / std::sqrt ((float) std::max (1, nAdd));

        // freeze capture on rising edge
        const bool freezeNow = p.freeze && onFrys;
        if (freezeNow && ! wasFrozen)
        {
            loopLen = std::max (256, std::min ((int) loop[0].size(), (int) (expMap (p.z_hold, 0.06f, 1.5f) * sr)));
            for (int c = 0; c < 2; ++c)
                for (int j = 0; j < loopLen; ++j)
                    loop[c][(size_t) j] = hist[c].read ((double) (abs - loopLen + j));
            loopPos = 0;
        }
        wasFrozen = freezeNow;

        for (int i = 0; i < n; ++i)
        {
            float x[2], y[2];
            x[0] = ch[0][i]; x[1] = numCh > 1 ? ch[1][i] : x[0];
            y[0] = x[0]; y[1] = x[1];

            // BÅND: wow/flutter + hiss
            if (onTape)
            {
                wowPh += 0.5f / sr; if (wowPh >= 1) wowPh -= 1;
                flutPh += 7.1f / sr; if (flutPh >= 1) flutPh -= 1;
                const float d = p.w_wow * p.w_wow * 0.006f * sr * (1 + std::sin (kTwoPi * wowPh))
                              + p.w_flut * 0.0006f * sr * (1 + std::sin (kTwoPi * flutPh));
                for (int c = 0; c < 2; ++c)
                {
                    tape[c].write (abs, y[c]);
                    y[c] = tape[c].read ((double) abs - d);
                    y[c] += hissLp[c].lp (rng.bi()) * p.w_hiss * p.w_hiss * 0.05f;
                }
            }

            // TÅGE: drifting lowpass
            if (onFog)
            {
                fogPh += fogRate / sr; if (fogPh >= 1) fogPh -= 1;
                if ((i & 63) == 0)
                {
                    fogRandT += (rng.bi() - fogRandT) * 0.02f;
                    fogRand += (fogRandT - fogRand) * 0.05f;
                    const float mod = 0.6f * std::sin (kTwoPi * fogPh) + 0.4f * fogRand * 4.0f;
                    const float cut = fogBase * std::pow (2.0f, p.t_drift * 4.0f * mod);
                    fog[0].set (cut, 0.9f, sr); fog[1].set (cut, 0.9f, sr);
                }
                y[0] = fog[0].lp (y[0]); y[1] = fog[1].lp (y[1]);
            }

            hist[0].write (abs, y[0]); hist[1].write (abs, y[1]);
            float add[2] = { 0.0f, 0.0f };

            // KORN: granular cloud
            if (onKorn && grainRate > 0.05f)
            {
                grainClock += grainRate / sr;
                if (grainClock >= 1.0f)
                {
                    grainClock -= 1.0f;
                    for (auto& g : grains)
                        if (! g.active)
                        {
                            g.active = true; g.len = grainLen * (0.7f + 0.6f * rng.uni()); g.t = 0;
                            if (rng.uni() < p.k_spread * 0.3f) g.rate = rng.uni() < 0.5f ? 0.5f : 2.0f;
                            else g.rate = std::pow (2.0f, rng.bi() * p.k_spread * 0.3f / 12.0f);
                            const float back = g.len * std::max (1.0f, g.rate) + 64.0f + rng.uni() * p.k_spread * 1.5f * sr;
                            g.start = (double) abs - std::min (back, (float) hist[0].size() * 0.9f);
                            g.pan = 0.5f + rng.bi() * 0.5f * p.k_spread;
                            break;
                        }
                }
                float gl = 0, gr = 0;
                for (auto& g : grains)
                    if (g.active)
                    {
                        const float w = 0.5f - 0.5f * std::cos (kTwoPi * g.t / g.len);
                        const double pos = g.start + g.t * g.rate;
                        gl += hist[0].read (pos) * w * (1 - g.pan);
                        gr += hist[1].read (pos) * w * g.pan;
                        g.t += 1.0f;
                        if (g.t >= g.len) g.active = false;
                    }
                const float norm = 0.9f / std::sqrt (1.0f + grainRate * grainLen / sr);
                add[0] += gl * norm; add[1] += gr * norm;
            }

            // FRYS: frozen loop
            freezeLevel = freezeNow ? 1.0f + freezeAttack * (freezeLevel - 1.0f) : freezeLevel * freezeRelease;
            if (freezeLevel > 0.0005f)
            {
                const int xf = std::max (16, loopLen / 8);
                for (int c = 0; c < 2; ++c)
                {
                    float s = loop[c][(size_t) loopPos];
                    const int tail = loopLen - loopPos;
                    if (tail < xf) s = lerp (loop[c][(size_t) (xf - tail)], s, (float) tail / (float) xf);
                    add[c] += freezeLp[c].lp (s) * freezeLevel;
                }
                if (++loopPos >= loopLen) loopPos = xf;
            }

            // SKY + SHIMMER
            if (onSky || onShim)
            {
                pre[0].write (abs, y[0]); pre[1].write (abs, y[1]);
                const float pl = pre[0].read ((double) abs - preDelay), pr = pre[1].read ((double) abs - preDelay);
                const float shimFb = onShim ? p.h_amt * 0.55f / rev.loopGain() : 0.0f;
                float rl, rr;
                rev.process (pl + shimL * shimFb, pr + shimR * shimFb, rl, rr);
                if (onShim)
                {
                    shimL = std::tanh (glitLp[0].lp (shift[0].process (rl, ratio)));
                    shimR = std::tanh (glitLp[1].lp (shift[1].process (rr, ratio)));
                }
                else shimL = shimR = 0;
                const float wet = onSky ? 1.8f : 1.0f * p.h_amt;
                add[0] += rl * wet + shimL * (onShim ? p.h_amt * 0.35f : 0.0f);
                add[1] += rr * wet + shimR * (onShim ? p.h_amt * 0.35f : 0.0f);
            }
            y[0] += add[0] * addNorm; y[1] += add[1] * addNorm;

            ch[0][i] = lerp (x[0], y[0], p.atmosMix);
            if (numCh > 1) ch[1][i] = lerp (x[1], y[1], p.atmosMix);
            ++abs;
        }
    }

private:
    struct Grain { bool active = false; double start = 0; float t = 0, len = 1, rate = 1, pan = 0.5f; };
    float sr = 44100;
    History hist[2], tape[2], pre[2];
    SVF fog[2];
    OnePole hissLp[2], freezeLp[2], glitLp[2];
    PitchShift shift[2];
    Reverb rev;
    std::array<Grain, 24> grains;
    std::vector<float> loop[2];
    int loopLen = 1, loopPos = 0;
    int64_t abs = 0;
    float grainClock = 0, freezeLevel = 0, wowPh = 0, flutPh = 0, fogPh = 0, fogRand = 0, fogRandT = 0, shimL = 0, shimR = 0;
    bool wasFrozen = false;
    Rng rng;
};

//==============================================================================
// KAOS modulators. Evaluated per sub-block; outputs in [-1,1] (FØLGER is 0..1).
class Mods
{
public:
    void prepare (double sampleRate) { sr = (float) sampleRate; for (auto& v : out) v = 0; diceT = diceV = diceS = 0; lfoPh = 0; env = 0; }

    // knobs[m][0..2] = rate, depth, extra ; inputLevel = block peak of the input
    void advance (int n, const float knobs[4][3], float inputLevel, double ppq, uint32_t seed)
    {
        const float dt = (float) n / sr;
        // TERNING: sample & hold random, smoothed
        {
            const float rate = expMap (knobs[0][0], 0.1f, 16.0f);
            diceT += rate * dt;
            if (diceT >= 1.0f) { diceT -= std::floor (diceT); rng.seed (seed ^ (uint32_t) (++diceCount * 7919)); diceS = rng.bi(); }
            const float tau = 0.002f + knobs[0][2] * (1.0f / rate);
            diceV += (diceS - diceV) * (1.0f - std::exp (-dt / tau));
            out[0] = diceV;
        }
        // LFO: sine -> triangle -> square
        {
            const float rate = expMap (knobs[1][0], 0.03f, 12.0f);
            lfoPh += rate * dt; lfoPh -= std::floor (lfoPh);
            const float s = std::sin (kTwoPi * lfoPh);
            const float tri = 1.0f - 4.0f * std::abs (lfoPh - 0.5f);
            const float sq = lfoPh < 0.5f ? 1.0f : -1.0f;
            const float f = knobs[1][2];
            out[1] = f < 0.5f ? lerp (s, tri, f * 2.0f) : lerp (tri, sq, (f - 0.5f) * 2.0f);
        }
        // FØLGER: envelope follower (ANSLAG = attack, "SLIP" = release)
        {
            const float att = expMap (1.0f - knobs[2][0], 0.001f, 0.3f);
            const float rel = expMap (knobs[2][1], 0.02f, 2.0f);
            const float target = std::min (1.0f, inputLevel * 3.0f);
            const float tau = target > env ? att : rel;
            env += (target - env) * (1.0f - std::exp (-dt / tau));
            out[2] = env;
        }
        // TRINMOD: stepped random sequence synced to host
        {
            static const double divs[] = { 0.125, 0.25, 0.5, 1.0, 2.0, 4.0 };
            const double len = divs[std::min (5, (int) (knobs[3][0] * 6.0f))];
            const int steps = 2 + (int) std::round (knobs[3][2] * 14.0f);
            const int64_t idx = (int64_t) std::floor (ppq / len);
            const int s = (int) (((idx % steps) + steps) % steps);
            out[3] = hash01 (seed, (uint32_t) s, 4242) * 2.0f - 1.0f;
        }
    }

    float value (int m) const { return out[m]; }

private:
    float sr = 44100;
    float out[4] {};
    float diceT = 0, diceV = 0, diceS = 0, lfoPh = 0, env = 0;
    uint32_t diceCount = 0;
    Rng rng;
};

} // namespace bong
