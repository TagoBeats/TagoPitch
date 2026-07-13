# TagoPitch v1 – Requirements

Monophoner Vocal-Pitcher als VST3/AU/Standalone (macOS), Referenz: Little AlterBoy (SoundToys).
Abgeleitet aus dem abgenommenen Mockup (`mockup/index.html`, 12.07.2026) und der Projekt-Notiz im Vault.
Dieses Dokument beschreibt was v1 tun muss. Wie gearbeitet wird steht in `CLAUDE.md`.

## 1. Produktziel

Ein Vocal-Pitcher, den Robin selbst in Produktionen nutzt: Pitch, Formant, Mix, Output-Gain.
Doppelziel: fertiges Plugin (Release-Ziel 26.09.2026, free gegen E-Mail auf tagobeats.com)
plus Case Study auf robinbusse.dev. v1 ist bewusst schlank, damit die Workflow-Kette
(Python-Prototyp → C++-Port → WebView-UI) einmal komplett durchläuft.

## 2. Formate und Kanäle

- VST3, AU, Standalone. AU muss `auval` bestehen (tut es im aktuellen Skeleton bereits).
- Mono→Mono und Stereo→Stereo (Input-Layout == Output-Layout, sonst abgelehnt).
  Die Engine ist monophon gedacht; bei Stereo laufen beide Kanäle durch dieselbe Verarbeitung.
- Latenz der Engine wird über `setLatencySamples()` an den Host gemeldet
  (Python-Referenz: 5292 Samples bei 44.1 kHz).

## 3. Parameter (Contract, bereits verdrahtet)

IDs sind der Vertrag zwischen Processor, WebView-UI und Python-Prototyp
(`tagodsp.pitch.PitchShifter`). Niemals umbenennen.

| ID                  | Typ    | Range      | Default | Step      | UI |
|---------------------|--------|------------|---------|-----------|----|
| `pitch_semitones`   | int    | −12…+12 st | 0       | 1 (rastend) | Knob groß (118 px), bipolar |
| `formant_semitones` | float  | −12…+12 st | 0.0     | stufenlos | Knob groß (118 px), bipolar, Anzeige 1 Dezimale |
| `mix`               | float  | 0…100 %    | 100     | stufenlos | Knob klein (64 px), unipolar, oben im Stack |
| `gain_db`           | float  | −18…+18 dB | 0.0     | stufenlos | Knob klein (64 px), bipolar, unten im Stack, Anzeige 1 Dezimale |
| `bypass`            | bool   | –          | false   | –         | Power-Button im Header |
| `formant_base_hz`   | float  | 0…500 Hz   | 0       | 1         | **kein UI-Element**, nicht automatisierbar |

`formant_base_hz`: 0 = automatische Hüllkurven-Schätzung der Engine. Hörtest-Ergebnis (12.07.):
auf die Grundfrequenz des Sängers geankert klingt es bei großen Shifts hörbar besser. In v1 nur
versteckter Parameter (per State setzbar), ab v2 intern per Pitch-Detection nachgeführt. Kein User-Regler.

## 4. DSP (Signalweg)

1. **Engine:** signalsmith-stretch als Pitch/Formant-Engine. Die Header werden 1:1 aus
   `~/Documents/tagodsp` gevendort (inkl. `signalsmith-linear` 0.3.1, auf Apple mit Accelerate-FFT),
   damit Plugin und Python-Prototyp exakt dieselbe Engine fahren (Engine-Parität).
   Tonality-Limit fest auf **12 kHz** (Hörtest 13.07.: deutlich weniger Grain als die
   8-kHz-Empfehlung, bestätigt auf up5/up12 male, ohne Regression bei female/downshift).
   Engine-Konfiguration bleibt `presetDefault` (120/30 ms), dichteres Overlap brachte hörbar nichts.
2. **Reihenfolge:** Input → Pitch/Formant (signalsmith) → Pegel-Kompensation (aufs Wet)
   → Mix (Dry/Wet, latenzkompensiertes Dry-Signal)
   → Output-Gain (`gain_db`, nach dem Mix, gerampt) → Output.
   Referenz für alle Verhaltensfragen: `tagodsp.pitch.PitchShifter` (8/8 Tests grün, Parameter-Parität).
   **Pegel-Kompensation:** gemessene 25-Punkte-Tabelle pro Halbton (13.07., RMS wet vs dry
   auf beiden Test-Vocals, bis −6 dB Verlust bei +10), 0 dB bei Pitch 0, im Plugin 20 ms geglättet.
   Gleiche Tabelle im Prototyp (`level_compensation=True`).
3. **Bypass:** echter Passthrough (aktuelles Verhalten beibehalten), Host-Bypass via
   `getBypassParameter()` bleibt verdrahtet.
4. **Parameterglättung:** `gain_db` ist bereits gerampt (20 ms). Pitch/Formant/Mix dürfen beim Port
   nicht zippern; Glättung bzw. Engine-Verhalten gegen den Python-Prototyp per Delta-Render prüfen.
5. **Abnahmekriterium DSP:** Offline-Render des Plugins gegen den Python-Prototyp
   (gleiche Settings, gleiche Eingangsdatei) per /listen-pack vergleichen. Delta muss erklärbar
   klein sein; finale Beurteilung per Hörprobe durch Robin.

## 5. UI (Source of Truth: `mockup/index.html`)

Das Mockup ist abgenommen und wird nicht mehr verändert. Der React-Port übernimmt Markup,
Styles und die Knob-Geometrie so wörtlich wie möglich.

### 5.1 Fenster und Grundgerüst

- Feste Größe **560 × 360 px**, nicht resizable.
- TagoBeats-Design-Tokens exakt wie im Mockup (`--bg #12100E`, `--accent #00FDDC` usw.).
- Fonts: Archivo (400/500/700/800) und Space Mono (400/700). Im Plugin **lokal bündeln**,
  kein Google-Fonts-Request aus der WebView.
- Textur-Schichten: Punktraster im Control-Deck, Teal-Fog + Vignette (`::before`),
  Film-Grain (`::after`). Alles rein dekorativ, `pointer-events: none`.
- Drei Zonen: Header (Wordmark, Preset-Browser, "Mono · V1", Power), Control-Deck
  (Grid: Pitch | Formant | Mix/Gain-Stack | Meter), Footer (nur "TagoBeats").
- Keine Bedienhinweise in der UI.

### 5.2 Knobs (Look "Hardware", Variante B aus dem Varianten-Board)

- 270°-Sweep, Start bei −135°. Bipolare Knobs füllen den Arc von der 12-Uhr-Mitte aus
  und haben einen Center-Detent-Punkt; unipolare (Mix) füllen von links.
- Metall-Bezel, tiefe gewölbte Kappe (Radial-Gradient, Licht von oben), Skalenstriche außen
  (24 Ticks bei großen, 12 bei kleinen Knobs), satter Zeiger. Teal nur funktional:
  Arcs, Werte, Meter, Power.
- Geometrie relativ zur Knob-Größe (skaliert), wie in der Mockup-Klasse `Knob`.
- Interaktion: vertikaler Drag (volle Range über 220 px, mit Shift fein über 900 px),
  Mausrad (Step bzw. Range/48, Shift = Viertel-Schritt), Doppelklick = Default.
  `pitch_semitones` rastet auf ganze Halbtöne; der Parameter bleibt int, Shift verfeinert
  nur die Drag-Übersetzung.
- Wertanzeige unter dem Knob (Space Mono, Teal), Formatierung wie im Mockup
  (Vorzeichen, Einheit, Dezimalstellen je Parameter).
- Alle Regler-Gesten laufen über die JUCE-Relays (`getSliderState()`:
  `sliderDragStarted/Ended`, Normalised-Values), damit Host-Automation sauber funktioniert.

### 5.3 Header

- Wordmark "TAGOPITCH", einfarbig Cream (`--text-hi`), kein Teal-Akzent.
- Preset-Browser: ‹ / › blättern zyklisch durch die Preset-Liste, Name mittig (uppercase, mono).
  v1-Presets (aus dem Mockup übernehmen):
  - INIT: pitch 0, formant 0, mix 100, gain 0
  - OCTAVE UP: pitch +12, formant 0, mix 100, gain 0 (voll wet seit dem 12-kHz-Tuning, 13.07.)
  - DEEP VOICE: pitch −12, formant −12, mix 100, gain −4 (Formanten folgen dem Pitch;
    gewann den down12-Hörtest gegen die reine Hüllkurven-Schätzung, 13.07.)
  - DOUBLER: pitch 0, formant +3, mix 45, gain 0
  Presets setzen nur Parameterwerte (kein eigenes Preset-Dateiformat in v1). Host-State
  (APVTS-XML) bleibt die einzige Persistenz.
- "Mono · V1" als statisches Meta-Label.
- Power-Button = `bypass`. Bypassed: Button verliert Glow, Control-Deck dimmt auf 35 % Opacity,
  Regler bleiben sichtbar.

### 5.4 Meter

- In/Out-Paar rechts, vertikal, mit Peak-Hold-Linie (Hold ~1.5 s, dann sanfter Decay,
  wie die Mockup-Logik: 90 Frames Hold, Faktor 0.985).
- Im Plugin von echten Levels gespeist: In vor der Engine, Out nach dem Gain.
  Level-Übergabe vom Audio-Thread an die WebView entkoppelt (Atomics + Timer/Event,
  kein Blocking im Audio-Thread), Update-Rate ~30–60 Hz.
- Skala/Ballistik einfach halten (Peak mit Release genügt für v1, kein Loudness-Metering).

## 6. Verhalten und State

- State-Persistenz über APVTS-XML (bereits implementiert und von Robin am Standalone verifiziert).
- Doppelklick-Reset, Drag und Automation dürfen sich nicht beißen (Relays/Attachments sind
  bereits verdrahtet, der React-Port darf dieses Binding nicht umgehen).
- Keyboard-Falle: WebView darf DAW-Shortcuts nicht schlucken. Im DAW-Test explizit prüfen,
  bei Bedarf Key-Events ans native Fenster weiterleiten (bekannte JUCE-8-WebView-Falle).

## 7. Nicht-Ziele für v1

- Kein Hard-Tune, kein Drive (v2).
- Keine eigene PSOLA-Engine (späteres tagodsp-Projekt; Engine austauschbar kapseln).
- Kein User-Regler für `formant_base_hz`.
- Kein eigenes Preset-Dateiformat, kein Preset-Speichern aus der UI.
- Kein Resizing, kein Windows-Build (macOS first, Windows später).

## 8. Abnahme v1

1. `auval` weiterhin grün, VST3/AU/Standalone bauen warnungsfrei.
2. Delta-Render Plugin vs. Python-Prototyp (/listen-pack), Hörabnahme durch Robin.
3. Screenshot der Plugin-UI deckungsgleich mit `mockup/index.html` (Screenshot-Loop).
4. DAW-Test in Ableton/FL: Latenz-Report korrekt, Automation aller Parameter,
   State-Recall, Keyboard-Falle geprüft.
