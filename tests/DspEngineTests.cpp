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

TEST_CASE ("tie: a spanned effect differs from re-triggered and dry", "[dsp][span]")
{
    auto runPattern = [] (float step1, float step2, int span1)
    {
        HeavyHarness h;
        h.set ("host_playing", 0.0f);
        h.setAllSteps (0.0f);
        h.set ("step_1", step1);
        h.set ("step_2", step2);
        h.run (0.1);
        std::vector<float> out;
        for (int bar = 0; bar < 2; ++bar)
            for (int s = 0; s < 16; ++s)
            {
                h.set ("glitch_spansteps", (float) (s == 0 ? span1 : 1));
                h.tick (s);
                auto o = h.run (0.125);
                out.insert (out.end(), o.begin(), o.end());
            }
        return out;
    };

    auto spanned = runPattern (1.0f, 10.0f, 2);    // tapestop tied across 2 steps
    auto retriggered = runPattern (1.0f, 1.0f, 1); // two separate 1-step tapestops
    auto dry = runPattern (0.0f, 0.0f, 1);

    REQUIRE (maxDiff (spanned, dry) > 0.05f);        // span audibly glitches
    REQUIRE (maxDiff (retriggered, dry) > 0.05f);
    REQUIRE (maxDiff (spanned, retriggered) > 0.02f); // and differs from re-triggering
}

TEST_CASE ("post stage: per-effect pan, filter and envelope shape the wet path", "[dsp][post]")
{
    auto channelRms = [] (const std::vector<float>& v, int ch)
    {
        double s = 0;
        long n = 0;
        for (size_t i = (size_t) ch; i < v.size(); i += 2) { s += (double) v[i] * v[i]; ++n; }
        return (float) std::sqrt (s / (double) n);
    };

    // Pan hard left: right channel starves while the gater plays.
    {
        HeavyHarness h;
        h.setAllSteps (7.0f);
        h.set ("glitch_post_pan", -1.0f);
        h.set ("host_playing", 1.0f);
        h.run (0.5);
        auto out = h.run (1.0);
        REQUIRE (channelRms (out, 0) > channelRms (out, 1) * 3.0f);
    }

    // Post lowpass at 300Hz dims the square-ish gater output.
    {
        HeavyHarness open_, lp;
        for (auto* h : { &open_, &lp }) { h->setAllSteps (7.0f); }
        lp.set ("glitch_post_mode", 1.0f);
        lp.set ("glitch_post_freq", 300.0f);
        open_.set ("host_playing", 1.0f);
        lp.set ("host_playing", 1.0f);
        open_.run (0.5);
        lp.run (0.5);
        REQUIRE (rms (lp.run (1.0)) < rms (open_.run (1.0)) * 0.9f);
    }

    // Full step envelope decays each step: quieter than no envelope.
    {
        HeavyHarness flat, env;
        env.set ("seq_stepenv", 1.0f);
        for (auto* h : { &flat, &env }) { h->setAllSteps (0.0f); h->set ("host_playing", 1.0f); h->run (0.5); }
        REQUIRE (rms (env.run (1.0)) < rms (flat.run (1.0)) * 0.85f);
    }
}

TEST_CASE ("master: drive mix, resonance and filter mix are live", "[dsp][master]")
{
    auto runWith = [] (std::initializer_list<std::pair<const char*, float>> params)
    {
        HeavyHarness h;
        h.setAllSteps (0.0f);
        for (const auto& [name, v] : params)
            h.set (name, v);
        h.set ("host_playing", 1.0f);
        h.run (0.5);
        return h.run (1.0);
    };

    // Full drive distorts; drive mix 0 restores the clean signal.
    auto clean = runWith ({});
    auto driven = runWith ({ { "master_drive", 10.0f } });
    auto parallel0 = runWith ({ { "master_drive", 10.0f }, { "master_drive_mix", 0.0f } });
    REQUIRE (maxDiff (driven, clean) > 0.05f);
    REQUIRE (maxDiff (parallel0, clean) < 0.02f);

    // Resonance audibly rings against the plain filtered path.
    auto plain = runWith ({ { "master_lowpass", 500.0f } });
    auto ringing = runWith ({ { "master_lowpass", 500.0f }, { "master_reso", 1.0f } });
    REQUIRE (maxDiff (ringing, plain) > 0.02f);

    // Filter mix 0 bypasses even a brutal highpass.
    auto hp = runWith ({ { "master_filter_type", 1.0f }, { "master_lowpass", 8000.0f } });
    auto hpBypassed = runWith ({ { "master_filter_type", 1.0f }, { "master_lowpass", 8000.0f },
                                 { "master_filter_mix", 0.0f } });
    REQUIRE (rms (hp) < rms (clean) * 0.2f);
    REQUIRE (rms (hpBypassed) > rms (clean) * 0.8f);
}

TEST_CASE ("retrigger: pitch 2 actually pitches the first slice", "[dsp][fx]")
{
    // rate 1 = one slice per step, so before the negative-delay fix the whole
    // step degenerated to live input whenever pitch > 1.
    auto zeroCrossings = [] (const std::vector<float>& v)
    {
        int n = 0;
        for (size_t i = 2; i < v.size(); i += 2)
            if ((v[i - 2] < 0.0f) != (v[i] < 0.0f))
                ++n;
        return n;
    };

    auto runPitched = [] (float pitch)
    {
        HeavyHarness h;
        h.set ("host_playing", 1.0f);
        h.setAllSteps (3.0f);
        h.set ("fx_retrigger_rate", 1.0f);
        h.set ("fx_retrigger_pitch", pitch);
        h.run (0.5);
        return h.run (1.0);
    };

    auto normal = runPitched (1.0f);
    auto pitched = runPitched (2.0f);
    REQUIRE (allFinite (pitched));
    const float ratio = (float) zeroCrossings (pitched) / (float) zeroCrossings (normal);
    REQUIRE (ratio > 1.6f);
    REQUIRE (ratio < 2.4f);
}

TEST_CASE ("tapestop: speed 0.1 parks instead of replaying stale buffer", "[dsp][fx]")
{
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    h.setAllSteps (10.0f);
    h.set ("step_1", 1.0f);
    h.set ("fx_tapestop_speed", 0.1f);
    h.run (4.0); // fill the 4s capture so a runaway read would find loud stale audio
    h.set ("glitch_spansteps", 8.0f);
    h.tick (0);
    auto out = h.run (1.0); // one 8-step span at 120bpm
    REQUIRE (allFinite (out));

    const size_t quarter = (out.size() / 8) * 2; // quarter of the span, even index
    std::vector<float> head (out.begin(), out.begin() + (long) quarter);
    std::vector<float> tail (out.end() - (long) quarter, out.end());
    // Once fully rewound the read parks at the span start, each channel
    // holding one interpolated sample. The held level is whatever the input
    // was at that instant — it lands differently per channel and per SIMD
    // width — so level tells us nothing. Oscillation does: a runaway read
    // replays the 220Hz fill (~110 crossings per quarter span), a parked
    // hold barely moves.
    auto channel = [] (const std::vector<float>& inter, size_t first) {
        std::vector<float> v;
        for (size_t i = first; i < inter.size(); i += 2)
            v.push_back (inter[i]);
        return v;
    };
    auto crossings = [] (const std::vector<float>& v) {
        int n = 0;
        for (size_t i = 1; i < v.size(); ++i)
            if ((v[i - 1] < 0.0f) != (v[i] < 0.0f))
                ++n;
        return n;
    };
    REQUIRE (crossings (channel (head, 0)) > 15); // sanity: tape still moving early on
    REQUIRE (crossings (channel (tail, 0)) < 10);
    REQUIRE (crossings (channel (tail, 1)) < 10);
}

TEST_CASE ("smoothing: block-rate parameter jumps stay bounded", "[dsp][fx]")
{
    struct Site
    {
        const char* param;
        float fx, lo, hi, factor;
    };
    // Drive toggles below the clipper knee so the zipper isn't saturated away;
    // the gater's edges are inherently steep, so its bound is only a smoke check.
    const Site sites[] = {
        { "fx_crush_drive", 6.0f, 1.0f, 1.9f, 1.5f },
        { "fx_delay_feedback", 8.0f, 0.0f, 0.95f, 1.5f },
        { "fx_gate_duty", 7.0f, 0.05f, 0.95f, 3.0f },
    };

    for (const auto& s : sites)
    {
        DYNAMIC_SECTION (s.param)
        {
            auto maxStep = [] (const std::vector<float>& v)
            {
                float m = 0;
                for (size_t i = 2; i < v.size(); i += 2)
                    m = std::max (m, std::fabs (v[i] - v[i - 2]));
                return m;
            };

            auto runToggling = [&s] (bool toggle)
            {
                HeavyHarness h;
                h.set ("host_playing", 1.0f);
                h.setAllSteps (s.fx);
                h.set ("fx_crush_rate", 20000.0f);
                h.set (s.param, s.hi);
                h.run (0.5);
                std::vector<float> out;
                for (int b = 0; b < SR / BLOCK; ++b)
                {
                    if (toggle)
                        h.set (s.param, (b & 1) != 0 ? s.lo : s.hi);
                    auto o = h.run ((double) BLOCK / SR);
                    out.insert (out.end(), o.begin(), o.end());
                }
                return out;
            };

            auto toggled = runToggling (true);
            auto steady = runToggling (false);
            REQUIRE (allFinite (toggled));
            REQUIRE (maxStep (toggled) < std::max (s.factor * maxStep (steady), 0.05f));
        }
    }
}

TEST_CASE ("seed: fixed seed reproduces chaos and shuffle", "[dsp][seq]")
{
    auto chaosRun = [] (float seed)
    {
        HeavyHarness h;
        h.set ("seq_seed", seed);
        h.setAllSteps (0.0f);
        h.set ("seq_chaos", 1.0f);
        h.set ("host_playing", 1.0f);
        h.run (0.5);
        return h.run (1.5);
    };
    auto a = chaosRun (42.0f);
    auto b = chaosRun (42.0f);
    auto c = chaosRun (43.0f);
    REQUIRE (allFinite (a));
    REQUIRE (maxDiff (a, b) < 1e-6f);
    REQUIRE (maxDiff (a, c) > 0.01f);

    auto shuffleRun = [] (float seed)
    {
        HeavyHarness h;
        h.set ("seq_seed", seed);
        h.setAllSteps (4.0f);
        h.set ("fx_shuffle_range", 8.0f);
        h.set ("host_playing", 1.0f);
        h.run (0.5);
        return h.run (1.5);
    };
    auto sa = shuffleRun (7.0f);
    auto sb = shuffleRun (7.0f);
    auto sc = shuffleRun (8.0f);
    REQUIRE (maxDiff (sa, sb) < 1e-6f);
    REQUIRE (maxDiff (sa, sc) > 0.01f);
}

TEST_CASE ("modulator: ring mod is phase-coherent per step", "[dsp][fx]")
{
    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    h.setAllSteps (2.0f);
    h.set ("fx_mod_freq", 3.0f); // slow tremolo, deliberately incommensurate with the step grid
    std::vector<float> in;
    auto out = h.run (2.0, &in);
    REQUIRE (allFinite (out));

    // The osc resets to cos(0)=1 at each step start. Heavy applies messages to
    // signal inlets with up to a block of latency, so probe 15-20ms after the
    // trigger: late enough for the worst-case reset, early enough that a 3Hz
    // cosine has barely moved off its peak.
    int checked = 0;
    float worst = 1.0f;
    for (size_t n = 1; n < playheadEvents().size(); ++n) // skip the startup step
    {
        const auto& ev = playheadEvents()[n];
        const size_t a = ((size_t) ev.sample + (size_t) (0.015 * SR)) * 2;
        const size_t b = ((size_t) ev.sample + (size_t) (0.020 * SR)) * 2;
        if (b >= out.size())
            break;
        std::vector<float> ow (out.begin() + (long) a, out.begin() + (long) b);
        std::vector<float> iw (in.begin() + (long) a, in.begin() + (long) b);
        worst = std::min (worst, rms (ow) / rms (iw));
        ++checked;
    }
    REQUIRE (checked >= 8);
    REQUIRE (worst > 0.7f);
}

TEST_CASE ("crusher: bit depth limits distinct output levels", "[dsp][fx]")
{
    auto clusters = [] (const std::vector<float>& v)
    {
        std::set<long> distinct;
        for (float x : v)
            distinct.insert (std::lround (x * 100.0f));
        return distinct.size();
    };

    // The harness has no JUCE wrapper, so push the Pd-side level count
    // (2^(bits-1)) that pushChangedParameters derives from fx_crush_bits.
    auto runLevels = [] (float levels)
    {
        HeavyHarness h;
        h.set ("host_playing", 1.0f);
        h.setAllSteps (6.0f);
        h.set ("fx_crush_drive", 1.0f);
        h.set ("fx_crush_rate", 20000.0f);
        h.set ("master_filter_mix", 0.0f); // keep the master IIR from smearing the grid
        h.set ("fx_crush_level", levels);
        h.run (0.5);
        return h.run (1.0);
    };

    auto crushed = runLevels (2.0f);     // bits 2
    auto clean = runLevels (32768.0f);   // bits 16, transparent
    REQUIRE (allFinite (crushed));
    REQUIRE (clusters (crushed) <= 5);   // 2^bits + 1 levels at most
    REQUIRE (clusters (clean) > 50);
    REQUIRE (maxDiff (crushed, clean) > 0.05f);
}

TEST_CASE ("delay: echoes ring past the step and decay", "[dsp][fx]")
{
    // Manual ticks so the single delay step deterministically fires with the
    // new pattern (the standalone metro would read the bar before it lands).
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    h.setAllSteps (0.0f);
    h.set ("step_1", 8.0f);
    h.set ("fx_delay_feedback", 0.7f);
    h.run (0.1);

    std::vector<float> out, in;
    for (int s = 0; s < 16; ++s)
    {
        h.set ("glitch_spansteps", 1.0f);
        h.tick (s);
        std::vector<float> i;
        auto o = h.run (0.125, &i);
        out.insert (out.end(), o.begin(), o.end());
        in.insert (in.end(), i.begin(), i.end());
    }
    REQUIRE (allFinite (out));

    auto windowDiff = [&] (double t0, double t1)
    {
        float m = 0;
        for (size_t i = (size_t) (t0 * SR) * 2; i < (size_t) (t1 * SR) * 2 && i < out.size(); ++i)
            m = std::max (m, std::fabs (out[i] - in[i]));
        return m;
    };

    // The delay step ends at ~120ms; the following steps are dry, so any
    // difference from the input there is the echo tail ringing out.
    const float earlyTail = windowDiff (0.15, 0.40);
    const float lateTail = windowDiff (1.40, 1.85);
    REQUIRE (earlyTail > 0.02f);
    REQUIRE (lateTail < 0.5f * earlyTail); // and it decays instead of piling up
}

TEST_CASE ("gates: equal-power crossfade between correlated steps", "[dsp][fx]")
{
    // Dry and a transparent crusher (drive 1, 20kHz rate, 16 bits) carry
    // near-identical audio, so the sqrt fade law shows as a ~sqrt(2) envelope
    // bump at the crossfade midpoint. Linear gates would stay flat at 1.
    HeavyHarness h;
    h.set ("host_playing", 0.0f);
    h.setAllSteps (0.0f);
    h.set ("step_2", 6.0f);
    h.set ("fx_crush_drive", 1.0f);
    h.set ("fx_crush_rate", 20000.0f);
    h.run (0.5);

    float worstBump = 1.0f;
    float steadyPeak = 0.0f;
    for (int t = 0; t < 16; ++t)
    {
        h.set ("glitch_spansteps", 1.0f);
        h.tick (t % 2); // alternate dry <-> crusher
        auto o = h.run (0.125);
        if (t < 2)
            continue; // let the alternation establish itself
        float bump = 0.0f, steady = 0.0f;
        for (size_t i = 0; i < o.size(); i += 2)
        {
            const double ms = 1000.0 * (double) (i / 2) / SR;
            if (ms < 25.0)
                bump = std::max (bump, std::fabs (o[i]));
            else if (ms > 35.0)
                steady = std::max (steady, std::fabs (o[i]));
        }
        worstBump = std::min (worstBump, bump);
        steadyPeak = std::max (steadyPeak, steady);
    }
    REQUIRE (worstBump > 0.58f);  // sqrt fades: ~0.707 peak on a 0.5 input
    REQUIRE (worstBump < 0.75f);  // and bounded, no runaway
    REQUIRE (steadyPeak < 0.56f); // steady-state stays unity
}

TEST_CASE ("reverser: per-channel amounts", "[dsp][fx]")
{
    auto channelDiff = [] (const std::vector<float>& a, const std::vector<float>& b, int ch)
    {
        float m = 0;
        for (size_t i = (size_t) ch; i < std::min (a.size(), b.size()); i += 2)
            m = std::max (m, std::fabs (a[i] - b[i]));
        return m;
    };

    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    h.setAllSteps (5.0f);
    h.set ("fx_rev_left", 1.0f);
    h.set ("fx_rev_right", 0.0f);
    h.run (0.5);
    std::vector<float> in;
    auto out = h.run (1.0, &in);
    REQUIRE (allFinite (out));
    REQUIRE (channelDiff (out, in, 1) < 0.05f); // right at 0% plays forward
    REQUIRE (channelDiff (out, in, 0) > 0.05f); // left at 100% still reverses
}

TEST_CASE ("stretcher: consecutive steps scrub instead of click", "[dsp][fx]")
{
    // Each step start rewinds the read pointer by half a step; the lop~ slew
    // turns that skip into a fast tape flick. Unslewed it was a 0.5 jump.
    HeavyHarness h;
    h.set ("host_playing", 1.0f);
    h.setAllSteps (9.0f);
    auto out = h.run (2.0);
    REQUIRE (allFinite (out));

    float worst = 0;
    for (size_t i = (size_t) (0.15 * SR) * 2 + 2; i < out.size(); i += 2)
        worst = std::max (worst, std::fabs (out[i] - out[i - 2]));
    REQUIRE (worst < 0.25f);
}
