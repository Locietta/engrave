# Audio Mix Fixtures

## `mix.mscz`

`mix.mscz` is an MSCZ fixture containing an embedded MuseScore 4 `audiosettings.json` file configuring default mixer settings for a score.

### Generation Method

1. **Base Score**: Created from `tests/data/inputs/golden/created.json` ("Native Evening / 原生晚曲"), which contains three instruments:
   - Part 1: Violin (midiProgram 41, trackId 0, instrumentId `violin`)
   - Part 2: Piano (midiProgram 1, trackId 1, instrumentId `piano`)
   - Part 3: Cello (midiProgram 43, trackId 2, instrumentId `baryton`)
2. **MSCZ Export**: Exported as an uncompressed/compressed MSCZ archive via native MCP `score_create` and `score_export` (`format: "mscz"`).
3. **Audio Settings Injection**: Added `audiosettings.json` at the root of the ZIP archive following the MuseScore 4 schema:
   - Root `master` object containing `volumeDb: 0.0` and `balance: 0.0`.
   - `tracks` array containing entries for each part/instrument (`partId`, `instrumentId`, `out: { volumeDb, balance }`, and `soloMuteState: { mute, solo }`).
   - Cello (`partId: "3"`, `instrumentId: "baryton"`) is set to `soloMuteState: { "mute": true, "solo": false }`.

### Test Purpose

This fixture verifies that:

- `score_open` reads the desktop mixer settings (`audiosettings.json`) from the MSCZ container.
- `score_query` (`mode: "audio_tracks"`) reports default mixer properties per track (`mix: { volume_db, balance, solo, mute }`, alongside `track`, `part_id`, `instrument_id`, `part_name`, `instrument_name`, and `chord_symbols`) and master defaults (`master: { volume_db, balance, mute, solo }`), showing Cello with `mix.mute: true`.
- Plain audio export (`score_export` with `format: "wav"`) renders the score with Cello silent by default.
- Supplying explicit `options.tracks` overrides (for example `{"track": <cello_track>, "mute": false}`) brings the Cello track back in the rendered audio.

## `mix_parts.mscz`

`mix_parts.mscz` is an MSCZ fixture with excerpts where both the full score and individual part excerpts carry their own `audiosettings.json` settings, matching MuseScore 4's per-notation mixer layout.

### Generation Method

1. **Base Score**: Created from `tests/data/inputs/golden/created.json` ("Native Evening / 原生晚曲") with 3 parts (Violin, Piano, Cello).
2. **Excerpts Generation**: Generated via native MCP `score_edit` (`op: "generate_excerpts"`) and exported to MSCZ via `score_export`.
3. **Settings Injection**:
   - **Root `audiosettings.json`**: Configures master volume/balance (0.0 dB) and track output volume/balance (0.0 dB). Violin (`partId: "1"`, `instrumentId: "violin"`) is configured with `soloMuteState: { "mute": true, "solo": false }` (muted in full score). Piano and Cello are unmuted.
   - **`Excerpts/0_Violin/audiosettings.json`**: Configures Violin with `soloMuteState: { "mute": false, "solo": false }` (unmuted for the excerpt).
   - **`Excerpts/2_Cello/audiosettings.json`**: Configures Cello with `soloMuteState: { "mute": true, "solo": false }` (muted for the excerpt, while unmuted in root).

### Test Purpose

This fixture verifies that:

- Volume and balance are project-wide and inherited from root `audiosettings.json`.
- Part (excerpt) exports do not inherit the full score's solo/mute:
  - Selecting the Violin excerpt by its excerpt EID (via `excerpt` in `score_query` or `score_export`) ignores the full score's mute on Violin and renders audible audio because `Excerpts/0_Violin/audiosettings.json` specifies `mute: false`.
  - Selecting the Cello excerpt by its excerpt EID applies `Excerpts/2_Cello/audiosettings.json`, rendering silent audio despite Cello being unmuted in the root score.
