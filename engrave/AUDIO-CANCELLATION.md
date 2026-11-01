# M2.1 offline audio cancellation and reuse assessment

2026-11-02. Implementation `c18c619de866c4de488043c881aaf6c11c1b5f3c`, branch
`feat/audio-cancel-reuse`, based on engrave-28 commit
`451143b716ba8e43f176a8b053e1db9e32f18180`. No tag, host pin or push performed.

## Changes and latency source

EXECUTED timings identify track setup as the long interval. Dynamic SoundFont
preset selection loads/decompresses samples again after the previous synth is
released (`fluid_defsfont.c:2221`, `FluidSynth::setupSound`, `fluidsynth.cpp:343`).
Before sample-load polling, 50 delayed cancellations on repeat70 measured Windows
p95 67.819 ms, max 87.507 ms even with synthesis and file-write polling tightened.
After sample-load polling the same focused test passes below.

The driver installs its caught, latched owner-thread callback before graph setup.
It polls between tracks, between preset sample loads, between MIDI events, every
256 frames copied from FluidSynth, and within AAC/FLAC loops and 4 KiB file writes.
Aborted preset selection still balances selection counts; destruction tolerates
samples whose loading was skipped. Callback exceptions never unwind through C.
The existing API owner-thread/no-reentry guard stays in force.

4096 frames at 44.1 kHz represent 92.880 ms of audio; this is not wall processing
time. Inner synth copies cover at most 256 frames (5.805 ms of audio), preserving
FluidSynth's internal buffer, outer DSP blocks, event sequencing and codec inputs.

## Focused measurements (EXECUTED)

Release build, 50 trials per row. Units ms, p50 / p95 / maximum. Nearest-rank
percentiles. Instrumented run with `ENGRAVE_AUDIO_TIMING=1`:

| Request point | Windows | Linux (physical cachy) |
|---|---|---|
| During a synthesis MIDI event | 2.809 / 3.943 / 4.335 | 0.147 / 0.165 / 0.176 |
| Within encoder file write | 2.503 / 3.861 / 4.277 | 0.136 / 0.156 / 0.160 |
| Within final encoder flush write | 2.974 / 3.905 / 4.540 | 0.140 / 0.165 / 0.182 |
| External atomic flag after 50 ms | 2.524 / 5.577 / 5.879 | 0.230 / 0.371 / 0.393 |

Synthesis requests occur on the owner during actual FluidSynth event handling.
Write/flush requests occur during an injected encoder's real 1 MiB file write,
after two inner file-write polls; these are deterministic granularity probes,
not distributions for each production codec's finalization. Time runs from flag
setting through driver return, including graph teardown and output removal.
The delayed flag runs on another thread during a warmed repeat70 render, without
a hold at a callback. Thread creation and the first warmup are outside that timing.
These are engine timings, not JSON-RPC notification-to-response measurements.

Timing-disabled delayed trials: Windows 2.932 / 4.862 / 6.872; Linux
0.217 / 0.309 / 0.400. Each phase also verifies that cancellation removes the
output, then rendering again reproduces the baseline bytes. OS file operations,
individual sample decompression and individual native codec calls remain
indivisible; the measurements do not establish a universal wall-clock bound for
arbitrary fonts, storage or scheduling.

Warm ghost_note WAV, 50 samples after one warmup:

| Cost | Windows p50 / p95 / max | Linux p50 / p95 / max |
|---|---|---|
| Entire graph setup | 79.922 / 99.680 / 110.918 | 42.155 / 42.521 / 42.756 |
| Synth allocation (included in setup) | 1.722 / 2.681 / 2.782 | 0.639 / 0.714 / 0.720 |
| SoundFont attachment (included in setup) | 0.057 / 0.072 / 0.085 | 0.009 / 0.014 / 0.014 |
| Graph destruction | 1.560 / 3.087 / 3.226 | 0.127 / 0.152 / 0.159 |
| WAV encoder finalization | 0.069 / 0.104 / 0.229 | 0.002 / 0.003 / 0.005 |

Attachment excludes preset sample loading inside setupSound. Model/track/aux
subtimings are also emitted by ENGRAVE_AUDIO_TIMING. Windows timings overlapped
other focused verification and are not an idle-machine performance gate.

## Graph reuse decision (SOURCE-REVIEWED)

Full graph reuse is not implemented. The existing reset cannot reproduce a fresh
synth. `OfflineRenderSession::resetSources` (offlinerendersession.cpp:90) only
seeks; `FluidSynth::setPlaybackPosition` (fluidsynth.cpp:398) resets sequencer
position; `doFlushSound` (:170) sends all-notes-off, which releases voices rather
than recreating DSP state. FluidSynth initializes ticks and the sample cursor in
`fluid_synth.c:819,961`; `fluid_synth_system_reset_LOCAL` (:2674) does not reset
those fields or the buffered PCM cursor read at :4454. A render can finish partway
through that buffer. Reusing it can expose preceding render samples even without
a score, font or mix change. Aux FX and control nodes also need an explicit fresh
render reset; the current source seek provides none.

The local PlaybackModel and graph are created in `engine_audio.cpp:588,645` and
released together today. Safely retaining them requires a reset protocol verified
against fresh synthesis, plus a revision/font/mix/selection/repeat cache key and
document-close invalidation. No existing public API exposes a complete reset.
Do not claim retaining services/parsed SoundFonts constitutes synth-track reuse.
The measured rebuild costs above let the host choose whether to accept rebuilding
or authorize that larger reset/lifetime change. This is a limitation of the current
implementation, not a claim that safe reuse is impossible with further changes.

## Validation and parent hand-off

COMPILED Windows native MSVC release and Linux native conda GCC, with
`--engrave_tests=y`: engrave_output_test, engrave_output_probe, io_boundary_test.
Windows additionally compiled write470_probe. No full engine gate ran.

EXECUTED on both hosts: focused Audio/AudioInternal/SoundFont suites, 23 passed,
2 skipped (unavailable excerpt fixture and opt-in timing test); timing invocation
runs both timing tests successfully; IO suites pass 4 Windows / 5 Linux tests.
SoundFont A is FluidR3Mono_GM.sf3, B is the existing generated sine.sf2 fixture.
The new test replaces the engine SoundFont list A/B/A with one live document:
both A WAVs are byte-identical and B differs. It restores the original list.

EXECUTED Windows: all 22 host golden corpus fixtures, five formats (WAV, FLAC,
Opus, MP3, AAC), 110/110 complete files byte-identical to the engrave-28 source
tree. Explicit allowed differences: **none**. No golden files changed.
The baseline build uses f3563d22, whose tracked tree equals engrave-28.
Also six default/4.70/5.00 MSCX/MSCZ XML spot comparisons match after only the
existing programRevision normalization (MSCZ XML extracted). Input is a captured
minimal 5.00 score with stable IDs, SHA-256
`4add54468d1e8dd1983f43844420bd8fc4742c2b48c7bd5ee1c751dc54c4eb5e`.
Initial spot probes on 4.70 inputs produced existing random Part IDs and/or
Unsupported refusals; those probes are excluded from the six passing comparisons.
This is not an independent 4.7.6 reader gate or raw ZIP-container equality claim.

Full commands, hashes, comparison rows, logs and JSON summary are in the host's
ignored `notes/e29/`. Windows test binary SHA-256:
`7297875636caacf3b56284f6dd9542ca745beef3798fd4cf3043b0e6bac06cfb`;
Linux: `41ac372151db78c1156943035f93452862855b4198c5597d1eb5c546a33da6a1`.

Parent owns independent review, the host release notification benchmark,
both-host full gates, reuse acceptance, engrave-29 tag and host pin. This slice
provides no new public API and adds no non-standard public-header dependency.
