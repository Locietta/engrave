# Audio Rendering Cooperative Cancellation Contract

This document specifies the cooperative cancellation contract, guarantees, and execution model for offline audio synthesis in `Document::render_audio`.

## 1. Calling Contract and Guarantees

Cancellation is cooperative and non-preemptive. The caller provides a cancellation predicate via `AudioOptions::cancelled`:

```cpp
struct AudioOptions {
    // ...
    std::function<bool()> cancelled;
};
```

### Execution Invariants
- **Owner Thread Only**: The predicate is invoked exclusively on the engine's dedicated owner thread.
- **No Re-entrancy**: The callback must not call back into the engine, mutate open documents, or invoke engine API methods.
- **Callback Exception Handling and Teardown Paths**: Exception handling and cleanup behavior differ between the internal render loop and the public API adapter:
  - **Render-Loop Path (`engrave/src/audio/engine_audio.cpp:536`)**: During synthesis and encoding in `export_score_audio`, the driver catches and latches any exception thrown by the `cancelled` callback (`callback_error = std::current_exception()`), treating the render as cancelled. The driver executes orderly teardown (idling the DSP graph, joining worker threads, closing file devices, and attempting to delete the intermediate output file via `std::filesystem::remove`). If removing the intermediate file fails, the driver returns `ErrorCode::IO` first. Otherwise, once cleanup completes, the latched exception is rethrown via `std::rethrow_exception` to propagate to the caller.
  - **Public Adapter Paths (`engrave/src/api/audio.cpp`)**: The public wrapper `Document::render_audio` calls `options.cancelled` directly outside the driver's render loop:
    - *Before rendering (~line 102)*: Polled directly before rendering setup. If the callback throws here, the exception propagates immediately without latching, before any temporary file is created.
    - *During publication (~line 289)*: Polled between chunk deliveries while streaming the finalized temporary file to the `ByteSink`. If the callback throws here (or if the sink throws), the exception is not latched by the driver; it propagates directly out of `render_audio`. During stack unwinding, the `TempFile` destructor executes `std::filesystem::remove` with an ignored `std::error_code`, discarding removal errors. Consequently, a publication-time exception propagates to the caller rather than returning `ErrorCode::IO`, and if removal fails, the temporary file can be left behind on disk.
- **No Artifact Publication**: When cancellation is observed, no output bytes are delivered to the caller's `ByteSink` (or delivery stops immediately if partial chunks were delivered during publication), and intermediate encoded files are deleted before returning (subject to the publication exception path noted above).
- **Unpoisoned State**: Cancellation leaves the `Document` and `Engine` in a clean, consistent state. Subsequent operations (edits, renders, or repeat audio exports) in the same session proceed normally, and repeat exports on unchanged input produce bit-identical results (subsequent document edits will naturally alter the output).
- **Error Code**: A cancelled render returns `ErrorCode::CANCELLED` in `Result<AudioRenderResult>`.

## 2. Cancellation Checkpoints and Stages

Cancellation is polled at six distinct execution boundaries:

1. **Setup & Sample Loading**:
   - Polled before constructing the FluidSynth synthesizer graph and before loading SoundFont preset sample banks.
   - Avoids expensive sample loading if the job is aborted before audio synthesis begins.

2. **Playback Model & Sequence Collection**:
   - Polled per timestamp and per copied or moved MPE playback event during sequence collection.

3. **Synthesis Inner Loop**:
   - Polled before FluidSynth DSP processing, before each sequence, between MIDI events, before tuning calculations, and at every 64-sample-frame sink copy (one FluidSynth internal buffer interval).

4. **Multi-Track Mixing**:
   - The mixer aborts processing immediately if a track is cancelled, and polls the callback between rendering successive audio tracks.

5. **Encoding & Intermediate File Writing**:
   - Polled before outer DSP blocks, before encoder execution, and at every intermediate file write (chunked at intervals <= 4 KiB).

6. **Publication & Sink Delivery**:
   - Polled before delivering each 1 MiB chunk of finalized audio data to the caller's `ByteSink`.
   - Halts delivery immediately if cancelled during streaming.

## 3. Concurrency and Teardown on Abort

Teardown behavior on abort depends on host OS characteristics to maintain responsiveness:
- **Windows**: When cancelling dense event collections (at least 16,384 timestamps across two or more tracks), origin-event stores from the playback model are moved into four batches and cleared in parallel across joined background threads before completing model destruction. This avoids blocking the owner thread during large memory release operations.
- **Linux**: Teardown is performed synchronously on the owner thread. Background multi-threaded freeing is disabled on Linux to prevent memory contention across `glibc` allocation arenas.

Under specific 50-trial dense-score probes (12 parts x 256 measures x 16 notes per measure = 49,152 notes), measurements showed p95 <= 30 ms and max <= 50 ms across all phases on reference test hardware. These timings represent empirical measurements from those specific test configurations and do not establish universal performance bounds or guarantees for arbitrary score inputs, custom soundfonts, disk performance, or host OS scheduling.

## 4. Unpolled and Indivisible Intervals

The following operations are indivisible or unpolled internally, several of which are reachable on the audio render path:
- **Document Loading & Initial Layout**: Score parsing, repair, and layout during `Engine::open -> load_native_score -> finish_load` run without internal cancellation polling. Because host audio workers may call `Engine::open` on an MSCZ immediately before `render_audio`, this interval is reachable in an audio export job before the audio callback scope is established.
- **Conditional Relayout**: Audio rendering calls `engine_audio_tracks -> music::select_score -> ensure_layout -> Score::doLayout` when the selected score (e.g. an unlaid-out excerpt) has no pre-computed pages.
- **First-Use Font & Service Setup**: First-use or changed-font parsing, text shaping cache population, and audio service initialisation.
- **Playback & State Caches**: Repeat and tempo cache updates, and individual playback render operations.
- **Automation Resampling**: Automation curve resampling within an automation layer.
- **Memory & Container Operations**: Heap allocations and bulk container copies/moves (including moving MPE origin-event store batches).
- **Atomic Library & File Operations**: Native codec and sample decompression calls, a single 64-frame FluidSynth buffer render, individual OS filesystem read/write system calls, and intermediate file deletion.
- **Active Sink Execution**: The cancellation predicate is polled between `ByteSink` chunks during publication, but cannot interrupt an active, in-flight sink delivery. Final cleanup remains indivisible.

Because these intervals are indivisible, fixed dense-score distribution tests do not establish arbitrary-input latency bounds.

## 5. Resource Limits and Budget Enforcement

In addition to explicit cancellation via `AudioOptions::cancelled`, audio exports support hard size limits:
- `AudioOptions::max_bytes`: Limits the maximum byte length of the finalized encoded file.
- `AudioOptions::max_write_bytes`: Limits the cumulative bytes written to the intermediate file (including encoder header rewrites).

Exceeding either budget terminates rendering immediately with `ErrorCode::RESOURCE_LIMIT` (or `ErrorCode::IO`), removes the intermediate file, and invokes no further sink callbacks.
