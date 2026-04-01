# PolySynth UI Architecture (Zone-Based Layout)

## Purpose
This document defines a professional, zone-based editor layout for `projects/poly_synth_vst3` and maps every currently implemented control/parameter into that layout.

The goals are:
- keep performance-critical controls obvious and fast to reach,
- scale cleanly from the current layered synth to deeper modulation/preset workflows,
- avoid burying foundational sound-shaping controls behind tabs/menus.

---

## Zone 1 — Left Fixed Sidebar
**Role:** persistent navigation + layer operations + preset/market entry points.

### Primary content
1. **Layer Management (always visible at top of sidebar)**
   - Layer list rows (select/focus)
   - Add Layer
   - Move Up / Move Down
   - Duplicate
   - Delete
   - Layer status toggles: Enable/Mute/Solo
   - Layer volume trim

2. **Preset Section (always visible below layers)**
   - Preset selector
   - Load
   - Save
   - Save As New
   - Preset status/validation message area

3. **Marketplace Entry Points (sidebar footer, can open modal/browser panel)**
   - Browse Marketplace
   - Upload/Share Preset
   - Sync/Refresh Library

### Control type guidance
- Layer select: **compact selector row** (list item with active highlight)
- Add/Move/Duplicate/Delete: **small action buttons**
- Enable/Mute/Solo: **switch/toggle buttons**
- Layer volume: **compact horizontal slider** (or mini-knob in dense mode)
- Preset picker: **compact selector (combo/dropdown)**
- Preset actions: **buttons**

---

## Zone 2 — Top Header Strip
**Role:** global identity + session context + always-on global actions.

### Primary content
- Plugin title / branding (`Poly Synth`)
- Current patch/preset name
- Selected layer name/index
- Global quick actions:
  - Undo / Redo (future)
  - A/B compare (future)
  - Panic / all notes off (future)
  - Settings / global options

### Control type guidance
- Patch name: **compact selector + editable text field**
- Global actions: **icon buttons/switches**
- Non-editable status (e.g., layer count, voice load): **compact readouts**

---

## Zone 3 — Main Synth Workspace
**Role:** primary sound design area. This is the largest region and should use section cards or tabs.

### Recommended section layout
Use one of:
- **Tabbed:** Oscillator | Amp Env | Mod | Voice/Unison | Output | Pitch/Layer Tuning
- **Single-page cards:** same sections stacked in a 2-column professional grid.

### Section breakdown + control styling

#### A) Oscillator / Source
- Waveform
- (future) sub/noise/mix controls

**Styling:**
- Waveform as **compact selector** or segmented **switch** (Sine/Saw/Square/Triangle)

#### B) Envelope (ADSR)
- Attack, Decay, Sustain, Release

**Styling:**
- Main representation: **envelope graph control** with draggable ADSR points
- Secondary precision controls: **knobs** (or mini numeric fields)

#### C) Modulation
- Mod Destination
- Mod Depth
- Mod Rate
- Velocity Sensitivity

**Styling:**
- Destination: **compact selector**
- Depth/Rate/Velocity: **knobs**

#### D) Voice / Allocation / Unison
- Voice Count
- Steal Policy
- Unison Voices
- Unison Detune

**Styling:**
- Voice Count: **compact selector** (or stepped knob)
- Steal Policy: **compact selector**
- Unison Voices: **compact selector** (integer)
- Unison Detune: **knob**

#### E) Output / Gain Management
- Output Stage (None / Normalize Voice Sum / Soft Limit)

**Styling:**
- Output Stage: **switch** or **compact selector**

#### F) Pitch / Layer Root
- Root Note (absolute MIDI note)
- Relative root semitone offset

**Styling:**
- Absolute root: **compact selector** (note-name display)
- Relative semitone: **knob** or compact stepped slider

---

## Zone 4 — Bottom Optional Performance Strip
**Role:** play/test feedback and runtime metering.

### Primary content
- Mini keyboard (audition + root note context)
- Active voice meter
- CPU/load meter
- Output mini-meter L/R

### Visibility behavior
- Docked by default in expanded editor sizes
- Collapsible in compact layouts
- Never overlays core editing controls

### Control type guidance
- Keyboard: **performance widget**
- Voice/CPU/output: **read-only meters**

---

## Parameter-to-Zone Mapping (Current Implementation)

The table below maps all currently implemented synth parameters and layer-state controls.

| Existing control / parameter | Zone | Section | Recommended control type |
|---|---|---|---|
| Layer select (visual index) | Left Sidebar | Layer Management | Compact selector row |
| Add Layer | Left Sidebar | Layer Management | Action button |
| Move Layer Up | Left Sidebar | Layer Management | Action button |
| Move Layer Down | Left Sidebar | Layer Management | Action button |
| Duplicate Layer | Left Sidebar | Layer Management | Action button |
| Delete Layer | Left Sidebar | Layer Management | Action button |
| Layer Mute | Left Sidebar | Layer Management | Switch/toggle |
| Layer Solo | Left Sidebar | Layer Management | Switch/toggle |
| Layer Volume | Left Sidebar | Layer Management | Compact slider |
| Preset Selector | Left Sidebar | Presets | Compact selector |
| Load Preset | Left Sidebar | Presets | Action button |
| Save Preset | Left Sidebar | Presets | Action button |
| Save As New Preset | Left Sidebar | Presets | Action button |
| Preset Status Message | Left Sidebar | Presets | Read-only status text |
| Plugin Title | Top Header | Identity | Read-only label |
| Current Patch Name | Top Header | Session context | Editable compact field/selector |
| Global Panel Toggle | Top Header | Global actions | Switch |
| Waveform (`waveform`) | Main Workspace | Oscillator | Compact selector or segmented switch |
| Voice Count (`maxVoices`) | Main Workspace | Voice / Allocation | Compact selector (stepped) |
| Voice Steal Policy (`stealPolicy`) | Main Workspace | Voice / Allocation | Compact selector |
| Attack (`attack`) | Main Workspace | Envelope | Envelope graph + knob |
| Decay (`decay`) | Main Workspace | Envelope | Envelope graph + knob |
| Sustain (`sustain`) | Main Workspace | Envelope | Envelope graph + knob |
| Release (`release`) | Main Workspace | Envelope | Envelope graph + knob |
| Mod Depth (`modDepth`) | Main Workspace | Modulation | Knob |
| Mod Rate (`modRate`) | Main Workspace | Modulation | Knob |
| Velocity Sensitivity (`velocitySensitivity`) | Main Workspace | Modulation | Knob |
| Mod Destination (`modDestination`) | Main Workspace | Modulation | Compact selector |
| Unison Voices (`unisonVoices`) | Main Workspace | Voice / Unison | Compact selector |
| Unison Detune (`unisonDetuneCents`) | Main Workspace | Voice / Unison | Knob |
| Output Stage (`outputStage`) | Main Workspace | Output | Switch or compact selector |
| Root Note Absolute | Main Workspace | Pitch / Layer Root | Compact selector |
| Root Note Relative (st) | Main Workspace | Pitch / Layer Root | Knob or stepped compact slider |
| Root Note Feedback Label | Main Workspace | Pitch / Layer Root | Read-only status text |
| Keyboard (planned) | Bottom Strip | Performance | Performance widget |
| Voice Meter (planned) | Bottom Strip | Performance | Meter |
| CPU Meter (planned) | Bottom Strip | Performance | Meter |

---

## “Must Stay Visible” Control List (Do Not Hide in Menus)

These controls should remain directly visible in the default editing view (no nested menu dependency):

1. **Waveform selector**
2. **ADSR envelope access** (graph and/or A/D/S/R knobs)
3. **Filter Cutoff + Resonance** *(reserved for immediate future filter module; keep top-level when introduced)*
4. **Layer enable/mute/solo**
5. **Layer select/focus**
6. **Voice Count + Steal Policy**
7. **Mod destination + depth**
8. **Unison voices + detune**
9. **Output Stage**
10. **Current patch name + preset load/save actions**

If space is constrained, reduce control footprint (compact selectors, tabs, collapsible secondary details) **without hiding** this must-stay-visible set.

---

## Notes on Current vs Target UI
- Current editor already contains most mapped controls, but in a mostly linear inspector form.
- This architecture re-groups the same controls into persistent navigation (left), global context (top), focused synthesis workspace (center), and performance telemetry (bottom).
- The existing “global panel” placeholder naturally fits as a top-header action opening a shared/global section.

