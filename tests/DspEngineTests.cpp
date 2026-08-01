// DSP-level tests: drive the Heavy context directly, no JUCE involved.
#include <catch2/catch_test_macros.hpp>
#include <set>

#include "TestHelpers.h"

using namespace testutil;

TEST_CASE ("engine: demo pattern glitches and stays finite", "[dsp]")
{
    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    auto out = h.run (2.0);

    REQUIRE (allFinite (out));
    REQUIRE (rms (out) > 0.05f);
    // 120 bpm -> 8 steps/sec -> ~16 playhead events in 2s
    REQUIRE (playheadEvents().size() >= 12);
    REQUIRE (playheadEvents().size() <= 20);
}

TEST_CASE ("engine: bypass is bit-transparent", "[dsp]")
{
    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    h.set ("bypass", 1.0f);
    h.run (0.5);
    std::vector<float> in;
    auto out = h.run (1.0, &in);
    REQUIRE (maxDiff (out, in) < 1e-6f);
}

TEST_CASE ("engine: all-dry grid is near-unity", "[dsp]")
{
    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    h.setAllSteps (0.0f);
    h.run (0.5);
    std::vector<float> in;
    auto out = h.run (1.0, &in);
    REQUIRE (allFinite (out));
    const float ratio = rms (out) / rms (in);
    REQUIRE (ratio > 0.85f);
    REQUIRE (ratio < 1.1f);
}

TEST_CASE ("engine: chaos overrides an empty grid", "[dsp]")
{
    HeavyHarness dry;
    dry.set ("host_playing", 1.0f);
    dry.setAllSteps (0.0f);
    dry.run (0.5);
    auto dryOut = dry.run (1.0);

    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    h.setAllSteps (0.0f);
    h.set ("seq_chaos", 1.0f);
    h.run (0.5);
    auto out = h.run (1.0);
    REQUIRE (allFinite (out));
    REQUIRE (maxDiff (out, dryOut) > 0.05f);
}

TEST_CASE ("engine: every effect alone is finite and audible", "[dsp]")
{
    for (int fx = 1; fx <= 9; ++fx)
    {
        DYNAMIC_SECTION ("effect " << fx)
        {
            HeavyHarness h;
            h.set ("host_playing", 1.0f);
            h.setAllSteps ((float) fx);
            auto out = h.run (1.0);
            REQUIRE (allFinite (out));
            REQUIRE (rms (out) > 0.01f);
        }
    }
}

TEST_CASE ("host_tick: external clock echoes indices and jumps", "[dsp][transport]")
{
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    h.run (0.2);
    playheadEvents().clear();

    const int sequence[] = { 0, 5, 10, 3, 15, 0 };
    for (int index : sequence)
    {
        h.tick (index);
        h.run (0.05);
    }
    REQUIRE (playheadEvents().size() == 6);
    for (size_t i = 0; i < 6; ++i)
        REQUIRE ((int) playheadEvents()[i].step == sequence[i]);
}

TEST_CASE ("host_tick: mid-block delayMs dispatch", "[dsp][transport]")
{
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    h.run (0.1);
    playheadEvents().clear();

    h.tick (0, 0.0);
    h.tick (1, 0.5 * 1000.0 * BLOCK / SR);
    h.run ((double) BLOCK / SR);
    REQUIRE (playheadEvents().size() == 2);
}

TEST_CASE ("host_tick: -1 falls back to clean dry", "[dsp][transport]")
{
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    h.setAllSteps (7.0f); // gater everywhere
    for (int i = 0; i < 8; ++i)
    {
        h.tick (i % 16);
        h.run (0.125);
    }
    h.tick (-1);
    h.run (0.3);
    std::vector<float> in;
    auto out = h.run (0.5, &in);
    const float ratio = rms (out) / rms (in);
    REQUIRE (ratio > 0.85f);
    REQUIRE (ratio < 1.1f);
}

TEST_CASE ("clock: pattern length cycles the counter", "[dsp][sequencer]")
{
    HeavyHarness h;
    h.set ("seq_length", 4.0f);
    h.set ("host_playing", 1.0f);
    h.run (2.0);

    std::set<int> seen;
    for (auto& e : playheadEvents())
    {
        REQUIRE (e.step >= 0.0f);
        REQUIRE (e.step <= 3.0f);
        seen.insert ((int) e.step);
    }
    REQUIRE (seen.size() == 4);
    REQUIRE (playheadEvents().size() >= 12); // rate unchanged
}

TEST_CASE ("clock: swing shifts odd steps", "[dsp][sequencer]")
{
    HeavyHarness h;
    h.set ("seq_swing", 0.5f);
    h.set ("host_playing", 1.0f);
    h.run (2.0);
    auto& events = playheadEvents();
    REQUIRE (events.size() >= 8);

    double longGap = 0, shortGap = 0;
    int nLong = 0, nShort = 0;
    for (size_t i = 2; i + 1 < events.size(); ++i)
    {
        const double gap = (double) events[i + 1].sample - events[i].sample;
        if (((int) events[i + 1].step & 1) != 0) { longGap += gap; ++nLong; }
        else { shortGap += gap; ++nShort; }
    }
    REQUIRE (nLong > 0);
    REQUIRE (nShort > 0);
    // full swing at 120 bpm: 9000 vs 3000 samples, one block of slack
    REQUIRE (std::abs (longGap / nLong - 9000.0) < 600.0);
    REQUIRE (std::abs (shortGap / nShort - 3000.0) < 600.0);
}

TEST_CASE ("master filter: three modes shape the wet path", "[dsp][filter]")
{
    auto filteredRms = [] (float type, float freq)
    {
        HeavyHarness h;
        h.setAllSteps (0.0f);
        h.set ("master_filter_type", type);
        h.set ("master_lowpass", freq);
        h.set ("host_playing", 1.0f);
        h.run (0.6); // settle
        return rms (h.run (0.8));
    };

    const float ref = 0.5f / std::sqrt (2.0f); // 220Hz sine input RMS
    REQUIRE (filteredRms (0.0f, 8000.0f) > ref * 0.85f);  // LP@8k passes 220
    REQUIRE (filteredRms (1.0f, 8000.0f) < ref * 0.1f);   // HP@8k kills 220
    REQUIRE (filteredRms (2.0f, 8000.0f) < ref * 0.2f);   // BP@8k rejects 220
    REQUIRE (filteredRms (2.0f, 220.0f) > ref * 0.4f);    // BP@220 passes 220
}
