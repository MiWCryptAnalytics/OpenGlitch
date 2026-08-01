# OpenGlitch

[![CI](https://github.com/MiWCryptAnalytics/OpenGlitch/actions/workflows/ci.yml/badge.svg)](https://github.com/MiWCryptAnalytics/OpenGlitch/actions/workflows/ci.yml)

![OpenGlitch UI](docs/screenshot.png)

OpenGlitch is a free recreation of **dblue Glitch 1.3**, the discontinued
freeware glitch effect originally created by Kieran Foster. It slices
incoming audio on a 16-step grid and fires a different effect on every
step: tape stops, ring mod, retriggers, shuffles, reverses, bit crushes,
gating, delay, tape-drag stretches.

It's an independent GPL project, not affiliated with or endorsed by
illformed. Under the hood the DSP is a Pure Data patch compiled to C++ with
[hvcc](https://github.com/Wasted-Audio/hvcc) and wrapped in
[JUCE 8](https://github.com/juce-framework/JUCE).

## Highlights

- Nine effects on a 9×16 grid — click to place, drag to paint, drag along a
  row to tie steps into longer spans (a tape stop that winds down over four
  steps, a reverser that flips a whole bar)
- 16 pattern banks, switchable live over MIDI (notes 36–51), with
  per-pattern length and swing for odd meters and polymeter loops
- Chaos, one-click DICE fills, and an FX randomiser with a reproducible seed
- Two tempo-synced LFOs with a live scope — LFO 2 can modulate LFO 1's rate
  or depth for cascaded movement
- Per-effect output strips (filter, pan, mix, gain) plus a master multimode
  filter with resonance, overdrive, and filter sweep envelopes
- Locks sample-accurately to the host transport; the standalone free-runs
  with identical timing, and you can drop a WAV/FLAC on it to loop through
  the engine

## Download

Grab the zip for your platform from the
[releases page](https://github.com/MiWCryptAnalytics/OpenGlitch/releases).
Each one contains the standalone app and:

- **Windows** — VST3
- **macOS** — VST3 and AU (universal: Apple Silicon + Intel)
- **Linux** — VST3 and LV2

Copy the plugin into your usual folder (`~/.vst3` and `~/.lv2` on Linux,
`~/Library/Audio/Plug-Ins` on macOS, `C:\Program Files\Common Files\VST3`
on Windows).

The macOS builds are not code-signed, so Gatekeeper will quarantine them.
After unzipping, clear the flag with:

```sh
xattr -cr OpenGlitch.vst3 OpenGlitch.component OpenGlitch.app
```

## Building

Requirements: CMake ≥ 3.22, a C++17 compiler, Python ≥ 3.9, and on Linux
the usual JUCE dev packages (ALSA, X11, freetype, fontconfig — the exact
apt list is in [.github/workflows/ci.yml](.github/workflows/ci.yml)).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The first configure fetches JUCE and hvcc and installs hvcc into a
virtualenv inside `build/`. The Pd patch is recompiled automatically
whenever it changes, and built plugins are copied into your user plugin
folders (pass `-DOPENGLITCH_INSTALL_PLUGIN=OFF` to skip that).

To run the tests:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOPENGLITCH_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build -j$(nproc)
```

## Documentation

- [docs/DESIGN.md](docs/DESIGN.md) — how it works: the Pd → hvcc → JUCE
  pipeline, the clock, effects and parameters, tests
- [docs/PARITY.md](docs/PARITY.md) — control-by-control comparison with the
  original Glitch 1.3

## License

GPLv3 — see [LICENSE](LICENSE). Original concept by Kieran Foster; this
project is not affiliated with illformed. Bundled fonts (Rubik Glitch,
Space Mono) are under the SIL Open Font License.
