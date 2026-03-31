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
- [ ] Introduce modulation routing expansion points beyond current depth/rate parameters. (planned: `SynthEngine.*`, `SynthVoice.*`)
- [x] Current modulation semantics: per-voice, note-retriggered sine LFO driving unipolar amplitude tremolo (`modDepth` blends dry level to fully modulated, `modRate` controls LFO Hz). (`SynthVoice.h`, `SynthVoice.cpp`)

### Allocator behaviour
- [x] Core allocator policies covered by tests (idle-first, released-first, oldest, quietest). (`tests/SynthEngineVoiceAllocatorTest.cpp`)
- [x] Poly-specific allocator constraints covered (active voice limit, scaling voice limit upward). (`tests/PolyAllocatorBehaviorTest.cpp`)

### State schema and migration
- [x] APVTS state schema version marker persisted/restored. (`PolySynthAudioProcessor.cpp`)
- [x] Legacy mono migration coverage retained (waveform preservation + new-parameter defaults). (`tests/StateSchemaMigrationTest.cpp`)
- [x] Poly-specific state round-trip coverage for allocator/modulation-facing params (`maxVoices`, `stealPolicy`, `modDepth`). (`tests/PolyStateRoundTripTest.cpp`)
- [ ] Add explicit future migration fixtures for schema version > 1 as new parameters land. (planned: `tests/StateSchemaMigrationTest.cpp`)

### UI/parameter parity with mono baseline
- [x] Waveform selection parity maintained with mono baseline (`Sine`, `Saw`, `Square`, `Triangle`). (`PolySynthAudioProcessorEditor.*`, `PolySynthAudioProcessor.*`)
- [ ] Poly-focused UI affordances (voice count, steal policy, modulation controls) exposed in editor.

## Immediate next steps
1. Rename processor/editor classes from mono-prefixed names to poly-specific names once target-level churn settles.
2. Add modulation matrix scaffolding and parameterized routing tests.
3. Add additional state migration fixture(s) before changing APVTS schema version.
