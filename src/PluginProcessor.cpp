#include "PluginProcessor.h"

OpenGlitchAudioProcessor::OpenGlitchAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (bypassParam = new juce::AudioParameterFloat (
        juce::ParameterID ("bypass", 1), "Bypass",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f));

    bypassHash = hv_stringToHash ("bypass");
}

void OpenGlitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    heavy.reset (hv_OpenGlitch_new (sampleRate));

    // Heavy only stores parameter defaults as metadata; the wrapper must push
    // them into the graph or receivers like host_playing stay at zero.
    const int numParams = hv_getParameterInfo (heavy.get(), 0, nullptr);
    for (int i = 0; i < numParams; ++i)
    {
        HvParameterInfo info;
        if (hv_getParameterInfo (heavy.get(), i, &info) > 0
            && info.type == HV_PARAM_TYPE_PARAMETER_IN)
        {
            hv_sendFloatToReceiver (heavy.get(), info.hash, info.defaultVal);
        }
    }

    heavyInput.setSize (2, samplesPerBlock);
    lastSentBypass = -1.0f; // force a resend on the first block
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

void OpenGlitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    if (heavy == nullptr)
        return;

    const int numSamples = buffer.getNumSamples();

    const float bypass = bypassParam->get();
    if (! juce::approximatelyEqual (bypass, lastSentBypass))
    {
        hv_sendFloatToReceiver (heavy.get(), bypassHash, bypass);
        lastSentBypass = bypass;
    }

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
    juce::MemoryOutputStream (destData, true).writeFloat (bypassParam->get());
}

void OpenGlitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream in (data, static_cast<size_t> (sizeInBytes), false);

    if (in.getNumBytesRemaining() >= static_cast<juce::int64> (sizeof (float)))
        *bypassParam = in.readFloat();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenGlitchAudioProcessor();
}
