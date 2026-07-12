# TagoPitch

Monophonic vocal pitcher (pitch, formant, mix, gain), VST3/AU, JUCE 8 + WebView UI (React).
DSP reference lives in the Python prototype: `tagodsp.pitch.PitchShifter` (~/Documents/tagodsp).

## Layout

- `plugin/` C++ sources (processor, WebView editor)
- `ui/` Vite + React + TypeScript frontend, bound via the official JUCE JS frontend library
- `mockup/` approved static HTML mockup, design source of truth for the UI port
- `third_party/JUCE` JUCE 8.0.14, pinned git submodule
- `scripts/build-ui.sh` builds the UI and bundles it as `ui/webui.zip` (BinaryData)

## Build

```sh
./scripts/build-ui.sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TagoPitch_Standalone   # or TagoPitch_VST3 / TagoPitch_AU
```

## UI dev loop

Develop the UI standalone in the browser first, then check it inside the plugin:

```sh
cd ui && npm run dev        # Vite dev server on :5173
cmake -B build-dev -DTAGOPITCH_DEV_UI=ON
cmake --build build-dev --target TagoPitch_Standalone
```

With `TAGOPITCH_DEV_UI=ON` the editor loads `http://localhost:5173` (hot reload)
instead of the bundled `ui/webui.zip`.

## Parameters

IDs are shared with the Python prototype and the WebView relays:
`pitch_semitones` (int, ±12), `formant_semitones` (±12), `mix` (0–100 %),
`gain_db` (±18, output gain after mix), `bypass`.
`formant_base_hz` exists as a hidden, non-automatable parameter: it will be
driven internally by pitch detection (v2 hard-tune groundwork), no user knob.

## Still open (v1 implementation step)

- Port the signalsmith-stretch engine (vendor the exact headers from tagodsp
  for engine parity), report latency via `setLatencySamples()`
- Port the approved mockup (`mockup/index.html`) to React, screenshot loop
- DAW test: Ableton/FL, latency report, keyboard focus trap check
