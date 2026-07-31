# OpenGlitch

An open-source recreation of **dblue Glitch 1.3** (original concept by Kieran Foster),
built from:

- **Pure Data** patches compiled to C/C++ by **[hvcc](https://github.com/Wasted-Audio/hvcc)** (Wasted-Audio fork)
- **[JUCE 8](https://github.com/juce-framework/JUCE)** for the plugin wrapper and GUI
- **CMake** for the build

Licensed under **GPLv3** (see [LICENSE](LICENSE)).

## Status

Phase 1 — build skeleton. The plugin compiles as VST3 and Standalone and passes
stereo audio through the hvcc-generated Heavy context, with a single `bypass`
parameter (0 = pass audio, 1 = mute with a 10 ms ramp) proving the
JUCE → Heavy parameter path works.

## Building

Requirements: CMake ≥ 3.22, a C++17 compiler, Python ≥ 3.9, and (on Linux) the
usual JUCE dev packages (ALSA, X11, freetype, fontconfig).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The first configure fetches JUCE and hvcc and installs hvcc into a virtualenv
inside `build/`. The Pd patch `pd/main.pd` is recompiled by hvcc automatically
whenever it changes.

Artifacts land in `build/OpenGlitch_artefacts/Release/`:

- `VST3/OpenGlitch.vst3`
- `Standalone/OpenGlitch`

## Layout

- `pd/main.pd` — the DSP, as a Pure Data patch (edit with plain Pd-vanilla)
- `src/` — JUCE C++ wrapper (processor, later the step-sequencer GUI)
- `CMakeLists.txt` — fetches JUCE + hvcc, runs hvcc, builds the plugin
