#include "PluginProcessor.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "PluginEditor.h"

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
        juce::AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 2); }));
}

std::unique_ptr<juce::AudioParameterFloat> makeHz (const char* id, const char* name,
                                                   float min, float max, float def, float skewCentre)
{
    juce::NormalisableRange<float> range (min, max);
    range.setSkewForCentre (skewCentre);
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID (id, 1), name, range, def,
        juce::AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
            [] (float v, int)
            {
                return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + "k"
                                    : juce::String (v, 0);
            }));
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

    // Index 10 ("Tie") continues the previous step's effect, so an effect can
    // span multiple steps like the original Glitch's stretched blocks.
    const juce::StringArray effectNames { "Dry", "Tape Stop", "Modulator", "Retrigger", "Shuffler",
                                          "Reverser", "Crusher", "Gater", "Delay", "Stretcher",
                                          "Tie" };
    // The demo pattern from the Pd patch defaults.
    const int stepDefaults[16] = { 0, 0, 3, 0, 7, 0, 3, 10, 0, 6, 0, 4, 3, 0, 9, 1 };

    auto sequencer = std::make_unique<juce::AudioProcessorParameterGroup> ("sequencer", "Sequencer", "|");
    for (int i = 0; i < 16; ++i)
        sequencer->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ("step_" + juce::String (i + 1), 1),
            "Step " + juce::String (i + 1), effectNames, stepDefaults[i]));
    sequencer->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("seq_chaos", 1), "Chaos",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentAttributes()));
    juce::StringArray patternNames;
    for (int i = 1; i <= 16; ++i)
        patternNames.add (juce::String (i));
    sequencer->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("pattern_select", 1), "Pattern", patternNames, 0));
    sequencer->addChild (makeInt ("seq_length", "Pattern Length", 1, 16, 16, "steps"));
    sequencer->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("seq_swing", 1), "Swing",
        juce::NormalisableRange<float> (0.0f, 0.5f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String ((int) std::round (v * 200.0f)) + " %"; })));
    sequencer->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("seq_declick", 1), "De-Click",
        juce::NormalisableRange<float> (1.0f, 30.0f), 5.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1); })));
    sequencer->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("seq_stepenv", 1), "Step Envelope",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentAttributes()));
    sequencer->addChild (makeInt ("seq_seed", "Seed", 0, 999, 0, ""));
    layout.add (std::move (sequencer));

    // Per-effect output strips: filter/pan/mix/gain per effect, applied by a
    // shared post stage snapped to whichever effect fires.
    auto post = std::make_unique<juce::AudioProcessorParameterGroup> ("fx_post", "Effect Output", "|");
    for (int n = 1; n <= 9; ++n)
    {
        const auto pid = [n] (const char* s) { return "fx" + juce::String (n) + "_" + s; };
        const auto pname = [&] (const char* s) { return effectNames[n] + " " + s; };
        post->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pid ("post_mode"), 1), pname ("Filter"),
            juce::StringArray { "Off", "Lowpass", "Highpass", "Bandpass" }, 0));
        post->addChild (makeHz (pid ("post_freq").toRawUTF8(), pname ("Filter Freq").toRawUTF8(),
                                100.0f, 20000.0f, 2000.0f, 2000.0f));
        post->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pid ("pan"), 1), pname ("Pan"),
            juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [] (float v, int)
                {
                    if (std::abs (v) < 0.01f) return juce::String ("C");
                    return (v < 0 ? "L" : "R") + juce::String ((int) std::round (std::abs (v) * 100.0f));
                })));
        post->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (pid ("mix"), 1), pname ("Mix"),
            juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percentAttributes()));
        post->addChild (makeFloat (pid ("gain").toRawUTF8(), pname ("Gain").toRawUTF8(),
                                   0.0f, 2.0f, 1.0f, 0.0f, "x"));
        post->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (pid ("sweep_shape"), 1), pname ("Sweep"),
            juce::StringArray { "Off", "Down", "Up", "Tri", "Sine", "Square" }, 0));
        post->addChild (makeFloat (pid ("sweep_amt").toRawUTF8(), pname ("Sweep Amount").toRawUTF8(),
                                   -3.0f, 3.0f, 2.0f, 0.0f, "oct"));
    }
    layout.add (std::move (post));

    auto effects = std::make_unique<juce::AudioProcessorParameterGroup> ("effects", "Effects", "|");
    effects->addChild (makeFloat ("fx_tapestop_speed", "Tape Stop Speed", 0.1f, 4.0f, 1.0f, 1.0f, "x step"));
    effects->addChild (makeHz ("fx_mod_freq", "Mod Frequency", 1.0f, 4000.0f, 150.0f, 200.0f));
    effects->addChild (makeInt ("fx_retrigger_rate", "Retrigger Slices", 1, 8, 4, "per step"));
    effects->addChild (makeFloat ("fx_retrigger_pitch", "Retrigger Pitch", 0.25f, 2.0f, 1.0f, 1.0f, "x"));
    effects->addChild (makeInt ("fx_shuffle_range", "Shuffle Range", 1, 8, 4, "steps"));
    effects->addChild (makeHz ("fx_crush_rate", "Crush Rate", 200.0f, 20000.0f, 2500.0f, 2500.0f));
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
    master->addChild (makeHz ("master_lowpass", "Filter Freq", 100.0f, 20000.0f, 20000.0f, 2000.0f));
    master->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("master_filter_type", 1), "Filter Type",
        juce::StringArray { "Lowpass", "Highpass", "Bandpass" }, 0));
    master->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master_drive_mix", 1), "Drive Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percentAttributes()));
    master->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master_reso", 1), "Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentAttributes()));
    master->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master_filter_mix", 1), "Filter Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percentAttributes()));
    master->addChild (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master_mix", 1), "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percentAttributes()));
    master->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("master_sweep_shape", 1), "Master Sweep",
        juce::StringArray { "Off", "Down", "Up", "Tri", "Sine", "Square" }, 0));
    master->addChild (makeFloat ("master_sweep_amt", "Master Sweep Amount", -3.0f, 3.0f, 2.0f, 0.0f, "oct"));
    master->addChild (makeFloat ("master_volume", "Volume", 0.0f, 2.0f, 1.0f, 1.0f, "x"));
    master->addChild (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID ("bypass", 1), "Bypass", false));
    layout.add (std::move (master));

    // Modulation: tempo-sync for the ring modulator, and two LFOs. LFO 2 can
    // target LFO 1's rate or depth for derivative, LFO-into-LFO motion.
    auto modulation = std::make_unique<juce::AudioProcessorParameterGroup> ("modulation", "Modulation", "|");
    modulation->addChild (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("fx_mod_sync", 1), "Mod Sync",
        juce::StringArray { "Free", "1/16", "1/8", "1/4", "1/2", "1 bar" }, 0));

    const juce::StringArray shapes { "Sine", "Triangle", "Saw", "Square", "Random" };
    const juce::StringArray rates { "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" };
    const juce::StringArray targets1 { "Off", "Filter Freq", "Drive", "Chaos",
                                       "Mod Freq", "Retrig Pitch", "Gate Duty",
                                       "Crush Rate", "Crush Drive", "Delay Feedback",
                                       "Stretch Speed", "TapeStop Speed",
                                       "Retrig Rate", "Gate Rate" };
    juce::StringArray targets2 = targets1;
    targets2.addArray ({ "LFO1 Rate", "LFO1 Depth" });

    for (int n = 1; n <= 2; ++n)
    {
        const auto id = [n] (const char* s) { return "lfo" + juce::String (n) + "_" + s; };
        const auto nm = [n] (const char* s) { return "LFO " + juce::String (n) + " " + s; };
        modulation->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (id ("shape"), 1), nm ("Shape"), shapes, 0));
        modulation->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (id ("rate"), 1), nm ("Rate"), rates, 4));
        modulation->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (id ("depth"), 1), nm ("Depth"),
            juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percentAttributes()));
        modulation->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID (id ("target"), 1), nm ("Target"),
            n == 1 ? targets1 : targets2, 0));
    }
    layout.add (std::move (modulation));

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
        "seq_chaos", "seq_length", "seq_swing", "seq_declick", "seq_stepenv",
        "fx_tapestop_speed", "fx_mod_freq", "fx_retrigger_rate", "fx_retrigger_pitch",
        "fx_shuffle_range", "fx_crush_rate", "fx_crush_drive", "fx_gate_rate", "fx_gate_duty",
        "fx_delay_div", "fx_delay_feedback", "fx_stretch_speed",
        "master_drive", "master_drive_mix", "master_lowpass", "master_filter_type",
        "master_reso", "master_filter_mix", "master_mix", "bypass"
    };

    for (auto* id : linkedIds)
    {
        auto* raw = apvts.getRawParameterValue (id);
        jassert (raw != nullptr); // parameter ID must match the Pd receiver name
        paramLinks.push_back ({ hv_stringToHash (id), raw, std::numeric_limits<float>::quiet_NaN() });
    }

    // LFO wiring: which pushed receivers can be modulated, and the LFO params.
    static const std::pair<const char*, int> modTargets[] = {
        { "master_lowpass", lfo::filterFreq }, { "master_drive", lfo::drive },
        { "seq_chaos", lfo::chaos },           { "fx_mod_freq", lfo::modFreq },
        { "fx_retrigger_pitch", lfo::retrigPitch }, { "fx_gate_duty", lfo::gateDuty },
        { "fx_crush_rate", lfo::crushRate },   { "fx_crush_drive", lfo::crushDrive },
        { "fx_delay_feedback", lfo::delayFeedback }, { "fx_stretch_speed", lfo::stretchSpeed },
        { "fx_tapestop_speed", lfo::tapestopSpeed }, { "fx_retrigger_rate", lfo::retrigRate },
        { "fx_gate_rate", lfo::gateRate },
    };
    for (auto& link : paramLinks)
        for (const auto& [id, target] : modTargets)
            if (link.hash == hv_stringToHash (id))
                link.modTarget = target;

    static const char* const lfoIds[] = { "shape", "rate", "depth", "target" };
    for (int n = 0; n < 2; ++n)
        for (int i = 0; i < 4; ++i)
            lfoRaw[n][i] = apvts.getRawParameterValue ("lfo" + juce::String (n + 1) + "_" + lfoIds[i]);
    modSyncRaw = apvts.getRawParameterValue ("fx_mod_sync");

    static const char* const postIds[] = { "post_mode", "post_freq", "pan", "mix", "gain" };
    static const char* const postReceivers[] = { "glitch_post_mode", "glitch_post_freq",
                                                 "glitch_post_pan", "glitch_post_mix",
                                                 "glitch_post_gain" };
    for (int n = 0; n < 9; ++n)
        for (int k = 0; k < 5; ++k)
            postRaw[n][k] = apvts.getRawParameterValue ("fx" + juce::String (n + 1) + "_" + postIds[k]);
    for (int k = 0; k < 5; ++k)
        postHash[k] = hv_stringToHash (postReceivers[k]);
    seedRaw = apvts.getRawParameterValue ("seq_seed");
    for (int n = 0; n < 9; ++n)
    {
        sweepRaw[n][0] = apvts.getRawParameterValue ("fx" + juce::String (n + 1) + "_sweep_shape");
        sweepRaw[n][1] = apvts.getRawParameterValue ("fx" + juce::String (n + 1) + "_sweep_amt");
    }
    masterSweepRaw[0] = apvts.getRawParameterValue ("master_sweep_shape");
    masterSweepRaw[1] = apvts.getRawParameterValue ("master_sweep_amt");
    masterVolumeRaw = apvts.getRawParameterValue ("master_volume");

    // Pattern system wiring
    patternParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("pattern_select"));
    lengthParam = apvts.getParameter ("seq_length");
    lengthRaw = apvts.getRawParameterValue ("seq_length");
    swingRaw = apvts.getRawParameterValue ("seq_swing");
    jassert (patternParam != nullptr && lengthParam != nullptr);

    for (int i = 0; i < 16; ++i)
    {
        const auto id = "step_" + juce::String (i + 1);
        stepParams[(size_t) i] = apvts.getParameter (id);
        apvts.addParameterListener (id, this);
    }
    apvts.addParameterListener ("seq_length", this);
    apvts.addParameterListener ("pattern_select", this);

    patternsTree(); // seed all 8 slots (slot A inherits the demo defaults)
}

// ---------------------------------------------------------------------------
// Pattern system
// ---------------------------------------------------------------------------
juce::ValueTree OpenGlitchAudioProcessor::patternsTree()
{
    auto tree = apvts.state.getOrCreateChildWithName ("PATTERNS", nullptr);
    for (int slot = 0; slot < numPatterns; ++slot)
    {
        if (! tree.getChildWithProperty ("index", slot).isValid())
        {
            juce::ValueTree pattern ("PATTERN");
            pattern.setProperty ("index", slot, nullptr);
            juce::StringArray steps;
            for (int i = 0; i < 16; ++i)
                steps.add (slot == 0 ? juce::String ((int) std::lround (
                                           paramLinks[(size_t) i].value->load()))
                                     : "0");
            pattern.setProperty ("steps", steps.joinIntoString (","), nullptr);
            pattern.setProperty ("length", 16, nullptr);
            tree.appendChild (pattern, nullptr);
        }
    }
    return tree;
}

void OpenGlitchAudioProcessor::storePattern (int slot)
{
    auto pattern = patternsTree().getChildWithProperty ("index", slot);
    juce::StringArray steps;
    for (int i = 0; i < 16; ++i)
        steps.add (juce::String ((int) std::lround (paramLinks[(size_t) i].value->load())));
    pattern.setProperty ("steps", steps.joinIntoString (","), nullptr);
    pattern.setProperty ("length", (int) std::lround (lengthRaw->load()), nullptr);
}

void OpenGlitchAudioProcessor::loadPattern (int slot)
{
    const auto pattern = patternsTree().getChildWithProperty ("index", slot);
    const auto steps = juce::StringArray::fromTokens (pattern["steps"].toString(), ",", "");

    const juce::ScopedValueSetter<bool> loading (loadingPattern, true);
    for (int i = 0; i < 16; ++i)
    {
        auto* p = stepParams[(size_t) i];
        p->setValueNotifyingHost (p->convertTo0to1 ((float) steps[i].getIntValue()));
    }
    lengthParam->setValueNotifyingHost (
        lengthParam->convertTo0to1 ((float) (int) pattern.getProperty ("length", 16)));
}

// Seed > 0 gives reproducible dice rolls, advancing per press like the original.
static juce::Random makeSeededRng (std::atomic<float>* seedRaw, int& pressCount)
{
    const int seed = (int) std::lround (seedRaw->load());
    if (seed > 0)
        return juce::Random ((juce::int64) seed * 1000 + pressCount++);
    return juce::Random (juce::Random::getSystemRandom().nextInt64());
}

void OpenGlitchAudioProcessor::randomizeFxKnobs()
{
    auto rng = makeSeededRng (seedRaw, dicePressCount);

    juce::StringArray ids { "fx_tapestop_speed", "fx_mod_freq", "fx_mod_sync",
                            "fx_retrigger_rate", "fx_retrigger_pitch", "fx_shuffle_range",
                            "fx_crush_rate", "fx_crush_drive", "fx_gate_rate", "fx_gate_duty",
                            "fx_delay_div", "fx_delay_feedback", "fx_stretch_speed" };
    static const char* const postIds[] = { "post_mode", "post_freq", "pan", "mix", "gain" };
    for (int n = 1; n <= 9; ++n)
        for (auto* s : postIds)
            ids.add ("fx" + juce::String (n) + "_" + s);

    for (const auto& id : ids)
    {
        auto* p = apvts.getParameter (id);
        jassert (p != nullptr);
        float v = rng.nextFloat();
        if (id.endsWith ("_gain"))
            v = 0.4f + v * 0.35f; // 0.8x..1.5x — never silence a step
        else if (id.endsWith ("_mix"))
            v = 0.5f + v * 0.5f;
        p->setValueNotifyingHost (v);
    }
}

void OpenGlitchAudioProcessor::loadTemplate (int templateIndex)
{
    // Factory patterns in the spirit of the original's Templates row.
    static const int templates[4][16] = {
        { 3, 0, 0, 0, 3, 0, 3, 10, 3, 0, 0, 3, 3, 10, 10, 10 },  // stutter
        { 7, 0, 7, 0, 3, 10, 3, 10, 3, 3, 3, 3, 9, 10, 1, 10 },  // buildup
        { 9, 10, 10, 10, 1, 10, 0, 0, 9, 10, 10, 10, 5, 10, 6, 0 }, // halftime wreck
        { 2, 10, 10, 10, 8, 10, 10, 10, 5, 10, 10, 10, 9, 10, 10, 10 }, // ambient smear
    };
    if (templateIndex < 0 || templateIndex > 3)
        return;

    const juce::ScopedValueSetter<bool> loading (loadingPattern, true);
    for (int i = 0; i < 16; ++i)
    {
        auto* p = stepParams[(size_t) i];
        p->setValueNotifyingHost (p->convertTo0to1 ((float) templates[templateIndex][i]));
    }
    lengthParam->setValueNotifyingHost (lengthParam->convertTo0to1 (16.0f));
    storePattern (activeSlot);
}

void OpenGlitchAudioProcessor::shiftActivePattern (int direction)
{
    const int length = juce::jlimit (1, 16, (int) std::lround (lengthRaw->load()));
    std::array<int, 16> values {};
    for (int i = 0; i < 16; ++i)
        values[(size_t) i] = (int) std::lround (paramLinks[(size_t) i].value->load());

    const juce::ScopedValueSetter<bool> loading (loadingPattern, true);
    for (int i = 0; i < length; ++i)
    {
        const int from = ((i - direction) % length + length) % length;
        auto* p = stepParams[(size_t) i];
        p->setValueNotifyingHost (p->convertTo0to1 ((float) values[(size_t) from]));
    }
    storePattern (activeSlot);
}

void OpenGlitchAudioProcessor::randomizeActivePattern()
{
    auto rng = makeSeededRng (seedRaw, dicePressCount);
    const juce::ScopedValueSetter<bool> loading (loadingPattern, true);
    int previous = 0;
    for (auto* p : stepParams)
    {
        // ~45% rests keep it musical; the rest is any of the nine effects,
        // sometimes tied into a longer span like the original's wide blocks.
        int effect = rng.nextFloat() < 0.45f ? 0 : rng.nextInt ({ 1, 10 });
        if (previous != 0 && effect != 0 && rng.nextFloat() < 0.3f)
            effect = 10;
        p->setValueNotifyingHost (p->convertTo0to1 ((float) effect));
        previous = effect;
    }
    storePattern (activeSlot);
}

void OpenGlitchAudioProcessor::copyActivePatternTo (int slot)
{
    if (slot < 0 || slot >= numPatterns)
        return;
    storePattern (slot);
    patternParam->setValueNotifyingHost (patternParam->convertTo0to1 ((float) slot));
}

void OpenGlitchAudioProcessor::parameterChanged (const juce::String& parameterID, float)
{
    if (loadingPattern)
        return;
    if (parameterID == "pattern_select")
        patternSelected.store (true);
    else
        patternEdited.store (true);
    triggerAsyncUpdate();
}

void OpenGlitchAudioProcessor::handleAsyncUpdate()
{
    if (patternEdited.exchange (false))
        storePattern (activeSlot);

    const int midiSlot = pendingMidiPattern.exchange (-1);
    if (midiSlot >= 0)
        patternParam->setValueNotifyingHost (patternParam->convertTo0to1 ((float) midiSlot));

    if (patternSelected.exchange (false) || midiSlot >= 0)
    {
        const int slot = patternParam->getIndex();
        if (slot != activeSlot)
        {
            activeSlot = slot;
            loadPattern (slot);
        }
    }
}

void OpenGlitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The default 2KB input queue overflows on the first block (defaults +
    // full APVTS push + transport all land before the first process call),
    // silently dropping the later messages. 32KB gives ample headroom.
    activePostEffect = 0;
    std::fill (std::begin (lastSentPost), std::end (lastSentPost),
               std::numeric_limits<float>::quiet_NaN());
    heavy.reset (hv_OpenGlitch_new_with_options (sampleRate, 10, 32, 2));
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
    standalonePpq = 0.0;
    lastSentBpm = 0.0;
    wasPlaying = false;
    lastFiredTick = std::numeric_limits<long long>::min();
    playheadStep.store (-1, std::memory_order_relaxed);

    ensureScratch (samplesPerBlock);
}

void OpenGlitchAudioProcessor::ensureScratch (int numSamples)
{
    if (numSamples <= scratchCapacity)
        return;

    const int channelLength = (numSamples + 7) & ~7; // keep each channel a SIMD multiple
    scratchAllocation.malloc ((size_t) channelLength * 4 + 8);
    auto* base = reinterpret_cast<float*> (
        (reinterpret_cast<std::uintptr_t> (scratchAllocation.get()) + 31u) & ~std::uintptr_t (31));
    for (int i = 0; i < 4; ++i)
        scratch[i] = base + (size_t) i * (size_t) channelLength;
    scratchCapacity = numSamples;
}

void OpenGlitchAudioProcessor::releaseResources()
{
    heavy.reset();
}

bool OpenGlitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // The Heavy context is fixed stereo internally; the wrapper adapts mono
    // hosts (duplicate in, average out). Fanning stereo down to mono is the
    // only combination refused.
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    if ((in != mono && in != stereo) || (out != mono && out != stereo))
        return false;
    return ! (in == stereo && out == mono);
}

void OpenGlitchAudioProcessor::heavySendHook (HeavyContextInterface* context, const char*,
                                              hv_uint32_t sendHash, const HvMessage* msg)
{
    if (sendHash == HV_OPENGLITCH_PARAM_OUT_PLAYHEAD)
        if (auto* self = static_cast<OpenGlitchAudioProcessor*> (hv_getUserData (context)))
            self->playheadStep.store ((int) hv_msg_getFloat (msg, 0), std::memory_order_relaxed);
}

void OpenGlitchAudioProcessor::updateLfos (int numSamples)
{
    std::fill (std::begin (modContrib), std::end (modContrib), 0.0f);

    const double sr = getSampleRate();
    if (sr <= 0.0)
        return;

    double bpm = (double) displayBpm.load (std::memory_order_relaxed);
    double ppq = 0.0;
    bool hosted = false, playing = false;
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

    const double blockSeconds = (double) numSamples / sr;
    const double sixteenthHz = bpm * 4.0 / 60.0;
    const bool lock = hosted && playing; // ppq-lock keeps synced LFOs bar-aligned
    auto nextRandom = [this] { return lfoRng.nextDouble() * 2.0 - 1.0; };
    auto intOf = [] (std::atomic<float>* raw) { return (int) std::lround (raw->load()); };

    // LFO 2 first: its output may steer LFO 1 (the derivative behaviour).
    const double steps2 = lfo::stepsPerCycle (intOf (lfoRaw[1][1]));
    const double v2 = lfo::advance (lfoStates[1], intOf (lfoRaw[1][0]), sixteenthHz / steps2,
                                    (double) lfoRaw[1][2]->load(), blockSeconds,
                                    lock, ppq * 4.0 / steps2, nextRandom);

    const int target2 = intOf (lfoRaw[1][3]);
    double rateFactor = 1.0, depthScale = 1.0;
    if (target2 == lfo::lfo1Rate)
        rateFactor = std::exp2 (2.0 * v2); // +/- 2 octaves of LFO-rate FM
    else if (target2 == lfo::lfo1Depth)
        depthScale = std::clamp (1.0 + v2, 0.0, 2.0);
    else if (target2 > lfo::off && target2 < lfo::lfo1Rate)
        modContrib[target2] += (float) v2;

    const double steps1 = lfo::stepsPerCycle (intOf (lfoRaw[0][1]));
    const double depth1 = std::clamp ((double) lfoRaw[0][2]->load() * depthScale, 0.0, 1.0);
    const double v1 = lfo::advance (lfoStates[0], intOf (lfoRaw[0][0]),
                                    sixteenthHz / steps1 * rateFactor, depth1, blockSeconds,
                                    lock && juce::exactlyEqual (rateFactor, 1.0),
                                    ppq * 4.0 / steps1, nextRandom);

    const int target1 = intOf (lfoRaw[0][3]);
    if (target1 > lfo::off && target1 < lfo::lfo1Rate)
        modContrib[target1] += (float) v1;

    // Filter sweep envelopes: phase within the current step/span.
    {
        const double g16 = hosted ? ppq * 4.0 : standalonePpq * 4.0;
        const bool sweeping = playing || ! hosted;
        const double phase = sweepSpanLength > 0
            ? (g16 - (double) sweepSpanStartG) / (double) sweepSpanLength
            : 0.0;
        auto factorFor = [&] (std::atomic<float>* shapeRaw, std::atomic<float>* amtRaw)
        {
            if (! sweeping)
                return 1.0f;
            const int shape = (int) std::lround (shapeRaw->load (std::memory_order_relaxed));
            if (shape == lfo::sweepOff)
                return 1.0f;
            const double amount = (double) amtRaw->load (std::memory_order_relaxed);
            return (float) std::exp2 (amount * lfo::sweepValue (shape, phase));
        };
        masterSweepFactor = factorFor (masterSweepRaw[0], masterSweepRaw[1]);
        postSweepFactor = activePostEffect >= 1
            ? factorFor (sweepRaw[activePostEffect - 1][0], sweepRaw[activePostEffect - 1][1])
            : 1.0f;
    }

    lfoPhaseA[0].store ((float) lfoStates[0].phase, std::memory_order_relaxed);
    lfoPhaseA[1].store ((float) lfoStates[1].phase, std::memory_order_relaxed);
    lfoValueA[0].store ((float) v1, std::memory_order_relaxed);
    lfoValueA[1].store ((float) v2, std::memory_order_relaxed);
}

void OpenGlitchAudioProcessor::pushPostParameters()
{
    // Live knob tweaks of the currently sounding effect stream immediately.
    if (activePostEffect < 1)
        return;
    for (int k = 0; k < 5; ++k)
    {
        float v = postRaw[activePostEffect - 1][k]->load (std::memory_order_relaxed);
        if (k == 1) // filter freq rides the sweep envelope
            v = juce::jlimit (100.0f, 20000.0f, v * postSweepFactor);
        if (! juce::exactlyEqual (v, lastSentPost[k]))
            if (hv_sendFloatToReceiver (heavy.get(), postHash[k], v))
                lastSentPost[k] = v;
    }
}

void OpenGlitchAudioProcessor::pushChangedParameters()
{
    for (auto& link : paramLinks)
    {
        float v = link.value->load (std::memory_order_relaxed);

        // Tempo-synced ring modulator: the Hz knob is replaced by a musical
        // division of the current tempo.
        if (link.modTarget == lfo::modFreq)
        {
            const int sync = (int) std::lround (modSyncRaw->load());
            if (sync > 0)
            {
                static const float syncSteps[] = { 0.0f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
                const float stepMs = 15000.0f / juce::jmax (1.0f, displayBpm.load());
                v = 1000.0f / (stepMs * syncSteps[sync]);
            }
        }

        if (link.modTarget == lfo::filterFreq)
            v *= masterSweepFactor; // sweep first; applyMod clamps to range

        if (link.modTarget != lfo::off)
            v = lfo::applyMod (link.modTarget, v, (double) modContrib[link.modTarget]);

        if (! juce::exactlyEqual (v, link.lastSent)) // NaN sentinel compares unequal, forcing the first send
        {
            // Only mark as sent if the queue accepted it, so a full queue
            // means a retry next block instead of a silently stale receiver.
            if (hv_sendFloatToReceiver (heavy.get(), link.hash, v))
                link.lastSent = v;
        }
    }
}

void OpenGlitchAudioProcessor::sendTick (int stepIndex, double delayMs)
{
    static const hv_uint32_t hostTickHash = hv_stringToHash ("host_tick");
    if (hv_sendMessageToReceiverV (heavy.get(), hostTickHash, delayMs, "f", (double) stepIndex))
        diagTickCount.fetch_add (1, std::memory_order_relaxed);
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

    diagTransport.store (hosted ? (playing ? hostedPlaying : hostedStopped) : standaloneClock,
                         std::memory_order_relaxed);

    // JUCE is the only clock: without a host timeline a virtual one free-runs,
    // so spans/swing/chaos behave identically hosted and standalone.
    if (! hosted)
    {
        playing = true;
        ppq = standalonePpq;
        if (const double sr = getSampleRate(); sr > 0.0)
            standalonePpq += (double) numSamples * bpm / (60.0 * sr);
    }

    {
        const double barPosition = ppq * 0.25;
        diagBarPhase.store ((float) (barPosition - std::floor (barPosition)),
                            std::memory_order_relaxed);
    }

    if (hostSyncActive != 0)
    {
        // Only latch on success: a full queue means a retry next block.
        if (hv_sendFloatToReceiver (heavy.get(), HV_OPENGLITCH_PARAM_IN_HOST_PLAYING, 0.0f))
            hostSyncActive = 0;
    }

    displayBpm.store ((float) bpm, std::memory_order_relaxed);

    // Effects derive slice lengths and ramp times from host_bpm.
    if (std::abs (bpm - lastSentBpm) > 0.001)
    {
        if (hv_sendFloatToReceiver (heavy.get(), HV_OPENGLITCH_PARAM_IN_HOST_BPM, (float) bpm))
            lastSentBpm = bpm;
    }

    if (playing)
    {
        const double sr = getSampleRate();
        if (sr <= 0.0 || bpm <= 0.0)
            return; // a hostless/misconfigured caller must not unbound the tick loop

        const double ppqEnd = ppq + (double) numSamples * bpm / (60.0 * sr);
        const auto length = (long long) juce::jlimit (1, 16, (int) std::lround (lengthRaw->load()));
        const double swingMs = (double) swingRaw->load() * 15000.0 / bpm; // fraction of a 16th
        auto stepOf = [length] (long long g) { return (int) (((g % length) + length) % length); };
        auto swingOf = [swingMs] (long long g) { return (g & 1) != 0 ? swingMs : 0.0; };

        // Span length at a step: 1 + the run of "Tie" (10) steps following it.
        auto spanAt = [this, length] (int step)
        {
            int n = 1;
            while (step + n < (int) length
                   && (int) std::lround (paramLinks[(size_t) (step + n)].value->load()) == 10)
                ++n;
            return n;
        };
        static const hv_uint32_t spanHash = hv_stringToHash ("glitch_spansteps");
        auto sendSpanAndTick = [&] (long long g, double delayMs)
        {
            const int step = stepOf (g);
            // Scheduled at the same offset as its tick so paired values stay
            // coherent even with several ticks inside one block.
            hv_sendMessageToReceiverV (heavy.get(), spanHash, delayMs, "f", (double) spanAt (step));

            // Snap the shared output stage to the firing effect's stored
            // settings (neutral for dry; ties keep the previous settings).
            const int v = (int) std::lround (paramLinks[(size_t) step].value->load());
            if (v != 10)
            {
                sweepSpanStartG = g;
                sweepSpanLength = spanAt (step);
                static const float neutral[5] = { 0.0f, 20000.0f, 0.0f, 1.0f, 1.0f };
                const int effect = (v >= 1 && v <= 9) ? v : 0;
                for (int k = 0; k < 5; ++k)
                {
                    float pv = effect > 0 ? postRaw[effect - 1][k]->load() : neutral[k];
                    if (k == 1 && effect > 0)
                    {
                        // freq leaves the gate already at the sweep's start value
                        const int shape = (int) std::lround (sweepRaw[effect - 1][0]->load());
                        if (shape != lfo::sweepOff)
                            pv = juce::jlimit (100.0f, 20000.0f,
                                pv * (float) std::exp2 ((double) sweepRaw[effect - 1][1]->load()
                                                        * lfo::sweepValue (shape, 0.0)));
                    }
                    hv_sendMessageToReceiverV (heavy.get(), postHash[k], delayMs, "f", (double) pv);
                    lastSentPost[k] = pv;
                }
                activePostEffect = effect;
            }

            sendTick (step, delayMs);
            lastFiredTick = g;
        };

        // Fire the current 16th immediately after any discontinuity
        // (transport start, relocate, loop wrap), then schedule every
        // boundary inside this block at its exact offset (odd 16ths are
        // pushed late by the swing amount).
        const auto g0 = (long long) std::floor (ppq * 4.0);
        if (g0 != lastFiredTick)
            sendSpanAndTick (g0, swingOf (g0));
        for (long long g = g0 + 1; (double) g / 4.0 < ppqEnd; ++g)
            sendSpanAndTick (g, ((double) g / 4.0 - ppq) * 60000.0 / bpm + swingOf (g));
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
    juce::ScopedNoDenormals noDenormals;

    if (heavy == nullptr)
        return;

    // MIDI notes C1..G1 (36..43) select pattern A..H, Glitch-style live
    // switching. Applied on the message thread via the async updater.
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const int slot = msg.getNoteNumber() - 36;
            if (slot >= 0 && slot < numPatterns)
            {
                pendingMidiPattern.store (slot);
                triggerAsyncUpdate();
            }
        }
    }

    updateLfos (buffer.getNumSamples());
    pushChangedParameters();
    pushPostParameters();
    pushTransport (buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();

    // Heavy renders in fixed SIMD sub-blocks of up to 8 samples; hv_process
    // requires a multiple of that. Trailing samples of an odd-sized host
    // block are left untouched (dry) — hosts almost always send multiples of 8.
    const int processable = numSamples - (numSamples % 8);
    if (processable == 0)
        return;

    ensureScratch (numSamples);

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    juce::FloatVectorOperations::copy (scratch[0], buffer.getReadPointer (0), numSamples);
    juce::FloatVectorOperations::copy (scratch[1], buffer.getReadPointer (numIn > 1 ? 1 : 0),
                                       numSamples);

    float* inputs[2]  = { scratch[0], scratch[1] };
    float* outputs[2] = { scratch[2], scratch[3] };

    hv_process (heavy.get(), inputs, outputs, processable);

    if (numOut > 1)
    {
        juce::FloatVectorOperations::copy (buffer.getWritePointer (0), scratch[2], processable);
        juce::FloatVectorOperations::copy (buffer.getWritePointer (1), scratch[3], processable);
    }
    else
    {
        auto* out = buffer.getWritePointer (0);
        juce::FloatVectorOperations::copy (out, scratch[2], processable);
        juce::FloatVectorOperations::add (out, scratch[3], processable);
        juce::FloatVectorOperations::multiply (out, 0.5f, processable);
    }

    // Master output volume (JUCE-side, ramped to avoid zipper noise).
    {
        const float target = masterVolumeRaw->load (std::memory_order_relaxed);
        for (int ch = 0; ch < numOut && ch < buffer.getNumChannels(); ++ch)
            buffer.applyGainRamp (ch, 0, processable, lastAppliedVolume, target);
        lastAppliedVolume = target;
    }

    // Diagnostic wet meter: how strongly the engine altered channel 0.
    {
        double diff = 0.0, ref = 0.0;
        const auto* in = scratch[0];
        const auto* out = buffer.getReadPointer (0);
        for (int i = 0; i < processable; ++i)
        {
            const double d = (double) out[i] - in[i];
            diff += d * d;
            ref += (double) in[i] * in[i];
        }
        const float instant = ref > 1e-9 ? (float) std::sqrt (diff / ref) : 0.0f;
        const float smoothed = 0.9f * diagWet.load (std::memory_order_relaxed) + 0.1f * instant;
        diagWet.store (smoothed, std::memory_order_relaxed);
    }

    // Amplitude history for the LCD waveform.
    {
        const int idx = outputPeakIndex.load (std::memory_order_relaxed);
        outputPeaks[(size_t) idx & 255] = buffer.getMagnitude (0, 0, processable);
        outputPeakIndex.store (idx + 1, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* OpenGlitchAudioProcessor::createEditor()
{
    return new OpenGlitchAudioProcessorEditor (*this);
}

bool OpenGlitchAudioProcessor::hasEditor() const              { return true; }

juce::AudioProcessorParameter* OpenGlitchAudioProcessor::getBypassParameter() const
{
    return bypassParam;
}

const juce::String OpenGlitchAudioProcessor::getName() const  { return "OpenGlitch"; }
bool OpenGlitchAudioProcessor::acceptsMidi() const            { return true; }
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
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            patternsTree(); // migrate pre-pattern states
            activeSlot = patternParam->getIndex();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenGlitchAudioProcessor();
}
