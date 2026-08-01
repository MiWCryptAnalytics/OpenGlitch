// Renders short demo WAVs of the engine — the whole demo pattern plus each
// effect soloed — for A/B listening against the original dBlue Glitch 1.3 in
// a DAW. Drives the Heavy context directly through the test harness.
//
//   OpenGlitchRender [output-dir]     (default: renders/ under the cwd)
#include <functional>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "TestHelpers.h"

using testutil::HeavyHarness;

namespace
{
std::vector<float> renderFree (const std::function<void (HeavyHarness&)>& setup, double seconds)
{
    HeavyHarness h;
    setup (h);
    h.set ("host_playing", 1.0f);
    return h.run (seconds);
}

// One bar of manually ticked steps, so single-step patterns land exactly.
std::vector<float> renderTicked (const std::function<void (HeavyHarness&)>& setup, int bars)
{
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    setup (h);
    h.run (0.1);
    std::vector<float> out;
    for (int s = 0; s < bars * 16; ++s)
    {
        h.set ("glitch_spansteps", 1.0f);
        h.tick (s % 16);
        auto o = h.run (0.125);
        out.insert (out.end(), o.begin(), o.end());
    }
    return out;
}

bool writeWav (const juce::File& file, const std::vector<float>& interleaved)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    if (stream == nullptr)
        return false;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.get(), (double) testutil::SR, 2, 24, {}, 0));
    if (writer == nullptr)
        return false;
    stream.release(); // the writer owns it now

    const int numSamples = (int) (interleaved.size() / 2);
    juce::AudioBuffer<float> buffer (2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        buffer.setSample (0, i, interleaved[(size_t) i * 2]);
        buffer.setSample (1, i, interleaved[(size_t) i * 2 + 1]);
    }
    return writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const auto outDir = juce::File::getCurrentWorkingDirectory()
                            .getChildFile (argc > 1 ? argv[1] : "renders");
    outDir.createDirectory();

    struct Scene
    {
        const char* name;
        std::function<std::vector<float>()> render;
    };

    auto solo = [] (float fx, std::function<void (HeavyHarness&)> extra = {})
    {
        return [fx, extra] {
            return renderFree ([fx, extra] (HeavyHarness& h)
            {
                h.setAllSteps (fx);
                if (extra)
                    extra (h);
            }, 4.0);
        };
    };

    const Scene scenes[] = {
        { "demo", [] { return renderFree ([] (HeavyHarness&) {}, 4.0); } },
        { "fx1_tapestop", solo (1.0f) },
        { "fx2_modulator", solo (2.0f) },
        { "fx3_retrigger", solo (3.0f) },
        { "fx4_shuffler", solo (4.0f) },
        { "fx5_reverser", solo (5.0f) },
        { "fx6_crusher", solo (6.0f) },
        { "fx7_gater", solo (7.0f) },
        { "fx8_delay", solo (8.0f) },
        { "fx9_stretcher", solo (9.0f) },
        // Scenes exercising the 1.0 parity fixes:
        { "tapestop_speed01", solo (1.0f, [] (HeavyHarness& h) { h.set ("fx_tapestop_speed", 0.1f); }) },
        { "retrig_pitch2_rate1", solo (3.0f, [] (HeavyHarness& h)
              { h.set ("fx_retrigger_rate", 1.0f); h.set ("fx_retrigger_pitch", 2.0f); }) },
        { "crush_bits2", solo (6.0f, [] (HeavyHarness& h)
              { h.set ("fx_crush_level", 2.0f); h.set ("fx_crush_drive", 1.0f); }) },
        { "reverser_lr_split", solo (5.0f, [] (HeavyHarness& h)
              { h.set ("fx_rev_left", 1.0f); h.set ("fx_rev_right", 0.0f); }) },
        { "delay_ringout", [] { return renderTicked ([] (HeavyHarness& h)
              { h.setAllSteps (0.0f); h.set ("step_1", 8.0f); h.set ("fx_delay_feedback", 0.7f); }, 2); } },
    };

    int failures = 0;
    for (const auto& scene : scenes)
    {
        const auto file = outDir.getChildFile (juce::String (scene.name) + ".wav");
        const bool ok = writeWav (file, scene.render());
        std::printf ("%-22s %s\n", scene.name, ok ? file.getFullPathName().toRawUTF8() : "FAILED");
        failures += ok ? 0 : 1;
    }
    return failures == 0 ? 0 : 1;
}
