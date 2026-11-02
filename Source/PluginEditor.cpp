#include "PluginEditor.h"

namespace
{
const juce::Colour kBackground { 0xff17191d };
const juce::Colour kPanel { 0xff22252b };
const juce::Colour kText { 0xffe6e8eb };
const juce::Colour kDimText { 0xff8b929c };

constexpr int kWindowWidth = 900;
constexpr int kWindowHeight = 548;
constexpr int kMargin = 14;
constexpr int kFooterHeight = 28;
constexpr int kTitleHeight = 52;
constexpr int kKnobCell = 124;   // height of one label + knob + value box
constexpr int kNoteHeight = 44;
} // namespace

RidgelineEditor::RidgelineEditor (RidgelineProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p)
{
    auto& bd = addPanel ("BASS DRUM", "C1", juce::Colour (0xffe5603b));
    addKnob (bd, "Frequency", "bd_freq");
    addKnob (bd, "Punch", "bd_punch");
    addKnob (bd, "Tone", "bd_tone");
    addKnob (bd, "Decay", "bd_decay");
    addKnob (bd, "Level", "bd_level");
    addKnob (bd, "Pan", "bd_pan");
    addNote (bd, "bd_note");

    auto& sd = addPanel ("SNARE", "D1", juce::Colour (0xfff2c14e));
    addKnob (sd, "Frequency", "sd_freq");
    addKnob (sd, "Tone", "sd_tone");
    addKnob (sd, "Snappy", "sd_snappy");
    addKnob (sd, "Decay", "sd_decay");
    addKnob (sd, "Level", "sd_level");
    addKnob (sd, "Pan", "sd_pan");
    addNote (sd, "sd_note");

    auto& hh = addPanel ("HI-HAT", "F#1", juce::Colour (0xff5bc0be));
    addKnob (hh, "Level", "hh_level");
    addKnob (hh, "Pan", "hh_pan");
    addNote (hh, "hh_note");

    auto& fm = addPanel ("FM DRUM", "A1", juce::Colour (0xffb07cc6));
    addKnob (fm, "Frequency", "fm_freq");
    addKnob (fm, "FM Amount", "fm_amount");
    addKnob (fm, "Decay", "fm_decay");
    addKnob (fm, "Noise / Drive", "fm_noise");
    addKnob (fm, "Level", "fm_level");
    addKnob (fm, "Pan", "fm_pan");
    addNote (fm, "fm_note");

    auto& master = addPanel ("MASTER", {}, juce::Colour (0xff9aa5b1));
    addKnob (master, "Level", "master_level");
    addKnob (master, "Velocity", "velocity_amount");

    setSize (kWindowWidth, kWindowHeight);
}

RidgelineEditor::~RidgelineEditor() = default;

RidgelineEditor::Panel& RidgelineEditor::addPanel (const juce::String& title, const juce::String& subtitle,
                                                   juce::Colour colour)
{
    auto panel = std::make_unique<Panel>();
    panel->title = title;
    panel->subtitle = subtitle;
    panel->colour = colour;
    panels_.push_back (std::move (panel));
    return *panels_.back();
}

void RidgelineEditor::addKnob (Panel& panel, const juce::String& labelText, const juce::String& paramId)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 18);
    knob->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                      juce::MathConstants<float>::pi * 2.8f, true);
    knob->slider.setColour (juce::Slider::rotarySliderFillColourId, panel.colour);
    knob->slider.setColour (juce::Slider::thumbColourId, kText);
    knob->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3e46));
    knob->slider.setColour (juce::Slider::textBoxTextColourId, kText);
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob->attachment = std::make_unique<Attachment> (processor_.apvts, paramId, knob->slider);

    knob->label.setText (labelText, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setColour (juce::Label::textColourId, kDimText);
    knob->label.setFont (juce::FontOptions (13.0f));

    addAndMakeVisible (knob->slider);
    addAndMakeVisible (knob->label);
    panel.knobs.push_back (std::move (knob));
}

void RidgelineEditor::addNote (Panel& panel, const juce::String& paramId)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setSliderStyle (juce::Slider::IncDecButtons);
    knob->slider.setIncDecButtonsMode (juce::Slider::incDecButtonsNotDraggable);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 24);
    knob->slider.setColour (juce::Slider::textBoxTextColourId, kText);
    knob->slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff17191d));
    knob->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff3a3e46));
    knob->attachment = std::make_unique<Attachment> (processor_.apvts, paramId, knob->slider);

    knob->label.setText ("MIDI note", juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centredLeft);
    knob->label.setColour (juce::Label::textColourId, kDimText);
    knob->label.setFont (juce::FontOptions (13.0f));

    addAndMakeVisible (knob->slider);
    addAndMakeVisible (knob->label);
    panel.note = std::move (knob);
}

void RidgelineEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    for (auto& panel : panels_)
    {
        const auto b = panel->bounds.toFloat();
        g.setColour (kPanel);
        g.fillRoundedRectangle (b, 8.0f);

        g.setColour (panel->colour);
        g.fillRoundedRectangle (b.withHeight (4.0f), 2.0f);

        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (panel->title, panel->bounds.withTrimmedTop (12).withHeight (24).reduced (12, 0),
                    juce::Justification::centredLeft);

        if (panel->title == "HI-HAT")
        {
            g.setColour (kDimText);
            g.setFont (juce::FontOptions (12.0f));
            g.drawFittedText ("The original hi-hat has no tone or decay controls, so only level and pan are available.",
                              panel->bounds.withTrimmedTop (kTitleHeight + kKnobCell + 10).reduced (14, 0).withHeight (60),
                              juce::Justification::topLeft, 4);
        }
    }

    g.setColour (kDimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Drum models from the Mutable Instruments Peaks firmware by Emilie Gillet (MIT licence). "
                "Not affiliated with or endorsed by Mutable Instruments.",
                getLocalBounds().removeFromBottom (kFooterHeight).reduced (kMargin, 0),
                juce::Justification::centredLeft);
}

void RidgelineEditor::resized()
{
    auto area = getLocalBounds().reduced (kMargin);
    area.removeFromBottom (kFooterHeight - kMargin / 2);

    const int gap = 10;
    const int count = (int) panels_.size();
    const int panelWidth = (area.getWidth() - gap * (count - 1)) / count;

    for (int i = 0; i < count; ++i)
    {
        auto& panel = *panels_[(size_t) i];
        panel.bounds = juce::Rectangle<int> (area.getX() + i * (panelWidth + gap), area.getY(),
                                             panelWidth, area.getHeight());

        auto inner = panel.bounds.withTrimmedTop (kTitleHeight).reduced (6, 0);
        const int cellWidth = inner.getWidth() / 2;

        // Knobs are laid out two per row, in the order they were added.
        // The hi-hat and master panels have only mixer knobs, so they sit lower for symmetry.
        for (size_t k = 0; k < panel.knobs.size(); ++k)
        {
            const int row = (int) k / 2;
            const int col = (int) k % 2;
            juce::Rectangle<int> cell (inner.getX() + col * cellWidth, inner.getY() + row * kKnobCell,
                                       cellWidth, kKnobCell);

            panel.knobs[k]->label.setBounds (cell.removeFromTop (16));
            panel.knobs[k]->slider.setBounds (cell);
        }

        if (panel.note != nullptr)
        {
            auto noteArea = juce::Rectangle<int> (panel.bounds.getX() + 10,
                                                  panel.bounds.getBottom() - kNoteHeight - 8,
                                                  panel.bounds.getWidth() - 20, kNoteHeight);
            panel.note->label.setBounds (noteArea.removeFromTop (16));
            panel.note->slider.setBounds (noteArea.removeFromLeft (juce::jmin (noteArea.getWidth(), 120)));
        }
    }
}
