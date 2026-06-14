# OmniDAW (FREEQUENCY)

A next-generation, **hybrid DAW** — combining FL Studio's pattern workflow with
Cubase's linear audio/MIDI editing — built with **modern C++20** and the
**JUCE 8** framework.

> **Status: Phase 1 — Project Scaffolding, Data Models & Engine Core.**
> There is intentionally no real UI yet; this phase establishes the architecture
> the rest of the application is built on.

## Architecture

OmniDAW follows a strict **Model / View / Engine** separation. The data models
and the real-time audio engine have **no dependency on any `juce::Component`** —
the UI observes the model and reads the engine, never the other way around.

```
src/
├── Core/      Shared types (ids, time units, colour helpers)
├── Models/    The serialisable document (pure data, no UI)
│   ├── Clip            Base region; AudioClip (file-backed) + MidiClip (sequence)
│   ├── Track           Polymorphic base; AudioTrack + MidiTrack
│   ├── Timeline        Tracks + musical grid (tempo / time signature)
│   ├── Mixer / Bus     Master / submix / FX bus topology
│   └── Project         Document root: Timeline + Mixer
├── Engine/    Real-time audio
│   ├── TrackProcessor  Custom juce::AudioProcessor channel-strip node
│   └── AudioEngine     Wraps AudioProcessorGraph + AudioDeviceManager
└── UI/        Minimal MainComponent (Phase 1 placeholder)
```

### The audio engine

The signal flow is a `juce::AudioProcessorGraph`. Every track and bus becomes a
`TrackProcessor` node, so the graph topology mirrors the mixer one-to-one:

```
[track nodes] ──▶ [master node] ──▶ [audio output IO] ──▶ soundcard
```

### Real-time thread safety

The audio thread is sacred. Inside `processBlock()` there is **no** allocation,
locking, file I/O or UI work. Parameters cross from the message thread to the
audio thread through `std::atomic`s and are de-zippered with
`juce::SmoothedValue`. The "console print when audio is processed" required by
Phase 1 is done the correct way: the node bumps an atomic sample counter, and a
message-thread `juce::Timer` reads it — nothing is logged from the audio thread.

## Building

Requires CMake ≥ 3.22 and a C++20 compiler. JUCE 8 is fetched automatically via
CMake `FetchContent` (no manual JUCE install needed).

On Linux, install the JUCE build dependencies first:

```bash
sudo apt-get install -y libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev \
  libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libwebkit2gtk-4.1-dev \
  libglu1-mesa-dev mesa-common-dev
```

Then configure and build:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target OmniDAW -j
```

The standalone application is produced at
`build/src/OmniDAW_artefacts/Release/OmniDAW` (Linux/macOS).

## Roadmap

- **Phase 1 — Engine core & data models** ✅ (this branch)
- **Phase 2 — VST3/AU hosting & instrument/audio processing nodes**
- **Phase 3 — Timeline UI & waveform/piano-roll rendering**
- **Phase 4 — Mixer, routing matrix & sends**
- **Phase 5 — Automation & disk recording**
