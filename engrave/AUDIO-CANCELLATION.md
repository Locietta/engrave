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


# M2.3 dense-score cancellation margin

2026-11-02. Base `0a2aa52ac9ab345a5d30f961ad97df7c27293a92`
(the supplied engrave-30 candidate SHA, not a merge commit). Implementation
`91b971cabd1a78af94c1c2bd85837793ea032ae5`; publication follow-up
`46b0d74bfa2b000c2ad8c147e3bc7e7e1b01e70f`. Branch
`feat/dense-audio-cancel`, existing worktree e30. No tag, pin or push.

## Latency source and implementation (SOURCE-REVIEWED / EXECUTED)

The old synthesis request is raised during an owner-thread MIDI callback, so it
already measures almost entirely work after detection, rather than the time to
reach the next 256-frame copy. An instrumented baseline run reproduced Windows
synthesis p95 48.665 ms / max 50.909 and encode 50.576 / 56.847. Its late-phase
graph teardown was typically 11-13 ms; total cancellation was roughly 41 ms at
p50. The remaining model lifetime ends after graph destruction. Source review
shows that origin events use reference-counted SharedMaps: dropping intermediate
snapshots does not eliminate their final destruction.

The driver now moves the model's origin-event stores into four batches after
all graph references are destroyed, clears three on joined threads and one on
the owner, then completes ordinary model destruction. This applies on Windows
only, when at least 16,384 event timestamps and two tracks exist. Only value-owned
MPE event stores move to workers. Score/listener access, synth and SoundFont
teardown, codecs, callbacks, file closing and removal stay on the owner thread.
Workers join before return, including callback-exception unwinding. Thread
creation failure falls back to synchronous cleanup for unstarted batches.
There is no retained graph or deferred cleanup, and no added FluidSynth reset.

An exploratory four-thread Linux cleanup regressed before-synthesis p95 to
57.175 ms and synthesis to 64.910 ms, consistent with contention freeing
allocations from one glibc arena. That variant is disabled on Linux; its final
measurements remain near the e30 baseline, rather than improving proportionally.
These discarded results are not acceptance evidence.

Polls now sit before FluidSynth processing, before each sequence, between MIDI
events, before tuning, and at every 64-frame sink copy (one FluidSynth internal
buffer). Sequence collection polls at each timestamp and each copied/moved
event. The mixer exits immediately after a cancelled track and polls between
mixing tracks. Existing construction, preset sample-load, encoder and 4 KiB
file-write polls remain. Public publication additionally polls before each
1 MiB encoded-file read/sink delivery. Event order, tuning, internal PCM cursor,
4096-frame outer DSP blocks and codec input blocks are unchanged.

No production render worker join was identified in the offline driver before
this change. FluidSynth's destructor can join lazy SoundFont-unload timers, but
the normal render path does not explicitly unload fonts into those timers.
Its ordinary voice/preset cleanup and graph destruction are still synchronous.
OS scheduling, closing/removal and native destruction have no universal bound.

## Dense distributions (EXECUTED)

Release builds, 50 trials per phase, timing instrumentation disabled. Same
12-part x 256-measure x 16-note test and request points as M2.2. Nearest-rank
p50 / p95 / maximum in milliseconds. Tests now assert p95 <=30 and max <=50.

| Phase | Windows | Linux, physical cachy |
|---|---|---|
| before-synthesis | 11.384 / 13.677 / 14.373 | 10.220 / 10.633 / 10.696 |
| playback-model | 5.675 / 7.883 / 8.353 | 4.007 / 7.457 / 8.186 |
| synthesis | 23.314 / 27.388 / 30.959 | 16.589 / 17.029 / 18.405 |
| encode | 23.098 / 25.672 / 27.704 | 16.651 / 16.879 / 18.368 |

All four phases pass both stricter limits on both hosts. Windows synthesis's
e30 p95 failure is gone. Detection-to-return includes graph/model cleanup,
thread joins and removal; model-construction trials include flag-to-detection.
These remain engine phase probes, not host notification-to-response timings.
Every phase checks Cancelled, absent output and byte-identical recovery. Dense
callbacks now assert the owner thread. The public construction cancellation and
throwing-callback checks still require zero sink calls and no intermediate.

## Validation (COMPILED / EXECUTED)

Both hosts compiled release engrave_output_test and engrave_output_probe with
--engrave_tests=y. Windows: native MSVC; Linux: native cachy conda GCC, no WSL.
Both ran Audio.*, AudioInternal.*, RuntimeAssets.SoundFont*: 24 passed, 2 skipped.
Skips are the unavailable excerpt fixture and opt-in graph timing test.
The repeat70 50-trial cancellation benchmark also passes in those suites.

The subsequent publication-only production change was checked on both hosts
with Audio.* plus CancellationDuringPublicationCleansUpAndRecovers: 18 passed,
1 skipped each. That new test cancels after the first sink chunk, requires
Cancelled and exactly one delivered chunk, checks that this render's intermediate
is absent, then requires byte-identical recovery. The four dense distributions
were measured at 91b971cabd; the follow-up only adds publication polling after
encoded rendering and leaves their measured internal paths unchanged.

Windows's final probe compared all 22 golden corpus inputs in WAV, FLAC, Opus,
MP3 and AAC against captured e30 candidate files: **110/110 byte-identical**.
The script verifies recorded input and baseline-output SHA-256 values first.
Explicit allowed differences: **none**. No golden files changed. The e30 files
also matched the e29-equivalent baseline in M2.2. Both hosts' dense recovery
renders match their own uncancelled baseline. No dense-test-named temporary files
remained on either host after the distributions.

An initial Windows build overlapped the baseline timing process, causing a
locked test executable and an incomplete probe; after rebuilding the probe the
final original suites passed. The publication test initially checked all old
cache entries rather than its own intermediate; its corrected check uses this
process and render index. Failed exploratory logs are kept separate.

Raw samples, commands, build/test logs, binary hashes and hash-pinned parity
rows are in host notes/e31/ (results.json and parity/results.json). The original
suite's Windows binary SHA-256 was
47d37cfbd4d6a4ae66c79b725dbc68166bda2eec828709d944333c231a1d993e;
Linux: 0d1eaa715fed9d62f2559780bd3c159cdd9fe03278dacfb42747e35878358d75.
Final publication-test binaries and probes are recorded in results.json.

## Remaining unpolled intervals (SOURCE-REVIEWED)

Engine::open -> load_native_score -> finish_load still imports, repairs and
lays out without internal cancellation. The host audio worker explicitly calls
Engine::open on an MSCZ before render_audio (native/worker_v2.cpp:235), so this
interval IS reachable in an audio export job, before the audio callback scope.
The normal loaded master has already been laid out.

Audio rendering also calls engine_audio_tracks -> music::select_score ->
ensure_layout -> Score::doLayout when the selected score has no pages. That
conditional relayout IS reachable from the render path, including a selected
unlaid-out excerpt; ordinary already-laid-out scores skip it. Neither import
nor layout is covered by these four phase distributions or changed here.

First-use/changed-font parsing and service setup, repeat/tempo cache updates,
individual playback render operations, automation resampling within a layer,
container allocations/copies, native codec/sample decompression calls, a single
64-frame FluidSynth render, file operations, individual sink calls and final
cleanup remain indivisible or unpolled internally. Several are reachable on the
audio path; the fixed dense/font tests do not establish arbitrary-input bounds.
Publication now checks between sink chunks; it cannot interrupt a running sink.

No full engine gate ran. Parent owns independent review, both-host engine/host
gates, tag and pin acceptance.
