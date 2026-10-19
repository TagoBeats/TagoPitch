# TagoPitch 1.0.2

Fixes two cases where the interface showed something other than what the
plugin was actually doing after you reopened the window.

## Fixed

- **Preset name reverted to INIT.** After closing and reopening the plugin
  window, the header said INIT even though the preset's settings were still
  loaded. The name now comes from the actual settings, and reads CUSTOM once
  you move a knob away from the preset.
- **Knobs could show default values.** On reopening, the knobs could sit at
  their defaults while the plugin kept running with your real settings. The
  interface now reads the current values as soon as it opens.

## Changed

- **New interface font.** The numbers and labels now use Martian Mono.

## Install

- **macOS 11+:** run the `.pkg` and pick the formats you want (AU + VST3,
  notarized).
- **Windows:** unzip and drop the `.vst3` into your VST3 folder.

Settings and presets from earlier versions carry over unchanged.
