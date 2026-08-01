// Renders short demo WAVs of the engine — the whole demo pattern plus each
// effect soloed — for A/B listening against the original dBlue Glitch 1.3 in
// a DAW. Drives the Heavy context directly through the test harness, feeding
// a deterministic drum loop (kick / snare / hats on a 16th grid) so the
// glitching has transients to chew on instead of a bare sine.
//
//   OpenGlitchRender [output-dir]     (default: renders/ under the cwd)
#include <functional>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include "TestHelpers.h"

using testutil::HeavyHarness;
using testutil::SR;

namespace
{
constexpr int stepSamples = 6000; // one 16th at 120bpm, and a multiple of 8
constexpr double twoPi = 6.283185307179586;

struct Rng // xorshift: deterministic noise, same WAV every run
{
    uint32_t s = 0x9e3779b9u;
    float next()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (float) (int32_t) s / 2147483648.0f;
    }
};

// 2-bar loop: kick on 1 and 3, snare on 2 and 4, hats on the off-8ths.
std::vector<float> makeDrumLoop (int totalSamples)
{
    std::vector<float> mono ((size_t) totalSamples, 0.0f);
    Rng rng;

    auto addHit = [&] (int at, const std::function<float (double, Rng&)>& voice, double lengthSec)
    {
        const int n = (int) (lengthSec * SR);
        for (int i = 0; i < n && at + i < totalSamples; ++i)
            mono[(size_t) (at + i)] += voice ((double) i / SR, rng);
    };

    auto kick = [] (double t, Rng&)
    {
        const double ph = twoPi * (50.0 * t + 70.0 * 0.02 * (1.0 - std::exp (-t / 0.02)));
        return 0.85f * (float) (std::sin (ph) * std::exp (-t / 0.09));
    };
    auto snare = [] (double t, Rng& r)
    {
        return (float) (0.4 * r.next() * std::exp (-t / 0.045)
                        + 0.3 * std::sin (twoPi * 185.0 * t) * std::exp (-t / 0.03));
    };
    auto hat = [] (double t, Rng& r)
    {
        return 0.3f * r.next() * (float) std::exp (-t / 0.015);
    };

    for (int at = 0; at < totalSamples; at += stepSamples)
    {
        const int p = (at / stepSamples) % 16;
        if (p % 8 == 0)
            addHit (at, kick, 0.15);
        if (p % 8 == 4)
            addHit (at, snare, 0.12);
        if (p % 2 == 1)
            addHit (at, hat, 0.05);
    }

    std::vector<float> interleaved ((size_t) totalSamples * 2);
    for (int i = 0; i < totalSamples; ++i)
    {
        const float v = std::clamp (mono[(size_t) i], -0.95f, 0.95f);
        interleaved[(size_t) i * 2] = v;
        interleaved[(size_t) i * 2 + 1] = v;
    }
    return interleaved;
}

// Feeds a slice of the interleaved input through the Heavy context.
void processChunk (HeavyHarness& h, const std::vector<float>& input, int start, int numSamples,
                   std::vector<float>& out)
{
    float inL[512], inR[512], outL[512], outR[512];
    float* ins[2] = { inL, inR };
    float* outs[2] = { outL, outR };
    for (int done = 0; done < numSamples;)
    {
        const int n = std::min (512, numSamples - done);
        for (int i = 0; i < n; ++i)
        {
            inL[i] = input[(size_t) (start + done + i) * 2];
            inR[i] = input[(size_t) (start + done + i) * 2 + 1];
        }
        hv_process (h.hv, ins, outs, n);
        for (int i = 0; i < n; ++i)
        {
            out.push_back (outL[i]);
            out.push_back (outR[i]);
        }
        done += n;
    }
}

std::vector<float> renderFree (const std::function<void (HeavyHarness&)>& setup, double seconds)
{
    HeavyHarness h;
    if (setup)
        setup (h);
    h.set ("host_playing", 1.0f); // after the pattern lands, so bar one is correct
    const int total = ((int) (seconds * SR) / 512) * 512;
    const auto input = makeDrumLoop (total);
    std::vector<float> out;
    out.reserve ((size_t) total * 2);
    processChunk (h, input, 0, total, out);
    return out;
}

// Manual ticks with per-step span values, locked to the 125ms musical grid.
std::vector<float> renderTicked (const std::function<void (HeavyHarness&)>& setup,
                                 const std::array<int, 16>& spans, int bars)
{
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    if (setup)
        setup (h);
    h.run (0.1);

    const int total = bars * 16 * stepSamples;
    const auto input = makeDrumLoop (total);
    std::vector<float> out;
    out.reserve ((size_t) total * 2);
    for (int s = 0; s < bars * 16; ++s)
    {
        h.set ("glitch_spansteps", (float) spans[(size_t) (s % 16)]);
        h.tick (s % 16);
        processChunk (h, input, s * stepSamples, stepSamples, out);
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
        format.createWriterFor (stream.get(), (double) SR, 2, 24, {}, 0));
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

    // A stretch span across half a bar, twice per bar, dry in between —
    // the tie-drag gesture the stretcher is really for.
    auto stretchSpans = [] (HeavyHarness& h)
    {
        h.setAllSteps (0.0f);
        for (int s : { 1, 9 })
        {
            h.set (("step_" + std::to_string (s)).c_str(), 9.0f);
            for (int t = s + 1; t < s + 4; ++t)
                h.set (("step_" + std::to_string (t)).c_str(), 10.0f);
        }
    };

    const Scene scenes[] = {
        { "demo", [] { return renderFree ({}, 4.0); } },
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
              { h.setAllSteps (0.0f); h.set ("step_1", 8.0f); h.set ("fx_delay_feedback", 0.7f); },
              { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, 2); } },
        { "stretcher_span", [&stretchSpans] { return renderTicked (stretchSpans,
              { 4, 1, 1, 1, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1 }, 2); } },
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
