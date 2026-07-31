#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

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

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // Exposed for the Phase 4 editor: parameter tree and the sequencer
    // position last reported by the Heavy context (-1 before playback).
    juce::AudioProcessorValueTreeState apvts;
    int getCurrentStep() const noexcept { return playheadStep.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static void heavySendHook (HeavyContextInterface* context, const char* sendName,
                               hv_uint32_t sendHash, const HvMessage* msg);

    void pushChangedParameters();
    void pushTransport (int numSamples);
    void sendTick (int stepIndex, double delayMs);

    struct HeavyContextDeleter
    {
        void operator() (HeavyContextInterface* ctx) const noexcept
        {
            if (ctx != nullptr)
                hv_delete (ctx);
        }
    };

    std::unique_ptr<HeavyContextInterface, HeavyContextDeleter> heavy;

    // One entry per DAW-facing parameter: the APVTS atomic it mirrors and the
    // Heavy receiver hash it feeds. Values are pushed from the audio thread.
    struct ParamLink
    {
        hv_uint32_t hash;
        std::atomic<float>* value;
        float lastSent;
    };
    std::vector<ParamLink> paramLinks;

    juce::AudioParameterBool* bypassParam = nullptr;

    // Pre-allocated copy of the host input so Heavy never reads from the
    // buffer it is writing into.
    juce::AudioBuffer<float> heavyInput;

    // Transport state. hostSyncActive: -1 unknown, 0 standalone metro, 1 DAW-driven.
    int hostSyncActive = -1;
    double lastSentBpm = 0.0;
    bool wasPlaying = false;
    long long lastFiredTick = std::numeric_limits<long long>::min();

    std::atomic<int> playheadStep { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGlitchAudioProcessor)
};
