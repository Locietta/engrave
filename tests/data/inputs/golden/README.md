# Golden fixtures

Inputs for `tests/support/golden.py` and `tests/integration/test_roundtrip.py`. Copied verbatim
from MuseScore test data (GPL-3.0, same license as the engine) so the goldens do
not depend on the vendored source path.

| File | Source |
| --- | --- |
| `created.json` | `score_create` input (same score as `tests/integration/test_mcp_smoke.py`) |
| `repeat_with_2_voltas.mscx` | webmscore-4.7.5-1 `src/engraving/tests/playback/playbackmodel_data/repeat_with_2_voltas/repeat_with_2_voltas.mscx` |
| `repeat70.mscx` | MuseScore v4.7.6 `src/engraving/tests/repeat_data/repeat70.mscx` (open volta after exhausted repeats) |
| `pedal.mscx` | webmscore-4.7.5-1 `src/importexport/mei/tests/data/pedal-01.mscx`, first Pedal given user `placement`/`offset`/`textSizeIsSpatiumDependent` overrides (v4.7.6 writes these, PR #34917) |
| `hairpins_and_repeats.mscx` | webmscore-4.7.5-1 `src/engraving/tests/playback/playbackcontext_data/dynamics/hairpins_and_repeats.mscx` |
| custom_part_name.mscx | webmscore-4.7.5-1 src/engraving/tests/playback/playbackcontext_data/dynamics/hairpins_and_repeats.mscx with Part <trackName> changed to Solo Klavier (differing from instrument <longName>Klavier</longName>) |
| `linked_parts.mscx` | webmscore-4.7.5-1 `src/engraving/tests/parts_data/linked-ties-parts.mscx` (excerpts) |
| `lyrics.mscx` | webmscore-4.7.5-1 `vtest/scores/lyrics-1.mscx` |
| `slurs.mscz` | webmscore-4.7.5-1 `vtest/scores/slurs-11.mscz` |
| `drums_import.musicxml` | webmscore-4.7.5-1 `src/importexport/musicxml/tests/data/testImportDrums.xml` |
| `perc_stem_dir.musicxml` | MuseScore v4.7.6 `src/importexport/musicxml/tests/data/testInferredPercStemDir.xml` |
| `bend.gp` | webmscore-4.7.5-1 `src/importexport/guitarpro/tests/data/bend.gp` |
| `bends.mid` | webmscore-4.7.5-1 `src/importexport/midi/tests/midiexport_data/testBends1-ref.mid` |
| `layout_elements.mscx` | webmscore-4.7.5-1 `src/engraving/tests/all_elements_data/layout_elements.mscx` (one of nearly every element type, 3.01 format) |
| `coverage.musicxml` | hand-written: pickup measure, triplets, slashed grace, tie across a barline, ottava, clef changes, Fine / D.C. al Fine |
| `repeat71.mscx`, `repeat72.mscx` | MuseScore v4.7.6 `src/engraving/tests/repeat_data/` (volta playback cases) |
| `named_noteheads_perc.musicxml` | MuseScore v4.7.6 `src/importexport/musicxml/tests/data/testNamedNoteheadsPercussion.xml` |
| `tab_grace_prebend.mscx` | MuseScore v4.7.6 `src/importexport/guitarpro/tests/guitarbendimporter_data/split_durations/prebend+bend-gp.mscx` (4.50 file, linked tab staff, grace notes, parentheses) |
| `ghost_note.mscx` | MuseScore v4.7.6 `src/engraving/tests/midi/midirenderer_data/ghost_note.mscx` (4.10 tab staff, ghost notes) |

`../evening.musicxml` is also part of the golden set.

`chordline_path.mscx`: MuseScore v4.7.6 `src/engraving/tests/parts_data/part-chordline.mscx` with a hand-edited ChordLine `<Path>`; golden.py generates its part excerpts (v4.7.6 copies the edited path into parts).

Native fixtures were upgraded from their original committed bytes using engrave-4 (tree d8c21df149). MSCZ inputs retain complete archives; MSCX inputs embed engine-produced master Style and excerpt Score/Style blocks. The original files and conversion ledger remain available in Git history.
