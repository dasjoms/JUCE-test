# Agent Workflow: UI Snapshot Iteration

This tool is for rapid UI feedback loops **without** a DAW.

## Iteration loop

1. Implement UI change.
2. Run UI capture app with a scenario.
3. Inspect generated PNG.
4. Adjust code.
5. Re-run until correct.

## Configure/build/run commands

From repository root:

```bash
cmake -S projects/poly_synth_vst3 -B build/poly_synth_ui_capture -DPOLYSYNTH_ENABLE_UI_CAPTURE_APP=ON
cmake --build build/poly_synth_ui_capture --target PolySynthUiCaptureApp -j
```

Capture a full-editor snapshot:

```bash
build/poly_synth_ui_capture/PolySynthUiCaptureApp \
  --scenario projects/poly_synth_vst3/tools/ui_capture_app/scenarios/full-editor-basic.json \
  --output build/poly_synth_ui_capture/captures/full-basic.png
```

Capture a cropped section snapshot:

```bash
build/poly_synth_ui_capture/PolySynthUiCaptureApp \
  --scenario projects/poly_synth_vst3/tools/ui_capture_app/scenarios/oscillator-section-focus.json \
  --output build/poly_synth_ui_capture/captures/oscillator-focus.png \
  --wait-ms 80
```

Use `--output-dir` auto naming:

```bash
build/poly_synth_ui_capture/PolySynthUiCaptureApp \
  --scenario projects/poly_synth_vst3/tools/ui_capture_app/scenarios/modulation-section-focus.json \
  --output-dir build/poly_synth_ui_capture/captures
```

## Creating a new scenario

1. Copy any existing JSON in `scenarios/`.
2. Set `editorWidth/editorHeight` and optional `crop` first.
3. Add `densityMode`, panel toggles, and `selectedLayer` to reach intended layout.
4. Add `parameters` values (APVTS IDs) and optional `controls` values (component IDs) for visual state.
5. Run capture and adjust scenario values until the frame highlights the review area.

Keep scenarios focused and name them after their review target (for example `filter-ribbon-focus.json`).
