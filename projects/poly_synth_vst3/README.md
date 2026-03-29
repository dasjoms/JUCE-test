# Poly Synth VST3 — Continuation Plan

This handoff defines the next implementation phase, reusing mono decisions instead of replacing them.

## Objective
Evolve `projects/mono_synth_vst3` into a polyphonic VST3 synth with predictable voice allocation, modulation growth path, and versioned preset/state handling.

## Phase Plan

### Phase 1 — Voice abstraction and behavior parity
- Create `Voice` (or `MonoVoice`) class encapsulating:
  - oscillator phase + waveform rendering
  - envelope/gate state
  - note metadata (`midiNote`, `frequency`, active/releasing flags, age/start sample)
- Keep waveform parameter semantics identical to mono (`waveform` choice).
- Ensure 1-voice mode reproduces current mono behavior exactly (including retrigger phase reset).

### Phase 2 — Voice allocator and stealing
- Add `SynthEngine` that owns fixed/preallocated voice array.
- On note-on:
  1. Prefer an idle voice.
  2. Otherwise steal by configured policy.
- Implement policy options (document default):
  - **Oldest**: predictable, musically neutral default.
  - **Released-first**: minimizes audible truncation of held notes.
  - **Quietest**: best perceptual quality once per-voice level is tracked.
- Keep allocator lock-free/RT-safe (no dynamic allocations in audio callback).

### Phase 3 — Modulation architecture
- Introduce modulation sources with small, explicit interfaces:
  - per-voice envelope(s)
  - global/per-voice LFO(s)
- Route modulation through a destination matrix (start minimal: pitch, amp, filter-ready placeholder).
- Store modulation amounts as APVTS parameters for host automation/state persistence.

### Phase 4 — Preset/state versioning
- Add explicit state schema version (integer) in APVTS-backed state blob.
- On `setStateInformation`, branch migrations by version to keep backward compatibility.
- Preserve existing parameter IDs where possible (especially `waveform`) to avoid automation breakage.

## Architecture Targets

### Proposed runtime ownership
- `MonoSynthAudioProcessor`
  - owns APVTS, editor integration, and one `SynthEngine`
- `SynthEngine`
  - owns voice pool, allocator, render orchestration, sample-rate setup
- `Voice`
  - owns oscillator/envelope/note runtime and per-sample generation

### Real-time rules to preserve
- No blocking, locks, or heap allocations in `processBlock` path.
- Preallocate all voices and temp buffers in `prepareToPlay`/constructor.
- Keep per-sample branch/work bounded and deterministic.

## Recommended default decisions
- Default max voices: 8 (small CPU footprint for first iteration).
- Default stealing policy: **released-first**, fallback to **oldest**.
- Keep mono-style short attack/release defaults initially for continuity.

## Validation checklist for next agent
- Build VST3 target successfully.
- Verify note-on/off lifecycle under overlapping MIDI notes.
- Verify stealing behavior with sustain/release edge cases.
- Verify waveform switching under active voices (no crashes/state desync).
- Verify save/load for old mono states and new poly states.
