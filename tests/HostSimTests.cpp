// Host-simulation tests: drive the full JUCE processor with a fake
// AudioPlayHead (transport, tempo, ppq) the way a DAW does. This is the layer
// pluginval and the DSP tests both miss.
#include <catch2/catch_test_macros.hpp>
#include <set>

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "TestHelpers.h"

using testutil::allFinite;
using testutil::maxDiff;
using testutil::rms;

namespace
{
struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        return info;
    }
};

struct Harness
{
    static constexpr int SR = 48000, BLOCK = 512;

    juce::ScopedJuceInitialiser_GUI juceInit;
    OpenGlitchAudioProcessor proc;
    FakePlayHead playhead;
    juce::AudioBuffer<float> buffer { 2, BLOCK };
    juce::MidiBuffer midi;
    int sampleIndex = 0;

    explicit Harness (bool hosted = true)
    {
        if (hosted)
            proc.setPlayHead (&playhead);
        // Real hosts set the play config before preparing; skipping this left
        // getSampleRate() at 0 and once unbounded the transport tick loop.
        proc.setPlayConfigDetails (2, 2, (double) SR, BLOCK);
        proc.prepareToPlay ((double) SR, BLOCK);
    }

    void setParam (const juce::String& id, float value)
    {
        auto* p = proc.apvts.getParameter (id);
        REQUIRE (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void setAllSteps (float v)
    {
        for (int i = 1; i <= 16; ++i)
            setParam ("step_" + juce::String (i), v);
    }

    // Feeds a deterministic square-ish 220Hz tone (rich in harmonics so
    // filters are audible); returns interleaved output, optionally input.
    std::vector<float> run (double seconds, std::vector<float>* inputOut = nullptr)
    {
        const int blocks = (int) (seconds * SR / BLOCK);
        std::vector<float> out, in;
        out.reserve ((size_t) blocks * BLOCK * 2);
        in.reserve ((size_t) blocks * BLOCK * 2);

        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < BLOCK; ++i, ++sampleIndex)
            {
                const float s = std::sin (2.0f * juce::MathConstants<float>::pi
                                          * 220.0f * (float) sampleIndex / SR);
                const float v = 0.4f * (s > 0.0f ? 1.0f : -1.0f) + 0.1f * s;
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, -v);
            }
            for (int i = 0; i < BLOCK; ++i)
            {
                in.push_back (buffer.getSample (0, i));
                in.push_back (buffer.getSample (1, i));
            }

            proc.processBlock (buffer, midi);
            midi.clear();

            for (int i = 0; i < BLOCK; ++i)
            {
                out.push_back (buffer.getSample (0, i));
                out.push_back (buffer.getSample (1, i));
            }

            if (playhead.playing)
                playhead.ppq += (double) BLOCK / SR * playhead.bpm / 60.0;
        }
        if (inputOut != nullptr)
            *inputOut = in;
        return out;
    }
};
} // namespace

TEST_CASE ("host: standalone free-run glitches the demo pattern", "[host]")
{
    Harness h (false);
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, in) > 0.05f);
    REQUIRE (h.proc.getCurrentStep() >= 0);
}

TEST_CASE ("host: playing transport glitches and cycles steps", "[host]")
{
    Harness h;
    h.playhead.playing = true;
    std::vector<float> in, out;
    std::set<int> steps;
    for (int chunk = 0; chunk < 21; ++chunk)
    {
        std::vector<float> i2;
        auto o2 = h.run (0.1, &i2);
        out.insert (out.end(), o2.begin(), o2.end());
        in.insert (in.end(), i2.begin(), i2.end());
        steps.insert (h.proc.getCurrentStep());
    }
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, in) > 0.05f);
    REQUIRE (steps.size() >= 8);
    REQUIRE (*steps.begin() >= 0);
}

TEST_CASE ("host: step index follows ppq after relocate", "[host]")
{
    Harness h;
    h.playhead.playing = true;
    h.playhead.ppq = 1.0; // beat 2 == 16th number 4
    h.run (0.02);
    const int step = h.proc.getCurrentStep();
    REQUIRE (step >= 4);
    REQUIRE (step <= 5);
}

TEST_CASE ("host: transport stop settles to clean dry", "[host]")
{
    Harness h;
    h.playhead.playing = true;
    h.run (1.0);
    h.playhead.playing = false;
    h.run (0.3);
    std::vector<float> in;
    auto out = h.run (0.5, &in);
    REQUIRE (allFinite (out));
    const float ratio = rms (out) / rms (in);
    REQUIRE (ratio > 0.85f);
    REQUIRE (ratio < 1.1f);
    REQUIRE (h.proc.getCurrentStep() < 0);
}

TEST_CASE ("host: bypass is bit-transparent while playing", "[host]")
{
    Harness h;
    h.playhead.playing = true;
    h.setParam ("bypass", 1.0f);
    h.run (0.3);
    std::vector<float> in;
    auto out = h.run (0.5, &in);
    REQUIRE (maxDiff (out, in) < 1e-6f);
}

TEST_CASE ("host: midi note 37 selects pattern B", "[host][midi]")
{
    Harness h;
    h.playhead.playing = true;
    h.midi.addEvent (juce::MidiMessage::noteOn (1, 37, (juce::uint8) 100), 0);
    h.run (0.05);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    REQUIRE (h.proc.getActivePattern() == 1);
}

TEST_CASE ("host: highpass mode thins the tone through the processor", "[host][filter]")
{
    Harness h;
    h.playhead.playing = true;
    h.setAllSteps (0.0f);
    h.setParam ("master_filter_type", 1.0f);
    h.setParam ("master_lowpass", 8000.0f);
    h.run (0.5);
    std::vector<float> in;
    auto out = h.run (0.5, &in);
    REQUIRE (rms (out) < rms (in) * 0.5f);
}

TEST_CASE ("host: LFO on the filter audibly animates a dry grid", "[host][lfo]")
{
    Harness still;
    still.playhead.playing = true;
    still.setAllSteps (0.0f);
    auto ref = still.run (1.0);

    Harness h;
    h.playhead.playing = true;
    h.setAllSteps (0.0f);
    h.setParam ("lfo1_target", 1.0f); // filter freq
    h.setParam ("lfo1_depth", 1.0f);
    h.setParam ("lfo1_rate", 2.0f); // 1/4
    auto out = h.run (1.0);
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, ref) > 0.05f);
}
