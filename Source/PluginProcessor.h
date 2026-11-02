#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PeaksVoice.h"

class RidgelineProcessor : public juce::AudioProcessor
{
public:
    RidgelineProcessor();
    ~RidgelineProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Voice order used throughout the plugin.
    enum Voice { BassDrum = 0, SnareDrum, HiHat, FmDrum, NumVoices };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void readParameters();
    void handleNoteOn (int note, float velocity);

    // Per-voice mixer controls.
    struct Strip
    {
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
        std::atomic<float>* note = nullptr;
        juce::SmoothedValue<float> levelSmoothed;
        juce::SmoothedValue<float> panSmoothed;
    };
    std::array<Strip, NumVoices> strips_;

    // The four Peaks drum parameters of each voice (hi-hat has none).
    std::array<std::atomic<float>*, 4> bdParams_ {}, sdParams_ {}, fmParams_ {};
    std::array<uint16_t, 4> bdApplied_ {}, sdApplied_ {}, fmApplied_ {};
    bool forceConfigure_ = true;

    std::atomic<float>* masterLevel_ = nullptr;
    std::atomic<float>* velocityAmount_ = nullptr;
    juce::SmoothedValue<float> masterSmoothed_;

    ridgeline::BassDrumVoice bassDrum_;
    ridgeline::SnareDrumVoice snareDrum_;
    ridgeline::HighHatVoice highHat_;
    ridgeline::FmDrumVoice fmDrum_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RidgelineProcessor)
};
