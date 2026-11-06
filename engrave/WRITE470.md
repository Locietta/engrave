# MuseScore 4.70 Format Export Contract

`engrave` provides backward-compatibility score export for the MuseScore 4.70 format (interoperable with MuseScore 4.7.0 through 4.7.6). This document defines the compatibility contract, what is preserved, what is degraded with warnings, and what is refused.

## 1. Activation and API Contract

### Requesting 4.70 Export
Set `ExportOptions::native_version = "4.70"` for MSCX or MSCZ exports:

```cpp
struct ExportOptions {
    Format format = Format::MSCZ; // MSCX or MSCZ
    std::string native_version = "4.70"; // default is "5.00"
    ScoreSel score;                      // Master by default, or specific excerpt
    CaptureOptions capture;
};
```

Any version string other than `"5.00"` or `"4.70"`, or requesting a non-default version on other file formats (MusicXML, MIDI), returns `ErrorCode::UNSUPPORTED`.

### Warning Structure and Ordering Contract
Use `Document::export_score_with_warnings(ExportOptions)` to receive exported bytes and ordered compatibility warnings:

```cpp
struct ExportWarning {
    std::string code;
    std::string message;
    std::optional<ExportWarningObject> object; // { kind, id }
};

struct ExportOutput {
    std::vector<std::byte> bytes;
    std::vector<ExportWarning> warnings;
};
```

- **Warning Contract**: Each warning has a stable, frozen `code`, human-readable `message`, and an object identity `{ kind, id }`. Every B470 warning carries an object identity `{ kind, id }` (the normalizer in `write470.cpp:453` requires non-empty `kind` and `id`, and the unsupported-target path supplies `{ "request", "version:<requested value>" }`; warnings cannot be anonymous). Host schemas must require `{ kind, id }` for every B470 warning and describe both EIDs and non-EID locator/composite forms (`id` is not uniformly an EID). Codes and messages are frozen.
- **Ordering Contract**:
  - `Document::export_score_with_warnings` returns ordered `ExportWarning` values.
  - On a successful export, warnings are not purely in document encounter order: `convert_degradations` first collects serialized degradations (`B470_SHARED_PART_DROPPED`, `B470_STAVE_SHARING_LABEL_DROPPED`, `B470_PAGE_LOCK_DROPPED`, `B470_GROUP_BRACKET_DROPPED`) during a pre-order traversal of the serialized score XML, followed by a separate pass over source scores and parts (`write470.cpp:643`) that appends instrument group-name warnings (`B470_INSTRUMENT_GROUP_NAME_DROPPED`).
  - On a refused export that reaches post-serialization checks, degraded warnings are prepended before unmapped feature warnings (`degraded` followed by `unsupported`). However, an invalid tie returns early (`write470.cpp:599`) before degradation and style scans run, so only `B470_INVALID_TIE` is reported and no degradation warnings precede it in that case.
  - Identical warning objects are deduplicated preserving the order of their first occurrence.
  - Non-EID warning locators retain document-wide score indices (master score 0, followed by excerpts in source order) even when exporting an excerpt.

### Fail-Closed Behavior
- **Byte-only Export**: Calling `Document::export_score(ExportOptions)` fails closed: if the export produces any compatibility warnings, the method returns `ErrorCode::UNSUPPORTED` and attaches the warnings to `Error::export_warnings`. This ensures callers cannot silently discard compatibility degradation.
- **Refused Export**: If an export is refused due to incompatible features, no bytes are produced (`bytes` is empty), and `Error::export_warnings` reports blocking elements. Refusal warnings are not exhaustive: an invalid tie returns early (`write470.cpp:599`) before degradation and style scans run, so only `B470_INVALID_TIE` is reported, and other blockers appear only after the invalid tie is fixed.

## 2. Bounded Supported Subset

The initial 4.70 exporter is intentionally small and fails closed. It accepts only an independently checked, bounded subset of 5.00 notation and structures; everything else is refused (`ErrorCode::UNSUPPORTED`).

The guard in `engrave/src/score/write470.cpp:376` accepts only:
- **Instruments**: Accepts only `piano` or `flute` instrument IDs (flute requires bounded pitch ranges `minPitchP=59`, `maxPitchP=98`, `minPitchA=60`, `maxPitchA=93`). All other instrument IDs and tokens are refused.
- **Staves & Clefs**: Standard pitched staves (`group="pitched"`, `name="stdNormal"`), and only G clefs (`concertClefType="G"`, `transposingClefType="G"`). Bass clef and all other clef types are refused.
- **Time & Key Signatures**: Only 4/4 time signature (`sigN="4"`, `sigD="4"`) and C major key signature (`concertKey="0"`). All other meters and key signatures are refused.
- **Notes, Rests & Durations**: Pitched notes, half rests, and whole, half, and quarter durations (`whole`, `half`, `quarter`). Eighth durations (`eighth`) and the specified grace forms (`appoggiatura` and `grace8after`) are accepted only for standalone excerpts; everything else is refused.
- **Ties & Spanners**: Same-voice, same-measure ties (including paired pitch edits). Note spanners must be `Tie`; non-note spanners must be `Pedal` matching the strictly checked subset (`write470.cpp:321`): no XML attributes, only permitted child fields (`eid`, `track`, `track2`, `startTick`, `ticks`, and optional `continueTextPlace`/`endTextPlace` which must be `"auto"`), track 0 (`track="0"`, `track2="0"`), staff 1, start tick 0 (`startTick="0/1"`), duration 960 ticks (`ticks="1/2"`), spanning from half-measure position 0 to 2 in staff 1 measure voice. All other pedal spanners, spanner properties, custom chord lists, and non-generated automation curves are refused.
- **Braces**: Ordinary score braces (`bracket type="1"`).
- **Identities**: Serialized musical, staff, and score EIDs supported by the bounded mapping, plus legacy numeric part IDs. MuseScore 4.70 has no part EID representation; part EIDs are omitted.
- **Excerpt Selections**: An excerpt selected via `ExportOptions::score` (such as the verified linked Flute excerpt) exports as a standalone 4.70 score with its own parts, style, and linked notation elements preserved.

## 3. Degraded Features (Exported with Warnings)

Five feature families are converted into compatible 4.70 structures while emitting structured warnings:

| Code | Exact Frozen Message | Object Kind & Source ID |
|---|---|---|
| `B470_SHARED_PART_DROPPED` | Shared-part mapping was removed; its staves and content were retained as ordinary parts. | `kind: "shared_part"`, `id: <SharedPart-EID>` (including disabled mappings) |
| `B470_STAVE_SHARING_LABEL_DROPPED` | Stave-sharing label was removed. | `kind: "stave_sharing_label"`, `id: <label-EID>` |
| `B470_PAGE_LOCK_DROPPED` | Page lock was removed. | `kind: "page_lock"`, `id: "<start-measure-EID>..<end-measure-EID>"` |
| `B470_GROUP_BRACKET_DROPPED` | Group bracket was removed. | `kind: "group_bracket"`, `id: "<start-staff-EID>:<zero-based-column>"` |
| `B470_INSTRUMENT_GROUP_NAME_DROPPED` | Instrument group-name override was removed. | `kind: "part"`, `id: <part-EID>` (one warning for multiple override fields) |

## 4. Refused Features

The exporter strictly refuses output (`ErrorCode::UNSUPPORTED`) when encountering constructs that cannot be faithfully represented or downgraded in 4.70:

| Code | Exact Frozen Message | Object Kind & Source ID |
|---|---|---|
| `B470_INVALID_TIE` | Cannot write 4.70: tied notes have different pitches. | `kind: "tie"` with tie EID; if tie has no EID, uses the owner's actual kind (e.g. `kind: "note"`) and owner EID; falls back to `kind: "tie"` with deterministic locator; refuses output |
| `B470_UNSUPPORTED_FEATURE` | Cannot write 4.70: the source feature has no defined downgrade. | Owning source object kind and EID; refuses output |
| `B470_UNSUPPORTED_TARGET` | Cannot write the requested score version. | `kind: "request"`, `id: "version:<requested value>"`; refuses output |

### Tie Warning Identities and Locators
Per `engrave/src/score/write470.cpp:581`, an invalid-tie warning never stringifies an invalid-EID sentinel:
1. If the tie has a valid EID, it uses `kind: "tie"` and the tie EID.
2. If the tie lacks an EID, it traverses to the nearest addressable ancestor with a valid EID and uses the owner's actual lowercase type name (e.g. `kind: "note"`) and that owner's EID.
3. If no addressable owner exists, `kind: "tie"` uses the explicitly non-EID deterministic locator:
```text
locator:score:<s>/measure:<m>/tick:<t>/track:<k>/note:<n>/tieFor
```
Score, measure, and note indices are zero-based in document, measure, and chord order (the locator's note index `<n>` is within its chord); `<t>` is the absolute engine tick (480 per quarter); and `<k>` is the engine staff/voice track index. The master is score zero, followed by excerpts in source order. This source-revision locator is deterministic and is not a query handle. Only the complete numeric tie-locator shape above is accepted (all five components and the final `tieFor` field are required).

## 5. Style Compatibility Guard

Style properties are guarded against the verified 4.70 style catalogue (`engrave/src/score/style470_catalog.inc`):
- Sets `defaultsVersion` to `470`.
- Omits neutral 5.00-only style fields (`windsNameByGroup`, `vocalsNameByGroup`, and default values for `lyricsStackingOrder`, `maskSlurs`, `maskTies`).
- Preserves legacy header/footer options (`headerFirstPage`, empty header/footer text, and `$C` center-footer macros in excerpts).
- Any unrecognized or modified style value outside the catalogue causes immediate export refusal.

## 6. Cooperative Cancellation

During 4.70 XML conversion, serialization progress is checked against `CaptureOptions::cancelled` every <= 4 KiB:
- An observed cancellation aborts serialization immediately with `ErrorCode::CANCELLED`.
- No output file or partial XML bytes are published.
- Working buffer budgets (`CaptureOptions::max_working_bytes`) and final byte budgets (`CaptureOptions::max_bytes`) remain strictly enforced throughout the conversion.
