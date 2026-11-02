#pragma once

#include <memory>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

class RidgelineEditor : public juce::AudioProcessorEditor
{
public:
    explicit RidgelineEditor (RidgelineProcessor&);
    ~RidgelineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<Attachment> attachment;
    };

    struct Panel
    {
        juce::String title;
        juce::String subtitle;
        juce::Colour colour;
        std::vector<std::unique_ptr<Knob>> knobs;
        std::unique_ptr<Knob> note; // bar-style MIDI note selector (null for the master panel)
        juce::Rectangle<int> bounds;
    };

    Panel& addPanel (const juce::String& title, const juce::String& subtitle, juce::Colour colour);
    void addKnob (Panel& panel, const juce::String& label, const juce::String& paramId);
    void addNote (Panel& panel, const juce::String& paramId);

    RidgelineProcessor& processor_;
    std::vector<std::unique_ptr<Panel>> panels_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RidgelineEditor)
};
