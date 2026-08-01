#include "PluginEditor.h"

// Normally injected by CMake from the project version; the fallback keeps
// ad-hoc builds (IDEs, single-file syntax checks) compiling.
#ifndef OPENGLITCH_VERSION
 #define OPENGLITCH_VERSION "0.0.0-dev"
#endif

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
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff262b33));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, cellStroke);
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
    g.setColour (fill.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
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
        if (column >= 0 && column < 16)
            columnFlash[column] = 1.0f;
        repaint();
    }
}

void StepMatrix::advanceFlashes()
{
    bool anyAlive = false;
    for (auto& flash : columnFlash)
    {
        if (flash > 0.02f)
        {
            flash *= 0.80f;
            anyAlive = true;
        }
        else
        {
            flash = 0.0f;
        }
    }
    if (anyAlive)
        repaint();
}

void StepMatrix::clearAll()
{
    for (int i = 0; i < 16; ++i)
        setStep (i, 0);
}

juce::String StepMatrix::getTooltip()
{
    const auto pos = getMouseXYRelative().toFloat();
    const int row = (int) std::floor ((pos.y - lampHeight) / rowHeight());
    if (row < 0 || row > 8)
        return {};

    if (pos.x < (float) labelWidth)
        return juce::String (glitch::effectName (row + 1)) + ": " + glitch::effectBlurb (row + 1);

    int col = 0, cellRow = 0;
    if (locateCell (pos, col, cellRow))
        return juce::String (glitch::effectName (cellRow + 1)) + ": "
               + glitch::effectBlurb (cellRow + 1)
               + "\n\nClick paints, click again clears. Hold and drag right to stretch "
                 "the block across steps (tie). Right-click erases the column.";
    return {};
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
        else if (columnFlash[c] > 0.03f)
        {
            g.setColour (lamp.withAlpha (columnFlash[c] * 0.8f)); // fading trail
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
            float flare = 0.0f;
            for (int fc = c; fc <= end; ++fc)
                flare = juce::jmax (flare, columnFlash[fc]);

            if (onPlayhead || flare > 0.05f)
            {
                g.setColour (colour.withAlpha (juce::jmax (onPlayhead ? 0.45f : 0.0f, 0.5f * flare)));
                g.fillRoundedRectangle (block.expanded (3.0f + 2.5f * flare), 5.0f);
            }
            auto fill = onPlayhead ? colour.brighter (0.35f + 0.4f * flare) : colour.withAlpha (0.88f);
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
    lengthSlider.setTooltip ("Pattern length in steps. Shorten it for odd meters and polymeter loops.");
    swingSlider.setTooltip ("Swing: every second 16th fires late.");

    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "seq_length", lengthSlider);
    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "seq_swing", swingSlider);
}

juce::Rectangle<float> SequencerBar::slotBounds (int slot) const
{
    return { 58.0f + (float) slot * 19.5f, 3.0f, 17.5f, (float) getHeight() - 6.0f };
}

juce::String SequencerBar::getTooltip()
{
    const auto pos = getMouseXYRelative().toFloat();
    for (int slot = 0; slot < OpenGlitchAudioProcessor::numPatterns; ++slot)
        if (slotBounds (slot).contains (pos))
            return "Pattern " + juce::String (slot + 1)
                   + ": click to switch. Shift-click copies the current pattern here. "
                     "MIDI notes 36-51 switch patterns live.";
    return {};
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
        if (isActive)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillRoundedRectangle (r.expanded (3.0f), 6.0f);
        }
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

    knobsForEffect[1] = { addKnob (state, "fx_tapestop_speed", "SPEED",
                                   "Wind-down speed in step lengths. Below 1x the tape stops early, then sucks backwards.") };
    knobsForEffect[2] = { addKnob (state, "fx_mod_freq", "FREQUENCY",
                                   "Ring-mod carrier. Slow is tremolo, fast turns drums into robot bells.") };
    knobsForEffect[3] = { addKnob (state, "fx_retrigger_rate", "SLICES",
                                   "How many slices of the step get looped."),
                          addKnob (state, "fx_retrigger_pitch", "PITCH",
                                   "Pitch of each repeat. Detune it for stutter arpeggios.") };
    knobsForEffect[4] = { addKnob (state, "fx_shuffle_range", "RANGE",
                                   "How many steps back the shuffler may dig.") };
    knobsForEffect[5] = { addKnob (state, "fx_rev_left", "LEFT",
                                   "How much of the left channel plays reversed."),
                          addKnob (state, "fx_rev_right", "RIGHT",
                                   "How much of the right channel plays reversed. Split them for ping-pong reversals.") };
    knobsForEffect[6] = { addKnob (state, "fx_crush_rate", "RATE",
                                   "Sample-rate decimation target."),
                          addKnob (state, "fx_crush_drive", "DRIVE",
                                   "Pushes the wreckage into a hard clip."),
                          addKnob (state, "fx_crush_bits", "BITS",
                                   "Bit depth. 16 is transparent; low values chew the signal.") };
    knobsForEffect[7] = { addKnob (state, "fx_gate_rate", "RATE",
                                   "Chops per step."),
                          addKnob (state, "fx_gate_duty", "DUTY",
                                   "Size of the holes the gate cuts.") };
    knobsForEffect[8] = { addKnob (state, "fx_delay_div", "TIME",
                                   "Echo time in step lengths."),
                          addKnob (state, "fx_delay_feedback", "FEEDBACK",
                                   "Echo regeneration (damped).") };
    knobsForEffect[9] = { addKnob (state, "fx_stretch_speed", "SPEED",
                                   "Playback speed. Half speed is an octave down.") };

    // The modulator gets a tempo-sync selector next to its frequency knob.
    if (auto* syncParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("fx_mod_sync")))
    {
        modSyncBox.addItemList (syncParam->choices, 1);
        modSyncBox.setTooltip ("Locks the ring-mod frequency to tempo divisions. "
                               "Free uses the FREQUENCY knob.");
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
    outModeBox.setTooltip ("Per-effect output filter. The shared output stage snaps to the "
                           "firing effect's settings on every trigger.");
    addAndMakeVisible (outModeBox);
    sweepLabel.setText ("SWEEP", juce::dontSendNotification);
    sweepLabel.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    sweepLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    sweepLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (sweepLabel);
    if (auto* shapeParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("fx1_sweep_shape")))
        sweepBox.addItemList (shapeParam->choices, 1);
    sweepBox.setTooltip ("Sweep envelope for the output filter across the step - "
                         "the original's waveform-button row.");
    addAndMakeVisible (sweepBox);

    static const char* const outNames[5] = { "FREQ", "PAN", "MIX", "GAIN", "SWP AMT" };
    static const char* const outTips[5] = {
        "Output filter cutoff for this effect's steps.",
        "Stereo position for this effect's steps.",
        "Wet/dry blend for this effect's steps.",
        "Level trim for this effect's steps.",
        "Sweep range in octaves. Negative sweeps downward."
    };
    for (int k = 0; k < 5; ++k)
    {
        outKnobs[k].slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        outKnobs[k].slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 14);
        outKnobs[k].slider.setTooltip (outTips[k]);
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
                                         const char* paramID, const char* name,
                                         const char* tooltip)
{
    auto knob = std::make_unique<Knob>();
    knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 78, 16);
    knob->slider.setTooltip (tooltip);
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
    static const char* const outIds[5] = { "post_freq", "pan", "mix", "gain", "sweep_amt" };
    for (int k = 0; k < 5; ++k)
    {
        outKnobs[k].attachment.reset();
        outKnobs[k].slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        outKnobs[k].attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            stateRef, pid (outIds[k]), outKnobs[k].slider);
    }
    sweepAttachment.reset();
    sweepAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        stateRef, pid ("sweep_shape"), sweepBox);

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
    outModeBox.setBounds (14, outTop + 18, 84, 21);
    sweepLabel.setBounds (14, outTop + 42, 84, 12);
    sweepBox.setBounds (14, outTop + 55, 84, 21);
    int ox = 106;
    for (int k = 0; k < 5; ++k)
    {
        outKnobs[k].label.setBounds (ox, outTop, 52, 13);
        outKnobs[k].slider.setBounds (ox, outTop + 13, 52, getHeight() - outTop - 18);
        ox += 54;
    }
}

void EffectPanel::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (6.0f, 1.0f, bounds.getWidth() - 12.0f, 1.0f);
    g.setColour (glitch::effectColour (currentEffect).withAlpha (0.9f));
    g.fillRoundedRectangle (0.0f, 0.0f, 4.0f, bounds.getHeight(), 2.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// LfoPanel
// ---------------------------------------------------------------------------
LfoPanel::LfoPanel (OpenGlitchAudioProcessor& proc)
    : processor (proc)
{
    auto& state = proc.apvts;
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
        col.shape.setTooltip ("LFO waveform.");
        col.rate.setTooltip ("Tempo-synced cycle length.");
        col.target.setTooltip (n == 0
            ? "What LFO 1 modulates."
            : "What LFO 2 modulates - including LFO 1's rate and depth, for cascaded motion.");

        col.depth.setSliderStyle (juce::Slider::LinearHorizontal);
        col.depth.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
        col.depth.setColour (juce::Slider::rotarySliderFillColourId,
                             n == 0 ? juce::Colour (0xffe040fb) : juce::Colour (0xff4fc3f7));
        col.depth.setTooltip ("Modulation amount.");
        addAndMakeVisible (col.depth);
        col.depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, id ("depth"), col.depth);
    }

    seedLabel.setText ("SEED", juce::dontSendNotification);
    seedLabel.setFont (juce::Font (juce::FontOptions (10.5f)).boldened());
    seedLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    addAndMakeVisible (seedLabel);
    seedSlider.setSliderStyle (juce::Slider::IncDecButtons);
    seedSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 38, 20);
    seedSlider.setTooltip ("0 = surprise me. Any other seed makes the DICE and FX rolls "
                           "reproducible - each press advances the same sequence.");
    addAndMakeVisible (seedSlider);
    seedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "seq_seed", seedSlider);

    static const char* const templateNames[4] = { "stutter", "buildup",
                                                  "halftime wreck", "ambient smear" };
    for (int t = 0; t < 4; ++t)
    {
        templateButtons[t].setButtonText ("T" + juce::String (t + 1));
        templateButtons[t].setTooltip ("Load factory template: " + juce::String (templateNames[t])
                                       + " (overwrites this pattern).");
        templateButtons[t].onClick = [this, t] { processor.loadTemplate (t); };
        addAndMakeVisible (templateButtons[t]);
    }

    startTimerHz (30);
}

void LfoPanel::timerCallback()
{
    repaint (scopeBounds().getSmallestIntegerContainer());
}

juce::Rectangle<float> LfoPanel::scopeBounds() const
{
    return { 8.0f, 134.0f, (float) getWidth() - 16.0f, (float) getHeight() - 134.0f - 34.0f };
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

    const int rowY = getHeight() - 28;
    seedLabel.setBounds (8, rowY, 34, 22);
    seedSlider.setBounds (42, rowY, 96, 22);
    for (int t = 0; t < 4; ++t)
        templateButtons[t].setBounds (getWidth() - 8 - (4 - t) * 34 + 2, rowY, 30, 22);
}

void LfoPanel::drawScope (juce::Graphics& g, juce::Rectangle<float> r) const
{
    using namespace glitch::palette;

    // Phosphor bed with the sequencer's 16-step grid
    g.setColour (juce::Colour (0xff090b0f));
    g.fillRoundedRectangle (r, 5.0f);
    for (int i = 0; i <= 16; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / 16.0f;
        g.setColour (juce::Colours::white.withAlpha (i % 4 == 0 ? 0.10f : 0.035f));
        g.fillRect (x, r.getY() + 2.0f, 1.0f, r.getHeight() - 4.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRect (r.getX() + 2.0f, r.getCentreY(), r.getWidth() - 4.0f, 1.0f);

    auto rawI = [&] (const juce::String& id)
    { return (int) std::lround (processor.apvts.getRawParameterValue (id)->load()); };
    auto rawF = [&] (const juce::String& id)
    { return processor.apvts.getRawParameterValue (id)->load(); };

    const double barPhase = (double) processor.getBarPhase();

    struct Cfg { int shape, rate, target; double depth, phaseNow, valueNow, cyclesPerBar; };
    Cfg c[2];
    for (int n = 0; n < 2; ++n)
    {
        const auto p = "lfo" + juce::String (n + 1) + "_";
        c[n] = { rawI (p + "shape"), rawI (p + "rate"), rawI (p + "target"),
                 (double) rawF (p + "depth"), (double) processor.getLfoPhase (n),
                 (double) processor.getLfoValue (n), 0.0 };
        c[n].cyclesPerBar = 16.0 / lfo::stepsPerCycle (c[n].rate);
    }

    // Evaluate both traces across one bar. LFO 2 is closed-form; LFO 1 is
    // integrated so LFO 2's rate-FM warping shows exactly as it sounds.
    constexpr int N = 96;
    float v1[N + 1], v2[N + 1];
    for (int k = 0; k <= N; ++k)
    {
        const double x = (double) k / N;
        if (c[1].shape == lfo::random)
        {
            v2[k] = (float) c[1].valueNow;
            continue;
        }
        double ph = c[1].phaseNow + (x - barPhase) * c[1].cyclesPerBar;
        ph -= std::floor (ph);
        v2[k] = (float) (lfo::shapeValue (c[1].shape, ph, 0.0) * c[1].depth);
    }
    {
        double integral[N + 1];
        integral[0] = 0.0;
        for (int k = 1; k <= N; ++k)
        {
            const double rateFactor = c[1].target == lfo::lfo1Rate ? std::exp2 (2.0 * v2[k - 1]) : 1.0;
            integral[k] = integral[k - 1] + rateFactor * c[0].cyclesPerBar / N;
        }
        const double cursorIndex = juce::jlimit (0.0, (double) N, barPhase * N);
        const int ci = (int) cursorIndex;
        const double cursorIntegral = integral[ci]
            + (ci < N ? (integral[ci + 1] - integral[ci]) * (cursorIndex - ci) : 0.0);
        for (int k = 0; k <= N; ++k)
        {
            if (c[0].shape == lfo::random)
            {
                v1[k] = (float) c[0].valueNow;
                continue;
            }
            const double depthEff = c[1].target == lfo::lfo1Depth
                ? juce::jlimit (0.0, 1.0, c[0].depth * (1.0 + v2[k]))
                : c[0].depth;
            double ph = c[0].phaseNow + integral[k] - cursorIntegral;
            ph -= std::floor (ph);
            v1[k] = (float) (lfo::shapeValue (c[0].shape, ph, 0.0) * depthEff);
        }
    }

    auto toY = [&] (float v) { return r.getCentreY() - v * (r.getHeight() * 0.42f); };
    auto drawTrace = [&] (const float* vals, juce::Colour colour, bool dimmed)
    {
        juce::Path path;
        path.startNewSubPath (r.getX() + 1.0f, toY (vals[0]));
        for (int k = 1; k <= N; ++k)
            path.lineTo (r.getX() + 1.0f + (r.getWidth() - 2.0f) * (float) k / N, toY (vals[k]));

        const float a = dimmed ? 0.30f : 1.0f;
        juce::Path fill (path);
        fill.lineTo (r.getRight() - 1.0f, r.getCentreY());
        fill.lineTo (r.getX() + 1.0f, r.getCentreY());
        fill.closeSubPath();
        g.setColour (colour.withAlpha (0.08f * a));
        g.fillPath (fill);

        using PST = juce::PathStrokeType;
        g.setColour (colour.withAlpha (0.10f * a));
        g.strokePath (path, PST (7.0f, PST::curved, PST::rounded));
        g.setColour (colour.withAlpha (0.30f * a));
        g.strokePath (path, PST (3.2f, PST::curved, PST::rounded));
        g.setColour (colour.withAlpha (0.95f * a));
        g.strokePath (path, PST (1.5f, PST::curved, PST::rounded));
    };

    const auto magenta = juce::Colour (0xffe040fb);
    const auto cyan = juce::Colour (0xff4fc3f7);
    drawTrace (v2, cyan, c[1].target == lfo::off || c[1].depth < 0.001);
    drawTrace (v1, magenta, c[0].target == lfo::off || c[0].depth < 0.001);

    // Beam cursor with glowing crossing dots
    const float cx = r.getX() + 1.0f + (r.getWidth() - 2.0f) * (float) barPhase;
    g.setColour (lamp.withAlpha (0.12f));
    g.fillRect (cx - 3.0f, r.getY() + 2.0f, 6.0f, r.getHeight() - 4.0f);
    g.setColour (lamp.withAlpha (0.75f));
    g.fillRect (cx - 0.5f, r.getY() + 2.0f, 1.0f, r.getHeight() - 4.0f);
    for (int n = 0; n < 2; ++n)
    {
        const auto colour = n == 0 ? magenta : cyan;
        const float y = toY ((float) c[n].valueNow);
        g.setColour (colour.withAlpha (0.30f));
        g.fillEllipse (cx - 5.0f, y - 5.0f, 10.0f, 10.0f);
        g.setColour (colour);
        g.fillEllipse (cx - 2.2f, y - 2.2f, 4.4f, 4.4f);
    }

    // Routing labels
    static const char* const targetNames[] = { "OFF", "FILTER", "DRIVE", "CHAOS", "MOD",
                                               "PITCH", "DUTY", "CRUSH", "C.DRIVE", "DLY FB",
                                               "STRETCH", "TAPE", "R.RATE", "G.RATE", "BITS",
                                               "L1 RATE", "L1 DEPTH" };
    g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
    g.setColour (magenta.withAlpha (0.75f));
    g.drawText (juce::String ("1 > ") + targetNames[juce::jlimit (0, 16, c[0].target)],
                (int) r.getX() + 6, (int) r.getY() + 3, 84, 11, juce::Justification::centredLeft);
    g.setColour (cyan.withAlpha (0.75f));
    g.drawText (juce::String ("2 > ") + targetNames[juce::jlimit (0, 16, c[1].target)],
                (int) r.getRight() - 90, (int) r.getY() + 3, 84, 11, juce::Justification::centredRight);
}

void LfoPanel::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (6.0f, 1.0f, bounds.getWidth() - 12.0f, 1.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    const int colW = (getWidth() - 24) / 2;
    g.setFont (juce::Font (juce::FontOptions (13.0f)).boldened());
    g.setColour (juce::Colour (0xffe040fb));
    g.drawText ("LFO 1", 8, 6, colW, 18, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff4fc3f7));
    g.drawText (juce::String::fromUTF8 ("LFO 2 \xe2\x80\x94 can drive LFO 1"),
                16 + colW, 6, colW + 60, 18, juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (10.5f)).boldened());
    g.setColour (glitch::palette::textDim);
    g.drawText ("TPL", getWidth() - 8 - 4 * 34 - 30, getHeight() - 28, 26, 22,
                juce::Justification::centredRight);

    drawScope (g, scopeBounds());
}

// ---------------------------------------------------------------------------
// MasterPanel
// ---------------------------------------------------------------------------
MasterPanel::MasterPanel (juce::AudioProcessorValueTreeState& state)
{
    addFader (state, "seq_chaos", "CHAOS", juce::Colour (0xffe040fb),
              "Odds that a step fires a random effect instead of what's drawn.");
    addFader (state, "master_drive", "DRIVE", juce::Colour (0xffff7043),
              "Master saturation drive.");
    addFader (state, "master_drive_mix", "D.MIX", juce::Colour (0xffffab91),
              "Blend of the driven signal.");
    addFader (state, "master_reso", "RESO", juce::Colour (0xffba68c8),
              "Master filter resonance.");
    addFader (state, "master_lowpass", "FILTER", juce::Colour (0xff4fc3f7),
              "Master filter cutoff.");
    addFader (state, "master_filter_mix", "F.MIX", juce::Colour (0xff4dd0e1),
              "Blend of the filtered signal.");
    addFader (state, "seq_declick", "CLICK", juce::Colour (0xffa1887f),
              "De-click time: smooths the joins at step boundaries.");
    addFader (state, "seq_stepenv", "ENV", juce::Colour (0xff90a4ae),
              "Per-step fade-in/fade-out envelope.");

    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, glitch::palette::lcd);
    mixSlider.setTooltip ("Global wet/dry: how much OpenGlitch replaces the input.");
    addAndMakeVisible (mixSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "master_mix", mixSlider);
    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
    mixLabel.setColour (juce::Label::textColourId,
                        glitch::palette::textDim.interpolatedWith (glitch::palette::lcd, 0.45f));
    addAndMakeVisible (mixLabel);

    volSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 15);
    volSlider.setColour (juce::Slider::rotarySliderFillColourId, glitch::palette::text);
    volSlider.setTooltip ("Master output volume, after everything else.");
    addAndMakeVisible (volSlider);
    volAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "master_volume", volSlider);
    volLabel.setText ("VOL", juce::dontSendNotification);
    volLabel.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
    volLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    addAndMakeVisible (volLabel);

    if (auto* shapeParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("master_sweep_shape")))
        sweepShapeBox.addItemList (shapeParam->choices, 1);
    sweepShapeBox.setTooltip ("Master filter sweep shape, retriggered every step.");
    addAndMakeVisible (sweepShapeBox);
    sweepShapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, "master_sweep_shape", sweepShapeBox);
    sweepAmtSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sweepAmtSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sweepAmtSlider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xff4fc3f7));
    sweepAmtSlider.setTooltip ("Master sweep range in octaves. Negative sweeps downward.");
    addAndMakeVisible (sweepAmtSlider);
    sweepAmtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, "master_sweep_amt", sweepAmtSlider);
    sweepRowLabel.setText ("SWP", juce::dontSendNotification);
    sweepRowLabel.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
    sweepRowLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    addAndMakeVisible (sweepRowLabel);

    if (auto* typeParam = dynamic_cast<juce::AudioParameterChoice*> (
            state.getParameter ("master_filter_type")))
    {
        filterTypeBox.addItemList (typeParam->choices, 1);
        filterTypeBox.setTooltip ("Master filter topology.");
        addAndMakeVisible (filterTypeBox);
        filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, "master_filter_type", filterTypeBox);
    }

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("True bypass - also automatable from the host.");
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, "bypass", bypassButton);
}

void MasterPanel::addFader (juce::AudioProcessorValueTreeState& state, const char* paramID,
                            const char* name, juce::Colour colour, const char* tooltip)
{
    auto fader = std::make_unique<Fader>();
    fader->slider.setSliderStyle (juce::Slider::LinearVertical);
    fader->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 15);
    fader->slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    fader->slider.setTooltip (tooltip);
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
    const int rowStep = 25;
    const int faderW = getWidth() / 4;
    const int bottomZone = bypassH + comboH + rowStep * 3 + 24;
    const int rowH = (getHeight() - top - bottomZone) / 2;
    for (size_t i = 0; i < faders.size(); ++i)
    {
        const int x = ((int) i % 4) * faderW;
        const int y = top + ((int) i / 4) * rowH;
        faders[i]->nameLabel.setBounds (x, y, faderW, 14);
        faders[i]->slider.setBounds (x, y + 16, faderW, rowH - 20);
    }
    int rowY = getHeight() - bottomZone + 2;
    mixLabel.setBounds (8, rowY, 30, 22);
    mixSlider.setBounds (38, rowY, getWidth() - 44, 22);
    rowY += rowStep;
    volLabel.setBounds (8, rowY, 30, 22);
    volSlider.setBounds (38, rowY, getWidth() - 44, 22);
    rowY += rowStep;
    sweepRowLabel.setBounds (8, rowY, 30, 22);
    sweepShapeBox.setBounds (38, rowY, 64, 22);
    sweepAmtSlider.setBounds (106, rowY, getWidth() - 112, 22);
    rowY += rowStep;
    filterTypeBox.setBounds (10, rowY, getWidth() - 20, comboH);
    bypassButton.setBounds (10, getHeight() - bypassH - 6, getWidth() - 20, bypassH);
}

void MasterPanel::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (6.0f, 1.0f, bounds.getWidth() - 12.0f, 1.0f);
    g.setColour (cellStroke);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
    g.drawText ("MASTER", getLocalBounds().removeFromTop (24), juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// AboutOverlay
// ---------------------------------------------------------------------------
AboutOverlay::AboutOverlay()
{
    setComponentID ("aboutOverlay"); // lets the screenshot tool show it headlessly
    setVisible (false);
}

void AboutOverlay::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    g.fillAll (juce::Colours::black.withAlpha (0.65f));

    auto card = juce::Rectangle<float> (0.0f, 0.0f, 560.0f, 392.0f)
                    .withCentre (getLocalBounds().toFloat().getCentre());
    g.setColour (panel);
    g.fillRoundedRectangle (card, 10.0f);
    g.setColour (glitch::effectColour (3).withAlpha (0.9f));
    g.drawRoundedRectangle (card, 10.0f, 1.5f);

    const int x = (int) card.getX() + 32;
    const int w = (int) card.getWidth() - 64;
    int y = (int) card.getY() + 24;

    const auto titleFont = juce::Font (juce::FontOptions (26.0f)).boldened();
    juce::GlyphArrangement measure;
    measure.addLineOfText (titleFont, "OPEN", 0.0f, 0.0f);
    const int openW = (int) std::ceil (measure.getBoundingBox (0, -1, true).getWidth());
    g.setFont (titleFont);
    g.setColour (text);
    g.drawText ("OPEN", x, y, openW + 4, 28, juce::Justification::centredLeft);
    g.setColour (glitch::effectColour (3));
    g.drawText ("GLITCH", x + openW, y, 200, 28, juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.setColour (textDim);
    g.drawText (juce::String ("v") + OPENGLITCH_VERSION
                    + juce::String::fromUTF8 ("  \xc2\xb7  build ") + __DATE__ + " " + __TIME__,
                x, y + 30, w, 14, juce::Justification::centredLeft);
    y += 58;

    auto para = [&] (const juce::String& s, int lines, float size, juce::Colour c)
    {
        g.setFont (juce::Font (juce::FontOptions (size)));
        g.setColour (c);
        g.drawFittedText (s, x, y, w, lines * (int) (size + 4), juce::Justification::topLeft, lines);
        y += lines * (int) (size + 4) + 10;
    };
    auto caption = [&] (const char* s)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.setColour (glitch::effectColour (3).withAlpha (0.85f));
        g.drawText (s, x, y, w, 13, juce::Justification::centredLeft);
        y += 17;
    };

    para (juce::String::fromUTF8 ("An independent, GPL-licensed recreation of dblue Glitch 1.3 "
                                  "\xe2\x80\x94 the discontinued freeware by Kieran Foster. Not "
                                  "affiliated with or endorsed by illformed."),
          2, 12.5f, text);

    caption ("CREDITS");
    para ("Original concept and design: Kieran Foster (illformed)\n"
          "DSP graph: Pure Data, compiled by hvcc (the Wasted Audio fork)\n"
          "Plugin wrapper and GUI: JUCE 8",
          3, 12.0f, textDim);

    caption ("LICENSE");
    para ("GNU General Public License v3. Free software with ABSOLUTELY NO WARRANTY;\n"
          "you may redistribute and modify it under the GPL's terms.",
          2, 12.0f, textDim);

    caption ("HIDDEN MOVES");
    para ("Drag right on the grid to stretch a block across steps (tie)\n"
          "Shift-click a pattern slot to copy the current pattern into it\n"
          "Right-click erases; drag to sweep columns clear\n"
          "MIDI notes 36-51 switch patterns; SEED makes DICE reproducible",
          4, 12.0f, textDim);

    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.setColour (textDim.withAlpha (0.7f));
    g.drawText ("click anywhere to close", (int) card.getX(), (int) card.getBottom() - 24,
                (int) card.getWidth(), 14, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// EditorContent
// ---------------------------------------------------------------------------
namespace
{
bool looksLikeLoopFile (const juce::String& name)
{
    return name.endsWithIgnoreCase (".wav") || name.endsWithIgnoreCase (".flac")
        || name.endsWithIgnoreCase (".aif") || name.endsWithIgnoreCase (".aiff")
        || name.endsWithIgnoreCase (".ogg") || name.endsWithIgnoreCase (".mp3");
}
} // namespace

EditorContent::EditorContent (OpenGlitchAudioProcessor& p)
    : processorRef (p),
      matrix (p.apvts),
      sequencerBar (p),
      effectPanel (p.apvts),
      lfoPanel (p),
      masterPanel (p.apvts)
{
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
    lcdLabel.setTooltip ("Active pattern | playing step | tempo.");
    addAndMakeVisible (lcdLabel);

    clearButton.setTooltip ("Wipe the pattern.");
    clearButton.onClick = [this] { matrix.clearAll(); };
    addAndMakeVisible (clearButton);
    diceButton.setTooltip ("Roll a random pattern. SEED makes it reproducible.");
    diceButton.onClick = [this] { processorRef.randomizeActivePattern(); };
    addAndMakeVisible (diceButton);
    fxDiceButton.setTooltip ("Randomize the effect knobs and output strips.");
    fxDiceButton.onClick = [this] { processorRef.randomizeFxKnobs(); };
    addAndMakeVisible (fxDiceButton);
    shiftLeftButton.setTooltip ("Rotate the pattern one step earlier.");
    shiftLeftButton.onClick = [this] { processorRef.shiftActivePattern (-1); };
    addAndMakeVisible (shiftLeftButton);
    shiftRightButton.setTooltip ("Rotate the pattern one step later.");
    shiftRightButton.onClick = [this] { processorRef.shiftActivePattern (1); };
    addAndMakeVisible (shiftRightButton);

    statusLabel.setFont (juce::Font (juce::FontOptions (
        juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain)));
    statusLabel.setColour (juce::Label::textColourId, glitch::palette::textDim);
    statusLabel.setTooltip ("Clock source | ticks delivered to the engine | "
                            "how much the engine is changing the audio.");
    addAndMakeVisible (statusLabel);

    // Loop player strip: standalone (and dev tools) only — a host feeds the
    // input everywhere else.
    if (processorRef.loopPlayerAvailable())
    {
        loopOpenButton.setTooltip ("Load a loop file. Or just drop a WAV/FLAC "
                                   "anywhere on the window.");
        loopOpenButton.onClick = [this] { openLoopFileChooser(); };
        addAndMakeVisible (loopOpenButton);

        loopPlayButton.setClickingTogglesState (true);
        loopPlayButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2e7d32));
        loopPlayButton.setTooltip ("Loop the loaded file through the engine.");
        loopPlayButton.onClick = [this]
        {
            processorRef.setLoopPlaying (loopPlayButton.getToggleState());
            refreshLoopStrip();
        };
        addAndMakeVisible (loopPlayButton);

        loopNameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
        loopNameLabel.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (loopNameLabel);
        refreshLoopStrip();
    }

    addChildComponent (about); // last child: sits above everything when shown

    startTimerHz (30);
    timerCallback(); // seed the LCD before the first tick
}

void EditorContent::refreshLoopStrip()
{
    if (! processorRef.loopPlayerAvailable())
        return;
    const bool hasFile = processorRef.hasLoopFile();
    const bool playing = hasFile && processorRef.isLoopPlaying();
    loopPlayButton.setEnabled (hasFile);
    loopPlayButton.setToggleState (playing, juce::dontSendNotification);
    loopPlayButton.setButtonText (playing ? "STOP" : "PLAY");
    loopNameLabel.setText (hasFile ? processorRef.getLoopFileName()
                                   : juce::String ("drop a wav / flac"),
                           juce::dontSendNotification);
    loopNameLabel.setColour (juce::Label::textColourId,
                             hasFile ? glitch::palette::lcd : glitch::palette::textDim);
}

void EditorContent::loadLoopFileIntoPlayer (const juce::File& file)
{
    const auto error = processorRef.loadLoopFile (file);
    if (error.isNotEmpty())
    {
        loopNameLabel.setText (error, juce::dontSendNotification);
        loopNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffef5350));
        return;
    }
    refreshLoopStrip();
}

void EditorContent::openLoopFileChooser()
{
    loopChooser = std::make_unique<juce::FileChooser> (
        "Load a loop", juce::File(), "*.wav;*.flac;*.aif;*.aiff;*.ogg;*.mp3");
    loopChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  if (chooser.getResult().existsAsFile())
                                      loadLoopFileIntoPlayer (chooser.getResult());
                              });
}

bool EditorContent::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (! processorRef.loopPlayerAvailable())
        return false;
    for (const auto& f : files)
        if (looksLikeLoopFile (f))
            return true;
    return false;
}

void EditorContent::fileDragEnter (const juce::StringArray&, int, int)
{
    fileDragActive = true;
    repaint();
}

void EditorContent::fileDragExit (const juce::StringArray&)
{
    fileDragActive = false;
    repaint();
}

void EditorContent::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragActive = false;
    repaint();
    for (const auto& f : files)
    {
        if (looksLikeLoopFile (f))
        {
            loadLoopFileIntoPlayer (juce::File (f));
            break;
        }
    }
}

void EditorContent::mouseDown (const juce::MouseEvent& e)
{
    if (logoBounds().contains (e.getPosition()))
        about.setVisible (true);
}

void EditorContent::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (logoBounds().contains (e.getPosition())
                        ? juce::MouseCursor::PointingHandCursor
                        : juce::MouseCursor::NormalCursor);
}

juce::String EditorContent::getTooltip()
{
    return logoBounds().contains (getMouseXYRelative())
               ? juce::String ("About OpenGlitch: version, credits, license, hidden moves.")
               : juce::String();
}

void EditorContent::timerCallback()
{
    matrix.setPlayheadColumn (processorRef.getCurrentStep());
    matrix.advanceFlashes();
    repaint (18, 8, 380, 44);                          // reactive logo glow
    repaint (lcdLabel.getBounds().expanded (6, 5));    // LCD waveform

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

void EditorContent::paint (juce::Graphics& g)
{
    using namespace glitch::palette;
    juce::ColourGradient chassis (juce::Colour (0xff1c2027), 0.0f, 0.0f,
                                  juce::Colour (0xff111318), 0.0f, (float) getHeight(), false);
    g.setGradientFill (chassis);
    g.fillAll();

    const auto titleFont = juce::Font (juce::FontOptions (27.0f)).boldened();
    juce::GlyphArrangement measure;
    measure.addLineOfText (titleFont, "OPEN", 0.0f, 0.0f);
    const int openWidth = (int) std::ceil (measure.getBoundingBox (0, -1, true).getWidth());

    g.setFont (titleFont);
    g.setColour (text);
    g.drawText ("OPEN", 20, 12, openWidth + 4, 30, juce::Justification::centredLeft);
    const auto brand = glitch::effectColour (3); // the retrigger amber is the brand
    const float heat = juce::jlimit (0.0f, 1.0f, processorRef.getWetActivity() * 1.4f);
    if (heat > 0.03f) // the logo runs hot with the engine
    {
        g.setColour (brand.withAlpha (0.16f * heat));
        for (int dx = -2; dx <= 2; dx += 2)
            for (int dy = -1; dy <= 1; dy += 2)
                g.drawText ("GLITCH", 20 + openWidth + dx, 12 + dy, 200, 30,
                            juce::Justification::centredLeft);
    }
    g.setColour (brand.interpolatedWith (juce::Colours::white, 0.25f * heat));
    g.drawText ("GLITCH", 20 + openWidth, 12, 200, 30, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText (juce::String ("v") + OPENGLITCH_VERSION
                    + juce::String::fromUTF8 ("  \xc2\xb7  dblue Glitch 1.3 tribute  \xc2\xb7  "
                                              "Pd \xe2\x86\x92 hvcc \xe2\x86\x92 JUCE")
                    + "  |  build " + __DATE__ + " "
                    + juce::String (__TIME__).dropLastCharacters (3),
                22, 40, 446, 16, juce::Justification::centredLeft);

    // LCD bezel with a scrolling output-amplitude trace and CRT scanlines
    const auto bezel = lcdLabel.getBounds().toFloat().expanded (4.0f, 3.0f);
    g.setColour (juce::Colour (0xff0d1f16));
    g.fillRoundedRectangle (bezel, 4.0f);
    {
        const int points = 104;
        const int head = processorRef.getOutputPeakIndex();
        g.setColour (lcd.withAlpha (0.30f));
        for (int i = 0; i < points; ++i)
        {
            const float peak = juce::jlimit (0.0f, 1.0f,
                processorRef.getOutputPeak (head - points + i) * 1.2f);
            const float px = bezel.getX() + 4.0f + (bezel.getWidth() - 8.0f) * (float) i / (points - 1);
            const float ph = juce::jmax (0.6f, (bezel.getHeight() - 8.0f) * 0.5f * peak);
            g.fillRect (px, bezel.getCentreY() - ph, 1.4f, ph * 2.0f);
        }
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        for (float sy = bezel.getY() + 2.5f; sy < bezel.getBottom() - 2.0f; sy += 3.0f)
            g.fillRect (bezel.getX() + 2.0f, sy, bezel.getWidth() - 4.0f, 1.0f);
    }
    g.setColour (lcd.withAlpha (0.25f));
    g.drawRoundedRectangle (bezel, 4.0f, 1.0f);
}

void EditorContent::paintOverChildren (juce::Graphics& g)
{
    if (! fileDragActive)
        return;
    using namespace glitch::palette;
    g.setColour (lcd.withAlpha (0.08f));
    g.fillAll();
    g.setColour (lcd.withAlpha (0.85f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 8.0f, 2.5f);
    g.setColour (lcd);
    g.setFont (juce::Font (juce::FontOptions (24.0f)).boldened());
    g.drawText ("DROP TO LOOP IT THROUGH THE ENGINE", getLocalBounds(),
                juce::Justification::centred);
}

void EditorContent::resized()
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

    // Loop strip lives in the free header band between the logo and buttons.
    loopOpenButton.setBounds (240, 16, 46, 24);
    loopPlayButton.setBounds (290, 16, 46, 24);
    loopNameLabel.setBounds (338, 16, 128, 24);

    about.setBounds (getLocalBounds());
}

// ---------------------------------------------------------------------------
// Editor — hosts the content behind a single aspect-locked scale transform
// ---------------------------------------------------------------------------
OpenGlitchAudioProcessorEditor::OpenGlitchAudioProcessorEditor (OpenGlitchAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      content (p)
{
    setLookAndFeel (&lookAndFeel);

    // Read the remembered scale before the resize machinery runs: installing
    // the limits clamps the still-0x0 bounds, which fires resized() and would
    // overwrite the stored value with the clamped minimum.
    const double scale = juce::jlimit (0.5, 2.5,
        (double) processorRef.apvts.state.getProperty ("editor_scale", 1.0));

    addAndMakeVisible (content);
    content.setBounds (0, 0, EditorContent::baseWidth, EditorContent::baseHeight);

    // Resize corner + host resizing, locked to the 940x700 aspect, driving
    // one global scale factor (50%..250%) remembered in the plugin state.
    setResizable (true, true);
    setResizeLimits (EditorContent::baseWidth / 2, EditorContent::baseHeight / 2,
                     EditorContent::baseWidth * 5 / 2, EditorContent::baseHeight * 5 / 2);
    getConstrainer()->setFixedAspectRatio ((double) EditorContent::baseWidth
                                           / (double) EditorContent::baseHeight);

    setSize (juce::roundToInt (EditorContent::baseWidth * scale),
             juce::roundToInt (EditorContent::baseHeight * scale));
}

OpenGlitchAudioProcessorEditor::~OpenGlitchAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void OpenGlitchAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (glitch::palette::bg); // only shows in 1px rounding slivers
}

void OpenGlitchAudioProcessorEditor::resized()
{
    const double scale = (double) getWidth() / (double) EditorContent::baseWidth;
    content.setTransform (juce::AffineTransform::scale ((float) scale));
    processorRef.apvts.state.setProperty ("editor_scale", scale, nullptr);
}
