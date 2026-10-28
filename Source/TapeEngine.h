#pragma once

// StudioFros - Tape Machine: DSP engine.
//
// Original implementation (no third-party DSP code). Signal flow:
//
//   in -> input gain -> tape compression
//      -> [2x oversampled] pre-emphasis -> asymmetric saturation -> de-emphasis
//      -> DC block -> head bump -> HF loss -> tone tilt
//      -> wow / flutter / drift (modulated delay) -> dropouts (degrade) -> + hiss
//      -> output gain -> dry/wet -> power/bypass
//
// The engine reports a fixed latency (oversampler + base delay of the modulated
// delay line); the dry path is delayed by the same amount so dry/wet and bypass
// stay phase-aligned.

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace tape
{
struct Settings
{
    float inputDb = 0.0f, outputDb = 0.0f;
    float drive = 0.35f, bias = 0.5f, saturation = 0.5f, headBump = 0.4f, tone = 0.0f;
    float loss = 0.3f, compression = 0.3f;
    float wow = 0.2f, flutter = 0.2f, drift = 0.2f, degrade = 0.0f, hiss = 0.2f;
    float mix = 1.0f;
    int speed = 1;   // 0 = 7.5 ips, 1 = 15 ips, 2 = 30 ips
    int type = 0;    // 0 = Vintage, 1 = Modern, 2 = High Output
    int eq = 0;      // 0 = NAB, 1 = IEC
    bool hissOn = false, link = true, bypass = false, power = true;
};

struct Meters
{
    float inPeak = 0.0f, inRms = 0.0f, outPeak = 0.0f, outRms = 0.0f;
};

namespace detail
{
    constexpr double twoPi = 6.283185307179586476925286766559;

    // RBJ cookbook biquad, double precision, transposed direct form II, 2 channels.
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[2] {}, z2[2] {};

        void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0.0; }

        inline float process (int ch, float xf) noexcept
        {
            const double x = xf;
            const double y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return (float) y;
        }

        void set (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
        {
            const double inv = 1.0 / A0;
            b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv; a1 = A1 * inv; a2 = A2 * inv;
        }

        static double clampFreq (double fs, double f) { return std::clamp (f, 5.0, fs * 0.45); }

        void lowpass (double fs, double f, double q)
        {
            const double w = twoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }

        void highpass (double fs, double f, double q)
        {
            const double w = twoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }

        void peak (double fs, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = twoPi * clampFreq (fs, f) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }

        void lowShelf (double fs, double f, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = twoPi * clampFreq (fs, f) / fs, c = std::cos (w), s = std::sin (w);
            const double beta = 2.0 * std::sqrt (A) * (s / std::sqrt (2.0));
            set (A * ((A + 1) - (A - 1) * c + beta),
                 2 * A * ((A - 1) - (A + 1) * c),
                 A * ((A + 1) - (A - 1) * c - beta),
                 (A + 1) + (A - 1) * c + beta,
                 -2 * ((A - 1) + (A + 1) * c),
                 (A + 1) + (A - 1) * c - beta);
        }

        void highShelf (double fs, double f, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = twoPi * clampFreq (fs, f) / fs, c = std::cos (w), s = std::sin (w);
            const double beta = 2.0 * std::sqrt (A) * (s / std::sqrt (2.0));
            set (A * ((A + 1) + (A - 1) * c + beta),
                 -2 * A * ((A - 1) + (A + 1) * c),
                 A * ((A + 1) + (A - 1) * c - beta),
                 (A + 1) - (A - 1) * c + beta,
                 2 * ((A - 1) - (A + 1) * c),
                 (A + 1) - (A - 1) * c - beta);
        }
    };

    // Circular delay line with 4-point Hermite interpolation.
    struct ModDelay
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void prepare (int minSize)
        {
            int n = 16;
            while (n < minSize) n <<= 1;
            buf.assign ((size_t) n, 0.0f);
            mask = n - 1;
            w = 0;
        }

        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

        inline float process (float x, double delaySamples) noexcept
        {
            buf[(size_t) w] = x;
            const double pos = (double) w - delaySamples;
            const int i = (int) std::floor (pos);
            const float f = (float) (pos - (double) i);
            const float y0 = buf[(size_t) ((i - 1) & mask)];
            const float y1 = buf[(size_t) (i & mask)];
            const float y2 = buf[(size_t) ((i + 1) & mask)];
            const float y3 = buf[(size_t) ((i + 2) & mask)];
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            w = (w + 1) & mask;
            return ((c3 * f + c2) * f + c1) * f + y1;
        }
    };

    // Slow, organic speed instability: two wow partials, two flutter partials + noise, random-walk drift.
    struct Modulator
    {
        double fs = 44100.0, p1 = 0, p2 = 0, p3 = 0, p4 = 0;
        float flNoise = 0, drift = 0, driftTarget = 0, driftCoef = 0.0001f;
        int driftCount = 0;
        juce::Random rng;

        void init (double sampleRate, int64_t seed, double phaseOffset)
        {
            fs = sampleRate;
            rng.setSeed (seed);
            p1 = phaseOffset; p2 = phaseOffset * 1.7; p3 = phaseOffset * 2.3; p4 = phaseOffset * 3.1;
            p1 -= std::floor (p1); p2 -= std::floor (p2); p3 -= std::floor (p3); p4 -= std::floor (p4);
            flNoise = drift = driftTarget = 0.0f;
            driftCount = 0;
            driftCoef = (float) (1.0 - std::exp (-1.0 / (1.2 * fs)));
        }

        inline void next (float& wow, float& flutter, float& driftOut) noexcept
        {
            wow = 0.7f * (float) std::sin (twoPi * p1) + 0.3f * (float) std::sin (twoPi * p2);
            flNoise += 0.02f * ((rng.nextFloat() * 2.0f - 1.0f) - flNoise);
            flutter = 0.55f * (float) std::sin (twoPi * p3) + 0.3f * (float) std::sin (twoPi * p4) + 1.2f * flNoise;

            if (--driftCount <= 0)
            {
                driftTarget = rng.nextFloat() * 2.0f - 1.0f;
                driftCount = (int) (fs * (0.4 + 1.2 * (double) rng.nextFloat()));
            }
            drift += driftCoef * (driftTarget - drift);
            driftOut = drift;

            p1 += 0.55 / fs;  p1 -= std::floor (p1);
            p2 += 1.37 / fs;  p2 -= std::floor (p2);
            p3 += 7.10 / fs;  p3 -= std::floor (p3);
            p4 += 11.7 / fs;  p4 -= std::floor (p4);
        }
    };

    // Random tape dropouts: short, smooth gain dips.
    struct Dropout
    {
        double fs = 44100.0;
        int remaining = 0, length = 1, countdown = 0;
        float depth = 0.0f;
        juce::Random rng;

        void init (double sampleRate, int64_t seed)
        {
            fs = sampleRate;
            rng.setSeed (seed);
            remaining = 0; length = 1; depth = 0.0f;
            countdown = (int) (fs * 1.0);
        }

        inline float next (float degrade) noexcept
        {
            if (degrade < 0.01f)
            {
                remaining = 0;
                return 1.0f;
            }

            if (remaining > 0)
            {
                const float progress = 1.0f - (float) remaining / (float) length;
                --remaining;
                const float env = 0.5f - 0.5f * std::cos ((float) twoPi * progress);
                return 1.0f - depth * env;
            }

            if (--countdown <= 0)
            {
                const double interval = (0.4 + 0.8 * (double) rng.nextFloat()) / (std::pow ((double) degrade, 1.5) + 1.0e-4);
                countdown = (int) (fs * std::min (interval, 120.0));
                length = std::max (1, (int) (fs * (0.005 + 0.035 * (double) rng.nextFloat())));
                remaining = length;
                depth = std::min (0.8f, (0.15f + 0.5f * rng.nextFloat()) * (0.3f + 0.7f * degrade));
            }
            return 1.0f;
        }
    };

    inline int nextPow2 (int v)
    {
        int n = 1;
        while (n < v) n <<= 1;
        return n;
    }
}

class Engine
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int numChannels)
    {
        fs = sampleRate;
        fsOs = sampleRate * 2.0;
        maxBlock = std::max (1, maximumBlockSize);
        nch = std::clamp (numChannels, 1, 2);

        os = std::make_unique<juce::dsp::Oversampling<float>> (
            2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        os->initProcessing ((size_t) maxBlock);

        baseDelay = juce::roundToInt (0.006 * fs);
        latency = baseDelay + juce::roundToInt (os->getLatencyInSamples());

        const int dryLen = detail::nextPow2 (latency + 8);
        for (auto& l : dryLine) l.assign ((size_t) dryLen, 0.0f);
        dryMask = dryLen - 1;
        dryW = 0;
        dryBuf.setSize (2, maxBlock, false, true, true);

        for (auto& d : delay) d.prepare (baseDelay * 2 + 16);

        mod[0].init (fs, 1234, 0.00);
        mod[1].init (fs, 5678, 0.37);
        drop[0].init (fs, 4242);
        drop[1].init (fs, 2424);
        hissRng[0].setSeed (99);
        hissRng[1].setSeed (101);

        compAtk = (float) (1.0 - std::exp (-1.0 / (0.010 * fs)));
        compRel = (float) (1.0 - std::exp (-1.0 / (0.150 * fs)));

        const double ramp = 0.03;
        for (auto* s : { &inGain, &outGain, &mixS, &procMix, &compAmt, &hissGain, &wowDepth, &flDepth, &driftDepth })
            s->reset (fs, ramp);
        for (auto* s : { &driveS, &kS, &biasS })
            s->reset (fsOs, ramp);

        // Start from a sane state, so the first block does not ramp from zero.
        Settings defaults;
        updateTargets (defaults, true);

        reset();
    }

    void reset()
    {
        if (os) os->reset();
        for (auto& l : dryLine) std::fill (l.begin(), l.end(), 0.0f);
        for (auto& d : delay) d.reset();
        for (auto* b : { &preEmph, &deEmph, &dc, &bump, &lossLp, &toneLo, &toneHi, &hissHp }) b->reset();
        compEnv[0] = compEnv[1] = 0.0f;
        dryW = 0;
    }

    int getLatencySamples() const noexcept { return latency; }

    Meters process (juce::AudioBuffer<float>& buffer, const Settings& s)
    {
        Accum acc;
        const int total = buffer.getNumSamples();
        const int nc = std::min (buffer.getNumChannels(), nch);

        for (int pos = 0; pos < total; pos += maxBlock)
        {
            const int len = std::min (maxBlock, total - pos);
            juce::AudioBuffer<float> sub (buffer.getArrayOfWritePointers(), nc, pos, len);
            processChunk (sub, s, acc);
        }

        Meters m;
        m.inPeak = acc.inPeak;
        m.outPeak = acc.outPeak;
        if (acc.count > 0)
        {
            m.inRms  = (float) std::sqrt (acc.inSq  / (double) acc.count);
            m.outRms = (float) std::sqrt (acc.outSq / (double) acc.count);
        }
        return m;
    }

private:
    struct Accum
    {
        float inPeak = 0, outPeak = 0;
        double inSq = 0, outSq = 0;
        long count = 0;
    };

    static float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

    void updateTargets (const Settings& s, bool jump = false)
    {
        static constexpr float speedMod[3]  = { 1.8f, 1.0f, 0.55f };
        static constexpr float typeK[3]     = { 1.0f, 0.75f, 0.5f };
        static constexpr float typeAsym[3]  = { 1.0f, 0.6f, 0.4f };
        static constexpr float typeBump[3]  = { 1.0f, 0.8f, 0.6f };
        static constexpr float speedHissDb[3] = { 6.0f, 0.0f, -6.0f };
        static constexpr float typeHissDb[3]  = { 3.0f, 0.0f, -4.0f };
        static constexpr double nabFc[3] = { 3180.0, 3180.0, 9095.0 };
        static constexpr double iecFc[3] = { 2274.0, 4547.0, 9095.0 };
        static constexpr double bumpFc[3] = { 45.0, 75.0, 130.0 };
        static constexpr double lossFc[3] = { 11000.0, 15000.0, 19500.0 };

        const int sp = std::clamp (s.speed, 0, 2);
        const int ty = std::clamp (s.type, 0, 2);

        auto set = [jump] (auto& smoothed, float v)
        {
            if (jump) smoothed.setCurrentAndTargetValue (v);
            else smoothed.setTargetValue (v);
        };

        set (inGain, dbToGain (s.inputDb));
        set (outGain, dbToGain (s.outputDb));
        set (mixS, std::clamp (s.mix, 0.0f, 1.0f));
        set (procMix, (s.power && ! s.bypass) ? 1.0f : 0.0f);
        set (compAmt, std::clamp (s.compression, 0.0f, 1.0f));

        const float hissDb = -64.0f + 36.0f * s.hiss + speedHissDb[sp] + typeHissDb[ty];
        set (hissGain, (s.hissOn && s.hiss > 0.001f) ? dbToGain (hissDb) : 0.0f);

        set (wowDepth,   s.wow * 1.5f * speedMod[sp]);
        set (flDepth,    s.flutter * 0.12f * speedMod[sp] * (1.0f + s.degrade));
        set (driftDepth, s.drift * 1.2f * speedMod[sp]);

        set (driveS, dbToGain (s.drive * 18.0f));
        set (kS, (0.5f + s.saturation * 2.5f) * typeK[ty] * (1.15f - 0.3f * s.bias));
        set (biasS, (0.5f - s.bias) * 0.8f * typeAsym[ty]);

        // Filters (block-rate coefficient updates)
        const double emphFc = (s.eq == 0 ? nabFc[sp] : iecFc[sp]);
        preEmph.highShelf (fsOs, emphFc, 6.0);
        deEmph.highShelf (fsOs, emphFc, -6.0);

        dc.highpass (fs, 15.0, 0.707);
        bump.peak (fs, bumpFc[sp], 0.9, (double) (s.headBump * 5.0f * typeBump[ty]));

        const double lossHz = lossFc[sp] * std::pow (0.18, (double) s.loss) * (1.0 - 0.55 * (double) s.degrade);
        lossLp.lowpass (fs, std::max (800.0, lossHz), 0.707);

        toneLo.lowShelf (fs, 350.0, (double) (-s.tone * 5.0f));
        toneHi.highShelf (fs, 3500.0, (double) (s.tone * 5.0f));
        hissHp.highpass (fs, 600.0, 0.6);
    }

    void processChunk (juce::AudioBuffer<float>& buf, const Settings& s, Accum& acc)
    {
        const int n = buf.getNumSamples();
        const int nc = buf.getNumChannels();
        if (n <= 0 || nc <= 0) return;

        updateTargets (s);

        float* ch[2] = { buf.getWritePointer (0), nc > 1 ? buf.getWritePointer (1) : nullptr };
        float* dry[2] = { dryBuf.getWritePointer (0), dryBuf.getWritePointer (1) };

        // 1) Dry path delay, input gain, tape compression --------------------------------------
        for (int i = 0; i < n; ++i)
        {
            const float g = inGain.getNextValue();
            const float ca = compAmt.getNextValue();
            float a[2] = { 0, 0 };

            for (int c = 0; c < nc; ++c)
            {
                const float x = ch[c][i];
                dryLine[c][(size_t) dryW] = x;
                dry[c][i] = dryLine[c][(size_t) ((dryW - latency) & dryMask)];

                const float y = x * g;
                ch[c][i] = y;
                acc.inPeak = std::max (acc.inPeak, std::abs (y));
                acc.inSq += (double) y * (double) y;
                a[c] = std::abs (y);
            }
            dryW = (dryW + 1) & dryMask;
            acc.count += nc;

            for (int c = 0; c < nc; ++c)
                compEnv[c] += (a[c] > compEnv[c] ? compAtk : compRel) * (a[c] - compEnv[c]);

            if (ca > 0.0005f)
            {
                const float linked = nc > 1 ? std::max (compEnv[0], compEnv[1]) : compEnv[0];
                const float thr = -4.0f - 20.0f * ca;
                const float ratio = 1.0f + 3.0f * ca;
                const float makeup = -thr * (1.0f - 1.0f / ratio) * 0.35f;

                for (int c = 0; c < nc; ++c)
                {
                    const float level = s.link ? linked : compEnv[c];
                    const float db = juce::Decibels::gainToDecibels (level, -100.0f);
                    const float over = db - thr;
                    const float gr = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
                    ch[c][i] *= dbToGain (makeup - gr);
                }
            }
        }

        // 2) Saturation, oversampled ------------------------------------------------------------
        {
            juce::dsp::AudioBlock<float> block (buf.getArrayOfWritePointers(), (size_t) nc, (size_t) n);
            auto up = os->processSamplesUp (block);
            const size_t m = up.getNumSamples();

            for (size_t i = 0; i < m; ++i)
            {
                const float dG = driveS.getNextValue();
                const float k = kS.getNextValue();
                const float b = biasS.getNextValue();
                const float t = std::tanh (k * b);
                const float invSqrt = 1.0f / std::sqrt (dG);
                const float comp = 1.0f / (1.0f - t * t);

                for (int c = 0; c < nc; ++c)
                {
                    float* d = up.getChannelPointer ((size_t) c);
                    float x = preEmph.process (c, d[i]);
                    const float u = x * dG;
                    float y = (std::tanh (k * (u + b)) - t) / k * comp * invSqrt;
                    d[i] = deEmph.process (c, y);
                }
            }
            os->processSamplesDown (block);
        }

        // 3) Tape body, instability, hiss, mix --------------------------------------------------
        const bool link = s.link || nc < 2;
        for (int i = 0; i < n; ++i)
        {
            const float wd = wowDepth.getNextValue();
            const float fl = flDepth.getNextValue();
            const float dr = driftDepth.getNextValue();
            const float hg = hissGain.getNextValue();
            const float og = outGain.getNextValue();
            const float mx = mixS.getNextValue();
            const float pm = procMix.getNextValue();

            float w0, f0, d0, w1 = 0, f1 = 0, d1 = 0;
            mod[0].next (w0, f0, d0);
            if (! link) mod[1].next (w1, f1, d1);

            const float g0 = drop[0].next (s.degrade);
            const float g1 = link ? g0 : drop[1].next (s.degrade);

            for (int c = 0; c < nc; ++c)
            {
                float x = ch[c][i];
                x = dc.process (c, x);
                x = bump.process (c, x);
                x = lossLp.process (c, x);
                x = toneLo.process (c, x);
                x = toneHi.process (c, x);

                const bool second = (c == 1 && ! link);
                const float ms = second ? (w1 * wd + f1 * fl + d1 * dr) : (w0 * wd + f0 * fl + d0 * dr);
                const double dly = std::clamp ((double) baseDelay + (double) ms * 0.001 * fs,
                                               3.0, (double) baseDelay * 1.95);
                x = delay[c].process (x, dly);
                x *= (second ? g1 : g0);

                if (hg > 0.0f)
                {
                    const float noise = (hissRng[c].nextFloat() * 2.0f - 1.0f) * 1.73f;
                    x += hg * hissHp.process (c, noise);
                }

                const float wet = x * og;
                const float d = dry[c][i];
                const float processed = d * (1.0f - mx) + wet * mx;
                const float out = d * (1.0f - pm) + processed * pm;

                ch[c][i] = out;
                acc.outPeak = std::max (acc.outPeak, std::abs (out));
                acc.outSq += (double) out * (double) out;
            }
        }
    }

    double fs = 44100.0, fsOs = 88200.0;
    int maxBlock = 512, nch = 2, baseDelay = 265, latency = 265;

    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float> dryLine[2];
    int dryMask = 0, dryW = 0;

    detail::Biquad preEmph, deEmph, dc, bump, lossLp, toneLo, toneHi, hissHp;
    detail::ModDelay delay[2];
    detail::Modulator mod[2];
    detail::Dropout drop[2];
    juce::Random hissRng[2];

    float compEnv[2] { 0, 0 };
    float compAtk = 0.002f, compRel = 0.0002f;

    juce::SmoothedValue<float> inGain { 1.0f }, outGain { 1.0f }, mixS { 1.0f }, procMix { 1.0f }, compAmt { 0.3f },
                               hissGain { 0.0f }, wowDepth { 0.0f }, flDepth { 0.0f }, driftDepth { 0.0f };
    juce::SmoothedValue<float> driveS { 1.0f }, kS { 1.0f }, biasS { 0.0f };
};
} // namespace tape
