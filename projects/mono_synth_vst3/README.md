# Mono Synth VST3 — Architecture Notes

This document captures the current monophonic implementation so the next iteration can evolve it into a polyphonic architecture without rewriting stable parts.

## Current Architecture Summary

### 1) MIDI handling and note lifecycle
- `processBlock` iterates all incoming MIDI messages first, then clears the MIDI buffer and renders audio for the full block.
- Monophonic retrigger policy: the latest `note on` always becomes the active note, updates frequency, and resets oscillator phase to `0`.
- `note off` only closes the gate when it matches the currently active MIDI note.
- `all notes off` and `all sound off` force gate close.

### 2) Oscillator and envelope path
- One oscillator is generated sample-by-sample from a phase accumulator (`phase`, `phaseIncrement`).
- Waveforms are selected from an enum: `sine`, `saw`, `square`, `triangle`.
- A simple linear attack/release envelope (`currentAmplitude`) is used for click reduction:
  - Attack and release step sizes are precomputed in `prepareToPlay` from sample rate.
  - Audio is only generated while gate is open or release tail is above a small threshold.
- Output gain is fixed by a conservative constant (`outputLevel`) before writing to all output channels.

### 3) Parameter and UI binding
- Parameter state is managed by `AudioProcessorValueTreeState` with one `AudioParameterChoice`:
  - ID: `waveform`
  - Choices: `Sine`, `Saw`, `Square`, `Triangle`
- Editor owns a `ComboBox` for waveform and binds it through `ComboBoxAttachment` to `waveform`.
- Audio thread reads a cached raw parameter pointer and updates an atomic waveform enum, avoiding allocation/locking in `processBlock`.

### 4) State behavior
- Plugin state is serialized/deserialized via APVTS XML (`getStateInformation` / `setStateInformation`).
- On restore, APVTS tree replacement is followed by waveform cache refresh so runtime state matches restored parameter state.

## Monophonic Decisions and Polyphonic Implications

These choices are intentional and should be preserved while extracting `Voice`/`SynthEngine` types:

1. **Retrigger semantics are explicit**: newest note wins and hard-resets phase. For polyphony, keep this behavior available as one legato/retrigger mode.
2. **Envelope stepping is sample-rate normalized**: compute per-voice attack/release coefficients at `prepareToPlay` (or voice-rate update points), not in the hot loop.
3. **Waveform selection is parameter-driven and host-automatable**: keep APVTS parameter IDs stable (`waveform`) unless a versioned migration is added.
4. **Audio-thread safety baseline is already established**: no heap allocation, no locks, and deterministic per-sample work in `processBlock`.
5. **State format foundation exists**: future preset/poly state should build on APVTS with explicit version tags rather than replacing the mechanism.

## Known Limitations (Current Scope)
- Single active voice only (no stacked notes, no note memory, no sustain pedal logic).
- Naive oscillator shapes (not band-limited; aliasing is expected at higher frequencies).
- No velocity/mod-wheel/aftertouch mapping.
- No dedicated voice abstraction yet (`Voice`, allocator, modulation routing not present).
- No explicit state schema version field yet (safe now, but required before larger parameter expansion).

## Suggested Extraction Boundary for Next Step
To minimize rewrite risk, retain current processor/editor contracts and extract internals in this order:

1. Move oscillator + envelope + note state into a `MonoVoice` class with an interface that can scale to N voices.
2. Introduce a `SynthEngine` wrapper in processor that owns voices and renders into block buffers.
3. Keep APVTS + editor attachment model unchanged while routing parameter reads through the engine.
4. Add allocator/stealing policy only after mono behavior parity tests pass.
