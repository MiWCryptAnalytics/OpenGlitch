# OpenGlitch — design notes

Working notes on how OpenGlitch is put together, moved out of the README.
The control-by-control comparison against the original Glitch 1.3 panel
lives in [PARITY.md](PARITY.md).

## Big picture

The DSP is a Pure Data patch (`pd/main.pd` plus abstractions, editable in
plain Pd-vanilla) compiled to C by [hvcc](https://github.com/Wasted-Audio/hvcc)
at CMake configure time, built as a static library, and wrapped in JUCE 8
(processor + custom GUI). Editing any patch file re-runs hvcc automatically
on the next build.

- `pd/glitch_clock.pd` — the 16-step sequencer
- `pd/fx_*.pd` — the nine effects plus dry
- `pd/glitch_post.pd` — the per-effect output stage
- `pd/glitch_master.pd` — master drive / filter / mix
- `src/` — JUCE processor and editor
- `tools/` — headless screenshot renderer and demo-audio renderer

## One clock

JUCE drives the sequencer everywhere. When a DAW timeline is present the Pd
metro is switched off and every 16th note is derived from `ppqPosition` and
scheduled sample-accurately into the Heavy context (`host_tick`), so the grid
locks to the DAW bar and follows loops, relocates and tempo changes. Step 1
is always the start of a bar; on transport stop the engine falls back to
clean dry. Without a timeline a virtual transport free-runs, so swing, chaos
and spans behave identically in DAWs and standalone.

Swing (0–100%) is sample-accurately scheduled in hosted mode and delays
odd-step firing in the standalone clock.

## Sequencer and effects

Nine effect rows × 16 step columns; only one effect sounds at a time, gated
in with equal-power crossfades (De-Click, 1–30 ms). Effect indices for the
`step_1..16` parameters:

| # | Effect | Character | Controls |
|---|--------|-----------|----------|
| 0 | Dry | untouched input | — |
| 1 | TapeStop | slows to a stop across the step | `fx_tapestop_speed` |
| 2 | Modulator | ring mod, tremolo → metallic | `fx_mod_freq` |
| 3 | Retrigger | loops the step's first slice | `fx_retrigger_rate`, `fx_retrigger_pitch` |
| 4 | Shuffler | swaps in a random earlier step | `fx_shuffle_range` |
| 5 | Reverser | plays recent audio backwards, per-channel amounts | `fx_rev_left`, `fx_rev_right` |
| 6 | Crusher | sample-rate crush + bit depth + drive | `fx_crush_rate`, `fx_crush_drive`, `fx_crush_bits` |
| 7 | Gater | rhythmic chopper | `fx_gate_rate`, `fx_gate_duty` |
| 8 | Delay | tempo-synced damped echo, tails ring past the step | `fx_delay_div`, `fx_delay_feedback` |
| 9 | Stretcher | tape-style half-speed drag | `fx_stretch_speed` |

Step parameters carry an 11th choice ("Tie") for **spans**: tied steps don't
re-trigger — a tape stop winds down across the whole span, a retrigger keeps
looping its first slice, a reverser reverses the full length, and filter
sweep envelopes stretch across the span.

`seq_chaos` randomly overrides the programmed grid. DICE fills the active
pattern with a musically weighted random grid (occasionally tying spans);
the FX randomiser dices every effect knob and output strip, with a non-zero
Seed making rolls reproducible and advancing per press. The Delay's bus
input (not its wet output) is gated, so its tail rings out past the step.

**Patterns:** 16 banks, per-pattern length (1–16 steps) for odd meters and
polymeter, and MIDI notes 36–51 (C1–D#2) switch banks live. Edits always
record into the active slot; shift-click a slot copies the current pattern
there; `<` `>` rotate the grid within its length. Four factory templates
(stutter, buildup, halftime wreck, ambient smear) sit on T1–T4.

## Output stages

Every effect has its own output strip — filter (Off/LP/HP/BP + freq), pan,
mix, gain — like the original's column strips. It is implemented as one
shared post stage that snaps to whichever effect fires (only one sounds at
a time); live tweaks stream instantly. Filter sweep envelopes (Down / Up /
Tri / Sine / Square) sweep the cutoff across each step in octaves, per
effect and on the master filter.

The master section adds a multimode filter (lowpass / highpass / bandpass —
the bandpass is a hip~/lop~ series pair) with Resonance (a bp~ peak layered
on the cutoff) and Filter Mix, Overdrive with a parallel Overdrive Mix, a
per-step decay envelope, and a ramped, click-free output volume. Mono
tracks are supported: mono in is duplicated, mono out averaged.

## LFOs

Two tempo-synced LFOs (sine / tri / saw / square / S&H random, 1/16 to
4 bars, ppq-locked to the DAW bar). Targets: filter freq, drive, chaos, mod
freq, retrigger pitch, gate duty — and LFO 2 can instead drive LFO 1's rate
or depth for cascaded, derivative modulation. The modulation scope draws
both LFOs over the bar from the real coupled math, so when LFO 2 warps
LFO 1's rate you watch the trace warp exactly as it sounds. The Modulator's
SYNC selector locks its ring-mod frequency to divisions from 1/16 to a bar.

## Parameters and state

Every parameter is exposed to the DAW through a JUCE
`AudioProcessorValueTreeState` (grouped Sequencer / Effects / Master, step
slots as named choices, proper units and log skews, `bypass` reported as
the host bypass parameter). Plugin state — including the editor scale —
saves and restores with the session. The sequencer emits a `playhead`
output parameter for the GUI.

Caveat for contributors: adding entries to an `AudioParameterChoice`
rescales saved normalized values in old sessions. That was accepted policy
before 1.0; from 1.0 on, changing a choice list needs a real migration
strategy.

## GUI

The editor recreates the Glitch matrix: click a cell to assign the effect,
click again to clear, drag to paint, drag across a row to tie a span,
right-click to erase a column. The playhead lamp and column wash follow the
sequencer, an LCD shows step and tempo, the bottom panel swaps to the knobs
of the last-touched effect, and the right-hand strip carries Chaos / Drive /
Filter / Mix plus Bypass. Everything binds through APVTS attachments. The
window is resizable (locked aspect ratio, scale persisted in plugin state),
every control has a tooltip, and clicking the logo opens the About overlay.

The standalone doubles as a loop player: drop a WAV/FLAC on the window and
it loops through the engine.

## Tests

A Catch2/CTest suite covers four layers: the Heavy DSP engine driven
directly (sequencer, effects, external clock, swing/length timing, filter
modes), the pure LFO/modulation math, full host simulation (a fake
`AudioPlayHead` feeding transport/tempo/ppq to the real processor — the
layer neither pluginval nor DSP tests can see), and wrapper tests that load
the actual built `.vst3` like a DAW. Each test case runs as an isolated
process with a timeout, so hangs fail instead of blocking. CI additionally
runs [pluginval](https://github.com/Tracktion/pluginval) at strictness 5 on
every platform.

## Tools

`-DOPENGLITCH_BUILD_TOOLS=ON` builds two console apps:

- `OpenGlitchScreenshot` renders the editor to PNG without a display
  (after running real audio through the engine); `--about` renders the
  About overlay, `--scale` a resized editor.
- `OpenGlitchRender` renders the demo pattern and each effect solo to WAVs
  in `build/renders/` for manual A/B against the original Glitch 1.3.

## CI and releases

GitHub Actions builds the full matrix — Linux (VST3, LV2, Standalone),
macOS (VST3, AU, Standalone, arm64+x86_64 universal), Windows (VST3,
Standalone) — runs ctest and pluginval on every push, and packages each
platform with the `openglitch_package` CMake target. Pushing a `v*` tag
publishes the zips as a GitHub release; the tag must match
`project(VERSION)` in CMakeLists.txt or the release job fails.
