#pragma once

#include <algorithm>
#include <cmath>

// Pure LFO / modulation math, kept free of JUCE so it can be unit-tested
// standalone. Phases are in [0,1); outputs are bipolar [-1,1].
namespace lfo
{
enum Shape { sine = 0, triangle, saw, square, random };

// Modulation destinations. lfo1Rate/lfo1Depth are only reachable from LFO 2 —
// that's the derivative, LFO-into-LFO behaviour — and must stay last so
// "every target below lfo1Rate is a parameter target" holds.
// Parameter targets are alphabetical by display name so related effect
// controls group together in the selector; the choice index must equal the
// enum value.
enum Target { off = 0, chaos, crushBits, crushDrive, crushRate, delayFeedback,
              drive, filterFreq, gateDuty, gateRate, modFreq, retrigPitch,
              retrigRate, stretchSpeed, tapestopSpeed,
              lfo1Rate, lfo1Depth, numTargets };

// 16th-note steps per LFO cycle, indexed by the rate choice parameter.
inline double stepsPerCycle (int rateIndex)
{
    static const double steps[] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0 };
    return steps[std::clamp (rateIndex, 0, 6)];
}

// heldRandom is the sample-and-hold value refreshed by the caller on wrap.
inline double shapeValue (int shape, double phase, double heldRandom)
{
    switch (shape)
    {
        case sine:     return std::sin (phase * 2.0 * 3.141592653589793);
        case triangle: return phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase;
        case saw:      return 2.0 * phase - 1.0;
        case square:   return phase < 0.5 ? 1.0 : -1.0;
        case random:   return heldRandom;
        default:       return 0.0;
    }
}

// Combine a bipolar contribution c (already depth-scaled) with a parameter's
// base value. Frequency-ish targets are exponential (octaves), the rest are
// linear offsets, all clamped to the parameter's Pd range.
inline float applyMod (int target, float base, double c)
{
    switch (target)
    {
        case filterFreq:    return std::clamp (base * (float) std::exp2 (3.0 * c), 100.0f, 20000.0f);
        case drive:         return std::clamp (base + 4.0f * (float) c, 1.0f, 10.0f);
        case chaos:         return std::clamp (base + (float) c, 0.0f, 1.0f);
        case modFreq:       return std::clamp (base * (float) std::exp2 (3.0 * c), 1.0f, 4000.0f);
        case retrigPitch:   return std::clamp (base * (float) std::exp2 (c), 0.25f, 2.0f);
        case gateDuty:      return std::clamp (base + 0.45f * (float) c, 0.05f, 0.95f);
        case crushRate:     return std::clamp (base * (float) std::exp2 (3.0 * c), 200.0f, 20000.0f);
        case crushDrive:    return std::clamp (base + 4.5f * (float) c, 1.0f, 10.0f);
        case delayFeedback: return std::clamp (base + 0.5f * (float) c, 0.0f, 0.95f);
        case stretchSpeed:  return std::clamp (base + 0.45f * (float) c, 0.1f, 1.0f);
        case tapestopSpeed: return std::clamp (base * (float) std::exp2 (c), 0.1f, 4.0f);
        case retrigRate:    return std::clamp (base + 3.5f * (float) c, 1.0f, 8.0f);
        case gateRate:      return std::clamp (base + 7.5f * (float) c, 1.0f, 16.0f);
        case crushBits:     return std::clamp (base + 7.5f * (float) c, 1.0f, 16.0f);
        default:            return base;
    }
}

// Per-step filter sweep envelopes (the original's waveform-button row):
// unipolar 0..1 over the step/span phase, scaled into octaves by the caller.
enum SweepShape { sweepOff = 0, sweepDown, sweepUp, sweepTri, sweepSine, sweepSquare };

inline double sweepValue (int shape, double phase)
{
    const double p = std::clamp (phase, 0.0, 1.0);
    switch (shape)
    {
        case sweepDown:   return 1.0 - p;
        case sweepUp:     return p;
        case sweepTri:    return 1.0 - std::abs (2.0 * p - 1.0);
        case sweepSine:   return 0.5 - 0.5 * std::cos (p * 2.0 * 3.141592653589793);
        case sweepSquare: return p < 0.5 ? 1.0 : 0.0;
        default:          return 0.0;
    }
}

struct State
{
    double phase = 0.0;
    double previousPhase = 1.0; // forces a fresh random value on the first cycle
    double heldRandom = 0.0;
};

// Advances (or ppq-locks) one LFO and returns its bipolar, depth-scaled value.
// nextRandom supplies a new [-1,1] value whenever the phase wraps.
template <typename NextRandom>
inline double advance (State& s, int shape, double cycleHz, double depth,
                       double blockSeconds, bool lockToPpq, double ppqPhase,
                       NextRandom&& nextRandom)
{
    if (lockToPpq)
        s.phase = ppqPhase - std::floor (ppqPhase);
    else
        s.phase += blockSeconds * cycleHz;

    if (s.phase >= 1.0)
        s.phase -= std::floor (s.phase);

    if (s.phase < s.previousPhase)
        s.heldRandom = nextRandom();
    s.previousPhase = s.phase;

    return shapeValue (shape, s.phase, s.heldRandom) * depth;
}
} // namespace lfo
