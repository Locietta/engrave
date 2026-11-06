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
- **Callback Exception Latching and Cleanup**: Per `engrave/src/audio/engine_audio.cpp:536`, the driver catches and latches any exception thrown by the `cancelled` callback (`callback_error = std::current_exception()`), treating the operation as cancelled. The driver proceeds through orderly teardown (idling the DSP graph, joining worker threads, closing file devices, and deleting any intermediate output file). If removing the intermediate file fails, the driver returns `ErrorCode::IO` first. Otherwise, once cleanup completes, the latched exception is rethrown via `std::rethrow_exception` to propagate to the caller.
- **No Artifact Publication**: When cancellation is observed, no output bytes are delivered to the caller's `ByteSink` (or delivery stops immediately if partial chunks were delivered during publication), and all intermediate encoded files are deleted before returning.
- **Unpoisoned State**: Cancellation leaves the `Document` and `Engine` in a clean, consistent state. Subsequent operations (edits, renders, or repeat audio exports) in the same session proceed normally and produce bit-identical results.
- **Error Code**: A cancelled render returns `ErrorCode::Cancelled` in `Result<AudioRenderResult>`.

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

Under dense scores (e.g. 12 parts, 256 measures, 16 notes per measure = 49,152 notes), internal polling ensures detection-to-return latency remains bounded (p95 <= 30 ms, max <= 50 ms across all phases).

## 4. Unpolled and Indivisible Intervals

The following operations are indivisible and are not interrupted mid-flight:
- **Document Loading & Initial Layout**: Score parsing and initial layout during `Engine::open` run without internal cancellation polling.
- **Conditional Relayout**: If an audio render is requested for an excerpt or score selection that has no pre-computed layout pages, `ensure_layout` invokes layout synchronously before audio synthesis begins.
- **Atomic Library Operations**: Single 64-frame FluidSynth block renders, individual native codec decompressions, and individual OS filesystem read/write system calls cannot be interrupted mid-call.
- **Active Sink Execution**: The callback is polled between `ByteSink` chunks, not within a running sink invocation.

## 5. Resource Limits and Budget Enforcement

In addition to explicit cancellation via `AudioOptions::cancelled`, audio exports support hard size limits:
- `AudioOptions::max_bytes`: Limits the maximum byte length of the finalized encoded file.
- `AudioOptions::max_write_bytes`: Limits the cumulative bytes written to the intermediate file (including encoder header rewrites).

Exceeding either budget terminates rendering immediately with `ErrorCode::ResourceLimit` (or `ErrorCode::Io`), removes the intermediate file, and invokes no further sink callbacks.
