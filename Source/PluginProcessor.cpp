#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatParam = juce::AudioParameterFloat;
using IntParam = juce::AudioParameterInt;

juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

// Peaks' frequency knobs on the bass and snare drums span +/- 7 semitones.
juce::String semitoneText (float v, int)
{
    const float st = (v - 0.5f) * 14.0f;
    return juce::String (st, 1) + " st";
}

juce::String panText (float v, int)
{
    const int p = juce::roundToInt (v * 100.0f);
    if (p == 0)
        return "C";
    return (p < 0 ? "L " : "R ") + juce::String (std::abs (p));
}

juce::String noteText (int note, int) { return juce::MidiMessage::getMidiNoteName (note, true, true, 3); }

// Accepts "36", "C1", "F#1", "Bb0" ... (middle C = C3, as in most DAWs).
int noteFromText (const juce::String& text)
{
    auto t = text.trim();
    if (t.containsOnly ("0123456789"))
        return juce::jlimit (0, 127, t.getIntValue());

    static const int semis[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const auto letter = juce::CharacterFunctions::toUpperCase (t[0]);
    if (letter < 'A' || letter > 'G')
        return 36;

    int semitone = semis[letter - 'A'];
    int i = 1;
    if (t[i] == '#') { ++semitone; ++i; }
    else if (t[i] == 'b' || t[i] == 'B') { --semitone; ++i; }
    const int octave = t.substring (i).getIntValue();
    return juce::jlimit (0, 127, (octave + 2) * 12 + semitone);
}

std::unique_ptr<FloatParam> makeFloat (const juce::String& id, const juce::String& name, float def,
                                       std::function<juce::String (float, int)> text = percentText,
                                       float lo = 0.0f, float hi = 1.0f)
{
    return std::make_unique<FloatParam> (
        juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (lo, hi), def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text)));
}

std::unique_ptr<IntParam> makeNote (const juce::String& id, const juce::String& name, int def)
{
    return std::make_unique<IntParam> (
        juce::ParameterID { id, 1 }, name, 0, 127, def,
        juce::AudioParameterIntAttributes()
            .withStringFromValueFunction (noteText)
            .withValueFromStringFunction (noteFromText));
}

// level / pan / note controls shared by every voice
void addStrip (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& prefix,
               const juce::String& label, float defaultLevel, int defaultNote)
{
    layout.add (makeFloat (prefix + "_level", label + " Level", defaultLevel));
    layout.add (makeFloat (prefix + "_pan", label + " Pan", 0.0f, panText, -1.0f, 1.0f));
    layout.add (makeNote (prefix + "_note", label + " Note", defaultNote));
}
} // namespace

RidgelineProcessor::RidgelineProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    const char* prefixes[NumVoices] = { "bd", "sd", "hh", "fm" };
    for (int i = 0; i < NumVoices; ++i)
    {
        const juce::String p (prefixes[i]);
        strips_[(size_t) i].level = apvts.getRawParameterValue (p + "_level");
        strips_[(size_t) i].pan = apvts.getRawParameterValue (p + "_pan");
        strips_[(size_t) i].note = apvts.getRawParameterValue (p + "_note");
    }

    const char* bd[4] = { "bd_freq", "bd_punch", "bd_tone", "bd_decay" };
    const char* sd[4] = { "sd_freq", "sd_tone", "sd_snappy", "sd_decay" };
    const char* fm[4] = { "fm_freq", "fm_amount", "fm_decay", "fm_noise" };
    for (size_t i = 0; i < 4; ++i)
    {
        bdParams_[i] = apvts.getRawParameterValue (bd[i]);
        sdParams_[i] = apvts.getRawParameterValue (sd[i]);
        fmParams_[i] = apvts.getRawParameterValue (fm[i]);
    }

    masterLevel_ = apvts.getRawParameterValue ("master_level");
    velocityAmount_ = apvts.getRawParameterValue ("velocity_amount");
}

juce::AudioProcessorValueTreeState::ParameterLayout RidgelineProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Bass drum: frequency / punch / tone / decay
    layout.add (makeFloat ("bd_freq", "Bass Drum Frequency", 0.5f, semitoneText));
    layout.add (makeFloat ("bd_punch", "Bass Drum Punch", 0.6f));
    layout.add (makeFloat ("bd_tone", "Bass Drum Tone", 0.5f));
    layout.add (makeFloat ("bd_decay", "Bass Drum Decay", 0.5f));
    addStrip (layout, "bd", "Bass Drum", 0.8f, 36);

    // Snare drum: frequency / tone / snappy / decay
    layout.add (makeFloat ("sd_freq", "Snare Frequency", 0.5f, semitoneText));
    layout.add (makeFloat ("sd_tone", "Snare Tone", 0.5f));
    layout.add (makeFloat ("sd_snappy", "Snare Snappy", 0.5f));
    layout.add (makeFloat ("sd_decay", "Snare Decay", 0.5f));
    addStrip (layout, "sd", "Snare", 0.7f, 38);

    // Hi-hat has no tweakable parameters in the original firmware.
    addStrip (layout, "hh", "Hi-Hat", 0.6f, 42);

    // FM drum: frequency / FM amount / decay / noise (below 50 % = overdrive)
    layout.add (makeFloat ("fm_freq", "FM Drum Frequency", 0.38f));
    layout.add (makeFloat ("fm_amount", "FM Drum FM Amount", 0.125f));
    layout.add (makeFloat ("fm_decay", "FM Drum Decay", 0.5f));
    layout.add (makeFloat ("fm_noise", "FM Drum Noise/Drive", 0.5f));
    addStrip (layout, "fm", "FM Drum", 0.5f, 45);

    layout.add (makeFloat ("master_level", "Master Level", 0.8f));
    layout.add (makeFloat ("velocity_amount", "Velocity Sensitivity", 1.0f));

    return layout;
}

void RidgelineProcessor::prepareToPlay (double sampleRate, int)
{
    bassDrum_.prepare (sampleRate);
    snareDrum_.prepare (sampleRate);
    highHat_.prepare (sampleRate);
    fmDrum_.prepare (sampleRate);
    forceConfigure_ = true;

    for (auto& s : strips_)
    {
        s.levelSmoothed.reset (sampleRate, 0.02);
        s.panSmoothed.reset (sampleRate, 0.02);
        s.levelSmoothed.setCurrentAndTargetValue (s.level->load());
        s.panSmoothed.setCurrentAndTargetValue (s.pan->load());
    }
    masterSmoothed_.reset (sampleRate, 0.02);
    masterSmoothed_.setCurrentAndTargetValue (masterLevel_->load());
}

bool RidgelineProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && layouts.getMainInputChannelSet().isDisabled();
}

void RidgelineProcessor::readParameters()
{
    auto apply = [this] (auto& voice, const std::array<std::atomic<float>*, 4>& params,
                         std::array<uint16_t, 4>& applied)
    {
        uint16_t now[4];
        bool changed = forceConfigure_;
        for (size_t i = 0; i < 4; ++i)
        {
            now[i] = ridgeline::toPeaksParam (params[i]->load());
            changed = changed || now[i] != applied[i];
        }
        if (changed)
        {
            std::copy (now, now + 4, applied.begin());
            voice.drum().Configure (now, peaks::CONTROL_MODE_FULL);
        }
    };

    apply (bassDrum_, bdParams_, bdApplied_);
    apply (snareDrum_, sdParams_, sdApplied_);
    apply (fmDrum_, fmParams_, fmApplied_);
    forceConfigure_ = false;

    for (auto& s : strips_)
    {
        s.levelSmoothed.setTargetValue (s.level->load());
        s.panSmoothed.setTargetValue (s.pan->load());
    }
    masterSmoothed_.setTargetValue (masterLevel_->load());
}

void RidgelineProcessor::handleNoteOn (int note, float velocity)
{
    const float amount = velocityAmount_->load();
    const float gain = 1.0f - amount * (1.0f - juce::jlimit (0.0f, 1.0f, velocity));

    for (int i = 0; i < NumVoices; ++i)
    {
        if (juce::roundToInt (strips_[(size_t) i].note->load()) != note)
            continue;

        switch (i)
        {
            case BassDrum:  bassDrum_.trigger (gain); break;
            case SnareDrum: snareDrum_.trigger (gain); break;
            case HiHat:     highHat_.trigger (gain); break;
            case FmDrum:    fmDrum_.trigger (gain); break;
            default: break;
        }
    }
}

void RidgelineProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    buffer.clear();

    readParameters();

    float* left = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    auto midiIt = midi.begin();

    for (int n = 0; n < numSamples; ++n)
    {
        while (midiIt != midi.end() && (*midiIt).samplePosition <= n)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn())
                handleNoteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            ++midiIt;
        }

        const float voices[NumVoices] = { bassDrum_.next(), snareDrum_.next(), highHat_.next(), fmDrum_.next() };

        float l = 0.0f, r = 0.0f;
        for (size_t v = 0; v < (size_t) NumVoices; ++v)
        {
            const float level = strips_[v].levelSmoothed.getNextValue();
            const float pan = strips_[v].panSmoothed.getNextValue();
            const float angle = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f; // constant-power pan
            const float sample = voices[v] * level;
            l += sample * std::cos (angle);
            r += sample * std::sin (angle);
        }

        const float master = masterSmoothed_.getNextValue();
        l = juce::jlimit (-1.0f, 1.0f, l * master);
        r = juce::jlimit (-1.0f, 1.0f, r * master);

        if (right != nullptr)
        {
            left[n] = l;
            right[n] = r;
        }
        else
        {
            left[n] = 0.5f * (l + r);
        }
    }
}

juce::AudioProcessorEditor* RidgelineProcessor::createEditor()
{
    return new RidgelineEditor (*this);
}

void RidgelineProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RidgelineProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RidgelineProcessor();
}
