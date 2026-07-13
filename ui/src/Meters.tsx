// In/Out meter pair, ported from the mockup: bar plus peak-hold line
// (90 frames hold, then 0.985 decay per frame). In the plugin the levels
// come from the audio thread via "levels" events; in a plain browser the
// mockup's idle animation runs instead so the screenshot loop has life.

import { useEffect, useRef } from "react";
import { inJuce, onLevels } from "./bridge";

interface MeterState {
  bar: HTMLDivElement | null;
  peakEl: HTMLDivElement | null;
  peak: number;
  age: number;
}

function drawMeter(m: MeterState, shown: number) {
  if (!m.bar || !m.peakEl) return;
  m.bar.style.height = (shown * 100).toFixed(1) + "%";
  if (shown >= m.peak) {
    m.peak = shown;
    m.age = 0;
  } else if (++m.age > 90) {
    m.peak *= 0.985;
  }
  m.peakEl.style.bottom = (m.peak * 100).toFixed(1) + "%";
}

export default function Meters({ bypassed, gainDb }: { bypassed: boolean; gainDb: number }) {
  const inBar = useRef<HTMLDivElement>(null);
  const inPeak = useRef<HTMLDivElement>(null);
  const outBar = useRef<HTMLDivElement>(null);
  const outPeak = useRef<HTMLDivElement>(null);
  const levels = useRef({ in: 0, out: 0 });
  const bypassedRef = useRef(bypassed);
  const gainRef = useRef(gainDb);
  bypassedRef.current = bypassed;
  gainRef.current = gainDb;

  useEffect(() => {
    const mIn: MeterState = { bar: inBar.current, peakEl: inPeak.current, peak: 0, age: 0 };
    const mOut: MeterState = { bar: outBar.current, peakEl: outPeak.current, peak: 0, age: 0 };
    const unsub = onLevels((l) => (levels.current = l));

    let raf = 0;
    let lvl = 0.4;
    // displayed bar values with ballistics: the raw block peaks arrive at
    // ~30 Hz and would step visibly, so rise over a few frames and fall
    // with an exponential release
    let shownIn = 0;
    let shownOut = 0;
    const ballistics = (shown: number, target: number) =>
      target > shown ? shown + (target - shown) * 0.5 : shown * 0.88;
    const tick = () => {
      if (inJuce) {
        shownIn = ballistics(shownIn, Math.min(1, levels.current.in));
        shownOut = ballistics(shownOut, Math.min(1, levels.current.out));
        drawMeter(mIn, shownIn);
        drawMeter(mOut, shownOut);
      } else {
        // mockup idle animation (random walk, out follows the gain knob)
        lvl += (0.35 + 0.35 * Math.random() - lvl) * 0.07;
        const inShown = Math.max(0, lvl + 0.06 * Math.sin(Date.now() / 230));
        drawMeter(mIn, inShown);
        const gainFactor = bypassedRef.current ? 1 : Math.pow(10, (gainRef.current / 20) * 0.5);
        drawMeter(mOut, Math.min(1, inShown * gainFactor));
      }
      raf = requestAnimationFrame(tick);
    };
    tick();
    return () => {
      cancelAnimationFrame(raf);
      unsub();
    };
  }, []);

  return (
    <div className="meter-unit">
      <div className="meter-pair">
        <div className="meter-col">
          <div className="meter" id="meter-in">
            <div className="bar" ref={inBar} />
            <div className="peak" ref={inPeak} />
          </div>
          <div className="meter-label">In</div>
        </div>
        <div className="meter-col">
          <div className="meter" id="meter-out">
            <div className="bar" ref={outBar} />
            <div className="peak" ref={outPeak} />
          </div>
          <div className="meter-label">Out</div>
        </div>
      </div>
    </div>
  );
}
