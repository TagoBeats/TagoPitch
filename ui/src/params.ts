// Parameter contract, mirrors tagopitch::param in PluginProcessor.h.
// IDs must never change; ranges match the APVTS layout.

export interface ParamSpec {
  id: string;
  label: string;
  min: number;
  max: number;
  def: number;
  step: number; // 0 = continuous
  bipolar: boolean;
  size: number; // knob diameter in px, from the mockup
  fmt: (v: number) => string;
}

// U+2212 minus, exactly like the mockup formatting
const sign = (v: number) => (v > 0 ? "+" : v < 0 ? "−" : "");

export const PARAMS: Record<string, ParamSpec> = {
  pitch: {
    id: "pitch_semitones",
    label: "pitch",
    min: -12,
    max: 12,
    def: 0,
    step: 1,
    bipolar: true,
    size: 118,
    fmt: (v) => sign(v) + Math.abs(v) + " st",
  },
  formant: {
    id: "formant_semitones",
    label: "formant",
    min: -12,
    max: 12,
    def: 0,
    step: 0,
    bipolar: true,
    size: 118,
    fmt: (v) => sign(v) + Math.abs(v).toFixed(1) + " st",
  },
  mix: {
    id: "mix",
    label: "mix",
    min: 0,
    max: 100,
    def: 100,
    step: 0,
    bipolar: false,
    size: 64,
    fmt: (v) => Math.round(v) + " %",
  },
  gain: {
    id: "gain_db",
    label: "gain",
    min: -18,
    max: 18,
    def: 0,
    step: 0,
    bipolar: true,
    size: 64,
    fmt: (v) => sign(v) + Math.abs(v).toFixed(1) + " dB",
  },
};

// v1 presets (REQUIREMENTS.md 5.3): parameter values only, no file format.
export const PRESETS: Array<[string, Record<string, number>]> = [
  ["INIT", { pitch: 0, formant: 0, mix: 100, gain: 0 }],
  ["OCTAVE UP", { pitch: 12, formant: 0, mix: 50, gain: 0 }],
  ["DEEP VOICE", { pitch: -12, formant: -2, mix: 100, gain: -4 }],
  ["DOUBLER", { pitch: 0, formant: 3, mix: 45, gain: 0 }],
];
