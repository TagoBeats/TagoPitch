import { useEffect, useState } from "react";
import * as Juce from "juce-framework-frontend";
import "./App.css";

// Deliberately bare-bones: this page only proves the parameter binding
// end to end. The real UI is ported from mockup/index.html later.

function ParamSlider({ id, label }: { id: string; label: string }) {
  const state = Juce.getSliderState(id);
  const [value, setValue] = useState(state.getNormalisedValue());

  useEffect(() => {
    const listenerId = state.valueChangedEvent.addListener(() =>
      setValue(state.getNormalisedValue())
    );
    return () => state.valueChangedEvent.removeListener(listenerId);
  }, [state]);

  return (
    <label className="param">
      <span>{label}</span>
      <input
        type="range"
        min={0}
        max={1}
        step={0.001}
        value={value}
        onChange={(e) => state.setNormalisedValue(Number(e.target.value))}
        onMouseDown={() => state.sliderDragStarted()}
        onMouseUp={() => state.sliderDragEnded()}
      />
      <code>{state.getScaledValue().toFixed(2)}</code>
    </label>
  );
}

function BypassToggle() {
  const state = Juce.getToggleState("bypass");
  const [on, setOn] = useState(state.getValue());

  useEffect(() => {
    const listenerId = state.valueChangedEvent.addListener(() =>
      setOn(state.getValue())
    );
    return () => state.valueChangedEvent.removeListener(listenerId);
  }, [state]);

  return (
    <label className="param">
      <span>Bypass</span>
      <input
        type="checkbox"
        checked={on}
        onChange={(e) => state.setValue(e.target.checked)}
      />
      <code>{on ? "on" : "off"}</code>
    </label>
  );
}

export default function App() {
  return (
    <main>
      <h1>TagoPitch – Binding-Test</h1>
      <ParamSlider id="pitch_semitones" label="Pitch" />
      <ParamSlider id="formant_semitones" label="Formant" />
      <ParamSlider id="mix" label="Mix" />
      <ParamSlider id="gain_db" label="Gain" />
      <BypassToggle />
    </main>
  );
}
