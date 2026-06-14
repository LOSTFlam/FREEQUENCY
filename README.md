# FREEQUENCY (FREEQUENCY)

A next-generation, **hybrid DAW** — combining FL Studio's pattern workflow with
Cubase's linear audio/MIDI editing — built with **modern C++20** and the
**JUCE 8** framework.

FREEQUENCY makes sound out of the box (built-in synth), hosts VST3/AU plugins, and
ships a full arrange view, mixing console, automation and disk recording.

## Features

- **Multitrack engine** built on `juce::AudioProcessorGraph` with a strict
  **Model / View / Engine** separation (the models and audio engine have **no**
  `juce::Component` dependency).
- **Transport**: sample-accurate play/stop/loop, tempo, bars·beats·ticks display.
- **Built-in instrument**: a 16-voice polyBLEP synth so MIDI tracks are audible
  with zero plugins installed.
- **VST3 / AU hosting**: scan, load instruments and insert-effect chains.
- **Arrange view**: track headers (mute/solo/volume/pan), audio **waveforms**
  (`juce::AudioThumbnail`), a **mini piano-roll** for MIDI, bar/beat ruler with
  click-to-seek, and a transport-synced **playhead**. Double-click to add a MIDI
  pattern or import audio.
- **Mixing console**: per-track / bus / master **channel strips** with faders,
  pan, mute/solo, **meters**, **aux sends** and **FX bus** routing.
- **Automation**: editable per-track volume curves that drive the fader
  sample-accurately at playback.
- **Recording**: punch-in disk recording of the audio input to 24-bit WAV.
- **Project save/load**: `.freq` XML (ValueTree) documents.

## Architecture

```
src/
├── Core/      Shared types (ids, time units, colour helpers)
├── Models/    Serialisable document (pure data, no UI)
│   ├── Clip / AudioClip / MidiClip
│   ├── Track / AudioTrack / MidiTrack   (volume, pan, mute, solo, sends, automation)
│   ├── Timeline   (tracks + tempo / time signature)
│   ├── Mixer / Bus   (master / submix / FX routing topology)
│   ├── AutomationCurve
│   ├── Project    (document root)
│   └── ProjectSerializer  (ValueTree <-> .freq XML)
├── Engine/    Real-time audio (no UI, no locks/allocation in processBlock)
│   ├── Transport             lock-free clock
│   ├── SnapshotHolder<T>     lock-free publish of immutable snapshots
│   ├── TrackProcessor        channel-strip node (gain/pan/mute + automation + metering)
│   ├── MidiSourceProcessor   per-track MIDI sequencer node
│   ├── SynthProcessor        built-in instrument
│   ├── AudioClipProcessor    audio clip playback node
│   ├── PluginManager         VST3/AU discovery + instantiation
│   └── AudioEngine           graph builder + AudioIODeviceCallback + recorder
└── UI/        Views (depend on model+engine; never the reverse)
    ├── FreequencyLookAndFeel, TransportBar
    ├── ArrangeView, TrackHeaderComponent, TrackLaneComponent,
    │   TimelineRuler, PlayheadOverlay
    └── MixerView, ChannelStrip
```

### Real-time thread safety

The audio thread is sacred. Inside any `processBlock()` there is **no**
allocation, locking, file I/O or UI work. Parameters cross from the message
thread via `std::atomic`; MIDI/clip/automation data via lock-free immutable
**snapshots** (`SnapshotHolder<T>`, with message-thread garbage collection);
de-zippering via `juce::SmoothedValue`. Disk recording streams through a
`ThreadedWriter` FIFO drained on a background thread.

## Building

Requires CMake ≥ 3.22 and a C++20 compiler. JUCE 8 is fetched automatically via
CMake `FetchContent` (no manual JUCE install needed).

On Linux, install the JUCE build dependencies first:

```bash
sudo apt-get install -y libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev \
  libglu1-mesa-dev mesa-common-dev libstdc++-13-dev
```

Then configure and build (use GCC — the default `c++`/clang on some images can't
find libstdc++):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc
cmake --build build --target FREEQUENCY -j
```

The standalone app is produced at
`build/src/FREEQUENCY_artefacts/Release/FREEQUENCY`.

> Tip: add `-DFREEQUENCY_ENABLE_LTO=OFF` for much faster incremental link times
> during development.

### Headless self-test

The engine can be verified without a soundcard or display:

```bash
./build/src/FREEQUENCY_artefacts/Release/FREEQUENCY --selftest
```

This renders a MIDI note through the full graph, performs a project save/load
round-trip, and checks that a zero automation curve silences the track —
printing `[PASS]` for each.

## Roadmap

- **Phase 1 — Engine core & data models** ✅
- **Phase 2 — VST3/AU hosting & playback engine** ✅
- **Phase 3 — Timeline UI & waveform/piano-roll rendering** ✅
- **Phase 4 — Mixer, routing matrix & sends** ✅
- **Phase 5 — Automation & disk recording** ✅
- Project save/load ✅
- Next: full piano-roll editor, clip drag/trim, time-stretch, more built-in FX.
