# Audio Render Pipeline Architecture

This document describes the architecture, data flow, ownership model, and deterministic behavior of the `engrave` engine's offline audio render pipeline (`Document::render_audio`).

## Pipeline Overview

Audio rendering transforms an in-memory musical score into a WAV, FLAC, Ogg Opus, or MP3 stream through a data-oriented execution pipeline:

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

- **Engine Audio Resources (`AudioResources`)**: Engine-lifetime audio resources, including loaded SoundFonts, sound preset metadata, and audio services, are owned by `EngineAudio` on the engine owner thread. Render sessions borrow these resources by reference; sessions never outlive the engine.
- **Render Session (`OfflineRenderSession`)**: Each export creates a fresh `OfflineRenderSession`. The session owns its track list, aux bus configurations, reverb effect processors (`fx::ReverbProcessor`), and intermediate mix buffers. Render configuration parameters (`OfflineRenderSettings`) are snapshotted and owned by value.
- **Track Synthesis (`FluidTrack`)**: Each track owns its FluidSynth synthesizer instance (`std::unique_ptr<Fluid> fluid_`), its channel mapping (`ChannelMap`), and its sequencer (`FluidSequencer`). Synthesizers are not shared across tracks or exports.
- **Flat MPE Store (`PlaybackScore`)**: Owned by the export orchestration in `engine_audio.cpp`. Tracks borrow event slices only while lowering their MIDI sequences; once lowered, the flat store is released before synthesis begins, draining cold event owners and the retained playback context in polled strides during the `EVENT_RELEASE` phase before freeing the flat buffers.
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

`OfflineRenderSession` executes offline mixing in a deterministic order (`synthesize` and `mix` in `offlinerendersession.cpp`):

1. **Per-Track Synthesis**: Each non-muted track synthesizes audio into its own dedicated buffer (`track.buffer`) via `process_track` (which drives the track's source, insert effects, and level controls). Tracks run serially on the owner thread or in parallel on the worker pool (see below).
2. **Combined Dry Accumulation and Aux Send Pass**: On the owner thread, in a single pass over active tracks in ascending index order, non-silent track buffers are summed into the master output buffer while simultaneously scaling by active aux send amounts and accumulating into the reverb send buffer (`reverb_.buffer`).
3. **Aux Reverb Processing and Mixdown**: If reverb is present and active (or non-silent), `reverb_.buffer` is processed through `fx::ReverbProcessor` (Muse Reverb DSP) and its wet return is summed into the master output buffer.

Disjoint track buffers isolate synthesis state from mixing, ensuring identical floating-point summation order across runs and thread counts. Cooperative cancellation is polled before synthesis, before reverb buffer zeroing, before each track's accumulation pass, and before reverb processing.

### Parallel Per-Track Synthesis

`EngineOptions::audio_threads` (`EngineAudioSetup::audio_threads` internally) selects how many threads synthesize a block's tracks: `0` is `hardware_concurrency - 1` clamped to 1..8, `1` is the serial path, larger values are capped at 64, and negative values are rejected. A positive integer in `ENGRAVE_AUDIO_THREADS` overrides the configured value; other values are ignored. Synthesis stays serial while a MIDI event hook is installed, while playback-harness capture, allocation counting or fault injection is active, if the OS refuses thread creation, and for renders with at most one active (unmuted) track (so a single-instrument score with a muted metronome track never starts workers).

The pool (`muse/framework/audio/engine/internal/workerpool.h`) is owned by `EngineAudio::State`, starts its threads on the first render that fans out, reuses them across renders and joins them when the engine audio is destroyed. Per block, the owner thread handles muted tracks inline, hands active tracks to workers longest-previous-block first, waits, then mixes in track order. Invariants:

- **Per-thread FP state**: synthesis and encoding pin the active FP mode (FTZ/DAZ plus rounding mode) via `ScopedDenormalGuard::RenderModePin`, and every worker task applies and restores that pinned mode via `ScopedDenormalGuard::WorkerScope` (`denormalguard.h`). Mid-render cancellation callbacks still see and keep their own rounding-mode changes on return without altering serial vs. parallel audio output.
- **Shared SoundFont, read-mostly**: synths share the cached defsfont. FluidSynth stays built with `NO_THREADS`; two patches make worker-side writes safe: sample voice refcounts use atomic builtins (`fluid_sample_incr_ref`/`fluid_sample_decr_ref` in `fluid_sfont.h`), and `fluid_zone_inside_range` stores the zone `ignore` flag only when it is set (only mono legato sets it; tracks pin legato mode 0).
- **Owner-only loading**: SoundFont loads, sample loads, preset selection and synth creation happen during setup on the owner thread. `FluidTrack::handle_event` refuses `PROGRAM_CHANGE` during `PLAYING_OFFLINE`. `initialize_fluid_globals()` installs the FluidSynth log sink and runs FluidSynth's one-time table setup once, on the owner thread, before any render.
- **Cancellation**: only the owner thread calls `AudioOptions::cancelled`. While waiting for a block it polls about every millisecond (on Windows through a high-resolution waitable timer, since condition-variable timeouts round up to the scheduler tick). Once the predicate returns true or throws, the pool's stop flag is set; worker tasks see only that flag through their own `OfflineCancellation` scope, unwind at their next poll, and the owner surfaces the cancellation or rethrows after the block drains. A worker exception is rethrown on the owner thread the same way. If worker thread creation fails completely, synthesis falls back to the ordinary serial loop so the owner's live cancellation scope stays active inside each track.

### Timing Instrumentation

With `ENGRAVE_AUDIO_TIMING` set, each export prints `offline_audio_seconds key=value` lines to stderr. `synth` (equal to `synth_wall`) is the wall time of per-track synthesis and `mix` is the rest of the block pull; `synth_total` and `synth_max_track` sum each track's own synthesis time, whichever thread ran it. A final `offline_audio_synth threads=N tracks=M` line gives the fan-out actually used. With `ENGRAVE_RENDER_TIMING` set, each export prints `render_seconds format=... page=...` (or `pages=A-B` for multi-page PDF) `dpi=... raster=... encode=... bytes=...`, splitting drawing from encoding (PNG deflate, or Cairo page emission and surface finish across the exported PDF page range).

## Encoder Seam (`pro::proxy`)

Format encoders are decoupled from the render loop using proxy (`pro::proxy`, ngcpp/proxy 4.1.0, the continuation of the archived microsoft/proxy) instead of virtual inheritance:

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

## Cooperative Cancellation and Budgets

Cancellation is cooperative and non-preemptive via `AudioOptions::cancelled`:

```cpp
struct AudioOptions {
    // ...
    std::function<bool()> cancelled;
};
```

### Calling Contract and Invariants

- **Owner thread only, no re-entrancy**: The predicate runs exclusively on the
  engine owner thread and must not call back into the engine or mutate open
  documents.
- **Unpoisoned state and error code**: A cancelled render returns
  `ErrorCode::CANCELLED` in `Result<AudioRenderResult>` and leaves `Document`
  and `Engine` clean for subsequent operations. Repeat exports on unchanged
  input stay bit-identical.
- **No partial publication on pre-publication abort**: When cancellation is
  observed before sink delivery begins, intermediate encoded files are removed
  and no bytes reach `ByteSink`. Cancellation during sink streaming halts
  further chunk delivery immediately.
- **Callback exception handling**:
  - Inside the driver (`export_score_audio` in `engrave/src/audio/engine_audio.cpp`),
    exceptions thrown by `cancelled` are caught and latched as a cancellation so
    the DSP graph is idled, the file device is closed, and the intermediate file
    is removed before unwinding. If removing the intermediate file fails,
    `ErrorCode::IO` is returned first; otherwise the latched exception is
    rethrown.
  - Outside the driver (`Document::render_audio` in `engrave/src/api/audio.cpp`),
    pre-render and `ByteSink` chunk-delivery polls propagate callback (or sink)
    exceptions directly. During stack unwinding, `TempFile`'s destructor removes
    the temporary file while ignoring removal errors.

### Checkpoints

Cancellation is polled at bounded boundaries across all stages:

- **Setup and track build**: Checked before playback build, per track build, and
  after track setup.
- **Score walk and MPE freeze**: Checked every `FreezeProgress::k_stride` (4096
  records), and during context materialization (cold event scans and dynamic
  layer collection).
- **Sequencer lowering and sorting**: Checked every 4096 copied events and every
  8192 merge steps in `bounded_stable_sort_by`.
- **Event store release (`EVENT_RELEASE`)**: Checked in polled strides while
  draining cold event owners and the retained context before freeing flat
  buffers.
- **Mix and render loops**: Checked before each track synthesis (serial path) or
  before each block and about every millisecond while waiting for its parallel
  track synthesis, before reverb buffer zeroing/processing, before each
  4096-frame block pull, before each encoder write, and at intermediate file
  writes (chunked <= 4 KiB).
- **Sink delivery**: Checked before delivering each 1 MiB chunk of the finalized
  file to `ByteSink`.

### Unpolled Intervals

The following stages execute without internal cancellation polling:

- Document loading and initial layout (`Engine::open`) and conditional relayout
  (`Score::doLayout`) when an unlaid-out score or excerpt is selected.
- First-use font parsing, text shaping cache population, and audio service
  initialization.
- Repeat/tempo cache updates and automation curve resampling.
- Single heap allocations, bulk container moves, native codec/sample
  decompression calls, a single 64-frame FluidSynth render block, individual OS
  file syscalls, intermediate file deletion, and an in-flight `ByteSink` call.

### Resource Budgets

`AudioOptions::max_bytes` caps the finalized encoded output size and
`AudioOptions::max_write_bytes` caps cumulative intermediate file writes
(including encoder header rewrites). Exceeding either limit stops rendering with
`ErrorCode::RESOURCE_LIMIT` (or `ErrorCode::IO`), removes the intermediate file,
and invokes no further sink callbacks.

### Latency Design Targets

- Request-to-observed latency: p95 <= 5 ms, max <= 10 ms.
- Cleanup and return latency: p95 <= 50 ms, max <= 100 ms.

These targets are checked by `test_playback_dod.py measure` against a generated
dense score (`dense.musicxml`) rather than guaranteed universally across
arbitrary score topologies or unpolled codec frames.

## Deterministic Output and the Identity Contract

The render pipeline guarantees deterministic, byte-identical output across runs on the same platform:

- **Dump A**: Full MPE event dump (`A.txt`), serialized in the legacy text schema from the flat store records.
- **Dump B**: Flat-record projection (`B.txt`), verified against Dump A's inventory and compared between captures that contain a complete projection.
- **Dump C**: Synthesizer input dump (`C.txt`), capturing instrumented FluidSynth setup operations (such as channel configurations, SoundFont loads, and program selections) and the ordered raw MIDI event slices delivered to the synthesizer.
- **Dump D**: PCM hash (`D.json`), SHA-256 hash of the decoded 32-bit float WAV audio chunk.

### Verifying with `test_playback_dod.py`

See [playback-harness.md](playback-harness.md) for harness build flags, probe
options, allocation-counting rules, and scheduled-poll cancellation sampling.
Basic commands:

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
    --exe build/playback/playback_probe.exe \
    --assets .runtime/assets \
    --font /path/to/FluidR3Mono_GM.sf3 \
    --output notes/runs/capture-run

# Run cancellation timing and export performance measurements (uncounted probe)
python tests/integration/test_playback_dod.py measure \
    --exe build/playback/playback_probe.exe \
    --assets .runtime/assets \
    --font /path/to/FluidR3Mono_GM.sf3 \
    --output notes/runs/playback-measurements \
    --trials 7 \
    --cancel-trials 20
```

> **Note**: Timing and cancellation measurements require the uncounted `playback_probe.exe`. The separate `playback_counting_probe.exe` is used strictly for allocation tracking, as hooked allocator interception introduces measurement overhead.

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

- **Cancellation Targets and Harness Coverage**: The cancellation latency targets (request-to-observed <= 10 ms, cleanup <= 100 ms) are checked by `test_playback_dod.py measure` against a synthetic generated dense score (`dense.musicxml`). The measurement harness reports percentile distributions without enforcing pass/fail assertion limits, and its scheduled-poll samples test cooperative polling intervals rather than modeling arbitrary asynchronous interruption during unpolled stages. Exceptionally large individual memory allocations, unpolled score routines, or third-party codec internal calls (such as long Vorbis or FLAC compression frames) execute outside engine checkpoints and can dominate a poll interval.

## Future Directions

- **Parallel Reverb and Mix**: Parallel synthesis is bounded by the heaviest track, and the owner-thread mix (sums, reverb, master stages) is now a comparable share of a dense render; both are candidates for later work. ThreadSanitizer runs (`--config=tsan`, Linux and macOS only) have not yet been part of validation.
- **Encode Overlap**: Pipelining the synchronous 4096-frame PCM block synthesis with an asynchronous encoding queue, overlapping audio rendering with codec compression.
- **Releasing the Owner Thread**: Detaching the offline render session from the single engine owner thread by snapshotting score properties, duration, and track configurations at the start of export, allowing long exports to run on background worker threads without blocking interactive score operations.
