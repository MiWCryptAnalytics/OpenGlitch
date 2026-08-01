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
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff262b33));
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, cellStroke);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xffffb300));
    setColour (juce::PopupMenu::highlightedTextColourId, bg);
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
    using namespace glitch::palette;

    if (style == juce::Slider::LinearHorizontal)
    {
        const float cy = (float) y + (float) height * 0.5f;
        const auto fill = slider.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Rectangle<float> track ((float) x, cy - 3.0f, (float) width, 6.0f);
        g.setColour (juce::Colour (0xff30353e));
        g.fillRoundedRectangle (track, 3.0f);

        juce::Rectangle<float> filled ((float) x, cy - 3.0f, sliderPos - (float) x, 6.0f);
        g.setColour (fill.withAlpha (0.85f));
        g.fillRoundedRectangle (filled, 3.0f);

        juce::Rectangle<float> thumb (sliderPos - 4.0f, cy - 9.0f, 8.0f, 18.0f);
        g.setColour (text);
        g.fillRoundedRectangle (thumb, 3.0f);
        g.setColour (cellStroke);
        g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
        return;
    }

    if (style != juce::Slider::LinearVertical)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                                minSliderPos, maxSliderPos, style, slider);
        return;
    }

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

    if (auto* lengthParam = state.getParameter ("seq_length"))
    {
        lengthAttachment = std::make_unique<juce::ParameterAttachment> (
            *lengthParam,
            [this] (float v)
            {
                patternLength = juce::jlimit (1, 16, (int) std::lround (v));
                repaint();
            });
        lengthAttachment->sendInitialUpdate();
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

void StepMatrix::handlePress (int col, int row)
{
    // Pressing a cell that already holds this effect arms a clear-on-release,
    // so a plain click toggles but press-and-drag stretches the block instead.
    pendingClearOnUp = (steps[(size_t) col] == row + 1);
    if (! pendingClearOnUp)
        setStep (col, row + 1);
    lastPaintCol = col;
    lastPaintRow = row;
    if (onEffectTouched)
        onEffectTouched (row + 1);
}

void StepMatrix::handleDrag (int col, int row)
{
    if (col == lastPaintCol && row == lastPaintRow)
        return; // wobble within the pressed cell
    pendingClearOnUp = false;

    // Dragging rightwards along the same row extends the block as a span
    // (Tie steps), like stretching a block in the original Glitch. A fast
    // drag can skip columns, so every column passed over is filled in.
    // Changing row (or dragging left) paints a fresh trigger instead.
    if (row == lastPaintRow && col > lastPaintCol && steps[(size_t) lastPaintCol] != 0)
    {
        for (int c = lastPaintCol + 1; c <= col; ++c)
            setStep (c, 10);
    }
    else
    {
        setStep (col, row + 1);
    }
    lastPaintCol = col;
    lastPaintRow = row;
}

void StepMatrix::handleRelease()
{
    if (pendingClearOnUp && lastPaintCol >= 0)
        setStep (lastPaintCol, 0);
    pendingClearOnUp = false;
}

void StepMatrix::handleErase (int col)
{
    setStep (col, 0);
}

void StepMatrix::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    int col = 0, row = 0;

    if (e.mods.isPopupMenu())
    {
        if (locateCell (pos, col, row))
            handleErase (col);
        return;
    }

    if (pos.x < (float) labelWidth) // row label: focus the effect panel only
    {
        row = (int) std::floor ((pos.y - lampHeight) / rowHeight());
        if (row >= 0 && row <= 8 && onEffectTouched)
            onEffectTouched (row + 1);
        return;
    }

    if (locateCell (pos, col, row))
        handlePress (col, row);
}

void StepMatrix::mouseDrag (const juce::MouseEvent& e)
{
    int col = 0, row = 0;
    if (! locateCell (e.position, col, row))
        return;
    if (e.mods.isPopupMenu())
        handleErase (col); // right-drag sweeps columns clear
    else
        handleDrag (col, row);
}

void StepMatrix::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        handleRelease();
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

        // Pass 1: background cells (spans are drawn on top afterwards).
        for (int c = 0; c < 16; ++c)
        {
            auto cell = juce::Rectangle<float> (columnX (c), ry, cw, rh).reduced (2.0f);
            const bool onPlayhead = c == playheadColumn;

            // Alternate the 4-step beat groups so the bar stays readable.
            auto base = ((c / 4) % 2 == 1) ? cellOff.brighter (0.13f) : cellOff;
            if (c >= patternLength)
                base = base.darker (0.5f);
            g.setColour (onPlayhead ? base.brighter (0.25f) : base);
            g.fillRoundedRectangle (cell, 3.5f);
            g.setColour (cellStroke);
            g.drawRoundedRectangle (cell, 3.5f, 1.0f);
        }

        // Pass 2: active blocks. A run of Tie steps (value 10) after a
        // trigger renders as one elongated block, like the original Glitch.
        for (int c = 0; c < 16; ++c)
        {
            if (steps[(size_t) c] != r + 1)
                continue;
            int end = c;
            while (end + 1 < 16 && steps[(size_t) (end + 1)] == 10)
                ++end;

            auto block = juce::Rectangle<float> (columnX (c), ry,
                                                 columnX (end) + cw - columnX (c), rh)
                             .reduced (2.0f);
            const bool onPlayhead = playheadColumn >= c && playheadColumn <= end;
            const bool beyondLength = c >= patternLength;

            if (onPlayhead)
            {
                g.setColour (colour.withAlpha (0.45f));
                g.fillRoundedRectangle (block.expanded (3.0f), 5.0f);
            }
            auto fill = onPlayhead ? colour.brighter (0.35f) : colour.withAlpha (0.88f);
            if (beyondLength)
                fill = fill.withMultipliedAlpha (0.25f);
            g.setColour (fill);
            g.fillRoundedRectangle (block, 3.5f);
            g.setColour (colour.brighter (0.6f).withMultipliedAlpha (beyondLength ? 0.25f : 1.0f));
            g.drawRoundedRectangle (block, 3.5f, 1.2f);

            // Tick marks at internal step boundaries keep the grid readable.
            g.setColour (colour.darker (0.6f).withAlpha (0.6f));
            for (int t = c + 1; t <= end; ++t)
                g.fillRect (columnX (t) - 1.0f, ry + rh * 0.28f, 1.5f, rh * 0.44f);

            c = end;
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
// SequencerBar
// ---------------------------------------------------------------------------
SequencerBar::SequencerBar (OpenGlitchAudioProcessor& proc)
    : processor (proc)
{
    if (auto* param = proc.apvts.getParameter ("pattern_select"))
    {
        patternAttachment = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this] (float v)
            {
                activeSlot = juce::jlimit (0, OpenGlitchAudioProcessor::numPatterns - 1,
                                           (int) std::lround (v));
                repaint();
            });
        patternAttachment->sendInitialUpdate();
    }

    for (auto* slider : { &lengthSlider, &swingSlider })
    {
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 16);
        addAndMakeVisible (*slider);
    }
    lengthSlider.setColour (juce::Slider::rotarySliderFillColourId, glitch::palette::lamp);
    swingSlider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe040fb));

    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "seq_length", lengthSlider);
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "seq_swing", swingSlider);
}

juce::Rectangle<float> SequencerBar::slotBounds (int slot) const
{
    return { 58.0f + (float) slot * 19.5f, 3.0f, 17.5f, (float) getHeight() - 6.0f };
}

void SequencerBar::resized()
{
    lengthSlider.setBounds (428, 0, 150, getHeight());
    swingSlider.setBounds (622, 0, juce::jmax (100, getWidth() - 622), getHeight());
}

void SequencerBar::mouseDown (const juce::MouseEvent& e)
{
    for (int slot = 0; slot < OpenGlitchAudioProcessor::numPatterns; ++slot)
    {
        if (slotBounds (slot).contains (e.position))
        {
            if (e.mods.isShiftDown())
                processor.copyActivePatternTo (slot); // copy current pattern here, then switch
            else if (patternAttachment != nullptr)
                patternAttachment->setValueAsCompleteGesture ((float) slot);
            return;
        }
    }
}

void SequencerBar::paint (juce::Graphics& g)
{
    using namespace glitch::palette;

    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (10.5f)).boldened());
    g.drawText ("PTN", 0, 0, 30, getHeight(), juce::Justification::centredLeft);
    g.drawText ("LENGTH", 378, 0, 50, getHeight(), juce::Justification::centredLeft);
    g.drawText ("SWING", 584, 0, 40, getHeight(), juce::Justification::centredLeft);

    const auto accent = glitch::effectColour (3);
    for (int slot = 0; slot < OpenGlitchAudioProcessor::numPatterns; ++slot)
    {
        const auto r = slotBounds (slot);
        const bool isActive = slot == activeSlot;
        g.setColour (isActive ? accent : cellOff);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (isActive ? accent.brighter (0.5f) : cellStroke);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (isActive ? bg : textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
        g.drawText (juce::String (slot + 1), r, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------
// EffectPanel
// ---------------------------------------------------------------------------
EffectPanel::EffectPanel (juce::AudioProcessorValueTreeState& state)
    : stateRef (state)
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

    // The modulator gets a tempo-sync selector next to its frequency knob.
    if (auto* syncParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("fx_mod_sync")))
    {
        modSyncBox.addItemList (syncParam->choices, 1);
        addChildComponent (modSyncBox);
        modSyncLabel.setText ("SYNC", juce::dontSendNotification);
        modSyncLabel.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
        modSyncLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
        modSyncLabel.setJustificationType (juce::Justification::centred);
        addChildComponent (modSyncLabel);
        modSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, "fx_mod_sync", modSyncBox);
    }

    outCaption.setText ("OUTPUT", juce::dontSendNotification);
    outCaption.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    outCaption.setColour (juce::Label::textColourId, glitch::palette::textDim);
    addAndMakeVisible (outCaption);
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("fx1_post_mode")))
        outModeBox.addItemList (modeParam->choices, 1);
    addAndMakeVisible (outModeBox);
    static const char* const outNames[4] = { "FREQ", "PAN", "MIX", "GAIN" };
    for (int k = 0; k < 4; ++k)
    {
        outKnobs[k].slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        outKnobs[k].slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 14);
        addAndMakeVisible (outKnobs[k].slider);
        outKnobs[k].label.setText (outNames[k], juce::dontSendNotification);
        outKnobs[k].label.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        outKnobs[k].label.setColour (juce::Label::textColourId, glitch::palette::textDim);
        outKnobs[k].label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (outKnobs[k].label);
    }

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
    modSyncBox.setVisible (currentEffect == 2);
    modSyncLabel.setVisible (currentEffect == 2);

    // Rebind the OUTPUT strip to this effect's parameters.
    const auto pid = [this] (const char* s) { return "fx" + juce::String (currentEffect) + "_" + s; };
    outModeAttachment.reset();
    outModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        stateRef, pid ("post_mode"), outModeBox);
    static const char* const outIds[4] = { "post_freq", "pan", "mix", "gain" };
    for (int k = 0; k < 4; ++k)
    {
        outKnobs[k].attachment.reset();
        outKnobs[k].slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        outKnobs[k].attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            stateRef, pid (outIds[k]), outKnobs[k].slider);
    }

    resized();
    repaint();
}

void EffectPanel::resized()
{
    title.setBounds (14, 8, 220, 24);
    blurb.setFont (juce::Font (juce::FontOptions (12.0f)));
    blurb.setBounds (14, 30, getWidth() - 28, 30);

    int x = 10;
    const int knobW = 84;
    const int top = 62;
    for (auto* knob : knobsForEffect[(size_t) currentEffect])
    {
        knob->nameLabel.setBounds (x, top, knobW, 14);
        knob->slider.setBounds (x, top + 14, knobW, 84);
        x += knobW + 4;
    }
    modSyncLabel.setBounds (x + 6, top, 90, 14);
    modSyncBox.setBounds (x + 6, top + 20, 90, 22);

    const int outTop = getHeight() - 92;
    outCaption.setBounds (14, outTop, 70, 14);
    outModeBox.setBounds (14, outTop + 20, 84, 22);
    int ox = 108;
    for (int k = 0; k < 4; ++k)
    {
        outKnobs[k].label.setBounds (ox, outTop, 64, 13);
        outKnobs[k].slider.setBounds (ox, outTop + 13, 64, getHeight() - outTop - 18);
        ox += 66;
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
// LfoPanel
// ---------------------------------------------------------------------------
LfoPanel::LfoPanel (juce::AudioProcessorValueTreeState& state)
{
    for (int n = 0; n < 2; ++n)
    {
        auto& col = columns[n];
        const auto id = [n] (const char* s) { return "lfo" + juce::String (n + 1) + "_" + s; };

        auto setupBox = [&] (juce::ComboBox& box, const char* param,
                             std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    state.getParameter (id (param))))
            {
                box.addItemList (choice->choices, 1);
                addAndMakeVisible (box);
                att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    state, id (param), box);
            }
        };
        setupBox (col.shape, "shape", col.shapeAtt);
        setupBox (col.rate, "rate", col.rateAtt);
        setupBox (col.target, "target", col.targetAtt);

        col.depth.setSliderStyle (juce::Slider::LinearHorizontal);
        col.depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
        col.depth.setColour (juce::Slider::rotarySliderFillColourId,
                             n == 0 ? juce::Colour (0xffe040fb) : juce::Colour (0xff4fc3f7));
        addAndMakeVisible (col.depth);
        col.depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, id ("depth"), col.depth);
    }
}

void LfoPanel::resized()
{
    const int colW = (getWidth() - 24) / 2;
    for (int n = 0; n < 2; ++n)
    {
        auto& col = columns[n];
        const int x = 8 + n * (colW + 8);
        col.shape.setBounds (x, 28, colW, 21);
        col.rate.setBounds (x, 53, colW, 21);
        col.target.setBounds (x, 78, colW, 21);
        col.depth.setBounds (x, 103, colW, 24);
    }
}

void LfoPanel::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const int colW = (getWidth() - 24) / 2;
    g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    g.setColour (juce::Colour (0xffe040fb));
    g.drawText ("LFO 1", 8, 6, colW, 18, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff4fc3f7));
    g.drawText (juce::String::fromUTF8 ("LFO 2 \xe2\x80\x94 can drive LFO 1"),
                16 + colW, 6, colW + 60, 18, juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
// MasterPanel
// ---------------------------------------------------------------------------
MasterPanel::MasterPanel (juce::AudioProcessorValueTreeState& state)
{
    addFader (state, "seq_chaos", "CHAOS", juce::Colour (0xffe040fb));
    addFader (state, "master_drive", "DRIVE", juce::Colour (0xffff7043));
    addFader (state, "master_drive_mix", "D.MIX", juce::Colour (0xffffab91));
    addFader (state, "master_reso", "RESO", juce::Colour (0xffba68c8));
    addFader (state, "master_lowpass", "FILTER", juce::Colour (0xff4fc3f7));
    addFader (state, "master_filter_mix", "F.MIX", juce::Colour (0xff4dd0e1));
    addFader (state, "seq_declick", "CLICK", juce::Colour (0xffa1887f));
    addFader (state, "seq_stepenv", "ENV", juce::Colour (0xff90a4ae));

    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, glitch::palette::lcd);
    addAndMakeVisible (mixSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "master_mix", mixSlider);
    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
    mixLabel.setColour (juce::Label::textColourId,
                        glitch::palette::textDim.interpolatedWith (glitch::palette::lcd, 0.45f));
    addAndMakeVisible (mixLabel);

    if (auto* typeParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("master_filter_type")))
    {
        filterTypeBox.addItemList (typeParam->choices, 1);
        addAndMakeVisible (filterTypeBox);
        filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, "master_filter_type", filterTypeBox);
    }

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
    const int bypassH = 30;
    const int comboH = 22;
    const int mixH = 24;
    const int faderW = getWidth() / 4;
    const int bottomZone = bypassH + comboH + mixH + 26;
    const int rowH = (getHeight() - top - bottomZone) / 2;
    for (size_t i = 0; i < faders.size(); ++i)
    {
        const int x = ((int) i % 4) * faderW;
        const int y = top + ((int) i / 4) * rowH;
        faders[i]->nameLabel.setBounds (x, y, faderW, 14);
        faders[i]->slider.setBounds (x, y + 16, faderW, rowH - 20);
    }
    const int mixY = getHeight() - bottomZone + 2;
    mixLabel.setBounds (8, mixY, 30, mixH);
    mixSlider.setBounds (38, mixY, getWidth() - 44, mixH);
    filterTypeBox.setBounds (10, mixY + mixH + 4, getWidth() - 20, comboH);
    bypassButton.setBounds (10, getHeight() - bypassH - 6, getWidth() - 20, bypassH);
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
      sequencerBar (p),
      effectPanel (p.apvts),
      lfoPanel (p.apvts),
      masterPanel (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (matrix);
    addAndMakeVisible (sequencerBar);
    addAndMakeVisible (effectPanel);
    addAndMakeVisible (lfoPanel);
    addAndMakeVisible (masterPanel);

    matrix.onEffectTouched = [this] (int fx) { effectPanel.setEffect (fx); };

    lcdLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::plain)));
    lcdLabel.setColour (juce::Label::textColourId, glitch::palette::lcd);
    lcdLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (lcdLabel);

    clearButton.onClick = [this] { matrix.clearAll(); };
    addAndMakeVisible (clearButton);
    diceButton.onClick = [this] { processorRef.randomizeActivePattern(); };
    addAndMakeVisible (diceButton);
    fxDiceButton.onClick = [this] { processorRef.randomizeFxKnobs(); };
    addAndMakeVisible (fxDiceButton);
    shiftLeftButton.onClick = [this] { processorRef.shiftActivePattern (-1); };
    addAndMakeVisible (shiftLeftButton);
    shiftRightButton.onClick = [this] { processorRef.shiftActivePattern (1); };
    addAndMakeVisible (shiftRightButton);

    statusLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    statusLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    addAndMakeVisible (statusLabel);

    startTimerHz (30);
    timerCallback(); // seed the LCD before the first tick
    setSize (940, 700);
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
    const auto pattern = juce::String (processorRef.getActivePattern() + 1);
    lcdLabel.setText (pattern + " | " + stepText + " | "
                          + juce::String (processorRef.getDisplayBpm(), 1) + " BPM",
                      juce::dontSendNotification);

    static const char* const modes[] = { "no audio yet", "standalone clock",
                                         "host timeline: stopped", "host timeline: PLAYING" };
    statusLabel.setText (juce::String (modes[processorRef.getTransportMode() + 1])
                             + " | ticks " + juce::String (processorRef.getTickCount())
                             + " | fx " + juce::String ((int) std::round (
                                   processorRef.getWetActivity() * 100.0f)) + "%",
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
    g.drawText (juce::String::fromUTF8 ("a dblue Glitch 1.3 tribute  \xc2\xb7  Pure Data \xe2\x86\x92 hvcc \xe2\x86\x92 JUCE")
                    + "  |  build " + __DATE__ + " " + __TIME__,
                22, 40, 520, 16, juce::Justification::centredLeft);

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

    sequencerBar.setBounds (margin, matrix.getBottom() + 8, matrixRight - margin, 30);

    const int bottomY = sequencerBar.getBottom() + 8;
    const int bottomH = getHeight() - bottomY - margin;
    effectPanel.setBounds (margin, bottomY, 380, bottomH);
    lfoPanel.setBounds (margin + 390, bottomY, matrixRight - margin - 390, bottomH);

    const int lcdX = getWidth() - margin - 220;
    lcdLabel.setBounds (lcdX, 20, 214, 26);
    clearButton.setBounds (lcdX - 66, 20, 58, 26);
    fxDiceButton.setBounds (lcdX - 66 - 44, 20, 40, 26);
    diceButton.setBounds (lcdX - 66 - 44 - 58, 20, 54, 26);
    shiftRightButton.setBounds (lcdX - 66 - 44 - 58 - 32, 20, 28, 26);
    shiftLeftButton.setBounds (lcdX - 66 - 44 - 58 - 32 - 30, 20, 28, 26);
    statusLabel.setBounds (getWidth() - margin - 300, 48, 294, 13);
}
