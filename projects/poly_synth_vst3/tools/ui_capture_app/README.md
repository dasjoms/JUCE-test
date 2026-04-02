# PolySynth UI Capture App (Agent Tool)

This folder contains a **manual developer/agent utility** that renders deterministic PNG snapshots of `PolySynthAudioProcessorEditor` without launching a DAW or plugin host.

- Target: `PolySynthUiCaptureApp`
- Build gate: controlled by `POLYSYNTH_ENABLE_UI_CAPTURE_APP` (default `OFF`)
- CI/tests: **not** registered with CTest and not part of normal plugin test executables

## Scenario JSON format

Each scenario is a JSON object with these fields:

- `editorWidth` / `editorHeight` (int): editor size before snapshot.
- `scaleFactor` (double): global scale factor for deterministic rendering.
- `page` (`"main"` or `"library"`): which editor page to capture.
- `showGlobalPanel` (bool): show/hide global placeholder panel.
- `voicePanelExpanded` / `outputPanelExpanded` (bool): advanced section toggles.
- `densityMode` (`"basic"` or `"advanced"`).
- `selectedLayer` (int, zero-based).
- `selectedPreset` (string): preset name to load before capture.
- `parameters` (object): APVTS parameter IDs mapped to numeric values.
- `controls` (object): component IDs mapped to value overrides (numeric/bool/string where applicable).
- `crop` (object): `{ "x": int, "y": int, "width": int, "height": int }`.

## Seeded scenarios and intended review usage

- `scenarios/full-editor-basic.json`  
  Capture full editor in **basic** density for broad UI sanity checks.
- `scenarios/full-editor-advanced.json`  
  Capture full editor in **advanced** density with expanded details.
- `scenarios/oscillator-section-focus.json`  
  Cropped oscillator-focused frame for waveform/voice section iteration.
- `scenarios/modulation-section-focus.json`  
  Cropped modulation-focused frame for LFO/mod routing visuals.
- `scenarios/layer-stack-focus.json`  
  Cropped sidebar/layer stack frame for layer list interactions.

## CLI

- `--scenario <file>` **required**
- `--output <png-path>` optional if `--output-dir` is used
- `--output-dir <dir>` optional, auto-generates `<scenario-name>.png`
- `--open-after-capture` optional
- `--wait-ms <n>` optional async settle delay
- `--crop x,y,w,h` optional crop override

The app exits non-zero for invalid scenarios, unknown controls/parameters, or failed captures.
