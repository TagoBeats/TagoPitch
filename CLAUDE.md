# TagoPitch – Arbeitsregeln für Claude

Monophoner Vocal-Pitcher (VST3/AU/Standalone, JUCE 8 + WebView-UI mit React).
Was das Plugin tun muss steht in `REQUIREMENTS.md`. Diese Datei regelt wie hier gearbeitet wird.

## Projektkontext

- Robins eigenes Projekt (TagoBeats, nicht NoiseWorks). Plugin Nr. 1 der Tago-Linie.
- Doppelziel: nutzbares Plugin + Case Study auf robinbusse.dev. Release-Ziel 26.09.2026.
- DSP-Referenz ist der Python-Prototyp `tagodsp.pitch.PitchShifter` in `~/Documents/tagodsp`.
  Gleiche Engine (signalsmith-stretch), gleiche Parameter-IDs, gleiches Verhalten.

## Repo-Struktur

- `plugin/` C++ (Processor + WebView-Editor). Parameter-IDs zentral in `PluginProcessor.h`
  (`tagopitch::param`), Relays/Attachments im Editor bereits komplett verdrahtet.
- `ui/` Vite + React + TypeScript. JUCE-JS-Frontend-Library als Path-Dependency aus dem Submodule.
- `mockup/` abgenommenes Design (12.07.2026). **Read-only, Source of Truth für die UI.**
  `index.html` = Ziel-Design, `knob-variants.html` = historisches Varianten-Board.
- `third_party/JUCE` Submodule, gepinnt auf 8.0.14. Nicht ungefragt bumpen.
- `scripts/build-ui.sh` baut die UI und bündelt `ui/webui.zip` für BinaryData.

## Build und Dev-Loop

```sh
./scripts/build-ui.sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target TagoPitch_Standalone   # oder _VST3 / _AU

# UI-Entwicklung mit Hot Reload:
cd ui && npm run dev                                # Vite auf :5173
cmake -B build-dev -DTAGOPITCH_DEV_UI=ON
cmake --build build-dev --target TagoPitch_Standalone
```

UI-Reihenfolge: erst standalone im Browser gegen das Mockup entwickeln, dann im Plugin
(Dev-UI-Build) prüfen, DAW zuletzt. `auval` nach Processor-Änderungen laufen lassen.

## Harte Regeln

1. **Parameter-Contract:** Die IDs in `tagopitch::param` (`pitch_semitones`, `formant_semitones`,
   `mix`, `gain_db`, `bypass`, `formant_base_hz`) sind der Vertrag mit Python-Prototyp und
   WebView-Relays. Niemals umbenennen, Ranges/Defaults nur nach Absprache ändern.
2. **Mockup nicht anfassen:** `mockup/` ist abgenommen. Der React-Port richtet sich nach dem
   Mockup, nie umgekehrt. Design-Abweichungen erst mit Robin klären.
3. **DSP-Änderungen reviewt Robin selbst.** DSP-Code (Engine-Port, Signalweg, Latenz, Glättung)
   klar von UI-Arbeit trennen, im Ergebnis explizit als "DSP, bitte reviewen" ausweisen.
   UI-Code darf vibe-coded bleiben.
4. **Python-First:** Verhaltensfragen (Mix-Gesetz, Latenz, Formant-Anker) entscheidet der
   Python-Prototyp, nicht die Intuition. Bei Abweichungen Delta-Render (/listen-pack) statt
   Diskussion; klangliche Urteile fällt Robin per Hörprobe, nicht Claude per Plot.
5. **Kein Netz in der UI:** Fonts und Assets lokal bündeln, keine CDN/Google-Fonts-Requests
   aus der WebView.
6. **Audio-Thread-Disziplin:** kein Locking/Allokieren/Logging in `processBlock`.
   Meter-Daten per Atomics an die UI.
7. **Libraries nur gepinnt:** neue Dependencies gegen Upstream-Release prüfen und pinnen
   (Submodule auf Tag, npm exakt), keine ungeprüften Defaults.
8. **Keine em-dashes** in Code-Kommentaren, Commit-Messages und Docs. Kommentare auf Englisch,
   knapp, nur wo der Code es nicht selbst sagt.

## Verifikation

- UI: Screenshot-Loop gegen `mockup/index.html` (gleiche Größe 560×360, visueller Vergleich).
- DSP: Offline-Render gegen `tagodsp.pitch.PitchShifter`, Delta-Bounces via /listen-pack,
  finale Abnahme durch Robins Ohr.
- Formate: warnungsfreier Build (VST3/AU/Standalone), `auval` grün.
- Bekannte Falle für den DAW-Test: WebView fängt Keyboard-Fokus ab und kann DAW-Shortcuts
  blockieren; ggf. Key-Forwarding ans native Fenster.
