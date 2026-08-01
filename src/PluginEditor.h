#pragma once

#include <array>

#include "PluginProcessor.h"

namespace glitch
{
// The visual identity: dark chassis, one signature neon per effect, used
// consistently in the grid, the panel titles and the knobs.
namespace palette
{
    const juce::Colour bg         { 0xff17191d };
    const juce::Colour panel      { 0xff1f2329 };
    const juce::Colour cellOff    { 0xff272c34 };
    const juce::Colour cellStroke { 0xff101216 };
    const juce::Colour text       { 0xffd8dce4 };
    const juce::Colour textDim    { 0xff7b8290 };
    const juce::Colour lcd        { 0xff7cffb0 };
    const juce::Colour lamp       { 0xffffb74d };
}

juce::Colour effectColour (int effectIndex); // 0 = dry, 1..9 = effects
const char* effectName (int effectIndex);
} // namespace glitch

class OpenGlitchLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OpenGlitchLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// The 9x16 effect matrix: click assigns, click-again clears, drag paints,
// right-click erases the column. One effect per column, like the original.
class StepMatrix : public juce::Component
{
public:
    explicit StepMatrix (juce::AudioProcessorValueTreeState& state);

    std::function<void (int effectIndex)> onEffectTouched;

    void setPlayheadColumn (int column);
    void clearAll();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    float cellWidth() const;
    float rowHeight() const;
    float columnX (int col) const;
    bool locateCell (juce::Point<float> pos, int& col, int& row) const;
    void setStep (int col, int value);

    static constexpr int labelWidth = 112;
    static constexpr int lampHeight = 18;
    static constexpr float groupGap = 7.0f;

    std::array<int, 16> steps {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, 16> attachments;
    std::unique_ptr<juce::ParameterAttachment> lengthAttachment;
    int patternLength = 16;
    int playheadColumn = -1;
    bool eraseGesture = false;
    int lastPaintCol = -1, lastPaintRow = -1; // for drag-to-span gestures
};

// Strip below the matrix: pattern slots A..H (click to switch, shift-click to
// copy the current pattern there), plus pattern length and swing.
class SequencerBar : public juce::Component
{
public:
    explicit SequencerBar (OpenGlitchAudioProcessor& proc);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> slotBounds (int slot) const;

    OpenGlitchAudioProcessor& processor;
    std::unique_ptr<juce::ParameterAttachment> patternAttachment;
    int activeSlot = 0;
    juce::Slider lengthSlider, swingSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lengthAttachment,
        swingAttachment;
};

// Bottom panel: swaps to show the knobs of whichever effect was last touched.
class EffectPanel : public juce::Component
{
public:
    explicit EffectPanel (juce::AudioProcessorValueTreeState& state);

    void setEffect (int effectIndex);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    Knob* addKnob (juce::AudioProcessorValueTreeState& state, const char* paramID, const char* name);

    std::vector<std::unique_ptr<Knob>> knobs;
    std::array<std::vector<Knob*>, 10> knobsForEffect;
    juce::Label title, blurb;
    juce::ComboBox modSyncBox;
    juce::Label modSyncLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modSyncAttachment;
    int currentEffect = 3;
};

// Two tempo-synced LFOs. LFO 2's target list includes LFO 1's rate and depth
// for cascaded, derivative modulation.
class LfoPanel : public juce::Component
{
public:
    explicit LfoPanel (juce::AudioProcessorValueTreeState& state);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Column
    {
        juce::ComboBox shape, rate, target;
        juce::Slider depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAtt,
            rateAtt, targetAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt;
    };
    Column columns[2];
};

// Right-hand strip: chaos + master chain, and the bypass switch.
class MasterPanel : public juce::Component
{
public:
    explicit MasterPanel (juce::AudioProcessorValueTreeState& state);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Fader
    {
        juce::Slider slider;
        juce::Label nameLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addFader (juce::AudioProcessorValueTreeState& state, const char* paramID,
                   const char* name, juce::Colour colour);

    std::vector<std::unique_ptr<Fader>> faders;
    juce::ComboBox filterTypeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
};

class OpenGlitchAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit OpenGlitchAudioProcessorEditor (OpenGlitchAudioProcessor&);
    ~OpenGlitchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    OpenGlitchAudioProcessor& processorRef;
    OpenGlitchLookAndFeel lookAndFeel;

    StepMatrix matrix;
    SequencerBar sequencerBar;
    EffectPanel effectPanel;
    LfoPanel lfoPanel;
    MasterPanel masterPanel;
    juce::Label lcdLabel;
    juce::Label statusLabel; // transport/tick diagnostics
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton diceButton { "DICE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGlitchAudioProcessorEditor)
};
