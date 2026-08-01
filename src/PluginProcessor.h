#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

#include "Heavy_OpenGlitch.h"
#include "LfoEngine.h"

class OpenGlitchAudioProcessor : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AsyncUpdater
{
public:
    OpenGlitchAudioProcessor();
    ~OpenGlitchAudioProcessor() override { cancelPendingUpdate(); }

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
    float getDisplayBpm() const noexcept { return displayBpm.load (std::memory_order_relaxed); }

    // Diagnostics for the UI status line: what the processor believes the
    // host is telling it. Invaluable for debugging hosting issues remotely.
    enum TransportMode { transportUnknown = -1, standaloneClock = 0, hostedStopped = 1, hostedPlaying = 2 };
    int getTransportMode() const noexcept { return diagTransport.load (std::memory_order_relaxed); }
    juce::uint32 getTickCount() const noexcept { return diagTickCount.load (std::memory_order_relaxed); }

    // Pattern system: 8 slots (A..H) stored in the APVTS state tree. The step
    // and length parameters always hold the *active* pattern; edits are
    // recorded into the selected slot, switching loads another slot.
    static constexpr int numPatterns = 16;
    int getActivePattern() const noexcept { return activeSlot; }
    void copyActivePatternTo (int slot); // message thread only (shift-click in the UI)
    void randomizeActivePattern();       // message thread only (the DICE button)
    void randomizeFxKnobs();             // message thread only (the FX dice button)
    void shiftActivePattern (int direction); // message thread only (< > arrows)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Pattern storage
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    juce::ValueTree patternsTree();
    void storePattern (int slot);
    void loadPattern (int slot);

    juce::AudioParameterChoice* patternParam = nullptr;
    std::array<juce::RangedAudioParameter*, 16> stepParams {};
    juce::RangedAudioParameter* lengthParam = nullptr;
    std::atomic<float>* lengthRaw = nullptr;
    std::atomic<float>* swingRaw = nullptr;
    int activeSlot = 0;
    bool loadingPattern = false;
    std::atomic<bool> patternEdited { false }, patternSelected { false };
    std::atomic<int> pendingMidiPattern { -1 };
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
        int modTarget = lfo::off; // lfo::Target this receiver can be modulated by
    };
    std::vector<ParamLink> paramLinks;

    // LFO engine (audio thread only). Contributions are rebuilt every block
    // and folded into the values pushed to Heavy.
    void updateLfos (int numSamples);
    lfo::State lfoStates[2];
    juce::Random lfoRng;
    float modContrib[lfo::numTargets] = {};
    std::atomic<float>* lfoRaw[2][4] = {}; // [lfo][shape, rate, depth, target]
    std::atomic<float>* modSyncRaw = nullptr;

    // Per-effect output stage (shared glitch_post receivers): the firing
    // effect's stored settings are dispatched with each trigger, and live
    // knob tweaks of the sounding effect stream immediately.
    void pushPostParameters();
    std::atomic<float>* postRaw[9][5] = {}; // [effect][mode, freq, pan, mix, gain]
    hv_uint32_t postHash[5] = {};
    int activePostEffect = 0; // audio thread; 0 = dry/neutral
    float lastSentPost[5] = {};
    std::atomic<float>* seedRaw = nullptr;
    int dicePressCount = 0;

    juce::AudioParameterBool* bypassParam = nullptr;

    // Heavy's SIMD loads/stores require 16/32-byte aligned channel buffers.
    // Host buffers (and juce::AudioBuffer channels) don't guarantee that, so
    // audio is bounced through this explicitly aligned scratch block.
    void ensureScratch (int numSamples);
    juce::HeapBlock<float> scratchAllocation;
    float* scratch[4] = {}; // inL, inR, outL, outR
    int scratchCapacity = 0;

    // Transport state. hostSyncActive: -1 unknown, 0 = JUCE-driven (always).
    int hostSyncActive = -1;
    double standalonePpq = 0.0; // virtual timeline when no host provides one
    double lastSentBpm = 0.0;
    bool wasPlaying = false;
    long long lastFiredTick = std::numeric_limits<long long>::min();

    std::atomic<int> playheadStep { -1 };
    std::atomic<float> displayBpm { 120.0f };
    std::atomic<int> diagTransport { transportUnknown };
    std::atomic<juce::uint32> diagTickCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGlitchAudioProcessor)
};
