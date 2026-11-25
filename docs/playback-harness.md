# Playback Refactor Harness

This document describes the opt-in playback capture, identity-comparison,
timing, and allocation-counting probes behind `ENGRAVE_PLAYBACK_HARNESS` (off by
default in normal builds). For the pipeline architecture itself, see
[playback.md](playback.md).

## Building the Harness Targets

From an enclosing `engrave-mcp` checkout:

```powershell
pixi run --locked python scripts/bbuild.py --build-directory build/playback --playback-harness build -j 12 playback_probe playback_counting_probe playback_container_probe playback_dump_test
```

From a standalone `engrave` checkout:

```bash
bazelisk build --config=release --//bazel:playback_harness=true //:playback_probe //:playback_counting_probe //:playback_container_probe //:playback_dump_test
```

All capture sites are gated by `ENGRAVE_PLAYBACK_HARNESS`; normal builds keep
their uninstrumented code paths.

## Probe Outputs (`playback_probe`)

`playback_probe ASSETS FONT SCORE OUTDIR` writes:

- **`A.txt` (Dump A)**: Full MPE dump in `legacy-mpe-v1` format emitted from the
  flat store (`PlaybackScoreBuilder`). Each `NoteEvent` is serialized when
  frozen, and groups are rebuilt from records, cold events, and empty-group
  marks. Serialization uses const legacy accessors, UTF-16 code units for MPE
  strings, original-width integers, and hexadecimal float bits.
- **`B.txt` (Dump B)**: Flat-record projection of the track setup, dynamics,
  group order, and event fields, including hexadecimal float bits.
- **`C.txt` (Dump C v2)**: FluidSynth setup calls (using the exact call
  arguments) and ordered raw MIDI event slices.
- **WAV + metadata**: A FLOAT32 WAV, build metadata, and phase measurements.

Probe options: `--rate 44100|48000`, `--no-repeats`, `--no-reverb`,
`--mute-part N`, `--excerpt N`, `--measure`, `--cancel-phase 1..5`,
`--cancel-poll N`, `--next SCORE OUTDIR`.

## Running Comparisons and Measurements

Run `tests/integration/test_playback_dod.py` directly (from `engrave-mcp`, using
Pixi Python):

```powershell
pixi run python vendor/engrave/tests/integration/test_playback_dod.py executables --exe OLD/playback_probe.exe --other-exe NEW/playback_probe.exe --assets .runtime/assets --font .runtime/soundfonts/FluidR3Mono_GM.sf3 --output notes/runs/playback-comparison
pixi run python vendor/engrave/tests/integration/test_playback_dod.py compare --left DUMPS1 --right DUMPS2
pixi run python vendor/engrave/tests/integration/test_playback_dod.py measure --exe build/playback/playback_probe.exe --assets .runtime/assets --font .runtime/soundfonts/FluidR3Mono_GM.sf3 --output notes/runs/playback-measurements --trials 7 --cancel-trials 20
pixi run python vendor/engrave/tests/integration/test_playback_dod.py measure --exe build/playback/playback_counting_probe.exe --assets .runtime/assets --font .runtime/soundfonts/FluidR3Mono_GM.sf3 --output notes/runs/playback-counts --phases 0 --trials 7
```

- `capture` takes the same asset arguments and a single `--exe`. `--match SUBSTRING`
  selects diagnostic cases and marks the output `DIAGNOSTIC`.
- Full comparisons require the exact corpus in
  `tests/data/playback_dod/corpus.json` (corpus gaps are noted in
  [tests/data/playback_dod/README.md](../tests/data/playback_dod/README.md)).
  `--rate 44100` limits a smoke capture to one sample rate.
- Each capture runs fresh processes across both rates, hashes only the WAV data
  chunk into `D.json`, pins font/profile hashes and options, and deletes the
  large WAV.
- The comparator checks identical input manifests first, then verifies A/C/D
  bytes (reporting the first differing byte offset with context). Missing track
  records (A/C), `total_frames` (C), or provenance fail even if both captures
  omit them. Optional `--expect-exe-sha256 OLD NEW` checks each capture against
  an expected executable hash.
- The comparator validates Dump B against Dump A's inventory and compares
  complete projections byte for byte. The older left capture may contain
  the original deferred-projection marker.

## Timing vs. Allocation Measurements

- Use the **uncounted** `playback_probe --measure` for wall-time baselines and
  the separate `playback_counting_probe` for allocations. `--measure` disables
  A/B/C capture; allocation hooking adds overhead and must not supply normal
  wall-time baselines.
- Measurements cover the first export (including teardown) after score import
  and resource bootstrap. Peak working set is process-wide.
- Phase times follow `OfflineTiming` (nested/inclusive); allocation attribution
  uses the innermost active timer (exclusive). Unscoped work is attributed to
  `other`. MPE release runs inside `tracks` during setup, and `teardown` covers
  `OfflineRenderSession` destruction.

### Allocation Columns (Windows)

Columns are `new delete malloc realloc free calloc aligned_new`:

- `new` includes ordinary and aligned C++ allocations; ordinary `new` also calls
  hooked `malloc`. **Do not add `new` and `malloc`.**
- New allocation attempts equal `malloc + calloc + aligned_new`; report
  `realloc` attempts separately.
- The Windows hook patches the executable's UCRT import slots (covering static
  engine and codec objects, including `amc`) and runs a self-check before each
  counted export with all replacement `new`/`delete` forms provided. It does not
  intercept allocations inside external DLLs or CRT-internal `malloc` calls.
  Linux counting is deferred.

## Scheduled-Poll Cancellation Sampling

Cancellation phase numbers are `1=model`, `2=sequencer`, `3=synthesis`,
`4=encoding`, `5=event_release` (`--phases 1` selects model-only measurements):

- Model samples arm at poll `2000 + trial * 3000` (matching the dense regression
  test offsets); other phases arm at `1 + trial * 3`.
- The chosen poll arms a request while returning `false`; the next positive poll
  records observation, capturing intervening engine work without arbitrary
  sleeps.
- Every trial runs in a fresh process. Raw samples record request-to-observed,
  observed-to-return, and request-to-return; summaries use nearest-rank
  p50/p95/max.
- Scheduled-poll samples do not model asynchronous cancellation during unpolled
  bulk stages; existing dense asynchronous cancellation/recovery tests remain
  part of the engine gate.
