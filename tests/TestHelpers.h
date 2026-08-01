#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Heavy_OpenGlitch.h"

// Shared helpers for driving the Heavy DSP context directly (no JUCE).
namespace testutil
{
constexpr int SR = 48000;
constexpr int BLOCK = 512;

struct PlayheadEvent
{
    float step;
    unsigned sample;
};

// The send hook has to be a plain function pointer, so event capture is global.
inline std::vector<PlayheadEvent>& playheadEvents()
{
    static std::vector<PlayheadEvent> events;
    return events;
}

inline void sendHook (HeavyContextInterface* c, const char* name, hv_uint32_t, const HvMessage* m)
{
    if (std::strcmp (name, "playhead") == 0)
        playheadEvents().push_back ({ hv_msg_getFloat (m, 0), hv_getCurrentSample (c) });
}

// Creates a context with defaults pushed the way the JUCE wrapper does
// (everything except host_playing, which transport logic owns).
struct HeavyHarness
{
    HeavyContextInterface* hv;

    HeavyHarness()
        : hv (hv_OpenGlitch_new_with_options ((double) SR, 10, 32, 2))
    {
        playheadEvents().clear();
        hv_setSendHook (hv, sendHook);
        const int n = hv_getParameterInfo (hv, 0, nullptr);
        for (int i = 0; i < n; ++i)
        {
            HvParameterInfo info;
            if (hv_getParameterInfo (hv, i, &info) > 0
                && info.type == HV_PARAM_TYPE_PARAMETER_IN
                && info.hash != hv_stringToHash ("host_playing"))
                hv_sendFloatToReceiver (hv, info.hash, info.defaultVal);
        }
    }

    // hv_OpenGlitch_free, not hv_delete: hv_delete plain-deletes memory that
    // came from (aligned) hv_malloc + placement new — heap corruption on the
    // Windows CRT.
    ~HeavyHarness() { hv_OpenGlitch_free (hv); }

    void set (const char* name, float v) { hv_sendFloatToReceiver (hv, hv_stringToHash (name), v); }

    void setAllSteps (float v)
    {
        char name[16];
        for (int i = 1; i <= 16; ++i)
        {
            std::snprintf (name, sizeof (name), "step_%d", i);
            set (name, v);
        }
    }

    void tick (int index, double delayMs = 0.0)
    {
        hv_sendMessageToReceiverV (hv, hv_stringToHash ("host_tick"), delayMs, "f", (double) index);
    }

    // Processes a 220Hz sine; returns interleaved output (and input if asked).
    std::vector<float> run (double seconds, std::vector<float>* inputOut = nullptr)
    {
        const int total = ((int) (seconds * SR) / BLOCK) * BLOCK;
        std::vector<float> out ((size_t) total * 2), in ((size_t) total * 2);
        float inL[BLOCK], inR[BLOCK], outL[BLOCK], outR[BLOCK];
        float* ins[2] = { inL, inR };
        float* outs[2] = { outL, outR };
        for (int start = 0; start < total; start += BLOCK)
        {
            for (int i = 0; i < BLOCK; ++i)
            {
                const float s = 0.5f * std::sin (2.0f * 3.14159265f * 220.0f
                                                 * (float) (start + i) / (float) SR);
                inL[i] = s;
                inR[i] = -s;
            }
            hv_process (hv, ins, outs, BLOCK);
            for (int i = 0; i < BLOCK; ++i)
            {
                out[(size_t) (start + i) * 2] = outL[i];
                out[(size_t) (start + i) * 2 + 1] = outR[i];
                in[(size_t) (start + i) * 2] = inL[i];
                in[(size_t) (start + i) * 2 + 1] = inR[i];
            }
        }
        if (inputOut != nullptr)
            *inputOut = in;
        return out;
    }
};

inline bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}

inline float rms (const std::vector<float>& v)
{
    double s = 0;
    for (float x : v)
        s += (double) x * x;
    return (float) std::sqrt (s / (double) v.size());
}

inline float maxDiff (const std::vector<float>& a, const std::vector<float>& b)
{
    float m = 0;
    const size_t n = std::min (a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
        m = std::max (m, std::fabs (a[i] - b[i]));
    return m;
}
} // namespace testutil
