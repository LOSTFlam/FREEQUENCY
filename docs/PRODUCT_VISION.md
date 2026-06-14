# FREEQUENCY — Product Vision: Frequency Field

## The problem with today's DAWs

Pitch correction, time-stretch, comping, MIDI slides, and pattern editing live in **separate tools** with **separate mental models**:

| Task | Cubase | FL Studio | Logic |
|------|--------|-----------|-------|
| Pitch-correct vocals | VariAudio (separate editor) | Newtone / Edison | Flex Pitch |
| Stretch audio | Audio Warp (offline) | Stretch in playlist | Flex Time |
| Comp takes | Lanes + crossfade tool | — | Quick Swipe |
| MIDI portamento | Piano roll + expression | Slide notes | — |
| Pattern workflow | — | Channel Rack + Playlist | Live Loops |

Musicians bounce between windows. Context is lost. Ghost notes from other tracks rarely appear in audio editors. Elastic preview is often offline-only.

## The killer feature: **Frequency Field**

> **One spectral canvas where pitch, time, takes, and MIDI slides are the same thing.**

**Frequency Field** is FREEQUENCY's flagship differentiator — a unified harmonic editing surface that no other DAW offers as a single workflow:

```
┌─────────────────────────────────────────────────────────────────┐
│  FREQUENCY FIELD — unified pitch · time · take editor           │
├─────────────────────────────────────────────────────────────────┤
│  ░░ ghost notes (patterns + other tracks)                       │
│  ─── pitch contour (VariAudio segments, resynth preview)        │
│  ≋≋≋ elastic warp curve (real-time + offline OLA)               │
│  ▓▓▓ comp swipe crossfade (take A ↔ take B)                     │
│  ♪  portamento slides (MIDI, same grid as audio pitch)          │
└─────────────────────────────────────────────────────────────────┘
```

### Why people switch

1. **Edit vocals like MIDI, on the same grid as your beat.** VariAudio segments and piano-roll notes share one pitch axis. No export/import between tools.

2. **Hear elastic time live.** Warp markers drive a real-time granular/OLA preview during playback — not only after a bounce. Offline OLA (current) remains the quality export path.

3. **Comp by swiping through frequency layers.** Takes stack as translucent waveforms; swipe crossfade blends them in the arrange lane *and* inside Frequency Field for surgical edits.

4. **Patterns and audio in one harmonic context.** `PatternClip` playback feeds ghost notes into the field — you always see how the vocal line sits against the hook pattern.

5. **Portamento is pitch motion, not a separate CC lane.** Slide notes draw Bézier pitch paths that the resynth engine and built-in synth both honour.

### Brand promise

**FREEQUENCY** = frequency-first production. The name is the product: everything you touch is visible as **frequency over time**.

---

## Feature integration map

| Capability | User-facing name | Model | Engine | UI |
|------------|------------------|-------|--------|-----|
| Step pitch correction + resynth | **VariAudio** | `VariAudioSegment[]` on `AudioClip` | `ResynthProcessor` (offline bake + RT preview) | Frequency Field pitch lane |
| Pitch glide on notes | **Portamento slides** | `PortamentoSlide` on `MidiClip` | `MidiSourceProcessor` PB curve | Piano roll + Field overlay |
| Take blending | **Swipe comp** | `CompSwipeRegion[]` on `AudioClip` | Crossfade in `AudioClipProcessor` | Lane swipe + Field stack |
| FL patterns on timeline | **PatternClip playback** | `Pattern` + `PatternClip` | `PatternExpander` → MIDI snapshot | Pattern blocks in arrange |
| Context from other clips | **Ghost notes** | `GhostNoteConfig` on editor session | Read-only snapshot merge | Piano roll + Field (dimmed) |
| Live tempo/pitch warp | **Elastic audio** | `ElasticMode` on `AudioClip` | RT granular node + offline OLA | Warp handles + Field curve |

### Processing order (audio thread safe)

Message thread builds immutable snapshot:

```
source file → load/resample → [offline OLA stretch] → [VariAudio resynth segments]
           → [comp swipe mix of takes] → reverse → publish to AudioClipSnapshot
```

Real-time elastic preview (Phase B): parallel RT node reads warp curve atomics; offline bake on commit.

---

## Visual identity (Spectral UI)

The UI expresses the Frequency Field metaphor:

- **Deep void background** (`#0A0E14`) with **spectral aurora** gradients (teal → violet → amber)
- **Glass panels**: 8–12% white fill, 40px backdrop blur, 1px luminous border
- **Glow accents**: teal `#2DD4BF` bloom on active clips, playhead, and pitch contours
- **Reflection strip**: subtle horizontal specular on transport bar and mixer faders
- **Frequency Field zone**: semi-transparent pitch grid with glowing note/contour dots

Figma source: [FREEQUENCY — Spectral UI](https://www.figma.com/design/ACH9Fh5jq0xP3BDmMQFrXr)

### UI Guide (coach marks)

Animated spectral overlays teach the workspace layout:

- **Welcome tour** on first launch (transport → arrange → mixer → browser → themes)
- **Fly-beam hints** when floating panels close (Appearance, Audio, Keys, piano roll) or when mixer/browser hide
- **Six effect styles**: pulse ring, aurora glow, spotlight, fly-beam, ripple, shimmer
- **Three custom pins** — place anywhere via Appearance (F3); positions persist in `guide_settings.xml`

---

## Phased delivery

| Phase | Deliverable | Status |
|-------|-------------|--------|
| A | OLA stretch, resample pitch, take switch, trim | ✅ |
| B | Model foundations (`HarmonicEdit.h`), docs, Spectral theme | 🔄 this PR |
| C | Ghost notes in piano roll + PatternClip engine playback | ✅ |
| D | Swipe comp crossfade (lane UI + snapshot mix) | ✅ |
| E | VariAudio segments + offline resynth bake | ⏳ |
| F | Portamento slide curves (MIDI export + synth) | ⏳ |
| G | Real-time elastic preview node | ⏳ |
| H | Frequency Field editor (dedicated view) | ⏳ |
