# Audio Render Pipeline Architecture

This document describes the architecture, data flow, ownership model, and deterministic behavior of the `engrave` engine's offline audio render pipeline (`Document::render_audio`).

## Pipeline Overview

Audio rendering transforms an in-memory musical score into an encoded audio stream (such as WAV, FLAC, Ogg Opus, MP3, or AAC) through a data-oriented execution pipeline:

```
Score (DOM)
  │
  ▼
PlaybackScoreBuilder (Score walk)
  │  emits hot & cold MPE events
  ▼
PlaybackScore (Flat MPE Store)
  │
  ▼
FluidSequencer (Per-track lowering & sorting)
  │  lowers MPE to TimedMidiEvent timeline
  ▼
FluidTrack / FluidSynth (Synthesis)
  │  renders audio into per-track PCM buffers
  ▼
OfflineRenderSession (Fixed-order mix)
  │  dry accumulation + aux reverb send/return
  ▼
SoundTrackWriter (Render loop)
  │  pulls 4096-frame PCM chunks
  ▼
AudioEncoder (pro::proxy runtime seam)
  │  encodes PCM to target format
  ▼
ByteSink / IODevice (Stream output)
```

The pipeline operates on the single dedicated engine owner thread, keeping data separate from behavior and avoiding runtime virtual dispatch outside genuine format seams.

## Data Flow and Ownership

The pipeline enforces strict single ownership and avoids shared ownership graphs or reference cycles:

- **Engine Audio Resources (`AudioResources`)**: Process-lifetime audio resources, including loaded SoundFonts, sound preset metadata, and audio services, are owned by `EngineAudio` on the engine owner thread. Render sessions borrow these resources by reference; sessions never outlive the engine.
- **Render Session (`OfflineRenderSession`)**: Each export creates a fresh `OfflineRenderSession`. The session owns its track list, aux bus configurations, reverb effect processors (`fx::ReverbProcessor`), and intermediate mix buffers. Render configuration parameters (`OfflineRenderSettings`) are snapshotted and owned by value.
- **Track Synthesis (`FluidTrack`)**: Each track owns its FluidSynth synthesizer instance (`std::unique_ptr<Fluid> fluid_`), its channel mapping (`ChannelMap`), and its sequencer (`FluidSequencer`). Synthesizers are not shared across tracks or exports.
- **Flat MPE Store (`PlaybackScore`)**: Owned by the export orchestration in `engine_audio.cpp`. Tracks borrow event slices only while lowering their MIDI sequences, after which the MPE store is released immediately via cooperative cancellation checkpoints before synthesis begins.
- **Encoders (`AudioEncoder`)**: Format encoders are owned exclusively through type-erased proxy wrappers (`UniqueEncoder`), which enforce sole ownership via `std::unique_ptr`.

## Flat Playback Walk and Articulation Profiles

Score playback data is extracted by `PlaybackScoreBuilder` using the shared score walk (`playbackwalk.h`).

- **Score Walk**: Iterates score parts, measures, segments, and chord rests on the owner thread. It does not install score listeners or track incremental edit history.
- **Articulation Profile Bundle (`ArticulationProfileBundle`)**: Maps instrument `SoundCategory` enums to `ArticulationsProfilePtr` handles. In production, the bundle is owned by the builder and queries the concrete `ArticulationProfilesRepository` on the owner thread via a non-virtual member call on first access per family, memoizing results in an enum-indexed array. In tests, the bundle can be populated directly with mock profiles without requiring a repository instance.
- **Output**: Produces a `PlaybackScore` containing a list of `(InstrumentTrackId, TrackEvents)` pairs sorted deterministically by track ID.

## MPE Event Buffers (Hot vs Cold Storage)

The flat MPE store (`muse::mpe::TrackEvents` in `flatevents.h`) separates performance-critical playback events from cold metadata to maximize cache locality:

### Hot Playback Events

Trivially copyable structures stored in contiguous buffers (`Buf<T>`, backed by `amc::vector<T>`):

- `NoteRecord`: Represents a note execution with nominal and actual timestamps, duration, nominal dynamic level, velocity override, and `Span` descriptors pointing into auxiliary buffers.
- `ArticulationRecord`: Articulation metadata (type, duration, timestamps) stored in ascending `ArticulationType` order.
- `CurvePoint`: Point on pitch and expression curves (`duration_percentage_t at`, `percentage_t value`).
- `DynamicLayer` and `DynamicPoint`: Volume and automation curves.
- `EmptyGroup`: Preserves legacy chord alignment timestamps without notes.

Hot records reference child ranges via `Span` (offset and count). Buffer allocations and span accesses are guarded by `checked_grow` and `checked_span`, which throw `StoreLimit` if offsets exceed 32-bit limits.

### Cold Events

Non-trivially copyable events stored separately in `std::vector<ColdEvent>`:

- `ColdEvent`: Holds a `std::variant<TextArticulationEvent, SoundPresetChangeEvent, SyllableEvent>` along with group timestamp and sequence ordinal.
- Separation ensures that string allocations for lyrics, staff text, and preset identifiers do not fragment or pollute the hot note playback buffers.

## Event Lowering and Sequencing

`FluidSequencer` lowers a track's MPE events into timed MIDI events:

1. **Dynamic Lowering**: Copies dynamic layers and points with cooperative cancellation polling every 4096 events.
2. **Note and CC Lowering**: Converts `NoteRecord` entries into MIDI Note-On/Note-Off events, pedal CC events, and pitch bend curves. Channel allocation is managed per-track via `ChannelMap`.
3. **Bounded Stable Sort**: MIDI events are sorted chronologically using `bounded_stable_sort_by`. Sorting runs in fixed blocks of 8192 elements (`k_sort_block_size`), followed by pairwise iterative merges. This preserves the relative emission order of simultaneous events while polling cancellation between blocks and every 8192 merge steps.
4. **Timeline Slicing**: Slices of `TimedMidiEvent` are emitted forward in time chunks as requested by the synth processing loop.

## Fixed-Order Flat Mix Plan

`OfflineRenderSession` executes offline mixing in a deterministic, sequential order:

1. **Per-Track Synthesis**: Iterates active tracks in `tracks_`. Each track synthesizes audio into its own dedicated buffer (`track.buffer`) via `FluidTrack::process`. Muted tracks skip synthesis.
2. **Dry Accumulation**: Synthesized track audio is summed into the master output buffer in ascending track index order.
3. **Aux Reverb Sends**: Active aux sends scale each track's buffer and accumulate the signal into `reverb_.buffer`.
4. **Reverb Processing**: If reverb is present and active, `reverb_.buffer` is processed through `fx::ReverbProcessor` (Muse Reverb DSP) and summed into the master output.
5. **Master Processing**: Master volume and pan controls apply to the mixed output.

Disjoint track buffers isolate synthesis state from mixing, ensuring identical floating-point summation order across runs.

## Encoder Seam (`pro::proxy`)

Format encoders are decoupled from the render loop using Microsoft proxy (`pro::proxy` 4.0.1) instead of virtual inheritance:

- **Facade Convention (`EncoderFacade`)**:
  ```cpp
  PRO_DEF_MEM_DISPATCH(EncoderBegin, begin);
  PRO_DEF_MEM_DISPATCH(EncoderEncode, encode);
  PRO_DEF_MEM_DISPATCH(EncoderEnd, end);

  struct EncoderFacade : pro::facade_builder
      ::add_convention<EncoderBegin, bool(samples_t)>
      ::add_convention<EncoderEncode, size_t(samples_t, const float *)>
      ::add_convention<EncoderEnd, Ret()>
      ::build {};

  using AudioEncoder = pro::proxy<EncoderFacade>;
  ```
- **Unique Ownership (`UniqueEncoder`)**:
  `struct UniqueEncoder` wraps `AudioEncoder`, accepting only `std::unique_ptr<T>` from encoder factories. It normalizes null instances and rejects raw or shared pointers at compile time, guaranteeing sole ownership of encoder state.
- **Render Loop (`SoundTrackWriter`)**:
  Pulls audio from `OfflineRenderSession` in fixed 4096-frame steps, zeroes its intermediate buffer, invokes `session->process(...)`, checks cancellation, and passes the chunk to `encoder_ptr_->encode(chunk, buffer)`.

## Cooperative Cancellation

Cancellation is polled cooperatively at bounded boundaries throughout all execution phases:

- **Checkpoints**:
  - Score walk and MPE freeze loops: checked every `FreezeProgress::k_stride` (4096 records).
  - Context materialization: checked during cold event scans and dynamic layer collection.
  - Sequencer lowering and sorting: checked every 4096 copied events and 8192 merge steps.
  - Mix loop: checked before each track synthesis, before reverb processing, and after aux checks.
  - Render loop: checked before each 4096-frame block pull and before each encoder block write.
- **Latency Limits**:
  - Request-to-observed latency: p95 <= 5 ms, max <= 10 ms.
  - Cleanup and return latency: p95 <= 50 ms, max <= 100 ms.

Cancellation unlinks and removes partial output files from disk before returning.

## Deterministic Output and the Identity Contract

The render pipeline guarantees deterministic, byte-identical output across runs on the same platform:

- **Dump A**: Full MPE event dump (`A.txt`), serialized in the legacy text schema from the flat store records.
- **Dump B**: Flat-record projection (`B.txt`), compared against an independent legacy oracle projection (`B_legacy.txt`) loaded from `PlaybackModel` within the same binary.
- **Dump C**: Synthesizer input dump (`C.txt`), capturing every FluidSynth API call, channel configuration, and ordered raw MIDI event word.
- **Dump D**: PCM hash (`D.json`), SHA-256 hash of the decoded 32-bit float WAV audio chunk.

### Verifying with `test_playback_dod.py`

The test harness script `tests/integration/test_playback_dod.py` verifies the identity contract:

```bash
# Compare output between two probe binaries across 44.1 kHz and 48 kHz
python tests/integration/test_playback_dod.py executables \
    --exe path/to/old/playback_probe.exe \
    --other-exe path/to/new/playback_probe.exe \
    --assets .runtime/assets \
    --font /path/to/FluidR3Mono_GM.sf3 \
    --output notes/playback-comparison

# Compare two previously captured dump directories
python tests/integration/test_playback_dod.py compare \
    --left notes/playback-comparison/left \
    --right notes/playback-comparison/right

# Capture dumps from a single probe binary
python tests/integration/test_playback_dod.py capture \
    --exe build/gate/playback_probe.exe \
    --assets .runtime/assets \
    --font /path/to/FluidR3Mono_GM.sf3 \
    --output .runtime/capture-run

# Run allocation and cancellation timing measurements
python tests/integration/test_playback_dod.py measure \
    --exe build/gate/playback_counting_probe.exe \
    --assets .runtime/assets \
    --font /path/to/FluidR3Mono_GM.sf3 \
    --output .runtime/playback-counts \
    --trials 7 \
    --cancel-trials 20
```

## Deliberate Music-Behaviour Rules

The pipeline enforces deliberate precedence and normalization rules to resolve ambiguous musical semantics:

1. **Articulation Precedence**:
   Arpeggio articulations take highest precedence over ornaments and tremolos (`ARPEGGIO`, `ARPEGGIO_UP`, `ARPEGGIO_DOWN`, `ARPEGGIO_STRAIGHT_UP`, `ARPEGGIO_STRAIGHT_DOWN`), ensuring rolled chords stay rolled. Unlisted articulations follow ascending `ArticulationType` enum order.
2. **Grace Note Timing**:
   Both pre-grace and post-grace groups on a chord are rendered using proportional "share half" timing (`GraceChordCtx::build_grouped_ctx`). If the combined duration of before and after grace chords exceeds half of the principal chord duration, both groups shrink proportionally to occupy exactly half, leaving the remaining duration for the principal chord.
3. **Bend Collision Resolution**:
   When overlapping pitch bend curves or reset events (such as centering to 8192) occur at the exact same timestamp, events are emitted in ascending `ArticulationType` enum order. In the resulting MIDI event stream, the later event wins.
4. **Program Choice (Technique over Timbre)**:
   When multiple mapped articulations apply to a single note, technique takes precedence over timbre:
   `SNAP_PIZZICATO` > `PIZZICATO` > `SLAP` > `POP` > `HARMONIC` > `PALM_MUTE` > `MUTE` > `JAZZ_TONE` > `DISTORTION` > `OVERDRIVE`, falling back to ascending enum order for unlisted articulations.
5. **Competing Apply-to-All Flags**:
   When multiple "apply to all staves" sound flags target the same unflagged staff on a segment, precedence is determined by insertion order: the first flag encountered on the segment wins.

## Known Limitations

- **Cancellation Bounds**: The measured cancellation bounds (request-to-observed <= 10 ms, cleanup <= 100 ms) are verified against the standard test corpus. Extremely large single memory allocations or third-party codec internal encode calls (such as long Vorbis/FLAC compression blocks) execute outside engine checkpoints and can extend a poll interval.

## Future Directions

- **Parallel Per-Track Synthesis**: The separation of per-track buffers in `OfflineRenderSession` allows track synthesis loops to run concurrently across worker threads before the sequential dry/reverb mix pass, once thread safety in FluidSynth logging and soundfont instance loading is addressed.
- **Encode Overlap**: Pipelining the synchronous 4096-frame PCM block synthesis with an asynchronous encoding queue, overlapping audio rendering with codec compression.
- **Releasing the Owner Thread**: Detaching the offline render session from the single engine owner thread by snapshotting score properties, duration, and track configurations at the start of export, allowing long exports to run on background worker threads without blocking interactive score operations.
