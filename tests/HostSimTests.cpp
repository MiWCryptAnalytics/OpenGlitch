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
    bool provideBpm = true; // some hosts omit tempo from the process context

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        if (provideBpm)
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
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    int sampleIndex = 0;
    int numIn, numOut;

    explicit Harness (bool hosted = true, int numInputs = 2, int numOutputs = 2)
        : buffer (juce::jmax (numInputs, numOutputs), BLOCK),
          numIn (numInputs), numOut (numOutputs)
    {
        if (hosted)
            proc.setPlayHead (&playhead);
        // Real hosts set the play config before preparing; skipping this left
        // getSampleRate() at 0 and once unbounded the transport tick loop.
        proc.setPlayConfigDetails (numIn, numOut, (double) SR, BLOCK);
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

    // Processes one block of `size` samples of the deterministic square-ish
    // 220Hz test tone, appending channel-0/1 data to the given vectors.
    void processOne (int size, std::vector<float>& out, std::vector<float>& in)
    {
        for (int i = 0; i < size; ++i, ++sampleIndex)
        {
            const float s = std::sin (2.0f * juce::MathConstants<float>::pi
                                      * 220.0f * (float) sampleIndex / SR);
            const float v = 0.4f * (s > 0.0f ? 1.0f : -1.0f) + 0.1f * s;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, ch == 0 ? v : -v);
        }
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(),
                                        buffer.getNumChannels(), size);
        for (int i = 0; i < size; ++i)
        {
            in.push_back (slice.getSample (0, i));
            in.push_back (slice.getSample (juce::jmin (1, numOut - 1, slice.getNumChannels() - 1), i));
        }

        proc.processBlock (slice, midi);
        midi.clear();

        for (int i = 0; i < size; ++i)
        {
            out.push_back (slice.getSample (0, i));
            out.push_back (slice.getSample (juce::jmin (1, numOut - 1, slice.getNumChannels() - 1), i));
        }

        if (playhead.playing)
            playhead.ppq += (double) size / SR * playhead.bpm / 60.0;
    }

    // Feeds whole BLOCK-sized buffers for `seconds`.
    std::vector<float> run (double seconds, std::vector<float>* inputOut = nullptr)
    {
        const int blocks = (int) (seconds * SR / BLOCK);
        std::vector<float> out, in;
        for (int b = 0; b < blocks; ++b)
            processOne (BLOCK, out, in);
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

TEST_CASE ("host: mono track (mono in, mono out) still glitches", "[host][buses]")
{
    Harness h (true, 1, 1);
    h.playhead.playing = true;
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, in) > 0.05f);
}

TEST_CASE ("host: mono in to stereo out still glitches", "[host][buses]")
{
    Harness h (true, 1, 2);
    h.playhead.playing = true;
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, in) > 0.05f);
}

TEST_CASE ("host: Ardour lifecycle - processes stopped first, then rolls", "[host][transport]")
{
    Harness h;
    h.run (1.0); // DAWs process constantly; stopped must be clean dry
    std::vector<float> inStopped;
    auto stopped = h.run (0.5, &inStopped);
    REQUIRE (maxDiff (stopped, inStopped) < 0.2f);
    REQUIRE (h.proc.getTransportMode() == OpenGlitchAudioProcessor::hostedStopped);

    h.playhead.playing = true;
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (maxDiff (out, in) > 0.05f);
    REQUIRE (h.proc.getTransportMode() == OpenGlitchAudioProcessor::hostedPlaying);
    REQUIRE (h.proc.getTickCount() > 0);
}

TEST_CASE ("host: irregular block splits keep the groove", "[host][transport]")
{
    Harness h;
    h.playhead.playing = true;
    // Hosts split buffers at automation points and loop edges - including
    // sizes that are not multiples of Heavy's 8-sample SIMD block.
    const int sizes[] = { 512, 96, 40, 256, 12, 480, 128, 4 };
    std::vector<float> out, in;
    int total = 0;
    for (int i = 0; total < Harness::SR * 2; ++i)
    {
        const int size = sizes[i % 8];
        h.processOne (size, out, in);
        total += size;
    }
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, in) > 0.05f);
}

TEST_CASE ("host: session starting mid-timeline still locks", "[host][transport]")
{
    Harness h;
    h.playhead.playing = true;
    h.playhead.ppq = 12345.0; // deep into a long session
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (maxDiff (out, in) > 0.05f);
    REQUIRE (h.proc.getCurrentStep() >= 0);
    REQUIRE (h.proc.getCurrentStep() <= 15);
}

TEST_CASE ("host: missing tempo info falls back to 120 and still glitches", "[host][transport]")
{
    Harness h;
    h.playhead.provideBpm = false;
    h.playhead.playing = true;
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, in) > 0.05f);
}

TEST_CASE ("host: bypass engages and releases cleanly mid-playback", "[host][transport]")
{
    Harness h;
    h.playhead.playing = true;
    h.run (1.0);

    h.setParam ("bypass", 1.0f);
    h.run (0.3);
    std::vector<float> inBypassed;
    auto bypassed = h.run (0.5, &inBypassed);
    REQUIRE (maxDiff (bypassed, inBypassed) < 1e-6f);

    h.setParam ("bypass", 0.0f);
    h.run (0.3);
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (maxDiff (out, in) > 0.05f); // effects come back
}

// Flush the pattern system's async updater the way a DAW's message loop would.
static void pumpMessages (int ms = 60)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

TEST_CASE ("host: clearing the grid mid-playback goes dry", "[host][pattern]")
{
    Harness h;
    h.playhead.playing = true;

    std::vector<float> in;
    auto out = h.run (1.0, &in);
    REQUIRE (maxDiff (out, in) > 0.05f); // demo pattern audibly glitches first

    h.setAllSteps (0.0f); // the CLEAR button
    pumpMessages();
    h.run (0.4); // settle crossfades and tails

    std::vector<float> in2;
    auto out2 = h.run (1.0, &in2);
    const float ratio = rms (out2) / rms (in2);
    REQUIRE (maxDiff (out2, in2) < 0.2f);
    REQUIRE (ratio > 0.85f);
    REQUIRE (ratio < 1.1f);
}

TEST_CASE ("host: painting the grid mid-playback engages effects", "[host][pattern]")
{
    Harness h;
    h.playhead.playing = true;
    h.setAllSteps (0.0f);
    pumpMessages();
    h.run (0.5);
    std::vector<float> inDry;
    auto dry = h.run (1.0, &inDry);
    REQUIRE (maxDiff (dry, inDry) < 0.2f); // empty grid passes clean

    h.setAllSteps (7.0f); // paint gater everywhere
    pumpMessages();
    h.run (0.2);
    std::vector<float> in2;
    auto out2 = h.run (1.0, &in2);
    REQUIRE (maxDiff (out2, in2) > 0.05f); // now audibly chopped
}

TEST_CASE ("host: switching pattern slots changes the audible grid", "[host][pattern]")
{
    Harness h;
    h.playhead.playing = true;

    // Slot A: silence the grid. Slot B: paint gaters.
    h.setAllSteps (0.0f);
    pumpMessages();
    h.setParam ("pattern_select", 1.0f);
    pumpMessages();
    h.setAllSteps (7.0f);
    pumpMessages();
    REQUIRE (h.proc.getActivePattern() == 1);

    // Back to A: the step parameters must reload as the cleared grid.
    h.setParam ("pattern_select", 0.0f);
    pumpMessages();
    REQUIRE (h.proc.getActivePattern() == 0);
    for (int i = 1; i <= 16; ++i)
        REQUIRE (juce::exactlyEqual (h.proc.apvts.getRawParameterValue ("step_" + juce::String (i))->load(), 0.0f));

    h.run (0.4);
    std::vector<float> inA;
    auto outA = h.run (1.0, &inA);
    REQUIRE (maxDiff (outA, inA) < 0.2f); // A plays dry

    h.setParam ("pattern_select", 1.0f);
    pumpMessages();
    for (int i = 1; i <= 16; ++i)
        REQUIRE (juce::exactlyEqual (h.proc.apvts.getRawParameterValue ("step_" + juce::String (i))->load(), 7.0f));
    h.run (0.2);
    std::vector<float> inB;
    auto outB = h.run (1.0, &inB);
    REQUIRE (maxDiff (outB, inB) > 0.05f); // B glitches
}

TEST_CASE ("host: cleared grid survives a state save/load round-trip", "[host][pattern][state]")
{
    juce::MemoryBlock blob;
    {
        Harness h;
        h.playhead.playing = true;
        h.run (0.2);
        h.setAllSteps (0.0f);
        pumpMessages();
        h.proc.getStateInformation (blob);
    }

    Harness h2;
    h2.proc.setStateInformation (blob.getData(), (int) blob.getSize());
    pumpMessages();
    for (int i = 1; i <= 16; ++i)
        REQUIRE (juce::exactlyEqual (h2.proc.apvts.getRawParameterValue ("step_" + juce::String (i))->load(), 0.0f));

    h2.playhead.playing = true;
    h2.run (0.4);
    std::vector<float> in;
    auto out = h2.run (1.0, &in);
    REQUIRE (maxDiff (out, in) < 0.2f); // restored session must NOT play the demo
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

TEST_CASE ("host: tie spans run an effect across beats", "[host][span]")
{
    auto runGrid = [] (std::initializer_list<std::pair<int, float>> cells)
    {
        Harness h;
        h.playhead.playing = true;
        h.setAllSteps (0.0f);
        for (const auto& [step, v] : cells)
            h.setParam ("step_" + juce::String (step), v);
        h.run (0.1);
        return h.run (2.0);
    };

    auto dry = runGrid ({});
    auto spanned = runGrid ({ { 1, 1.0f }, { 2, 10.0f }, { 3, 10.0f }, { 4, 10.0f } });
    auto single = runGrid ({ { 1, 1.0f } });
    REQUIRE (maxDiff (spanned, dry) > 0.05f);
    REQUIRE (maxDiff (spanned, single) > 0.02f);
}

TEST_CASE ("host: per-effect output strip is dispatched with the trigger", "[host][post]")
{
    auto channelRms = [] (const std::vector<float>& v, int ch)
    {
        double s = 0;
        long n = 0;
        for (size_t i = (size_t) ch; i < v.size(); i += 2) { s += (double) v[i] * v[i]; ++n; }
        return (float) std::sqrt (s / (double) n);
    };

    Harness h;
    h.playhead.playing = true;
    h.setAllSteps (3.0f);            // retrigger everywhere
    h.setParam ("fx3_pan", -1.0f);   // its strip pans hard left
    h.run (0.5);
    auto out = h.run (1.5);
    REQUIRE (channelRms (out, 0) > channelRms (out, 1) * 2.0f);
}

TEST_CASE ("host: FX dice with a seed is reproducible", "[host][dice]")
{
    auto rollTwice = [] (float seed)
    {
        Harness h;
        h.setParam ("seq_seed", seed);
        h.proc.randomizeFxKnobs();
        std::vector<float> values;
        for (auto* id : { "fx_mod_freq", "fx_gate_rate", "fx3_pan", "fx7_post_freq" })
            values.push_back (h.proc.apvts.getRawParameterValue (id)->load());
        return values;
    };

    auto a = rollTwice (42.0f);
    auto b = rollTwice (42.0f);
    auto c = rollTwice (43.0f);
    REQUIRE (a == b);
    REQUIRE (a != c);
}

TEST_CASE ("host: shift rotates the pattern within its length", "[host][shift]")
{
    Harness h;
    h.setAllSteps (0.0f);
    h.setParam ("step_1", 3.0f);
    h.setParam ("seq_length", 4.0f);
    h.proc.shiftActivePattern (1);
    REQUIRE (juce::exactlyEqual (h.proc.apvts.getRawParameterValue ("step_2")->load(), 3.0f));
    h.proc.shiftActivePattern (1);
    h.proc.shiftActivePattern (1);
    h.proc.shiftActivePattern (1); // full cycle within length 4: back to the start
    REQUIRE (juce::exactlyEqual (h.proc.apvts.getRawParameterValue ("step_1")->load(), 3.0f));
    REQUIRE (juce::exactlyEqual (h.proc.apvts.getRawParameterValue ("step_2")->load(), 0.0f));
}

TEST_CASE ("host: 16 pattern banks, midi note 51 selects the last", "[host][pattern]")
{
    Harness h;
    h.playhead.playing = true;
    h.midi.addEvent (juce::MidiMessage::noteOn (1, 51, (juce::uint8) 100), 0);
    h.run (0.05);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    REQUIRE (h.proc.getActivePattern() == 15);
}

#include "PluginEditor.h"

TEST_CASE ("matrix gestures: paint, span-drag, stretch, toggle, erase", "[host][ui]")
{
    Harness h;
    StepMatrix matrix (h.proc.apvts);
    auto step = [&] (int i)
    {
        return (int) std::lround (h.proc.apvts.getRawParameterValue ("step_" + juce::String (i))->load());
    };
    for (int i = 1; i <= 16; ++i)
        h.setParam ("step_" + juce::String (i), 0.0f);

    // Plain click paints; clicking again toggles it off.
    matrix.handlePress (4, 2); // retrigger at step 5
    matrix.handleRelease();
    REQUIRE (step (5) == 3);
    matrix.handlePress (4, 2);
    matrix.handleRelease();
    REQUIRE (step (5) == 0);

    // Press empty and drag right: trigger plus ties.
    matrix.handlePress (4, 2);
    matrix.handleDrag (5, 2);
    matrix.handleDrag (6, 2);
    matrix.handleRelease();
    REQUIRE (step (5) == 3);
    REQUIRE (step (6) == 10);
    REQUIRE (step (7) == 10);

    // Press the EXISTING trigger and drag right: stretches, must NOT clear.
    matrix.handlePress (4, 2);
    matrix.handleDrag (8, 2); // fast flick skipping columns
    matrix.handleRelease();
    REQUIRE (step (5) == 3);
    for (int i = 6; i <= 9; ++i)
        REQUIRE (step (i) == 10);

    // Wobble within the pressed cell must not cancel a pending toggle-off.
    matrix.handlePress (4, 2);
    matrix.handleDrag (4, 2);
    matrix.handleRelease();
    REQUIRE (step (5) == 0); // cleared: it was a click, not a drag

    // Row change mid-drag starts a new block on the other row.
    matrix.handlePress (0, 2);
    matrix.handleDrag (1, 2);
    matrix.handleDrag (2, 6); // jump to gater row
    matrix.handleDrag (3, 6);
    matrix.handleRelease();
    REQUIRE (step (1) == 3);
    REQUIRE (step (2) == 10);
    REQUIRE (step (3) == 7);
    REQUIRE (step (4) == 10);

    // Erase clears a column regardless of content.
    matrix.handleErase (2);
    REQUIRE (step (3) == 0);
}
