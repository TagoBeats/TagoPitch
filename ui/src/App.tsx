// TagoPitch v1 UI, ported 1:1 from mockup/index.html (approved 12.07.2026).
// Markup and class names match the mockup so the styles stay literal.

import { useEffect, useMemo, useState } from "react";
import type React from "react";
import Knob from "./Knob";
import Meters from "./Meters";
import { PARAMS, PRESETS } from "./params";
import { makeParam, makeToggle } from "./bridge";
import "./App.css";

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

  const [gainDb, setGainDb] = useState(params.gain.getScaled());
  useEffect(() => params.gain.subscribe(() => setGainDb(params.gain.getScaled())), [params]);

  const [presetIdx, setPresetIdx] = useState(0);
  const [listOpen, setListOpen] = useState(false);
  const applyPreset = (i: number) => {
    const idx = (i + PRESETS.length) % PRESETS.length;
    setPresetIdx(idx);
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

  return (
    <div id="plugin" className={(bypassed ? "bypassed" : "") + (listOpen ? " preset-open" : "")}>
      <header>
        <div className="wordmark">TAGOPITCH</div>
        <div className="preset">
          <button id="prev" title="Previous preset" onClick={() => applyPreset(presetIdx - 1)}>
            ‹
          </button>
          <div
            className="name"
            id="preset-name"
            onPointerDown={stopPointer}
            onClick={() => setListOpen((o) => !o)}
          >
            {PRESETS[presetIdx][0]}
          </div>
          <button id="next" title="Next preset" onClick={() => applyPreset(presetIdx + 1)}>
            ›
          </button>
          {listOpen && (
            <div className="preset-list" onPointerDown={stopPointer}>
              {PRESETS.map(([name], i) => (
                <div
                  key={name}
                  className={"preset-item" + (i === presetIdx ? " active" : "")}
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
