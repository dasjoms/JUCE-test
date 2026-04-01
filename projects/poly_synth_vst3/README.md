# Poly Synth VST3 — Implementation Checklist

This project is the polyphonic continuation of `projects/mono_synth_vst3`, while keeping the mono project intact as a reference baseline.

## Transition rules
- [x] Keep `projects/mono_synth_vst3` unchanged as the baseline reference while poly evolves.
- [x] Copy mono project structure/files into `projects/poly_synth_vst3` only after core engine/voice abstractions are stable enough to avoid duplicate churn.
- [x] Update poly-specific target naming and plugin identifiers in `projects/poly_synth_vst3/CMakeLists.txt`.

## Checklist by subsystem

### Engine and voice runtime
- [x] `SynthEngine` owns a fixed voice pool and exposes voice-count + steal-policy configuration. (`SynthEngine.h`, `SynthEngine.cpp`)
- [x] `SynthVoice` encapsulates oscillator/envelope/note runtime metadata used by allocator policies. (`SynthVoice.h`, `SynthVoice.cpp`)
- [x] Introduce modulation routing expansion points beyond current depth/rate parameters (`amplitude`, `pitch`, `pulseWidth` placeholder) and plumb APVTS destination selection through processor → engine → voices. (`PolySynthAudioProcessor.*`, `SynthEngine.*`, `SynthVoice.*`)
- [x] Current modulation semantics: per-voice, note-retriggered sine LFO. `modDepth` scales destination intensity, `modRate` controls LFO Hz, and `modDestination` selects the routed target. (`SynthVoice.h`, `SynthVoice.cpp`)
- [x] ADSR envelope semantics are active per voice: Attack ramps from 0→1, Decay ramps 1→Sustain while note is held, Sustain holds constant until note-off, and Release ramps to idle. Defaults are `attack=0.005s`, `decay=0.08s`, `sustain=0.8`, `release=0.03s`. (`PolySynthAudioProcessor.cpp`, `SynthEngine.cpp`, `SynthVoice.cpp`)

### Allocator behaviour
- [x] Core allocator policies covered by tests (idle-first, released-first, oldest, quietest). (`tests/SynthEngineVoiceAllocatorTest.cpp`)
- [x] Poly-specific allocator constraints covered (active voice limit, scaling voice limit upward). (`tests/PolyAllocatorBehaviorTest.cpp`)

### State schema and migration
- [x] APVTS state schema version marker persisted/restored. (`PolySynthAudioProcessor.cpp`)
- [x] Legacy mono migration coverage retained (waveform preservation + new-parameter defaults). (`tests/StateSchemaMigrationTest.cpp`)
- [x] Poly-specific state round-trip coverage for allocator/modulation-facing params (`maxVoices`, `stealPolicy`, `modDepth`). (`tests/PolyStateRoundTripTest.cpp`)
- [x] Explicit future migration fixture added for schema version `2` (poly-expansion example with a new `spread` parameter) and centralized schema expectations enforce test updates before schema bumps. (`tests/StateSchemaMigrationTest.cpp`)
- [x] Migration expectation mirrored: during restore, unknown future parameters are ignored, known IDs are restored, and missing known IDs fall back to APVTS defaults (`release=0.03`, `modRate=2.0`). (`tests/StateSchemaMigrationTest.cpp`)

### UI/parameter parity with mono baseline
- [x] Waveform selection parity maintained with mono baseline (`Sine`, `Saw`, `Square`, `Triangle`). (`PolySynthAudioProcessorEditor.*`, `PolySynthAudioProcessor.*`)
- [x] Poly-focused UI affordances (voice count, steal policy, modulation controls) exposed in editor. (`PolySynthAudioProcessorEditor.h`, `PolySynthAudioProcessorEditor.cpp`)

## Immediate next steps
1. Add modulation matrix scaffolding and parameterized routing tests.
2. Keep the centralized schema constants in `tests/StateSchemaMigrationTest.cpp` updated first whenever `currentStateSchemaVersion` changes, then adjust migration fixtures/expectations.

## State schema migration policy
- Each persisted state must include `schemaVersion`, and `currentStateSchemaVersion` in `PolySynthAudioProcessor.cpp` is the single source of truth for the current format.
- Always add migrations as explicit one-step helpers (`migrateVnToVn+1`) and chain them in order during restore; avoid re-introducing monolithic "legacy restore" branches.
- When adding a new parameter, keep default APVTS values backward compatible so older states safely hydrate missing fields from defaults.
- If a parameter ID is renamed, preserve compatibility by mapping the old ID during migration and cover this with a fixture test.
- Clamp migrated values through the destination parameter range to guarantee out-of-range legacy values restore safely.
- When bumping schema: (1) increase schema version constant, (2) add migration helper, (3) add/refresh fixture coverage in `tests/StateSchemaMigrationTest.cpp` for missing, renamed (if any), clamped, and unknown future parameters.

## Modulation routing behavior
- **Routing path:** `PolySynthAudioProcessor` reads `modDepth`, `modRate`, and `modDestination` from APVTS each block, snapshots them, then applies the snapshot to `SynthEngine`; `SynthEngine` pushes the same values to each active `SynthVoice`. This keeps per-voice behavior deterministic while allowing destination changes from host automation.
- **LFO characteristics:** Each voice runs a deterministic per-note sine LFO reset on `noteOn`.
- **Destination scaling ranges:**
  - `amplitude`: unipolar tremolo gain from `1.0 - modDepth` up to `1.0` (depth range `[0, 1]`).
  - `pitch`: bipolar vibrato up to `±12 semitones * modDepth` (depth range `[0, 1]`).
  - `pulseWidth`: placeholder destination currently neutral (no audible change yet).
