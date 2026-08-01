// Loads the actual built VST3 bundle through JUCE's plugin-hosting layer —
// the same wrapper path a DAW uses (parameter flush, bus negotiation,
// process-context conversion) that direct-processBlock harnesses bypass.
#include <catch2/catch_test_macros.hpp>

#include <juce_audio_processors/juce_audio_processors.h>

#ifndef OPENGLITCH_VST3_DIR
#error "OPENGLITCH_VST3_DIR must point at the built .vst3 bundle"
#endif

namespace
{
struct WrapperPlayHead : juce::AudioPlayHead
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

struct HostedPlugin
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::VST3PluginFormat format;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    WrapperPlayHead playhead;
    double sampleRate;
    int blockSize;
    int sampleIndex = 0;

    explicit HostedPlugin (double sr = 48000.0, int block = 512)
        : sampleRate (sr), blockSize (block)
    {
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, OPENGLITCH_VST3_DIR);
        REQUIRE (! descriptions.isEmpty());

        juce::String error;
        instance = format.createInstanceFromDescription (*descriptions[0], sr, block, error);
        INFO (error.toStdString());
        REQUIRE (instance != nullptr);

        instance->setPlayHead (&playhead);
        instance->enableAllBuses();
        instance->setRateAndBufferSizeDetails (sr, block);
        instance->prepareToPlay (sr, block);
    }

    ~HostedPlugin()
    {
        if (instance != nullptr)
        {
            instance->releaseResources();
            instance->setPlayHead (nullptr);
        }
    }

    std::vector<float> run (double seconds, std::vector<float>* inputOut = nullptr)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        const int blocks = (int) (seconds * sampleRate / blockSize);
        std::vector<float> out, in;
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < blockSize; ++i, ++sampleIndex)
            {
                const float s = std::sin (2.0f * juce::MathConstants<float>::pi
                                          * 220.0f * (float) sampleIndex / (float) sampleRate);
                const float v = 0.4f * (s > 0.0f ? 1.0f : -1.0f) + 0.1f * s;
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, -v);
            }
            for (int i = 0; i < blockSize; ++i)
            {
                in.push_back (buffer.getSample (0, i));
                in.push_back (buffer.getSample (1, i));
            }
            instance->processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
            {
                out.push_back (buffer.getSample (0, i));
                out.push_back (buffer.getSample (1, i));
            }
            if (playhead.playing)
                playhead.ppq += (double) blockSize / sampleRate * playhead.bpm / 60.0;
        }
        if (inputOut != nullptr)
            *inputOut = in;
        return out;
    }
};

float wrapperMaxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    float m = 0;
    const size_t n = std::min (a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
        m = std::max (m, std::fabs (a[i] - b[i]));
    return m;
}
} // namespace

TEST_CASE ("wrapper: hosted VST3 glitches under a playing transport", "[wrapper]")
{
    HostedPlugin p;
    p.playhead.playing = true;
    std::vector<float> in;
    auto out = p.run (2.0, &in);
    REQUIRE (wrapperMaxDiff (out, in) > 0.05f);
}

TEST_CASE ("wrapper: hosted VST3 at 44.1kHz / 1024 samples glitches", "[wrapper]")
{
    HostedPlugin p (44100.0, 1024);
    p.playhead.playing = true;
    std::vector<float> in;
    auto out = p.run (2.0, &in);
    REQUIRE (wrapperMaxDiff (out, in) > 0.05f);
}

TEST_CASE ("wrapper: stopped transport passes clean dry", "[wrapper]")
{
    HostedPlugin p;
    p.run (0.5);
    std::vector<float> in;
    auto out = p.run (0.5, &in);
    REQUIRE (wrapperMaxDiff (out, in) < 0.2f);
}
