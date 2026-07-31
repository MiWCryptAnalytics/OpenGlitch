#include "PluginProcessor.h"

#include <cmath>
#include <limits>

namespace
{
juce::AudioParameterFloatAttributes percentAttributes()
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String ((int) std::round (v * 100.0f)) + " %"; });
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const char* id, const char* name,
                                                      float min, float max, float def,
                                                      float skewCentre, const char* label)
{
    juce::NormalisableRange<float> range (min, max);
    if (skewCentre > 0.0f)
        range.setSkewForCentre (skewCentre);
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id, 1), name, range, def,
        juce::AudioParameterFloatAttributes().withLabel (label));
}

std::unique_ptr<juce::AudioParameterInt> makeInt (const char* id, const char* name,
                                                  int min, int max, int def, const char* label)
{
    return std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID (id, 1), name, min, max, def,
        juce::AudioParameterIntAttributes().withLabel (label));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OpenGlitchAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::StringArray effectNames { "Dry", "Tape Stop", "Modulator", "Retrigger", "Shuffler",
                                          "Reverser", "Crusher", "Gater", "Delay", "Stretcher" };
    // The demo pattern from the Pd patch defaults.
    const int stepDefaults[16] = { 0, 0, 3, 0, 7, 0, 3, 5, 0, 6, 0, 4, 3, 0, 9, 1 };

    auto sequencer = std::make_unique<juce::AudioProcessorParameterGroup> ("sequencer", "Sequencer", "|");
    for (int i = 0; i < 16; ++i)
        sequencer->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ("step_" + juce::String (i + 1), 1),
            "Step " + juce::String (i + 1), effectNames, stepDefaults[i]));
    sequencer->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("seq_chaos", 1), "Chaos",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentAttributes()));
    layout.add (std::move (sequencer));

    auto effects = std::make_unique<juce::AudioProcessorParameterGroup> ("effects", "Effects", "|");
    effects->addChild (makeFloat ("fx_tapestop_speed", "Tape Stop Speed", 0.1f, 4.0f, 1.0f, 1.0f, "x step"));
    effects->addChild (makeFloat ("fx_mod_freq", "Mod Frequency", 1.0f, 4000.0f, 150.0f, 200.0f, "Hz"));
    effects->addChild (makeInt ("fx_retrigger_rate", "Retrigger Slices", 1, 8, 4, "per step"));
    effects->addChild (makeFloat ("fx_retrigger_pitch", "Retrigger Pitch", 0.25f, 2.0f, 1.0f, 1.0f, "x"));
    effects->addChild (makeInt ("fx_shuffle_range", "Shuffle Range", 1, 8, 4, "steps"));
    effects->addChild (makeFloat ("fx_crush_rate", "Crush Rate", 200.0f, 20000.0f, 2500.0f, 2500.0f, "Hz"));
    effects->addChild (makeFloat ("fx_crush_drive", "Crush Drive", 1.0f, 10.0f, 2.0f, 0.0f, "x"));
    effects->addChild (makeInt ("fx_gate_rate", "Gate Rate", 1, 16, 4, "per step"));
    effects->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("fx_gate_duty", 1), "Gate Duty",
        juce::NormalisableRange<float> (0.05f, 0.95f), 0.5f, percentAttributes()));
    effects->addChild (makeFloat ("fx_delay_div", "Delay Time", 0.25f, 4.0f, 0.75f, 1.0f, "x step"));
    effects->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("fx_delay_feedback", 1), "Delay Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f), 0.55f, percentAttributes()));
    effects->addChild (makeFloat ("fx_stretch_speed", "Stretch Speed", 0.1f, 1.0f, 0.5f, 0.0f, "x"));
    layout.add (std::move (effects));

    auto master = std::make_unique<juce::AudioProcessorParameterGroup> ("master", "Master", "|");
    master->addChild (makeFloat ("master_drive", "Drive", 1.0f, 10.0f, 1.0f, 0.0f, "x"));
    master->addChild (makeFloat ("master_lowpass", "Lowpass", 100.0f, 20000.0f, 20000.0f, 2000.0f, "Hz"));
    master->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master_mix", 1), "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percentAttributes()));
    master->addChild (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("bypass", 1), "Bypass", false));
    layout.add (std::move (master));

    return layout;
}

OpenGlitchAudioProcessor::OpenGlitchAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OpenGlitch", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    jassert (bypassParam != nullptr);

    static const char* const linkedIds[] = {
        "step_1", "step_2", "step_3", "step_4", "step_5", "step_6", "step_7", "step_8",
        "step_9", "step_10", "step_11", "step_12", "step_13", "step_14", "step_15", "step_16",
        "seq_chaos",
        "fx_tapestop_speed", "fx_mod_freq", "fx_retrigger_rate", "fx_retrigger_pitch",
        "fx_shuffle_range", "fx_crush_rate", "fx_crush_drive", "fx_gate_rate", "fx_gate_duty",
        "fx_delay_div", "fx_delay_feedback", "fx_stretch_speed",
        "master_drive", "master_lowpass", "master_mix", "bypass"
    };

    for (auto* id : linkedIds)
    {
        auto* raw = apvts.getRawParameterValue (id);
        jassert (raw != nullptr); // parameter ID must match the Pd receiver name
        paramLinks.push_back ({ hv_stringToHash (id), raw, std::numeric_limits<float>::quiet_NaN() });
    }
}

void OpenGlitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    heavy.reset (hv_OpenGlitch_new (sampleRate));
    hv_setUserData (heavy.get(), this);
    hv_setSendHook (heavy.get(), heavySendHook);

    // Heavy only stores parameter defaults as metadata; the wrapper must push
    // them into the graph or receivers like host_playing stay at zero.
    const int numParams = hv_getParameterInfo (heavy.get(), 0, nullptr);
    for (int i = 0; i < numParams; ++i)
    {
        HvParameterInfo info;
        if (hv_getParameterInfo (heavy.get(), i, &info) > 0
            && info.type == HV_PARAM_TYPE_PARAMETER_IN
            && info.hash != HV_OPENGLITCH_PARAM_IN_HOST_PLAYING) // transport logic owns this;
                                                                 // a default of 1 would briefly
                                                                 // start the metro when hosted
        {
            hv_sendFloatToReceiver (heavy.get(), info.hash, info.defaultVal);
        }
    }

    // Force a full resend of the real parameter state on the first block.
    for (auto& link : paramLinks)
        link.lastSent = std::numeric_limits<float>::quiet_NaN();

    hostSyncActive = -1;
    lastSentBpm = 0.0;
    wasPlaying = false;
    lastFiredTick = std::numeric_limits<long long>::min();
    playheadStep.store (-1, std::memory_order_relaxed);

    heavyInput.setSize (2, samplesPerBlock);
}

void OpenGlitchAudioProcessor::releaseResources()
{
    heavy.reset();
}

bool OpenGlitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The Heavy context is compiled with a fixed stereo [adc~]/[dac~] pair.
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void OpenGlitchAudioProcessor::heavySendHook (HeavyContextInterface* context, const char*,
                                              hv_uint32_t sendHash, const HvMessage* msg)
{
    if (sendHash == HV_OPENGLITCH_PARAM_OUT_PLAYHEAD)
        if (auto* self = static_cast<OpenGlitchAudioProcessor*> (hv_getUserData (context)))
            self->playheadStep.store ((int) hv_msg_getFloat (msg, 0), std::memory_order_relaxed);
}

void OpenGlitchAudioProcessor::pushChangedParameters()
{
    for (auto& link : paramLinks)
    {
        const float v = link.value->load (std::memory_order_relaxed);
        if (! juce::exactlyEqual (v, link.lastSent)) // NaN sentinel compares unequal, forcing the first send
        {
            hv_sendFloatToReceiver (heavy.get(), link.hash, v);
            link.lastSent = v;
        }
    }
}

void OpenGlitchAudioProcessor::sendTick (int stepIndex, double delayMs)
{
    static const hv_uint32_t hostTickHash = hv_stringToHash ("host_tick");
    hv_sendMessageToReceiverV (heavy.get(), hostTickHash, delayMs, "f", (double) stepIndex);
}

void OpenGlitchAudioProcessor::pushTransport (int numSamples)
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
    bool hosted = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto pos = hostPlayHead->getPosition())
        {
            bpm = pos->getBpm().orFallback (120.0);
            playing = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition())
            {
                hosted = true;
                ppq = *p;
            }
        }
    }

    // With a DAW timeline available, JUCE becomes the clock: the standalone
    // metro is switched off and 16th-note ticks are scheduled below.
    const int wantHostSync = hosted ? 1 : 0;
    if (wantHostSync != hostSyncActive)
    {
        hv_sendFloatToReceiver (heavy.get(), HV_OPENGLITCH_PARAM_IN_HOST_PLAYING,
                                hosted ? 0.0f : 1.0f);
        hostSyncActive = wantHostSync;
    }

    // Effects derive slice lengths and ramp times from host_bpm.
    if (std::abs (bpm - lastSentBpm) > 0.001)
    {
        hv_sendFloatToReceiver (heavy.get(), HV_OPENGLITCH_PARAM_IN_HOST_BPM, (float) bpm);
        lastSentBpm = bpm;
    }

    if (! hosted)
        return;

    if (playing)
    {
        const double sr = getSampleRate();
        const double ppqEnd = ppq + (double) numSamples * bpm / (60.0 * sr);
        auto wrap16 = [] (long long g) { return (int) (((g % 16) + 16) % 16); };

        // Fire the current 16th immediately after any discontinuity
        // (transport start, relocate, loop wrap), then schedule every
        // boundary inside this block at its exact offset.
        const auto g0 = (long long) std::floor (ppq * 4.0);
        if (g0 != lastFiredTick)
        {
            sendTick (wrap16 (g0), 0.0);
            lastFiredTick = g0;
        }
        for (long long g = g0 + 1; (double) g / 4.0 < ppqEnd; ++g)
        {
            sendTick (wrap16 (g), ((double) g / 4.0 - ppq) * 60000.0 / bpm);
            lastFiredTick = g;
        }
    }
    else if (wasPlaying)
    {
        sendTick (-1, 0.0); // transport stopped: fall back to dry
        lastFiredTick = std::numeric_limits<long long>::min();
        playheadStep.store (-1, std::memory_order_relaxed);
    }

    wasPlaying = playing;
}

void OpenGlitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    if (heavy == nullptr)
        return;

    pushChangedParameters();
    pushTransport (buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();

    // Heavy renders in fixed SIMD sub-blocks of up to 8 samples; hv_process
    // requires a multiple of that. Trailing samples of an odd-sized host
    // block are left untouched (dry) — hosts almost always send multiples of 8.
    const int processable = numSamples - (numSamples % 8);
    if (processable == 0)
        return;

    if (heavyInput.getNumSamples() < numSamples)
        heavyInput.setSize (2, numSamples, false, false, true);

    for (int ch = 0; ch < 2; ++ch)
        heavyInput.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    float* inputs[2]  = { heavyInput.getWritePointer (0), heavyInput.getWritePointer (1) };
    float* outputs[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };

    hv_process (heavy.get(), inputs, outputs, processable);
}

juce::AudioProcessorEditor* OpenGlitchAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

bool OpenGlitchAudioProcessor::hasEditor() const              { return true; }

juce::AudioProcessorParameter* OpenGlitchAudioProcessor::getBypassParameter() const
{
    return bypassParam;
}

const juce::String OpenGlitchAudioProcessor::getName() const  { return "OpenGlitch"; }
bool OpenGlitchAudioProcessor::acceptsMidi() const            { return false; }
bool OpenGlitchAudioProcessor::producesMidi() const           { return false; }
bool OpenGlitchAudioProcessor::isMidiEffect() const           { return false; }
double OpenGlitchAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int OpenGlitchAudioProcessor::getNumPrograms()                            { return 1; }
int OpenGlitchAudioProcessor::getCurrentProgram()                         { return 0; }
void OpenGlitchAudioProcessor::setCurrentProgram (int)                    {}
const juce::String OpenGlitchAudioProcessor::getProgramName (int)         { return "Default"; }
void OpenGlitchAudioProcessor::changeProgramName (int, const juce::String&) {}

void OpenGlitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OpenGlitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenGlitchAudioProcessor();
}
