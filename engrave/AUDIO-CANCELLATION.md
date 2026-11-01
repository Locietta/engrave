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


# M2.2 playback construction cancellation candidate

2026-11-02. Implementation `10e93fba04df6e6ed5741fccc3c156fee9442e2e`, based
on engrave-29 (`84d1f14f302f3a46b495cd9cf4984ed228be2ba4`). Branch
`feat/playback-cancel`, worktree e30. No tag, push or host pin change.

**Acceptance remains incomplete:** the playback-model gap is fixed and its
50-trial measurements meet the target on both hosts. Windows synthesis p95 is
50.613 ms, above the requested 50 ms. Do not mark the four-phase gate green.

## Changes (SOURCE-REVIEWED)

PlaybackModel::load accepts an optional owner-thread checkpoint and clears it on
success and exceptions. An aborted model must be discarded. The offline driver
passes its existing caught/latched cancellation poll and catches a private sentinel
outside construction. The local model's destructor detaches its score listener.
Callback exceptions remain latched and are rethrown through the existing setup
abort handler. No exception is newly allowed through a codec C callback; the
owner-thread/no-reentry contract remains in force.

Checkpoints cover model setup instruments/parts, context repeat/measure/segment
scanning, automation event insertion, segment annotations/elements, measure
repeats, event generation and metronome rebuilding. FluidSequencer also polls
per-score event conversion, dynamics layers and sostenuto channel setup. Its
partial graph is discarded by the driver's existing cancellation handling.

Primary locations: engine_audio.cpp:596, playbackmodel.cpp:107,423,451,492,513,
549,626,652; playbackcontext.cpp:301,313,322; fluidsequencer.cpp:133,150,370.
Internal phase scopes identify construction, sequencer and encoder work for the
tests. Encoding scopes wrap the real production encode calls and file writes.
No music arithmetic, ordering, DSP block sizes or codec inputs changed.

## Dense-score measurements (EXECUTED)

AudioInternal.DenseScoreCancellationLatencyAndRecovery generates the review's
12-part x 256-measure x 16-note shape (49,152 notes), opens it, and renders a
production WAV baseline. Each phase runs 50 trials and checks Cancelled, absent
output, p95 <=50 ms and max <=100 ms. After every phase, a full render must match
the uncancelled baseline byte-for-byte. Public adapter checks require Cancelled,
zero sink calls, absent intermediate, and byte-identical recovery after a throwing
callback during construction. All functional/recovery assertions passed on both
hosts; the Windows synthesis p95 assertion failed.

Construction trials set an external atomic flag 1-5 ms after selected construction
polls (2,000 through 149,000, stepping 3,000). They assert that the request is
observed while construction is still active. Other phases are synchronous
poll-granularity probes: first sequencer event before synthesis, an actual synthesis
MIDI event, and a real WAV encoder file-write poll. Timings run from flag setting
through driver return, including cleanup, graph/model destruction and output
removal. These are engine timings, not notification-to-JSON-RPC measurements or
a universal scheduling bound. Timing instrumentation is disabled. Nearest-rank
p50 / p95 / maximum, milliseconds:

| Request phase | Windows | Linux, physical cachy |
|---|---|---|
| before-synthesis | 28.159 / 37.301 / 39.094 | 10.286 / 10.725 / 10.940 |
| playback-model | 9.304 / 20.233 / 23.202 | 3.982 / 7.490 / 8.301 |
| synthesis | 40.877 / 50.613 / 57.926 | 16.775 / 17.024 / 17.091 |
| encode | 39.512 / 48.733 / 57.240 | 16.829 / 17.375 / 18.888 |

The existing repeat70 50-trial benchmark also ran on both hosts and passed all
four latency assertions. Raw samples, exact commands, fixture/output hashes and
logs are in the host's ignored notes/e30/: results.json, audio-tests.log,
linux-focused.log, linux-audio-tests.log, parity/results.json and build scripts.

An overlapping Windows parity/cancellation run also missed synthesis and encode
p95 (52.927 and 51.118 ms), retained in audio-tests-overlap.log. A first Linux run
passed all assertions but exited 1 because the generated XML was incorrectly
registered in a SoundFont leak-check list; fixture cleanup was corrected before
the final validation. Exploratory model/snapshot lifetime reductions did not
reliably meet Windows p95 (synthesis 52.617 and 51.346 ms), so those changes were
removed; they are absent from the implementation commit. Do not attribute a
measured speedup to those abandoned approaches.

## Verification (COMPILED / EXECUTED)

Both hosts compiled release engrave_output_test and engrave_output_probe with
--engrave_tests=y. Windows used native MSVC, Linux the native cachy toolchain.
Build exit codes: 0 on both. Original sources were restored after the discarded
experiments and rebuilt successfully; normalized SHA-256 manifests verified that
all tracked source files matched between hosts before this documentation update.

Executed Audio.*, AudioInternal.*, RuntimeAssets.SoundFont*: Windows exit 1,
23 passed, 2 skipped, dense test failed solely on synthesis p95; Linux exit 0,
24 passed, 2 skipped. Linux additionally ran the final dense + repeat70 focused
selection, exit 0, 2 passed. Skips: unavailable excerpt fixture and opt-in graph
timing test. The final restored-source Windows suite's tested executable SHA-256
was befb d7bf bafe 4c97 54d7 14a3 0d5a 3c67 0ebe 50fd 5432 c049 d1c1 0a2e 3947 878d
(spaces for readability); Linux focused executable:
a298ca00b032a7e921bec882acdcec111359288c7086316c882025ef738ca542.

Windows compared the complete output of all 22 host golden corpus fixtures in
WAV, FLAC, Opus, MP3 and AAC against an engrave-29-equivalent executable:
110/110 files byte-identical, no allowances and no golden changes. The e29
baseline checkout has no tree diff from engrave-29. Subsequent successful dense
renders match the baseline after every cancellation phase on both hosts.
No full engine gate ran. Parent owns both-host gates, independent review and pin.

## Remaining gaps (SOURCE-REVIEWED)

Engine::open delegates to load_native_score without internal cancellation
(api/engine.cpp:238, runtime/noqt_runtime.cpp:122). Audio score selection can reach
select_score -> ensure_layout -> Score::doLayout when the selected score has no
pages (score/score_excerpts.cpp:36,148,173). This relayout is unpolled internally;
laid-out documents do not take that branch. These paths were inspected, not
expanded or benchmarked here. Host preflight remains parent-owned.

First-use/changed-font parsing and service initialization, repeat-list/tempo cache
updates, single renderer operations, container copies, codec/sample decompression,
OS calls and teardown still have indivisible intervals. The outstanding measured
Windows synthesis p95 miss needs further work before accepting the all-phase
latency gate; late-phase measurements include cleanup and scheduling, and this
slice does not isolate their individual costs. The dense measurements do not
prove bounds for all scores, fonts, storage or competing workloads.
