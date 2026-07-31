#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Heavy_OpenGlitch.h"

class OpenGlitchAudioProcessor : public juce::AudioProcessor
{
public:
    OpenGlitchAudioProcessor();
    ~OpenGlitchAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    struct HeavyContextDeleter
    {
        void operator() (HeavyContextInterface* ctx) const noexcept
        {
            if (ctx != nullptr)
                hv_delete (ctx);
        }
    };

    std::unique_ptr<HeavyContextInterface, HeavyContextDeleter> heavy;

    juce::AudioParameterFloat* bypassParam = nullptr;
    hv_uint32_t bypassHash = 0;
    float lastSentBypass = -1.0f;

    // Pre-allocated copy of the host input so Heavy never reads from the
    // buffer it is writing into.
    juce::AudioBuffer<float> heavyInput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGlitchAudioProcessor)
};
