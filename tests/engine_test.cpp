// Stand-alone test of the voice engine (no JUCE needed).
// Build: cmake -B build -DRIDGELINE_BUILD_PLUGIN=OFF && cmake --build build && ./build/engine_test

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

#include <cstring>

#include "PeaksVoice.h"
#include "stmlib/utils/random.h"

using namespace ridgeline;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        ++g_checks;                                       \
        if (!(cond)) {                                    \
            ++g_failures;                                 \
            std::printf("  FAIL: ");                      \
            std::printf(__VA_ARGS__);                     \
            std::printf("\n");                            \
        }                                                 \
    } while (0)

using Params = std::array<uint16_t, 4>;

template <class Voice>
static std::vector<float> render (double rate, const Params& params, double seconds, double triggerAt,
                                  float gain = 1.0f)
{
    stmlib::Random::Seed (0x21);
    Voice v;
    v.prepare (rate);
    Params p = params;
    v.drum().Configure (p.data(), peaks::CONTROL_MODE_FULL);

    const int n = static_cast<int> (seconds * rate);
    const int trig = static_cast<int> (triggerAt * rate);
    std::vector<float> out (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
    {
        if (i == trig)
            v.trigger (gain);
        out[static_cast<size_t> (i)] = v.next();
    }
    return out;
}

static float peakOf (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    float p = 0.0f;
    to = std::min (to, x.size());
    for (size_t i = from; i < to; ++i)
        p = std::max (p, std::fabs (x[i]));
    return p;
}

// Milliseconds from the loudest 5 ms window until a window falls below 10 % of it.
static double decayMs (const std::vector<float>& x, double rate)
{
    const size_t win = static_cast<size_t> (rate * 0.005);
    std::vector<double> rms;
    for (size_t i = 0; i + win <= x.size(); i += win)
    {
        double e = 0;
        for (size_t j = 0; j < win; ++j)
            e += double (x[i + j]) * x[i + j];
        rms.push_back (std::sqrt (e / double (win)));
    }
    size_t pk = 0;
    for (size_t i = 0; i < rms.size(); ++i)
        if (rms[i] > rms[pk])
            pk = i;
    for (size_t i = pk; i < rms.size(); ++i)
        if (rms[i] < 0.1 * rms[pk])
            return double (i - pk) * 5.0;
    return -1.0;
}

template <class Voice>
static void testVoice (const char* name, const Params& params)
{
    std::printf("%s\n", name);
    const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
    for (double rate : rates)
    {
        const auto x = render<Voice> (rate, params, 3.0, 0.25);

        bool finite = true;
        for (float s : x)
            finite = finite && std::isfinite (s) && std::fabs (s) <= 1.5f;
        CHECK (finite, "%s @ %.0f Hz: non-finite or out-of-range sample", name, rate);

        const size_t trig = static_cast<size_t> (0.25 * rate);
        CHECK (peakOf (x, 0, trig) < 1e-6f, "%s @ %.0f Hz: sound before trigger", name, rate);

        const float pk = peakOf (x, trig, trig + static_cast<size_t> (0.05 * rate));
        CHECK (pk > 0.02f, "%s @ %.0f Hz: silent after trigger (peak %.4f)", name, rate, pk);

        std::printf("  %6.0f Hz  peak %.3f\n", rate, peakOf (x));
    }

    // Decay time must not depend on the host sample rate.
    const double d44 = decayMs (render<Voice> (44100.0, params, 3.0, 0.0), 44100.0);
    const double d48 = decayMs (render<Voice> (48000.0, params, 3.0, 0.0), 48000.0);
    const double d96 = decayMs (render<Voice> (96000.0, params, 3.0, 0.0), 96000.0);
    std::printf("  decay to 10%%: %.0f ms @44.1k, %.0f ms @48k, %.0f ms @96k\n", d44, d48, d96);
    CHECK (d48 > 0, "%s: never decays", name);
    CHECK (std::fabs (d44 - d48) <= 0.15 * d48 + 10.0, "%s: decay differs at 44.1k (%.0f vs %.0f ms)", name, d44, d48);
    CHECK (std::fabs (d96 - d48) <= 0.15 * d48 + 10.0, "%s: decay differs at 96k (%.0f vs %.0f ms)", name, d96, d48);

    // Velocity gain scales the whole hit.
    const float loud = peakOf (render<Voice> (48000.0, params, 1.0, 0.0, 1.0f));
    const float soft = peakOf (render<Voice> (48000.0, params, 1.0, 0.0, 0.5f));
    CHECK (std::fabs (soft / loud - 0.5f) < 0.05f, "%s: velocity gain %.3f (expected 0.5)", name, soft / loud);
}

// At 48 kHz the resampler must be an exact pass-through (delayed by 2 samples).
template <class Voice, class Drum>
static void testPassThrough (const char* name, const Params& params)
{
    const int n = 4800;
    std::vector<float> got (static_cast<size_t> (n)), ref (static_cast<size_t> (n));
    Params p = params;

    // Our wrapper at 48 kHz...
    stmlib::Random::Seed (0x21);
    {
        Voice v;
        v.prepare (48000.0);
        v.drum().Configure (p.data(), peaks::CONTROL_MODE_FULL);
        v.trigger (1.0f);
        for (int i = 0; i < n; ++i)
            got[static_cast<size_t> (i)] = v.next();
    }

    // ...against the original drum code driven directly.
    stmlib::Random::Seed (0x21);
    {
        Drum reference;
        std::memset (static_cast<void*> (&reference), 0, sizeof (reference));
        reference.Init();
        reference.Configure (p.data(), peaks::CONTROL_MODE_FULL);
        for (int i = 0; i < n; ++i)
        {
            peaks::GateFlags f = i == 0 ? static_cast<peaks::GateFlags> (peaks::GATE_FLAG_RISING | peaks::GATE_FLAG_HIGH)
                                        : static_cast<peaks::GateFlags> (peaks::GATE_FLAG_LOW);
            int16_t s = 0;
            reference.Process (&f, &s, 1);
            ref[static_cast<size_t> (i)] = static_cast<float> (s) / 32768.0f;
        }
    }

    double maxErr = 0;
    for (int i = 2; i < n; ++i)
        maxErr = std::max (maxErr, double (std::fabs (got[static_cast<size_t> (i)] - ref[static_cast<size_t> (i - 2)])));
    std::printf("%s pass-through max error at 48 kHz: %g\n", name, maxErr);
    CHECK (maxErr < 1e-6, "%s: 48 kHz output differs from the original code (error %g)", name, maxErr);
}

int main()
{
    const Params bd { 32768, 39000, 32768, 32768 };
    const Params sd { 32768, 32768, 32768, 32768 };
    const Params hh { 0, 0, 0, 0 };
    const Params fm { 25000, 8192, 32768, 32768 };

    testVoice<BassDrumVoice> ("Bass drum", bd);
    testVoice<SnareDrumVoice> ("Snare drum", sd);
    testVoice<HighHatVoice> ("Hi-hat", hh);
    testVoice<FmDrumVoice> ("FM drum", fm);

    testPassThrough<BassDrumVoice, peaks::BassDrum> ("Bass drum", bd);
    testPassThrough<SnareDrumVoice, peaks::SnareDrum> ("Snare drum", sd);
    testPassThrough<HighHatVoice, peaks::HighHat> ("Hi-hat", hh);
    testPassThrough<FmDrumVoice, peaks::FmDrum> ("FM drum", fm);

    std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
