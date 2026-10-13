// TagoPitch v1 UI, ported 1:1 from mockup/index.html (approved 12.07.2026).
// Markup and class names match the mockup so the styles stay literal.

import { useEffect, useMemo, useState } from "react";
import type React from "react";
import Knob from "./Knob";
import Meters from "./Meters";
import { PARAMS, PRESETS } from "./params";
import { makeParam, makeToggle, type ParamHandle } from "./bridge";
import "./App.css";

// The host pushes the real parameter values only after the page is up, so a
// plain useState(initial read) would keep stale defaults. Re-read on mount and
// on every change event (TagoClip pattern, 15.07.2026).
function useScaled(param: ParamHandle): number {
  const [value, setValue] = useState(param.getScaled());
  useEffect(() => {
    setValue(param.getScaled());
    return param.subscribe(() => setValue(param.getScaled()));
  }, [param]);
  return value;
}

// Which preset is loaded is read back off the parameter values instead of being
// remembered in React. The WebView dies with the plugin window, so a local
// index came back as INIT on every reopen while the sound was still the
// preset's (reported three times against TagoClip, same UI architecture).
// Deriving it also stops the label from claiming a preset after a knob was
// moved. Returns -1 when nothing matches, which the header shows as CUSTOM.
function matchPreset(values: Record<string, number>, held: number): number {
  const hits: number[] = [];

  PRESETS.forEach(([, vals], i) => {
    const same = Object.entries(vals).every(([key, v]) => {
      const spec = PARAMS[key];
      // Half a step where the parameter is stepped, otherwise a thousandth of
      // the range: wide enough for the float32 round trip through the host,
      // tight enough that a real knob move counts as a change.
      const eps = spec.step > 0 ? spec.step / 2 : (spec.max - spec.min) * 1e-3;
      return Math.abs(values[key] - v) < eps;
    });

    if (same) hits.push(i);
  });

  if (hits.length === 0) return -1;
  // Value-identical presets would otherwise rename the user's pick, so keep
  // whatever is already on screen.
  return hits.includes(held) ? held : hits[0];
}

export default function App() {
  const params = useMemo(
    () =>
      Object.fromEntries(
        Object.entries(PARAMS).map(([key, spec]) => [
          key,
          makeParam(spec.id, spec.min, spec.max, spec.def),
        ])
      ),
    []
  );
  const bypassToggle = useMemo(() => makeToggle("bypass"), []);

  const [bypassed, setBypassed] = useState(bypassToggle.get());
  useEffect(() => bypassToggle.subscribe(() => setBypassed(bypassToggle.get())), [bypassToggle]);

  const pitchSemis = useScaled(params.pitch);
  const formantSemis = useScaled(params.formant);
  const mixPct = useScaled(params.mix);
  const gainDb = useScaled(params.gain);

  // Only a tiebreaker for value-identical presets and the step base while the
  // values sit on no preset at all. The label never comes from here.
  const [heldIdx, setHeldIdx] = useState(0);
  const [listOpen, setListOpen] = useState(false);
  const applyPreset = (i: number) => {
    const idx = (i + PRESETS.length) % PRESETS.length;
    setHeldIdx(idx);
    const [, vals] = PRESETS[idx];
    for (const [key, v] of Object.entries(vals)) params[key].setScaled(v);
  };

  useEffect(() => {
    // native right-click menu (reload etc.) makes no sense in a plugin window
    const prevent = (e: Event) => e.preventDefault();
    window.addEventListener("contextmenu", prevent);
    return () => window.removeEventListener("contextmenu", prevent);
  }, []);

  useEffect(() => {
    if (!listOpen) return;
    const close = () => setListOpen(false);
    window.addEventListener("pointerdown", close);
    return () => window.removeEventListener("pointerdown", close);
  }, [listOpen]);

  const stopPointer = (e: React.PointerEvent) => e.stopPropagation();

  const matchedIdx = matchPreset(
    { pitch: pitchSemis, formant: formantSemis, mix: mixPct, gain: gainDb },
    heldIdx
  );
  const presetName = matchedIdx >= 0 ? PRESETS[matchedIdx][0] : "CUSTOM";
  // Stepping starts where the label stands; on CUSTOM it continues from the
  // preset that was loaded last, so the arrows never jump back to INIT.
  const stepBase = matchedIdx >= 0 ? matchedIdx : heldIdx;

  return (
    <div id="plugin" className={(bypassed ? "bypassed" : "") + (listOpen ? " preset-open" : "")}>
      <header>
        <div className="wordmark">TAGOPITCH</div>
        <div className="preset">
          <button id="prev" title="Previous preset" onClick={() => applyPreset(stepBase - 1)}>
            ‹
          </button>
          <div
            className="name"
            id="preset-name"
            onPointerDown={stopPointer}
            onClick={() => setListOpen((o) => !o)}
          >
            {presetName}
          </div>
          <button id="next" title="Next preset" onClick={() => applyPreset(stepBase + 1)}>
            ›
          </button>
          {listOpen && (
            <div className="preset-list" onPointerDown={stopPointer}>
              {PRESETS.map(([name], i) => (
                <div
                  key={name}
                  className={"preset-item" + (i === matchedIdx ? " active" : "")}
                  onClick={() => {
                    applyPreset(i);
                    setListOpen(false);
                  }}
                >
                  {name}
                </div>
              ))}
            </div>
          )}
        </div>
        <div className="header-right">
          <span className="header-meta">Mono&nbsp;·&nbsp;V1</span>
          <button id="power" title="Bypass" onClick={() => bypassToggle.set(!bypassed)}>
            <svg viewBox="0 0 24 24">
              <path d="M12 3v8" />
              <path d="M6.2 6.2a8 8 0 1 0 11.6 0" />
            </svg>
          </button>
        </div>
      </header>

      <main>
        <Knob spec={PARAMS.pitch} param={params.pitch} />
        <Knob spec={PARAMS.formant} param={params.formant} />
        <div className="stack">
          <Knob spec={PARAMS.mix} param={params.mix} />
          <Knob spec={PARAMS.gain} param={params.gain} />
        </div>
        <Meters bypassed={bypassed} gainDb={gainDb} />
      </main>

      <footer>
        <span>TagoBeats</span>
      </footer>
    </div>
  );
}
