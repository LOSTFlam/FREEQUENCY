# FREEQUENCY — Architecture Manifesto

FREEQUENCY is a next-generation (2026) **hybrid Digital Audio Workstation**: it
fuses FL Studio's pattern/step workflow and best-in-class piano roll with
Cubase's linear arrangement, live-audio editing, comping and advanced routing.

This document is the technical north star. It describes the subsystem map, the
folder/module structure, the threading model, the hybrid timeline data design,
and the plugin/DSP strategy. Code is written to this manifesto, one subsystem at
a time.

---

## 1. Non‑negotiable architectural directives

1. **Strict Model / Engine / View separation.**
   `Core` (data models) and `Engine` (audio graph, transport, sequencing) have
   **zero** dependency on `juce::Component` / any UI type. The UI is a *dumb
   observer*: it reads the model and the engine, and writes back through their
   public APIs. You could delete `src/UI` and still render projects headlessly
   (this is exactly what `--selftest` does).

2. **Hybrid timeline.** The `Project` simultaneously supports a
   **PatternSequencer** (FL Channel Rack / step patterns) and a
   **LinearArranger** (Cubase project window). A `Pattern` is reusable content;
   a `PatternClip` places an instance of a pattern on the linear timeline next to
   `AudioClip`s and `MidiClip`s. The same transport drives both.

3. **Real‑time safety.** The audio thread is sacred. Inside any `processBlock()`
   there is **no** heap allocation, **no** locks/mutexes, **no** file I/O and
   **no** logging. Cross‑thread data flows via `std::atomic`, lock‑free FIFOs
   and immutable, reference‑counted **snapshots** published from the message
   thread (`SnapshotHolder<T>`). Parameter changes are de‑zippered with
   `juce::SmoothedValue`.

4. **Plugin formats.** Hosting is built on `juce::AudioPluginFormatManager`.
   VST3 and AU are supported today; the host code path is format‑agnostic so
   **CLAP** can be added behind the same `PluginManager` API (see §6).

5. **Modular build.** The codebase compiles as separate static libraries —
   `FreequencyCore`, `FreequencyDSP`, `FreequencyEngine`, `FreequencyUI` — linked
   into the `FREEQUENCY` application. Dependencies only ever point *downward*
   (UI → Engine → {Core, DSP} → Core), enforcing the separation at link time.

---

## 2. Subsystem map (the 7 engines)

| # | Subsystem | Inspiration | Status |
|---|-----------|-------------|--------|
| 1 | **Hybrid Arranger** — playlist of pattern/audio/automation clips, folder & group tracks, markers, arranger track | FL playlist + Cubase arranger | Linear arranger ✅, patterns (model) ✅, folders/markers ⏳ |
| 2 | **Advanced MIDI Engine** — piano roll (ghost/slide notes, arpeggiator), step sequencer, chord track, expression maps, logical editor | FL piano roll + Cubase MIDI | Sequencer + mini piano‑roll ✅, step pattern model ✅, advanced editors ⏳ |
| 3 | **Audio Manipulation** — VariAudio pitch/formant, audio warp, comping (takes/swipe), built‑in audio editor | Cubase + FL Edison | Clip playback + record ✅, comping/warp/VariAudio ⏳ |
| 4 | **Routing & Mixing Console** — inserts, sends, sidechain, VCA faders, control room | Cubase | Strips/sends/FX buses ✅, VCA model ✅, sidechain/control‑room ⏳ |
| 5 | **Media Browser & Asset Mgmt** — tags, preview, drag‑and‑drop | both | ⏳ |
| 6 | **DSP & Plugin Ecosystem** — VST3 / AU / CLAP hosting + native DSP framework | both | VST3/AU host ✅, built‑in synth/limiter/metronome ✅, CLAP ⏳ |
| 7 | **Modern Modular UI** — docking windows, GPU‑accelerated vector rendering | Cubase/FL | Dark themed arrange+mixer+transport ✅, docking/OpenGL ⏳ |

✅ implemented · ⏳ planned (later phases)

---

## 3. Folder structure & modules

```
src/
├── Core/        [lib FreequencyCore]  Pure data, no UI, no audio I/O
│   └── Types.h                         ids, time units, colour helpers
├── Models/      [lib FreequencyCore]  The serialisable document (the "brain")
│   ├── Clip.h            Clip base → AudioClip (file-backed) | MidiClip (sequence)
│   ├── Track.h           Track base → AudioTrack | MidiTrack | BusTrack | VCATrack
│   ├── Pattern.h         FL channel-rack: channels × steps (+ piano-roll notes)
│   ├── Timeline.h        ordered tracks + tempo / time signature
│   ├── Mixer.h           master / submix / FX bus topology
│   ├── AutomationCurve.h breakpoint automation
│   ├── Project.h         document root (Timeline + Mixer + Patterns)
│   └── ProjectSerializer.h   ValueTree <-> .freq (XML) + bus-id remap
├── DSP/         [lib FreequencyDSP]   Self-contained signal processors (no transport)
│   ├── SynthProcessor.h      built-in 16-voice polyBLEP instrument
│   └── LimiterProcessor.h    transparent master brick-wall limiter
├── Engine/      [lib FreequencyEngine] Real-time audio graph & sequencing
│   ├── Transport.h           lock-free clock (play/stop/loop/tempo/position)
│   ├── SnapshotHolder.h      lock-free immutable snapshot hand-off + GC
│   ├── TrackProcessor.h      channel-strip node (gain/pan/mute + automation + meter)
│   ├── MidiSourceProcessor.h per-track MIDI sequencer node
│   ├── AudioClipProcessor.h  per-track audio clip player node
│   ├── MetronomeProcessor.h  click track (needs Transport → lives in Engine)
│   ├── PluginManager.h       VST3/AU (and future CLAP) discovery + instancing
│   └── AudioEngine.h         graph builder + AudioIODeviceCallback + recorder
└── UI/          [lib FreequencyUI]    Views (depend on Core + Engine, never reverse)
    ├── FreequencyLookAndFeel, TransportBar, StatusBar
    ├── ArrangeView, TrackHeaderComponent, TrackLaneComponent, TimelineRuler, PlayheadOverlay
    └── MixerView, ChannelStrip
```

The dependency graph (and therefore the link order) is strictly:

```
FREEQUENCY (app: Main.cpp)
        │
        ▼
   FreequencyUI ──▶ FreequencyEngine ──▶ FreequencyDSP
        │                  │                  │
        └──────────────────┴──────────────────┴──▶ FreequencyCore
```

`FreequencyCore` knows nothing about JUCE UI/devices; `FreequencyDSP` is pure
processors; `FreequencyEngine` orchestrates the graph; `FreequencyUI` only
observes. Naming note: the app is **FREEQUENCY** throughout (the previous
working name was OmniDAW); all namespaces are `freequency::{core,models,engine,
dsp,ui}`.

---

## 4. Threading model

Three cooperating threads, with strict ownership rules:

| Thread | Owns / does | May touch |
|--------|-------------|-----------|
| **Audio (device callback)** | renders the `AudioProcessorGraph`, advances the `Transport` | reads atomics & immutable snapshots only |
| **Message (UI/main)** | edits the model, builds snapshots, mutates graph topology while rendering is suspended | everything except writing audio-thread-only state |
| **Background (TimeSliceThread)** | disk streaming for recording (`AudioFormatWriter::ThreadedWriter`), waveform thumbnails | its own buffers |

Hand-off mechanisms:

- **Parameters** (volume/pan/mute, tempo, transport): `std::atomic`.
- **Sequences / clips / automation**: the message thread builds an immutable,
  reference-counted snapshot and atomically publishes it via
  `SnapshotHolder<T>`. The audio thread takes a `Ptr` copy (atomic refcount
  bump). Retired snapshots are freed by message-thread garbage collection — the
  audio thread never runs a destructor.
- **Graph topology** edits (add/remove tracks, FX, routing) happen on the
  message thread *with the device callback detached*, so the audio thread never
  observes a half-built graph.

---

## 5. The hybrid timeline data model

```
Project
├── Timeline
│   ├── tempo, time signature
│   └── Tracks: AudioTrack | MidiTrack | BusTrack | VCATrack
│       └── each Track owns Clips: AudioClip | MidiClip | (PatternClip → Pattern)
├── Patterns  (reusable FL channel-rack content, referenced by PatternClips)
└── Mixer  (master / submix / FX buses)
```

- **FL paradigm:** author a `Pattern` once (channels × steps, or piano-roll
  notes), then scatter `PatternClip`s of it across the arrangement — editing the
  pattern updates every placement.
- **Cubase paradigm:** drop unique `AudioClip`s / `MidiClip`s directly on the
  linear timeline for through-composed arrangements and live audio.
- Both feed the *same* engine: at render time a clip resolves to the events /
  samples that overlap the current block, regardless of whether it came from a
  pattern instance or a one-off region.

`std::variant` is used where a value is genuinely one-of-N (e.g. a pattern step's
payload, or a clip reference) so the model is exhaustive and type-safe without a
parallel enum + union.

---

## 6. DSP & plugin ecosystem

- **Native DSP** lives in `FreequencyDSP` as plain `juce::AudioProcessor`s with
  no transport/model dependency, so they're unit-testable and reusable: the
  built-in synth (polyBLEP saw + ADSR), the master limiter, etc.
- **Hosting** goes through `PluginManager` (wrapping
  `juce::AudioPluginFormatManager` + `KnownPluginList`). It registers VST3 and
  AU today. **CLAP** support slots in by adding a CLAP `AudioPluginFormat` to the
  same manager (e.g. via the `clap-juce-extensions` host bridge) — no call sites
  change, because tracks reference plugins by *identifier string*, not by type.

---

## 7. Persistence & undo

The whole `Project` round-trips through `juce::ValueTree` to the custom **`.freq`**
XML document (`ProjectSerializer`). Using `ValueTree` as the model's backing
store is also the path to full **undo/redo** (`juce::UndoManager`) in a later
phase.

---

## 8. Roadmap (phased)

1. **Core foundation** — models, engine skeleton, modular build *(this phase)*.
2. **Routing matrix & mixer** — sends ✅, VCA grouping logic, sidechain, control room.
3. **MIDI engine & piano roll** — slide/ghost notes, arpeggiator, step-pattern playback, chord track.
4. **Audio clip engine & comping** — takes, swipe crossfades, warp, VariAudio.
5. **Modular UI & docking** — dockable windows, GPU rendering.
6. **DSP/plugin** — CLAP hosting, native effect suite.
7. **Asset browser** — tags, preview, drag-and-drop.
