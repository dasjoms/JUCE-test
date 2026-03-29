# AGENTS.md — Project Guidance for Future Agents

## Project Purpose
This repository is the upstream **JUCE framework**. Our project work should build on JUCE without disrupting upstream structure.

### Long-Term Goal
Create a **polyphonic synthesizer plugin** for use in DAWs, targeting **VST3** format, with:
- Multiple simultaneous voices
- Voice stealing strategy
- Modulation system (e.g., envelopes/LFO/routing)
- Preset save/load management

### Immediate First Goal
Create a **simple monophonic synthesizer plugin** for DAWs in **VST3** format that:
- Produces a single voice from MIDI input
- Exposes a simple UI to switch waveform type
- Supports initial waveforms only: **Sine, Saw, Square, Triangle**

---

## Repository Orientation (Where to Find Things)
Use these directories/files first before inventing structure:

### JUCE Core and Audio Infrastructure
- `modules/` — JUCE framework modules.
  - Start with: `juce_audio_processors`, `juce_audio_plugin_client`, `juce_dsp`, `juce_gui_basics`, `juce_audio_utils`.

### Plugin Example Baselines
- `examples/CMake/AudioPlugin/` — minimal CMake audio-plugin template (best direct starter for a standalone project copy).
- `examples/Plugins/` — PIP plugin demos, including synth/effect references (`AudioPluginDemo`, `GainPluginDemo`, `SamplerPluginDemo`, `MultiOutSynthPluginDemo`, etc.).

### Build and API Docs
- `docs/CMake API.md` — JUCE CMake APIs (`juce_add_plugin`, flags, formats).
- `README.md` — framework overview, supported plugin formats, setup guidance.
- `docs/README.md` — docs layout and generation notes.

### Optional Tooling/Reference Projects
- `extras/AudioPluginHost/` — plugin host for local testing/debugging plugin behaviour.
- `extras/Projucer/` — optional project generation workflow.

---

## Project Layout Rule for New Work
**Do not modify existing JUCE framework/examples unless necessary.**

Create project work in a separate directory so upstream groundwork remains intact.

Recommended location:
- `projects/`
  - `projects/mono_synth_vst3/` (immediate goal)
  - `projects/poly_synth_vst3/` (long-term continuation)

If `projects/` does not exist, create it.

When adapting JUCE examples, **copy** from `examples/CMake/AudioPlugin/` into `projects/...` and then evolve from there.

---

## Implementation Guidance

### Immediate Monophonic Synth Scope (Phase 1)
- MIDI note on/off handling for one active voice
- Single oscillator waveform selection: Sine/Saw/Square/Triangle
- Basic gain safety and click avoidance where possible
- Minimal UI for waveform switching
- VST3 build target enabled

### Long-Term Polyphonic Scope (Phase 2+)
- Voice abstraction (`Voice`, `SynthEngine`, voice allocator)
- Voice stealing policy (oldest/released/quietest strategy documented)
- Modulation architecture (ADS(R), LFO, routable destinations)
- Preset serialization/versioning (state migration support)
- Performance profiling under realistic block sizes and sample rates

---

## Real-Time Audio Programming Constraints (Critical)
All audio-thread code must be written for real-time safety:

- No blocking calls on audio thread
- No dynamic allocations in `processBlock` or equivalent hot paths
- No file/network/locking operations in the real-time callback
- Avoid unbounded work per sample/block
- Prefer preallocation, lock-free/atomics where needed, deterministic CPU usage
- Keep denormal handling and numerical stability in mind
- Ensure behaviour is stable across sample rates and block sizes

Any deviation must be justified in code comments and addressed before merge.

---

## Testing & Verification Requirements (Mandatory)
Before committing any feature:

1. Add or update automated tests whenever feasible (unit/integration where applicable).
2. Run relevant test/build checks and document exact commands and outcomes.
3. Verify plugin state save/load consistency for changed parameters/state.
4. Verify no regressions in audio processing behaviour (silence handling, note lifecycle, waveform switching correctness).
5. Confirm successful VST3 build for target environment.

Minimum expected checks for synth feature work include:
- Configure + build project
- Run available tests
- Static checks/linting if configured

If a check cannot run due to environment limitations, manually modify the environment so tests can be ran and notify the user about what changes to the environment had to be made.
Specifically fix any missing Xrandr.h issues you encounter before running your build. Do not skip builds simply because it fails once but address the issue.
---

## Coding and Collaboration Meta-Instructions
- Keep changes scoped and minimal; do not refactor unrelated JUCE internals.
- Prefer clear separation between DSP engine, MIDI handling, UI, and state/preset code.
- Document architectural decisions in concise markdown notes under project directory.
- Maintain backward-compatible preset/state format when evolving saved state.
- Update this `AGENTS.md` if project structure/goals materially change.

---

## Suggested First Execution Plan
1. Create `projects/mono_synth_vst3/` by copying `examples/CMake/AudioPlugin/`.
2. Rename targets/classes to project-specific names.
3. Implement monophonic oscillator + MIDI note control.
4. Add waveform selector UI (Sine/Saw/Square/Triangle).
5. Add tests/checks and verify VST3 build.
6. Document what remains for polyphonic expansion in `projects/poly_synth_vst3/README.md`.
