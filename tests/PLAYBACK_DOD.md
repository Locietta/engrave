# Playback refactor harness

Windows host configuration (test-only capture is off by default):

```powershell
xmake f -P . -y -m release -o build/xmake-windows --engrave_tests=y --engrave_playback_harness=y
xmake build -P . -y -j 12 playback_probe playback_counting_probe playback_container_probe playback_dump_test
```

`playback_probe ASSETS FONT SCORE OUTDIR` writes legacy full MPE `A.txt`,
the flat-record projection `B.txt` with its independent legacy counterpart
`B_legacy.txt`, synth input `C.txt`, a FLOAT32 WAV, build metadata and phase
measurements. Options are `--rate 44100|48000`, `--no-repeats`, `--no-reverb`,
`--mute-part N`, and `--excerpt N`. Captures load the model with dual execution:
one score walk feeds the legacy map and the flat store. The probe fails when the two
projections differ; `--corrupt-flat` alters one flat record as a negative control,
and `--dual` enables dual execution in a `--measure` run.
Dump C v2 records each FluidSynth setup operation through the same arguments
used for the actual call, and each slice includes its ordered raw MIDI words.
All capture sites are behind `ENGRAVE_PLAYBACK_HARNESS`; normal builds retain
their original code paths. The serializer uses const legacy accessors, UTF-16
code units for MPE strings, original-width integers and hexadecimal float bits.

Run the plain integration script using the host Pixi Python:

```powershell
python vendor/engrave/tests/integration/test_playback_dod.py executables --exe OLD/playback_probe.exe --other-exe NEW/playback_probe.exe --assets .runtime/assets --font .runtime/soundfonts/FluidR3Mono_GM.sf3 --output notes/playback-comparison
python vendor/engrave/tests/integration/test_playback_dod.py compare --left DUMPS1 --right DUMPS2
python vendor/engrave/tests/integration/test_playback_dod.py measure --exe build/xmake-windows/playback_counting_probe.exe --assets .runtime/assets --font .runtime/soundfonts/FluidR3Mono_GM.sf3 --output notes/playback-counts --trials 7 --cancel-trials 20
```

`capture` accepts the same asset arguments and one `--exe`. `--match SUBSTRING`
selects cases for diagnosis and labels the result DIAGNOSTIC. Full comparisons
require exactly the names in `data/playback_dod/corpus.json`. `--rate 44100`
limits a smoke capture to one rate. Each capture starts fresh processes, runs both
rates, hashes only the WAV data chunk into `D.json`, pins font/profile hashes
and options, then removes the large WAV. Keep both directories and archived
executables. The comparator first requires identical input manifests, then
checks A/C/D bytes and reports the first differing byte offset with context.
Missing track records (A/C), total_frames (C), or provenance are rejected even
when both captures are equally incomplete. Optional `--expect-exe-sha256 OLD NEW`
checks each capture against its independently supplied executable hash. B is an internal
new-binary oracle, not an old/new byte comparison: the comparator requires a real
`B.txt` and `B_legacy.txt` from the right (new) capture and byte-equal projections,
and checks a left capture's B only when that executable wrote one. Corpus gaps are listed in
[the focused fixture README](data/playback_dod/README.md).

Use an **uncounted** `playback_probe --measure` run for performance numbers,
and the separate `playback_counting_probe` for allocations. `--measure` disables
A/C capture. Counting has additional overhead and must not supply the normal
wall-time baseline. Measurements cover the first export, including teardown,
after score import and resource bootstrap. Peak working set is process-wide.
Phase times follow OfflineTiming (nested/inclusive); allocation attribution
uses the innermost active timer (exclusive). In the test build, Windows MPE
release is additionally attributed to teardown. Unscoped work is `other`.

Allocation columns are `new delete malloc realloc free calloc aligned_new`.
`new` includes ordinary and aligned C++ allocations; ordinary new also calls
the hooked malloc. **Do not add new and malloc**. New allocation attempts are
`malloc + calloc + aligned_new`; report realloc attempts separately. The
Windows hook patches the executable's UCRT import slots, covering static
engine and codec objects, including amc. It does not intercept allocations
inside external DLLs or CRT-internal malloc calls. There is an executable
self-check before each counted export, and all replacement new/delete forms
are provided. Linux counting is deferred.

Cancellation phase numbers are 1=model, 2=sequencer, 3=synthesis, 4=encoding.
Model samples arm at poll `2000 + trial * 3000`, following the existing dense
regression test offsets; other phases arm at `1 + trial * 3`. `--phases 1`
selects model-only measurements. The chosen poll arms a request but returns false; the next positive poll
records observation. This includes intervening engine work without arbitrary
sleeps. Every trial is a fresh process. Raw samples retain request-to-observed,
observed-to-return and request-to-return; summaries use nearest-rank p50/p95/max.
These scheduled-poll samples do not model an asynchronous request during an
unpolled bulk stage. Existing dense asynchronous cancellation/recovery tests
remain part of the engine gate. K calibration requires step 2's batch-release
implementation and both platforms; no calibrated K is claimed here.
