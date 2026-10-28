// Headless sanity test for the tape DSP engine (no GUI, no audio device).
// Build:  cmake -B build-test -DTAPE_BUILD_TESTS=ON && cmake --build build-test
// Run:    ./build-test/TapeEngineTest_artefacts/Release/TapeEngineTest

#include <juce_dsp/juce_dsp.h>
#include "TapeEngine.h"
#include <cstdio>

static int failures = 0;

#define CHECK(cond, ...)                                         \
    do {                                                         \
        if (! (cond)) { ++failures; std::printf ("FAIL: "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
        else { std::printf ("ok:   "); std::printf (__VA_ARGS__); std::printf ("\n"); } \
    } while (0)

static juce::AudioBuffer<float> makeSine (int ch, int n, double fs, double f, float amp)
{
    juce::AudioBuffer<float> b (ch, n);
    for (int c = 0; c < ch; ++c)
        for (int i = 0; i < n; ++i)
            b.setSample (c, i, amp * (float) std::sin (2.0 * 3.14159265358979 * f * i / fs));
    return b;
}

static bool allFinite (const juce::AudioBuffer<float>& b, float limit = 8.0f)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v) || std::abs (v) > limit) return false;
        }
    return true;
}

static float rms (const juce::AudioBuffer<float>& b, int from, int to, int ch = 0)
{
    double s = 0;
    for (int i = from; i < to; ++i) { const double v = b.getSample (ch, i); s += v * v; }
    return (float) std::sqrt (s / std::max (1, to - from));
}

// Process in blocks of `block` samples, like a host would.
static void run (tape::Engine& e, juce::AudioBuffer<float>& b, const tape::Settings& s, int block)
{
    for (int pos = 0; pos < b.getNumSamples(); pos += block)
    {
        const int len = std::min (block, b.getNumSamples() - pos);
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), b.getNumChannels(), pos, len);
        e.process (sub, s);
    }
}

int main()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };

    for (double fs : rates)
    {
        std::printf ("---- %.0f Hz ----\n", fs);
        const int n = (int) fs * 2;

        // 1) Bypass is a pure delay by the reported latency
        {
            tape::Engine e; e.prepare (fs, 512, 2);
            tape::Settings s; s.bypass = true;
            auto in = makeSine (2, n, fs, 440.0, 0.5f);
            auto out = in;
            run (e, out, s, 480);
            const int L = e.getLatencySamples();
            double err = 0;
            for (int i = (int) (fs * 0.5); i < n - 1; ++i)
                err = std::max (err, (double) std::abs (out.getSample (0, i) - in.getSample (0, i - L)));
            CHECK (err < 1.0e-5, "bypass == input delayed by latency (%d samples), max err %.2e", L, err);
        }

        // 2) Mix = 0 behaves like bypass
        {
            tape::Engine e; e.prepare (fs, 512, 2);
            tape::Settings s; s.mix = 0.0f;
            auto in = makeSine (2, n, fs, 440.0, 0.5f);
            auto out = in;
            run (e, out, s, 256);
            const int L = e.getLatencySamples();
            double err = 0;
            for (int i = (int) (fs * 0.5); i < n - 1; ++i)
                err = std::max (err, (double) std::abs (out.getSample (1, i) - in.getSample (1, i - L)));
            CHECK (err < 1.0e-5, "dry/wet = 0 == dry delayed, max err %.2e", err);
        }

        // 3) Neutral settings keep the level close to unity and the signal clean
        {
            tape::Engine e; e.prepare (fs, 512, 2);
            tape::Settings s;
            s.drive = 0; s.saturation = 0; s.headBump = 0; s.loss = 0; s.compression = 0;
            s.wow = s.flutter = s.drift = s.degrade = 0; s.hissOn = false; s.bias = 0.5f;
            auto in = makeSine (2, n, fs, 1000.0, 0.25f);
            auto out = in;
            run (e, out, s, 512);
            const float rIn = rms (in, (int) fs, n - 1), rOut = rms (out, (int) fs, n - 1);
            const float dB = 20.0f * std::log10 (rOut / rIn);
            CHECK (std::abs (dB) < 1.0f, "neutral: 1 kHz level change %.2f dB", dB);
        }

        // 4) Default settings and extreme settings stay finite and bounded
        {
            tape::Engine e; e.prepare (fs, 512, 2);
            tape::Settings d;
            auto b = makeSine (2, n, fs, 220.0, 0.7f);
            run (e, b, d, 333);
            CHECK (allFinite (b), "default settings: finite and bounded");

            tape::Settings x;
            x.inputDb = 12; x.outputDb = 12; x.drive = 1; x.saturation = 1; x.bias = 0; x.headBump = 1;
            x.tone = 1; x.loss = 1; x.compression = 1; x.wow = x.flutter = x.drift = x.degrade = 1;
            x.hiss = 1; x.hissOn = true; x.speed = 0; x.type = 0; x.link = false; x.mix = 1;
            auto b2 = makeSine (2, n, fs, 3000.0, 1.0f);
            run (e, b2, x, 64);
            CHECK (allFinite (b2, 60.0f), "all controls at max: finite (%.2f peak)", b2.getMagnitude (0, n));
        }

        // 5) Randomised parameter sweeps (zipper / NaN hunting)
        {
            tape::Engine e; e.prepare (fs, 512, 2);
            juce::Random r (7);
            bool ok = true;
            for (int it = 0; it < 200 && ok; ++it)
            {
                tape::Settings s;
                s.inputDb = r.nextFloat() * 24 - 12; s.outputDb = r.nextFloat() * 24 - 12;
                s.drive = r.nextFloat(); s.saturation = r.nextFloat(); s.bias = r.nextFloat();
                s.headBump = r.nextFloat(); s.tone = r.nextFloat() * 2 - 1; s.loss = r.nextFloat();
                s.compression = r.nextFloat(); s.wow = r.nextFloat(); s.flutter = r.nextFloat();
                s.drift = r.nextFloat(); s.degrade = r.nextFloat(); s.hiss = r.nextFloat();
                s.mix = r.nextFloat(); s.speed = r.nextInt (3); s.type = r.nextInt (3); s.eq = r.nextInt (2);
                s.hissOn = r.nextBool(); s.link = r.nextBool(); s.bypass = r.nextInt (10) == 0; s.power = r.nextInt (10) != 0;
                auto b = makeSine (2, 1024, fs, 100.0 + r.nextFloat() * 8000.0, r.nextFloat());
                run (e, b, s, 1 + r.nextInt (700));
                ok = allFinite (b, 100.0f);
            }
            CHECK (ok, "200 random parameter sets: finite and bounded");
        }

        // 6) Mono and odd block sizes
        {
            tape::Engine e; e.prepare (fs, 1024, 1);
            tape::Settings s;
            auto b = makeSine (1, 5000, fs, 500.0, 0.5f);
            run (e, b, s, 17);
            CHECK (allFinite (b), "mono, 17-sample blocks");
        }
    }

    // 7) Behaviour checks at 48 kHz
    {
        const double fs = 48000.0;
        const int n = (int) fs * 3;

        // Saturation adds harmonics: compare output spectrum at 3 kHz for a 1 kHz tone
        auto thdRatio = [&] (float drive, float sat)
        {
            tape::Engine e; e.prepare (fs, 512, 2);
            tape::Settings s;
            s.drive = drive; s.saturation = sat; s.compression = 0; s.wow = s.flutter = s.drift = 0; s.headBump = 0; s.loss = 0;
            auto b = makeSine (2, n, fs, 1000.0, 0.5f);
            run (e, b, s, 512);
            const int start = (int) fs;
            double re1 = 0, im1 = 0, re3 = 0, im3 = 0;
            for (int i = start; i < n; ++i)
            {
                const double x = b.getSample (0, i), t = (double) i / fs;
                re1 += x * std::cos (2 * 3.14159265358979 * 1000 * t); im1 += x * std::sin (2 * 3.14159265358979 * 1000 * t);
                re3 += x * std::cos (2 * 3.14159265358979 * 3000 * t); im3 += x * std::sin (2 * 3.14159265358979 * 3000 * t);
            }
            return std::sqrt (re3 * re3 + im3 * im3) / std::sqrt (re1 * re1 + im1 * im1);
        };
        const double lo = thdRatio (0.0f, 0.0f), hi = thdRatio (1.0f, 1.0f);
        CHECK (hi > lo * 5.0, "saturation raises 3rd harmonic: %.5f -> %.5f", lo, hi);

        // Wow/flutter/drift modulate the pitch: measure the spread of cycle lengths of a 1 kHz tone
        auto periodSpread = [&] (const tape::Settings& st)
        {
            tape::Engine eng; eng.prepare (fs, 512, 2);
            auto sig = makeSine (2, n, fs, 1000.0, 0.5f);
            run (eng, sig, st, 512);
            std::vector<double> cross;
            for (int i = (int) fs + 1; i < n; ++i)
            {
                const float a0 = sig.getSample (0, i - 1), a1 = sig.getSample (0, i);
                if (a0 < 0 && a1 >= 0) cross.push_back ((double) (i - 1) + (double) (-a0 / (a1 - a0)));
            }
            double mn = 1e9, mx = 0;
            for (size_t k = 1; k < cross.size(); ++k)
            {
                const double per = cross[k] - cross[k - 1];
                mn = std::min (mn, per); mx = std::max (mx, per);
            }
            return (mx - mn) / (0.5 * (mx + mn));
        };
        tape::Settings steady;
        steady.drive = 0; steady.saturation = 0; steady.compression = 0; steady.wow = steady.flutter = steady.drift = 0;
        tape::Settings wobbly = steady;
        wobbly.wow = wobbly.flutter = wobbly.drift = 1; wobbly.speed = 0;
        const double s0 = periodSpread (steady), s1 = periodSpread (wobbly);
        CHECK (s0 < 0.002 && s1 > 0.01, "wow/flutter modulate pitch: cycle spread %.4f%% -> %.2f%%", s0 * 100, s1 * 100);

        // Hiss: silence in -> noise out only when enabled
        tape::Engine e2; e2.prepare (fs, 512, 2);
        tape::Settings h; h.hiss = 0.5f; h.hissOn = true;
        juce::AudioBuffer<float> sil (2, n); sil.clear();
        run (e2, sil, h, 512);
        const float hissRms = rms (sil, (int) fs, n);
        tape::Engine e3; e3.prepare (fs, 512, 2);
        h.hissOn = false;
        juce::AudioBuffer<float> sil2 (2, n); sil2.clear();
        run (e3, sil2, h, 512);
        CHECK (hissRms > 1.0e-5f && rms (sil2, (int) fs, n) < 1.0e-9f, "hiss only when on (on: %.1f dBFS)",
               20.0f * std::log10 (hissRms));
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
