// Ridgeline Drums - voice engine
//
// Wraps one of the Peaks drum models (MIT licensed, (c) Emilie Gillet, see
// THIRD_PARTY_NOTICES.md) so it can run inside a plugin host.
//
// The Peaks firmware is written for a fixed 48 kHz sample rate. This class runs
// the drum at 48 kHz and converts to whatever rate the host uses with a 4-point
// Catmull-Rom interpolator. At 48 kHz the conversion is an exact pass-through.
//
// This file deliberately has no JUCE dependency so it can be tested on its own.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "msvc_compat.h" // lets Microsoft's compiler read the Peaks headers

#include "peaks/drums/bass_drum.h"
#include "peaks/drums/fm_drum.h"
#include "peaks/drums/high_hat.h"
#include "peaks/drums/snare_drum.h"

#ifdef _MSC_VER
#undef __attribute__ // only needed while the Peaks headers above are parsed
#endif

namespace ridgeline
{

constexpr double kEngineRate = 48000.0;

// Convert a normalised 0..1 value to the 0..65535 range the Peaks code uses.
inline uint16_t toPeaksParam (float v)
{
    const float clamped = std::min (1.0f, std::max (0.0f, v));
    return static_cast<uint16_t> (clamped * 65535.0f + 0.5f);
}

template <class Drum>
class ResampledVoice
{
public:
    ResampledVoice() { reset(); }

    // Call before playback starts, and whenever the host sample rate changes.
    // Resets the drum, so parameters must be re-applied afterwards.
    void prepare (double hostSampleRate)
    {
        step_ = kEngineRate / (hostSampleRate > 0.0 ? hostSampleRate : kEngineRate);
        reset();
    }

    void reset()
    {
        // The original firmware relies on zeroed static storage (the hi-hat's
        // oscillator phases are never explicitly initialised), so start from
        // zeroed memory here too instead of whatever the heap contains.
        std::memset (static_cast<void*> (&drum_), 0, sizeof (drum_));
        drum_.Init();
        frac_ = 0.0;
        for (float& h : history_)
            h = 0.0f;
        pending_ = false;
        gain_ = 1.0f;
        pendingGain_ = 1.0f;
    }

    // Start the drum on the next engine sample. gain scales this hit (velocity).
    void trigger (float gain)
    {
        pending_ = true;
        pendingGain_ = gain;
    }

    // One output sample at the host sample rate, in the range -1..1.
    float next()
    {
        frac_ += step_;
        const int advances = static_cast<int> (frac_);
        frac_ -= advances;
        for (int i = 0; i < advances; ++i)
            advance();

        return cubic (history_[0], history_[1], history_[2], history_[3],
                      static_cast<float> (frac_))
               * gain_;
    }

    Drum& drum() { return drum_; }

private:
    void advance()
    {
        peaks::GateFlags flags = peaks::GATE_FLAG_LOW;
        if (pending_)
        {
            flags = static_cast<peaks::GateFlags> (peaks::GATE_FLAG_RISING | peaks::GATE_FLAG_HIGH);
            gain_ = pendingGain_;
            pending_ = false;
        }

        int16_t sample = 0;
        drum_.Process (&flags, &sample, 1);

        history_[0] = history_[1];
        history_[1] = history_[2];
        history_[2] = history_[3];
        history_[3] = static_cast<float> (sample) * (1.0f / 32768.0f);
    }

    static float cubic (float y0, float y1, float y2, float y3, float t)
    {
        const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float b = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c = -0.5f * y0 + 0.5f * y2;
        return ((a * t + b) * t + c) * t + y1;
    }

    Drum drum_;
    double step_ = 1.0;
    double frac_ = 0.0;
    float history_[4] = { 0, 0, 0, 0 };
    bool pending_ = false;
    float pendingGain_ = 1.0f;
    float gain_ = 1.0f;
};

using BassDrumVoice  = ResampledVoice<peaks::BassDrum>;
using SnareDrumVoice = ResampledVoice<peaks::SnareDrum>;
using HighHatVoice   = ResampledVoice<peaks::HighHat>;
using FmDrumVoice    = ResampledVoice<peaks::FmDrum>;

} // namespace ridgeline
