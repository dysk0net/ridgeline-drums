// End-to-end check of the plugin, written for developers (not needed to use it).
//
//  1. Drives the processor directly: MIDI -> audio, per-voice routing, knobs,
//     pan, velocity, state save/restore.
//  2. Loads the *built .vst3 file* the way a DAW would, and checks it makes sound.
//  3. Saves a screenshot of the plugin window.
//
// Build with -DRIDGELINE_BUILD_HOST_CHECK=ON, run:  HostCheck <path-to.vst3> <screenshot.png>

#include <juce_audio_utils/juce_audio_utils.h>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

static int g_checks = 0, g_failures = 0;

#define CHECK(cond, ...)                          \
    do {                                          \
        ++g_checks;                               \
        if (!(cond)) {                            \
            ++g_failures;                         \
            std::printf ("  FAIL: ");             \
            std::printf (__VA_ARGS__);            \
            std::printf ("\n");                   \
        }                                         \
    } while (0)

struct NoteOn { int note; int velocity; int sample; };

static juce::AudioBuffer<float> render (juce::AudioProcessor& p, double sr, int blockSize, int totalSamples,
                                        const std::vector<NoteOn>& notes)
{
    p.setPlayConfigDetails (0, 2, sr, blockSize);
    p.prepareToPlay (sr, blockSize);

    juce::AudioBuffer<float> out (2, totalSamples);
    out.clear();

    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const int n = std::min (blockSize, totalSamples - start);
        juce::AudioBuffer<float> block (2, n);
        block.clear();
        juce::MidiBuffer midi;
        for (auto& e : notes)
            if (e.sample >= start && e.sample < start + n)
                midi.addEvent (juce::MidiMessage::noteOn (1, e.note, (juce::uint8) e.velocity), e.sample - start);

        p.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, ch, 0, n);
    }
    p.releaseResources();
    return out;
}

static float peak (const juce::AudioBuffer<float>& b, int ch, int from, int to)
{
    float p = 0;
    for (int i = from; i < std::min (to, b.getNumSamples()); ++i)
        p = std::max (p, std::abs (b.getSample (ch, i)));
    return p;
}

static double energy (const juce::AudioBuffer<float>& b)
{
    double e = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            e += double (b.getSample (ch, i)) * b.getSample (ch, i);
    return e;
}

static juce::AudioProcessorParameter* findParam (juce::AudioProcessor& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (100) == name)
            return prm;
    return nullptr;
}

// Works on any AudioProcessor, whether created directly or loaded from the .vst3 file.
static void audioTests (juce::AudioProcessor& p, const char* label, double sr)
{
    std::printf ("\n== %s @ %.0f Hz ==\n", label, sr);
    const int total = (int) sr; // 1 second
    const int onset = 1000;

    CHECK (p.acceptsMidi(), "should accept MIDI");

    const struct { const char* name; int note; } voices[] = {
        { "bass drum", 36 }, { "snare", 38 }, { "hi-hat", 42 }, { "FM drum", 45 }
    };
    for (auto& v : voices)
    {
        auto out = render (p, sr, 480, total, { { v.note, 100, onset } });
        const float before = peak (out, 0, 0, onset);
        const float after = peak (out, 0, onset, onset + (int) (0.05 * sr));
        std::printf ("  %-9s note %d: before %.5f, after %.4f, L/R peak %.4f / %.4f\n", v.name, v.note, before, after,
                     peak (out, 0, 0, total), peak (out, 1, 0, total));
        CHECK (before < 1e-6f, "%s: sound before the note", v.name);
        CHECK (after > 0.01f, "%s: silent after note-on (%.5f)", v.name, after);
        CHECK (std::abs (peak (out, 0, 0, total) - peak (out, 1, 0, total)) < 1e-4f, "%s: not centred", v.name);
    }

    // A note nothing is mapped to makes no sound.
    {
        auto out = render (p, sr, 480, total, { { 60, 100, onset } });
        CHECK (peak (out, 0, 0, total) < 1e-6f, "unmapped note 60 made sound");
    }

    // Velocity: soft hit quieter than hard hit.
    {
        auto hard = render (p, sr, 480, total, { { 36, 127, onset } });
        auto soft = render (p, sr, 480, total, { { 36, 40, onset } });
        const float ratio = peak (soft, 0, 0, total) / peak (hard, 0, 0, total);
        std::printf ("  velocity 40 vs 127 peak ratio: %.3f (expect ~%.3f)\n", ratio, 40.0 / 127.0);
        CHECK (std::abs (ratio - 40.0f / 127.0f) < 0.05f, "velocity scaling off (%.3f)", ratio);
    }

    // Block size must not matter (sample-accurate MIDI). Uses the bass drum and FM
    // drum, which are deterministic; the snare draws fresh random noise each time.
    for (int note : { 36, 45 })
    {
        auto a = render (p, sr, 64, total, { { note, 100, onset + 37 } });
        auto b = render (p, sr, 1024, total, { { note, 100, onset + 37 } });
        double maxDiff = 0;
        for (int i = 0; i < total; ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (a.getSample (0, i) - b.getSample (0, i)));
        std::printf ("  note %d, block size 64 vs 1024 max difference: %g\n", note, maxDiff);
        CHECK (maxDiff < 1e-5, "note %d: output depends on block size (diff %g)", note, maxDiff);
    }

    // Knobs change the sound.
    if (auto* decay = findParam (p, "Bass Drum Decay"))
    {
        decay->setValueNotifyingHost (0.1f);
        const double shortE = energy (render (p, sr, 480, total, { { 36, 100, onset } }));
        decay->setValueNotifyingHost (0.9f);
        const double longE = energy (render (p, sr, 480, total, { { 36, 100, onset } }));
        decay->setValueNotifyingHost (decay->getDefaultValue());
        std::printf ("  bass drum energy, decay 10%% vs 90%%: %.2f vs %.2f\n", shortE, longE);
        CHECK (longE > shortE * 1.5, "decay knob has little effect");
    }
    else
        CHECK (false, "parameter 'Bass Drum Decay' not found");

    // Hard-left pan silences the right channel.
    if (auto* pan = findParam (p, "Bass Drum Pan"))
    {
        pan->setValueNotifyingHost (0.0f); // normalised 0 = hard left
        // Let the smoothers settle by rendering a block first.
        auto out = render (p, sr, 480, total, { { 36, 100, onset + 5000 } });
        std::printf ("  hard-left pan, L/R peak: %.4f / %.6f\n", peak (out, 0, 0, total), peak (out, 1, 5000, total));
        CHECK (peak (out, 1, onset + 5000, total) < 1e-4f && peak (out, 0, 0, total) > 0.05f, "pan not working");
        pan->setValueNotifyingHost (pan->getDefaultValue());
    }
    else
        CHECK (false, "parameter 'Bass Drum Pan' not found");

    // Re-mapping the note works.
    if (auto* note = findParam (p, "Snare Note"))
    {
        note->setValueNotifyingHost (50.0f / 127.0f); // note parameter spans 0..127
        auto moved = render (p, sr, 480, total, { { 50, 100, onset } });
        auto old = render (p, sr, 480, total, { { 38, 100, onset } });
        CHECK (peak (moved, 0, 0, total) > 0.05f, "snare did not respond to its new note");
        CHECK (peak (old, 0, 0, total) < 1e-6f, "snare still responds to its old note");
        note->setValueNotifyingHost (38.0f / 127.0f);
    }
    else
        CHECK (false, "parameter 'Snare Note' not found");
}

static void stateTest (const char* label, juce::AudioProcessor& a, juce::AudioProcessor& b)
{
    std::printf ("\n== state save/restore (%s) ==\n", label);
    auto* pa = findParam (a, "Snare Snappy");
    auto* pb = findParam (b, "Snare Snappy");
    if (pa == nullptr || pb == nullptr) { CHECK (false, "Snare Snappy missing"); return; }
    pa->setValueNotifyingHost (0.8125f);
    juce::MemoryBlock state;
    a.getStateInformation (state);
    b.setStateInformation (state.getData(), (int) state.getSize());
    std::printf ("  restored value: %.4f (expected 0.8125), state size %zu bytes\n", pb->getValue(), state.getSize());
    CHECK (std::abs (pb->getValue() - 0.8125f) < 1e-4f, "state not restored");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::String vst3Path = argc > 1 ? argv[1] : "";
    const juce::String pngPath = argc > 2 ? argv[2] : "editor.png";

    // 1. Direct tests of the processor
    {
        RidgelineProcessor proc;
        std::printf ("Parameters: %d\n", (int) proc.getParameters().size());
        CHECK (proc.getParameters().size() == 26, "expected 26 parameters, got %d", (int) proc.getParameters().size());
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            audioTests (proc, "direct", sr);

        RidgelineProcessor other;
        stateTest ("direct", proc, other);

        // 3. Screenshot of the editor
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        if (editor != nullptr)
        {
            editor->setBounds (0, 0, editor->getWidth(), editor->getHeight());
            auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            juce::File png (pngPath);
            png.deleteFile();
            juce::FileOutputStream stream (png);
            juce::PNGImageFormat format;
            if (stream.openedOk() && format.writeImageToStream (image, stream))
                std::printf ("\nSaved editor screenshot: %s (%d x %d)\n", pngPath.toRawUTF8(), image.getWidth(), image.getHeight());
            else
                CHECK (false, "could not write screenshot");
        }
        else
            CHECK (false, "no editor");
    }

    // 2. The real .vst3 file, loaded like a DAW would
    if (vst3Path.isNotEmpty())
    {
        juce::AudioPluginFormatManager formats;
        formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* format : formats.getFormats())
            format->findAllTypesForFile (found, vst3Path);

        std::printf ("\n== loading %s ==\n", vst3Path.toRawUTF8());
        CHECK (found.size() == 1, "expected 1 plugin in the file, found %d", found.size());
        if (found.size() >= 1)
        {
            auto& d = *found[0];
            std::printf ("  name '%s', manufacturer '%s', category '%s', synth=%d, inputs=%d, outputs=%d\n",
                         d.name.toRawUTF8(), d.manufacturerName.toRawUTF8(), d.category.toRawUTF8(), (int) d.isInstrument,
                         d.numInputChannels, d.numOutputChannels);
            CHECK (d.isInstrument, "not flagged as an instrument");

            juce::String error;
            auto a = formats.createPluginInstance (d, 48000.0, 480, error);
            auto b = formats.createPluginInstance (d, 48000.0, 480, error);
            CHECK (a != nullptr && b != nullptr, "could not create instance: %s", error.toRawUTF8());
            if (a != nullptr && b != nullptr)
            {
                std::printf ("  live instance: %d output channels\n", a->getTotalNumOutputChannels());
                CHECK (a->getTotalNumOutputChannels() == 2, "expected 2 output channels");
                // JUCE adds ~2000 hidden MIDI-controller slots to every VST3 instrument, so
                // look for our own parameters by name instead of counting.
                for (auto* name : { "Bass Drum Frequency", "Bass Drum Punch", "Bass Drum Tone", "Bass Drum Decay",
                                    "Snare Frequency", "Snare Tone", "Snare Snappy", "Snare Decay",
                                    "FM Drum Frequency", "FM Drum FM Amount", "FM Drum Decay", "FM Drum Noise/Drive",
                                    "Bass Drum Level", "Snare Level", "Hi-Hat Level", "FM Drum Level",
                                    "Bass Drum Pan", "Snare Pan", "Hi-Hat Pan", "FM Drum Pan",
                                    "Bass Drum Note", "Snare Note", "Hi-Hat Note", "FM Drum Note",
                                    "Master Level", "Velocity Sensitivity" })
                    CHECK (findParam (*a, name) != nullptr, "parameter '%s' missing from the .vst3", name);
                for (double sr : { 44100.0, 48000.0 })
                    audioTests (*a, "real .vst3", sr);
                stateTest ("real .vst3", *a, *b);
            }
        }
    }

    std::printf ("\n%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
