// Renders the plugin editor to a PNG without a display server. Used to
// preview and document the UI (and usable from CI).
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    OpenGlitchAudioProcessor processor;

    // Run some audio through the free-running sequencer so the snapshot
    // shows the playhead, lamp and LCD in their live state.
    // Dial in a photogenic LFO cascade for the scope: LFO1 sine -> filter,
    // LFO2 triangle -> LFO1 rate (the warp becomes visible in the trace).
    auto setParam = [&processor] (const char* id, float value)
    {
        auto* p = processor.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    setParam ("lfo1_shape", 0.0f);
    setParam ("lfo1_rate", 4.0f);
    setParam ("lfo1_depth", 0.85f);
    setParam ("lfo1_target", 1.0f);
    setParam ("lfo2_shape", 1.0f);
    setParam ("lfo2_rate", 5.0f);
    setParam ("lfo2_depth", 0.6f);
    setParam ("lfo2_target", 7.0f);

    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    int sample = 0;
    for (int block = 0; block < 60; ++block)
    {
        for (int i = 0; i < 512; ++i, ++sample)
        {
            const float s = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 220.0f * (float) sample / 48000.0f);
            audio.setSample (0, i, s);
            audio.setSample (1, i, -s);
        }
        processor.processBlock (audio, midi);
    }

    std::printf ("sequencer step after warmup: %d\n", processor.getCurrentStep());

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

    auto file = juce::File::getCurrentWorkingDirectory().getChildFile (
        argc > 1 ? argv[1] : "openglitch-ui.png");
    file.deleteFile();
    juce::FileOutputStream stream (file);
    if (! stream.openedOk())
        return 1;

    juce::PNGImageFormat png;
    if (! png.writeImageToStream (snapshot, stream))
        return 1;

    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(),
                 snapshot.getWidth(), snapshot.getHeight());
    return 0;
}
