# Playback identity corpus

Regenerate the focused MusicXML with `tests/integration/test_playback_dod.py generate`.
`cold_preset.mscx` is hand-written (MuseScore 4.6 format): two sound flags carry distinctive
presets (one non-ASCII), a playing technique and a lyric share the first chord's timestamp,
so the cold-event buffer sees presets, text and syllables in one group.
The shared `engrave_testing.playback_corpus` module also inventories all 26 engine
audio sources and all 78 playback MSCX sources. Dense (49,152 notes) and one-track
inputs are generated into each capture directory instead of committing megabytes.
Every case runs at 44.1 and 48 kHz. Additional variants disable repeats/reverb,
mute a track, and select a generated excerpt.

The MusicXML inputs test the imported result, not the spelling of a marking.
In particular, technique words are only requests to the importer. Do not infer
multi-articulation coverage from the filenames: inspect dump A before using a
case to approve step 1's semantic differences.

Known focused coverage gaps needing constructed MPE or native-score fixtures:

- Multi-mapped harmonic plus distortion and mute plus pizzicato are not proven
  by imported words. Existing native technique fixtures provide ordinary cases.
- Mixed before/after grace types *inside multibends*; the native guitar-bend
  fixture does not establish this specific interaction.
- Empty/nonempty NATURAL multi-articulation fallback in both single-note dynamics
  modes; custom-profile unit tests remain separate from default-profile exports.
- Precisely colliding overlapping bend reset/sample times.
- Channel exhaustion: the 20-staff candidate imports with single-note dynamics
  disabled and uses channel 0, so it does not exhaust the budget.
- Same-program insertion before an existing flat-map element and at a capacity
  boundary (no flat-map implementation exists yet); program override.
- Multi-part sound flags specifically combined with this focused set.
- The MusicXML swing candidate currently produces straight 250 ms nominal note
  spacing in dump A; native swing behavior is not established by this candidate.
- Negative-start, infinite-duration and exact-chunk-boundary synthetic events.
- The pedal spelling does not prove overlapping sostenuto. Verify native fixture
  payloads or add constructed MPE coverage before relying on that interaction.

These are explicit gaps, not assertions that MusicXML preserved the requested
internal event structure.
