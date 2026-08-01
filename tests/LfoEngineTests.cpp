// Pure-math tests for the LFO/modulation engine.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "LfoEngine.h"

using Catch::Matchers::WithinAbs;

TEST_CASE ("lfo shapes are bipolar and phase-correct", "[lfo]")
{
    REQUIRE_THAT (lfo::shapeValue (lfo::sine, 0.25, 0.0), WithinAbs (1.0, 1e-9));
    REQUIRE_THAT (lfo::shapeValue (lfo::sine, 0.0, 0.0), WithinAbs (0.0, 1e-9));
    REQUIRE_THAT (lfo::shapeValue (lfo::triangle, 0.0, 0.0), WithinAbs (-1.0, 1e-9));
    REQUIRE_THAT (lfo::shapeValue (lfo::triangle, 0.5, 0.0), WithinAbs (1.0, 1e-9));
    REQUIRE_THAT (lfo::shapeValue (lfo::saw, 0.75, 0.0), WithinAbs (0.5, 1e-9));
    REQUIRE (lfo::shapeValue (lfo::square, 0.25, 0.0) == 1.0);
    REQUIRE (lfo::shapeValue (lfo::square, 0.75, 0.0) == -1.0);
    REQUIRE_THAT (lfo::shapeValue (lfo::random, 0.5, 0.42), WithinAbs (0.42, 1e-9));
}

TEST_CASE ("modulation combine respects ranges", "[lfo]")
{
    REQUIRE_THAT (lfo::applyMod (lfo::filterFreq, 1000.0f, 1.0), WithinAbs (8000.0, 0.5));
    REQUIRE_THAT (lfo::applyMod (lfo::filterFreq, 20000.0f, 1.0), WithinAbs (20000.0, 0.01));
    REQUIRE_THAT (lfo::applyMod (lfo::retrigPitch, 1.0f, 1.0), WithinAbs (2.0, 1e-6));
    REQUIRE_THAT (lfo::applyMod (lfo::drive, 1.0f, 0.5), WithinAbs (3.0, 1e-6));
    REQUIRE_THAT (lfo::applyMod (lfo::chaos, 0.9f, 0.5), WithinAbs (1.0, 1e-6));

    // zero contribution must be an exact identity for every target
    for (int target = lfo::filterFreq; target <= lfo::gateDuty; ++target)
        REQUIRE (lfo::applyMod (target, 1234.5f, 0.0)
                 == lfo::applyMod (target, 1234.5f, 0.0)); // stable
    REQUIRE (lfo::applyMod (lfo::filterFreq, 20000.0f, 0.0) == 20000.0f);
    REQUIRE (lfo::applyMod (lfo::gateDuty, 0.5f, 0.0) == 0.5f);
}

TEST_CASE ("advance: S&H refreshes once per cycle, ppq lock pins phase", "[lfo]")
{
    lfo::State s;
    int randoms = 0;
    auto rng = [&randoms] { ++randoms; return 0.5; };

    double v = 0;
    for (int i = 0; i < 30; ++i) // 30 x 0.1s at 1Hz = 3 cycles
        v = lfo::advance (s, lfo::random, 1.0, 1.0, 0.1, false, 0.0, rng);
    REQUIRE (randoms == 3);
    REQUIRE_THAT (v, WithinAbs (0.5, 1e-9));

    lfo::advance (s, lfo::sine, 1.0, 1.0, 0.1, true, 2.3, rng);
    REQUIRE_THAT (s.phase, WithinAbs (0.3, 1e-9));
}
