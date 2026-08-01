# dBlue Glitch 1.3 → OpenGlitch parity map

Control-by-control comparison against the original Glitch 1.3.05 panel
(`docs/original-dblue-glitch-1.3.png`). **Implemented** = same capability,
**Adapted** = same musical intent, different control shape, **Deferred** =
not in 1.0.

## Top strip

| Original | OpenGlitch | Status |
|---|---|---|
| Amount | `master_mix` (MIX fader) | Adapted — one wet/dry blend instead of Amount + Output Mix |
| Seed | `seq_seed` (SEED box) | Implemented — also seeds the Pd-side chaos & shuffler randoms; 0 = fresh seed each session |
| De-Click | `seq_declick` (CLICK fader, 1–30 ms) | Implemented — drives every effect gate's equal-power crossfade |
| Step Envelope (steps + mode) | `seq_stepenv` (ENV fader) | Adapted — single per-step volume decay over the step/span |
| Overdrive Mix / Gain | `master_drive_mix` / `master_drive` | Implemented |
| Master Filter shape row | `master_sweep_shape` + `master_sweep_amt` | Implemented — sweep envelopes per step |
| Master Filter Mode / Mix / Freq / Q | `master_filter_type` / `master_filter_mix` / `master_lowpass` / `master_reso` | Implemented |
| Master Output Mix / Volume | `master_mix` / `master_volume` | Implemented |
| Disable MIDI / Disable Transport | — | Deferred — hosted transport is auto-detected; standalone free-runs |

## Sequencer row

| Original | OpenGlitch | Status |
|---|---|---|
| Chosen Effect (R, 1–9) + block timeline | 9×16 step matrix (paint, drag-Tie, right-click erase) | Adapted — deliberate redesign |
| Randomise Steps / FX / Both | DICE (steps) + FX (effect params) | Implemented — "Both" = press both |
| Templates 1–4 | T1–T4 buttons | Implemented |
| Pattern Bank 1–16 | PTN 1–16 + `pattern_select` + MIDI notes 36–51 | Implemented |
| Right-click MIDI learn | — | Deferred |
| Length arrows | `seq_length` slider (1–16) | Implemented |
| Shift arrows | `<` `>` header buttons | Implemented |
| — | `seq_swing`, `seq_chaos`, two LFOs (LFO2 can drive LFO1) | OpenGlitch additions |

## Effects

| # | Original controls | OpenGlitch | Status |
|---|---|---|---|
| 1 TapeStop | SlowDown / Delay / SpeedUp | `fx_tapestop_speed` | Adapted — one speed knob covers slow-down↔speed-up; separate delay curve Deferred |
| 2 Modulator | Frequency | `fx_mod_freq` + `fx_mod_sync` tempo-sync | Implemented (sync is an addition; phase resets per step) |
| 2 Modulator | Spread / FM Amt / FM Speed / Sine Mix | — | Deferred |
| 3 Retrigger | Speed | `fx_retrigger_rate` (slices per step) | Adapted |
| 3 Retrigger | Pitch | `fx_retrigger_pitch` (works across the full range, incl. pitch-up) | Implemented |
| 3 Retrigger | Length / S.Change | — | Deferred |
| 4 Shuffler | Minimum / Maximum | `fx_shuffle_range` | Adapted — single range control |
| 5 Reverser | Left / Right | `fx_rev_left` / `fx_rev_right` | Implemented — per-channel reverse amounts (defaults 100%/100%) |
| 5 Reverser | Ping Pong mode | — | Deferred |
| 6 Crusher | Amount (bits) | `fx_crush_bits` (1–16, LFO-modulatable) | Implemented |
| 6 Crusher | Quantise | `fx_crush_rate` (sample-rate decimation) | Adapted |
| 6 Crusher | Smooth | — | Deferred (drive into hard clip is an OpenGlitch addition) |
| 7 Gater | Speed / Length | `fx_gate_rate` / `fx_gate_duty` | Implemented |
| 7 Gater | Volume | `fx7_gain` (per-effect output gain) | Adapted |
| 8 Delay | Size | `fx_delay_div` (tempo divisions of a step) | Adapted |
| 8 Delay | Feedback | `fx_delay_feedback`; tails ring past the step like the original | Implemented |
| 8 Delay | Dry Vol / Wet Vol | `fx8_mix` / `fx8_gain` | Adapted |
| 8 Delay | Separation | — | Deferred |
| 9 Stretcher | Divisor / Amount / X-Fade | `fx_stretch_speed` | Adapted — single speed control |

## Per-effect column footer (all 9 effects)

| Original | OpenGlitch | Status |
|---|---|---|
| Init / Rand per effect | global FX dice (seeded) | Adapted |
| Probability ("50") / S / G toggles | — | Deferred |
| Sweep shape row | `fxN_sweep_shape` / `fxN_sweep_amt` | Implemented |
| Filter Mode / Freq | `fxN_post_mode` / `fxN_post_freq` (smoothed) | Implemented — shared post stage snapped per trigger |
| Filter Q | — (resonance lives on the master filter) | Deferred |
| Pan / Mix / Gain | `fxN_pan` / `fxN_mix` / `fxN_gain` | Implemented |

## Not in the original

Tie spans painted in the matrix, per-step chaos amount, two tempo-synced LFOs
with cascaded modulation (LFO2 → LFO1 rate/depth), crush-bit LFO target,
per-effect filter sweep envelopes, seeded reproducible randomness, standalone
drop-in loop player, resizable UI.
