# Audio Mix Fixtures

## `mix.mscz`

`mix.mscz` is an MSCZ fixture containing an embedded MuseScore 4 `audiosettings.json` file configuring default mixer settings for a score.

### Generation Method

1. **Base Score**: Created from `tests/data/inputs/golden/created.json` ("Native Evening / 原生晚曲"), which contains three instruments:
   - Part 1: Violin (midiProgram 41, trackId 0, instrumentId `violin`)
   - Part 2: Piano (midiProgram 1, trackId 1, instrumentId `piano`)
   - Part 3: Cello (midiProgram 43, trackId 2, instrumentId `baryton`)
2. **MSCZ Export**: Exported as an uncompressed/compressed MSCZ archive via native MCP `score_create` and `score_export` (`format: "mscz"`).
3. **Audio Settings Injection**: Added `audiosettings.json` at the root of the ZIP archive following the MuseScore 4 schema used by `ProjectAudioSettings` (`vendor/engrave/src/project/internal/projectaudiosettings.cpp`) and `NotationSoloMuteState` (`vendor/engrave/src/notation/internal/notationsolomutestate.cpp`):
   - Root `master` object containing `volumeDb: 0.0` and `balance: 0.0`.
   - `tracks` array containing entries for each part/instrument (`partId`, `instrumentId`, `out: { volumeDb, balance }`, and `soloMuteState: { mute, solo }`).
   - Cello (`partId: "3"`, `instrumentId: "baryton"`) is set to `soloMuteState: { "mute": true, "solo": false }`.

### Test Purpose

This fixture verifies that:
- `score_open` reads the desktop mixer settings (`audiosettings.json`) from the MSCZ container.
- `score_audio_tracks` and score metadata (`score_get`, `metadata.json`) report default mixer properties per track (`volumeDb`, `balance`, `solo`, `muted`, `forceMute`), showing Cello as `muted: true`.
- Plain audio export (`score_export` format `wav`) renders the score with Cello silent by default.
- Supplying an explicit export option `mix` overriding `muted: false` for Cello brings the Cello track back in the rendered audio.

## `mix_parts.mscz`

`mix_parts.mscz` is an MSCZ fixture with excerpts where both the full score and individual part excerpts carry their own `audiosettings.json` settings, matching MuseScore 4's per-notation mixer layout (`notationproject.cpp:247-259`).

### Generation Method

Generated using the helper script `artifacts/trim-tests/make_mix_parts_mscz.py`:
1. **Base Score**: Created from `tests/data/inputs/golden/created.json` ("Native Evening / 原生晚曲") with 3 parts (Violin, Piano, Cello).
2. **Excerpts Generation**: Generated via native MCP `score_generate_excerpts` and exported to MSCZ via `score_export`.
3. **Settings Injection**:
   - **Root `audiosettings.json`**: Configures master volume/balance (0.0 dB) and track output volume/balance (0.0 dB). Violin (`partId: "1"`, `instrumentId: "violin"`) is configured with `soloMuteState: { "mute": true, "solo": false }` (muted in full score). Piano and Cello are unmuted.
   - **`Excerpts/0_Violin/audiosettings.json`**: Configures Violin with `soloMuteState: { "mute": false, "solo": false }` (unmuted for the excerpt).
   - **`Excerpts/2_Cello/audiosettings.json`**: Configures Cello with `soloMuteState: { "mute": true, "solo": false }` (muted for the excerpt, while unmuted in root).

### Test Purpose

This fixture verifies that:
- Volume and balance are project-wide and inherited from root `audiosettings.json`.
- Part (excerpt) exports do not inherit the full score's solo/mute:
  - Exporting excerpt 0 (Violin, `excerptId: 0`) ignores the full score's mute on Violin and renders audible audio because its own excerpt `audiosettings.json` specifies `mute: false`.
  - Exporting excerpt 2 (Cello, `excerptId: 2`) applies the excerpt's own mute, rendering silent audio despite Cello being unmuted in the root score.

