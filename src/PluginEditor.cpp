#include "PluginEditor.h"

namespace glitch
{
juce::Colour effectColour (int effectIndex)
{
    switch (effectIndex)
    {
        case 1:  return juce::Colour (0xff4fc3f7); // TapeStop  - sky blue
        case 2:  return juce::Colour (0xffb388ff); // Modulator - violet
        case 3:  return juce::Colour (0xffffb300); // Retrigger - amber
        case 4:  return juce::Colour (0xff8bc34a); // Shuffler  - lime
        case 5:  return juce::Colour (0xfff06292); // Reverser  - pink
        case 6:  return juce::Colour (0xffef5350); // Crusher   - red
        case 7:  return juce::Colour (0xffffee58); // Gater     - yellow
        case 8:  return juce::Colour (0xff4db6ac); // Delay     - teal
        case 9:  return juce::Colour (0xff7986cb); // Stretcher - indigo
        default: return palette::cellOff;
    }
}

const char* effectName (int effectIndex)
{
    static const char* names[] = { "Dry", "TapeStop", "Modulator", "Retrigger", "Shuffler",
                                   "Reverser", "Crusher", "Gater", "Delay", "Stretcher" };
    return (effectIndex >= 0 && effectIndex <= 9) ? names[effectIndex] : "?";
}

static const char* effectBlurb (int effectIndex)
{
    switch (effectIndex)
    {
        case 1: return "Winds the tape down to a dead stop across the step. Below 1x it stops early, then sucks backwards.";
        case 2: return "Ring-modulates the input. Slow frequencies tremolo, fast ones turn drums into robot bells.";
        case 3: return "Loops the first slice of the step. Detune the pitch for stutter arpeggios.";
        case 4: return "Swaps this step for a randomly chosen earlier one. Range sets how far back it digs.";
        case 5: return "Plays the last moments backwards. No knobs - pure time travel.";
        case 6: return "Decimates the sample rate, then drives the wreckage into a hard clip.";
        case 7: return "Chops the step into rhythmic slices. Duty sets the size of the holes.";
        case 8: return "Tempo-synced echo with damped feedback. Time is measured in steps.";
        case 9: return "Drags the step at reduced speed, tape-style. Half speed is an octave down.";
        default: return "Untouched audio. Paint a colour into the grid to glitch this step.";
    }
}
} // namespace glitch

// ---------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------
OpenGlitchLookAndFeel::OpenGlitchLookAndFeel()
{
    using namespace glitch::palette;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, textDim);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff4fc3f7));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262b33));
    setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe53935));
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void OpenGlitchLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float rotaryStartAngle,
                                              float rotaryEndAngle, juce::Slider& slider)
{
    using namespace glitch::palette;
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

    const float bodyR = radius * 0.72f;
    g.setColour (juce::Colour (0xff2a2f37));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (cellStroke);
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.5f);

    const float arcR = radius * 0.92f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff30353e));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (fill);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const auto tip = centre.getPointOnCircumference (bodyR * 0.85f, angle);
    const auto tail = centre.getPointOnCircumference (bodyR * 0.35f, angle);
    g.setColour (fill.brighter (0.4f));
    g.drawLine ({ tail, tip }, 2.4f);
}

void OpenGlitchLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float sliderPos, float minSliderPos, float maxSliderPos,
                                              juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

    using namespace glitch::palette;
    const float cx = (float) x + (float) width * 0.5f;
    const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Rectangle<float> track (cx - 3.0f, (float) y, 6.0f, (float) height);
    g.setColour (juce::Colour (0xff30353e));
    g.fillRoundedRectangle (track, 3.0f);

    juce::Rectangle<float> filled (cx - 3.0f, sliderPos, 6.0f, (float) y + (float) height - sliderPos);
    g.setColour (fill.withAlpha (0.85f));
    g.fillRoundedRectangle (filled, 3.0f);

    juce::Rectangle<float> thumb (cx - 10.0f, sliderPos - 4.0f, 20.0f, 8.0f);
    g.setColour (text);
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// StepMatrix
// ---------------------------------------------------------------------------
StepMatrix::StepMatrix (juce::AudioProcessorValueTreeState& state)
{
    for (int i = 0; i < 16; ++i)
    {
        auto* param = state.getParameter ("step_" + juce::String (i + 1));
        jassert (param != nullptr);
        attachments[(size_t) i] = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this, i] (float v)
            {
                steps[(size_t) i] = (int) std::lround (v);
                repaint();
            });
        attachments[(size_t) i]->sendInitialUpdate();
    }
}

float StepMatrix::cellWidth() const  { return ((float) getWidth() - labelWidth - 3.0f * groupGap) / 16.0f; }
float StepMatrix::rowHeight() const  { return ((float) getHeight() - lampHeight) / 9.0f; }
float StepMatrix::columnX (int col) const
{
    return (float) labelWidth + (float) col * cellWidth() + (float) (col / 4) * groupGap;
}

bool StepMatrix::locateCell (juce::Point<float> pos, int& col, int& row) const
{
    row = (int) std::floor ((pos.y - lampHeight) / rowHeight());
    if (row < 0 || row > 8)
        return false;
    for (int c = 0; c < 16; ++c)
    {
        if (pos.x >= columnX (c) && pos.x < columnX (c) + cellWidth())
        {
            col = c;
            return true;
        }
    }
    return false;
}

void StepMatrix::setStep (int col, int value)
{
    if (steps[(size_t) col] != value)
        attachments[(size_t) col]->setValueAsCompleteGesture ((float) value);
}

void StepMatrix::setPlayheadColumn (int column)
{
    if (playheadColumn != column)
    {
        playheadColumn = column;
        repaint();
    }
}

void StepMatrix::clearAll()
{
    for (int i = 0; i < 16; ++i)
        setStep (i, 0);
}

void StepMatrix::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    int col = 0, row = 0;

    if (e.mods.isPopupMenu())
    {
        if (locateCell (pos, col, row))
            setStep (col, 0);
        return;
    }

    if (pos.x < (float) labelWidth) // row label: focus the effect panel only
    {
        row = (int) std::floor ((pos.y - lampHeight) / rowHeight());
        if (row >= 0 && row <= 8 && onEffectTouched)
            onEffectTouched (row + 1);
        return;
    }

    if (! locateCell (pos, col, row))
        return;

    eraseGesture = (steps[(size_t) col] == row + 1);
    setStep (col, eraseGesture ? 0 : row + 1);
    if (onEffectTouched)
        onEffectTouched (row + 1);
}

void StepMatrix::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    int col = 0, row = 0;
    if (locateCell (e.position, col, row))
        setStep (col, eraseGesture ? 0 : row + 1);
}

void StepMatrix::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    const float cw = cellWidth();
    const float rh = rowHeight();

    // Playhead lamps
    for (int c = 0; c < 16; ++c)
    {
        const float lx = columnX (c) + cw * 0.5f;
        juce::Rectangle<float> lampRect (lx - 4.5f, 3.0f, 9.0f, 9.0f);
        if (c == playheadColumn)
        {
            g.setColour (lamp.withAlpha (0.35f));
            g.fillEllipse (lampRect.expanded (3.5f));
            g.setColour (lamp);
        }
        else
        {
            g.setColour (juce::Colour (0xff30353e));
        }
        g.fillEllipse (lampRect);
    }

    for (int r = 0; r < 9; ++r)
    {
        const auto colour = glitch::effectColour (r + 1);
        const float ry = lampHeight + (float) r * rh;

        // Row label with colour chip
        g.setColour (colour);
        g.fillRoundedRectangle (6.0f, ry + rh * 0.5f - 4.0f, 8.0f, 8.0f, 2.0f);
        g.setColour (text.interpolatedWith (colour, 0.35f));
        g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
        g.drawText (juce::String (glitch::effectName (r + 1)).toUpperCase(),
                    juce::Rectangle<float> (20.0f, ry, (float) labelWidth - 28.0f, rh),
                    juce::Justification::centredLeft);

        for (int c = 0; c < 16; ++c)
        {
            auto cell = juce::Rectangle<float> (columnX (c), ry, cw, rh).reduced (2.0f);
            const bool active = steps[(size_t) c] == r + 1;
            const bool onPlayhead = c == playheadColumn;

            if (active)
            {
                if (onPlayhead)
                {
                    g.setColour (colour.withAlpha (0.45f));
                    g.fillRoundedRectangle (cell.expanded (3.0f), 5.0f);
                }
                g.setColour (onPlayhead ? colour.brighter (0.35f) : colour.withAlpha (0.88f));
                g.fillRoundedRectangle (cell, 3.5f);
                g.setColour (colour.brighter (0.6f));
                g.drawRoundedRectangle (cell, 3.5f, 1.2f);
            }
            else
            {
                // Alternate the 4-step beat groups so the bar stays readable.
                auto base = ((c / 4) % 2 == 1) ? cellOff.brighter (0.13f) : cellOff;
                g.setColour (onPlayhead ? base.brighter (0.25f) : base);
                g.fillRoundedRectangle (cell, 3.5f);
                g.setColour (cellStroke);
                g.drawRoundedRectangle (cell, 3.5f, 1.0f);
            }
        }
    }

    // Subtle full-height playhead wash
    if (playheadColumn >= 0 && playheadColumn < 16)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (columnX (playheadColumn), 0.0f, cw, (float) getHeight(), 3.0f);
    }
}

// ---------------------------------------------------------------------------
// EffectPanel
// ---------------------------------------------------------------------------
EffectPanel::EffectPanel (juce::AudioProcessorValueTreeState& state)
{
    using namespace glitch::palette;

    title.setFont (juce::Font (juce::FontOptions (18.0f)).boldened());
    addAndMakeVisible (title);
    blurb.setFont (juce::Font (juce::FontOptions (13.0f)));
    blurb.setColour (juce::Label::textColourId, textDim);
    addAndMakeVisible (blurb);

    knobsForEffect[1] = { addKnob (state, "fx_tapestop_speed", "SPEED") };
    knobsForEffect[2] = { addKnob (state, "fx_mod_freq", "FREQUENCY") };
    knobsForEffect[3] = { addKnob (state, "fx_retrigger_rate", "SLICES"),
                          addKnob (state, "fx_retrigger_pitch", "PITCH") };
    knobsForEffect[4] = { addKnob (state, "fx_shuffle_range", "RANGE") };
    knobsForEffect[5] = {};
    knobsForEffect[6] = { addKnob (state, "fx_crush_rate", "RATE"),
                          addKnob (state, "fx_crush_drive", "DRIVE") };
    knobsForEffect[7] = { addKnob (state, "fx_gate_rate", "RATE"),
                          addKnob (state, "fx_gate_duty", "DUTY") };
    knobsForEffect[8] = { addKnob (state, "fx_delay_div", "TIME"),
                          addKnob (state, "fx_delay_feedback", "FEEDBACK") };
    knobsForEffect[9] = { addKnob (state, "fx_stretch_speed", "SPEED") };

    setEffect (3);
}

EffectPanel::Knob* EffectPanel::addKnob (juce::AudioProcessorValueTreeState& state,
                                         const char* paramID, const char* name)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    addChildComponent (knob->slider);

    knob->nameLabel.setText (name, juce::dontSendNotification);
    knob->nameLabel.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    knob->nameLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    knob->nameLabel.setJustificationType (juce::Justification::centred);
    addChildComponent (knob->nameLabel);

    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, paramID, knob->slider);

    knobs.push_back (std::move (knob));
    return knobs.back().get();
}

void EffectPanel::setEffect (int effectIndex)
{
    currentEffect = juce::jlimit (1, 9, effectIndex);
    const auto colour = glitch::effectColour (currentEffect);

    title.setText (juce::String (glitch::effectName (currentEffect)).toUpperCase(),
                   juce::dontSendNotification);
    title.setColour (juce::Label::textColourId, colour);
    blurb.setText (glitch::effectBlurb (currentEffect), juce::dontSendNotification);

    for (auto& knob : knobs)
    {
        knob->slider.setVisible (false);
        knob->nameLabel.setVisible (false);
    }
    for (auto* knob : knobsForEffect[(size_t) currentEffect])
    {
        knob->slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        knob->slider.setVisible (true);
        knob->nameLabel.setVisible (true);
    }

    resized();
    repaint();
}

void EffectPanel::resized()
{
    title.setBounds (14, 8, 220, 24);
    blurb.setBounds (14, 32, getWidth() - 28, 18);

    int x = 10;
    const int knobW = 96;
    const int top = 56;
    for (auto* knob : knobsForEffect[(size_t) currentEffect])
    {
        knob->nameLabel.setBounds (x, top, knobW, 14);
        knob->slider.setBounds (x, top + 14, knobW, getHeight() - top - 20);
        x += knobW + 6;
    }
}

void EffectPanel::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (glitch::effectColour (currentEffect).withAlpha (0.9f));
    g.fillRoundedRectangle (0.0f, 0.0f, 4.0f, bounds.getHeight(), 2.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// MasterPanel
// ---------------------------------------------------------------------------
MasterPanel::MasterPanel (juce::AudioProcessorValueTreeState& state)
{
    addFader (state, "seq_chaos", "CHAOS", juce::Colour (0xffe040fb));
    addFader (state, "master_drive", "DRIVE", juce::Colour (0xffff7043));
    addFader (state, "master_lowpass", "FILTER", juce::Colour (0xff4fc3f7));
    addFader (state, "master_mix", "MIX", glitch::palette::lcd);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, "bypass", bypassButton);
}

void MasterPanel::addFader (juce::AudioProcessorValueTreeState& state, const char* paramID,
                            const char* name, juce::Colour colour)
{
    auto fader = std::make_unique<Fader>();
    fader->slider.setSliderStyle (juce::Slider::LinearVertical);
    fader->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);
    fader->slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    addAndMakeVisible (fader->slider);

    fader->nameLabel.setText (name, juce::dontSendNotification);
    fader->nameLabel.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
    fader->nameLabel.setColour (juce::Label::textColourId,
                                glitch::palette::textDim.interpolatedWith (colour, 0.45f));
    fader->nameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (fader->nameLabel);

    fader->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, paramID, fader->slider);

    faders.push_back (std::move (fader));
}

void MasterPanel::resized()
{
    const int top = 26;
    const int bypassH = 32;
    const int faderW = getWidth() / (int) faders.size();
    for (size_t i = 0; i < faders.size(); ++i)
    {
        const int x = (int) i * faderW;
        faders[i]->nameLabel.setBounds (x, top, faderW, 14);
        faders[i]->slider.setBounds (x, top + 16, faderW, getHeight() - top - 16 - bypassH - 14);
    }
    bypassButton.setBounds (10, getHeight() - bypassH - 8, getWidth() - 20, bypassH);
}

void MasterPanel::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.drawText ("MASTER", getLocalBounds().removeFromTop (24), juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------
OpenGlitchAudioProcessorEditor::OpenGlitchAudioProcessorEditor (OpenGlitchAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      matrix (p.apvts),
      effectPanel (p.apvts),
      masterPanel (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (matrix);
    addAndMakeVisible (effectPanel);
    addAndMakeVisible (masterPanel);

    matrix.onEffectTouched = [this] (int fx) { effectPanel.setEffect (fx); };

    lcdLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    lcdLabel.setColour (juce::Label::textColourId, glitch::palette::lcd);
    lcdLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (lcdLabel);

    clearButton.onClick = [this] { matrix.clearAll(); };
    addAndMakeVisible (clearButton);

    startTimerHz (30);
    timerCallback(); // seed the LCD before the first tick
    setSize (940, 600);
}

OpenGlitchAudioProcessorEditor::~OpenGlitchAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void OpenGlitchAudioProcessorEditor::timerCallback()
{
    matrix.setPlayheadColumn (processorRef.getCurrentStep());

    const int step = processorRef.getCurrentStep();
    const auto stepText = step >= 0 ? juce::String::formatted ("STEP %02d", step + 1)
                                    : juce::String ("STOPPED");
    lcdLabel.setText (stepText + "  |  " + juce::String (processorRef.getDisplayBpm(), 1) + " BPM",
                      juce::dontSendNotification);
}

void OpenGlitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    g.fillAll (bg);

    const auto titleFont = juce::Font (juce::FontOptions (27.0f)).boldened();
    juce::GlyphArrangement measure;
    measure.addLineOfText (titleFont, "OPEN", 0.0f, 0.0f);
    const int openWidth = (int) std::ceil (measure.getBoundingBox (0, -1, true).getWidth());

    g.setFont (titleFont);
    g.setColour (text);
    g.drawText ("OPEN", 20, 12, openWidth + 4, 30, juce::Justification::centredLeft);
    g.setColour (glitch::effectColour (3)); // the retrigger amber is the brand
    g.drawText ("GLITCH", 20 + openWidth, 12, 200, 30, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (juce::String::fromUTF8 ("a dblue Glitch 1.3 tribute  \xc2\xb7  Pure Data \xe2\x86\x92 hvcc \xe2\x86\x92 JUCE"),
                22, 40, 400, 16, juce::Justification::centredLeft);

    // LCD bezel
    g.setColour (juce::Colour (0xff0d1f16));
    g.fillRoundedRectangle (lcdLabel.getBounds().toFloat().expanded (4.0f, 3.0f), 4.0f);
    g.setColour (lcd.withAlpha (0.25f));
    g.drawRoundedRectangle (lcdLabel.getBounds().toFloat().expanded (4.0f, 3.0f), 4.0f, 1.0f);
}

void OpenGlitchAudioProcessorEditor::resized()
{
    const int margin = 18;
    const int headerH = 64;
    const int masterW = 156;

    const int masterX = getWidth() - margin - masterW;
    masterPanel.setBounds (masterX, headerH + 14, masterW, getHeight() - headerH - 14 - margin);

    const int matrixRight = masterX - 14;
    matrix.setBounds (margin, headerH + 14, matrixRight - margin, 300);

    effectPanel.setBounds (margin, matrix.getBottom() + 12, matrixRight - margin,
                           getHeight() - matrix.getBottom() - 12 - margin);

    lcdLabel.setBounds (getWidth() - margin - 220, 20, 214, 26);
    clearButton.setBounds (getWidth() - margin - 220 - 78, 20, 64, 26);
}
