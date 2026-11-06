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

Any version string other than `"5.00"` or `"4.70"`, or requesting a non-default version on other file formats (MusicXML, MIDI), returns `ErrorCode::Unsupported`.

### Warning Structure
Use `Document::export_score_with_warnings(ExportOptions)` to receive exported bytes and compatibility warnings:

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

- Each warning has a stable, frozen `code` and descriptive `message`.
- `ExportWarning::object` identifies the source element with `kind` and `id` strings (which may be an Element ID or a deterministic locator).

### Fail-Closed Behavior
- **Byte-only Export**: Calling `Document::export_score(ExportOptions)` fails closed: if the export produces any compatibility warnings, the method returns `ErrorCode::Unsupported` and attaches the warnings to `Error::export_warnings`. This ensures callers cannot silently discard compatibility degradation.
- **Refused Export**: If an export is refused due to incompatible features, no bytes are produced (`bytes` is empty), and `Error::export_warnings` details all blocking elements.

## 2. Bounded Supported Subset

The initial 4.70 exporter is intentionally small and fails closed. It accepts only an independently checked, bounded subset of 5.00 notation and structures; everything else is refused (`ErrorCode::UNSUPPORTED`).

The guard in `engrave/src/score/write470.cpp:376` accepts only:
- **Instruments**: Accepts only `piano` or `flute` instrument IDs (flute requires bounded pitch ranges `minPitchP=59`, `maxPitchP=98`, `minPitchA=60`, `maxPitchA=93`). All other instrument IDs and tokens are refused.
- **Staves & Clefs**: Standard pitched staves (`group="pitched"`, `name="stdNormal"`), and only G clefs (`concertClefType="G"`, `transposingClefType="G"`). Bass clef and all other clef types are refused.
- **Time & Key Signatures**: Only 4/4 time signature (`sigN="4"`, `sigD="4"`) and C major key signature (`concertKey="0"`). All other meters and key signatures are refused.
- **Notes, Rests & Durations**: Pitched notes, half rests, and whole, half, and quarter durations (`whole`, `half`, `quarter`). Eighth durations (`eighth`) and the specified grace forms (`appoggiatura` and `grace8after`) are accepted only for standalone excerpts; everything else is refused.
- **Ties & Spanners**: Same-voice, same-measure ties (including paired pitch edits). Note spanners must be `Tie`; non-note spanners must be `Pedal` (track-0 pedal markings). Custom chord lists and non-generated automation curves are refused.
- **Braces**: Ordinary score braces (`bracket type="1"`).
- **Identities**: Serialized musical, staff, and score EIDs supported by the bounded mapping, plus legacy numeric part IDs. MuseScore 4.70 has no part EID representation; part EIDs are omitted.
- **Excerpt Selections**: An excerpt selected via `ExportOptions::score` (such as the verified linked Flute excerpt) exports as a standalone 4.70 score with its own parts, style, and linked notation elements preserved.

## 3. Degraded Features (Exported with Warnings)

Five feature families are converted into compatible 4.70 structures while emitting structured warnings:

| Warning Code | Condition and Degradation | Object Kind & Source ID |
|---|---|---|
| `B470_SHARED_PART_DROPPED` | Shared-part mapping is converted to an ordinary part. Staves and notation content are preserved; shared part references are removed. | `kind: "shared_part"`, `id: <SharedPart-EID>` |
| `B470_STAVE_SHARING_LABEL_DROPPED` | Stave-sharing labels are removed from exported staves. | `kind: "stave_sharing_label"`, `id: <label-EID>` |
| `B470_PAGE_LOCK_DROPPED` | Page lock ranges are removed. | `kind: "page_lock"`, `id: "<start-measure-EID>..<end-measure-EID>"` |
| `B470_GROUP_BRACKET_DROPPED` | Group brackets are removed; standard score braces are retained. | `kind: "group_bracket"`, `id: "<start-staff-EID>:<column>"` |
| `B470_INSTRUMENT_GROUP_NAME_DROPPED` | Enabled wind/vocal group naming overrides are removed per owning part; ordinary long/short instrument names are retained. | `kind: "part"`, `id: <part-EID>` |

## 4. Refused Features

The exporter strictly refuses output (`ErrorCode::Unsupported`) when encountering constructs that cannot be faithfully represented or downgraded in 4.70:

| Warning Code | Condition | Object Kind & Source ID |
|---|---|---|
| `B470_INVALID_TIE` | Tied notes have unequal pitches. | `kind: "tie"`, `id: <tie-EID>` (or start note EID, ancestor EID, or deterministic locator) |
| `B470_UNSUPPORTED_FEATURE` | Constructs with no defined 4.70 downgrade (e.g. non-generated automation, custom chord lists, unsupported spanner types). | Owning source object kind and ID |
| `B470_UNSUPPORTED_TARGET` | Requested version is not `"5.00"` or `"4.70"`. | `kind: "request"`, `id: "version:<requested>"` |

### Tie Locators for Unaddressable Ties
When an invalid tie lacks a direct EID or addressable ancestor, `id` uses the deterministic non-EID locator:
```text
locator:score:<s>/measure:<m>/tick:<t>/track:<k>/note:<n>/tieFor
```
Where `s`, `m`, `n` are zero-based indices for score, measure, and note; `t` is absolute engine ticks (480 per quarter note); and `k` is the staff/voice track index.

## 5. Style Compatibility Guard

Style properties are guarded against the verified 4.70 style catalogue (`engrave/src/score/style470_catalog.inc`):
- Sets `defaultsVersion` to `470`.
- Omits neutral 5.00-only style fields (`windsNameByGroup`, `vocalsNameByGroup`, and default values for `lyricsStackingOrder`, `maskSlurs`, `maskTies`).
- Preserves legacy header/footer options (`headerFirstPage`, empty header/footer text, and `$C` center-footer macros in excerpts).
- Any unrecognized or modified style value outside the catalogue causes immediate export refusal.

## 6. Cooperative Cancellation

During 4.70 XML conversion, serialization progress is checked against `CaptureOptions::cancelled` every <= 4 KiB:
- An observed cancellation aborts serialization immediately with `ErrorCode::Cancelled`.
- No output file or partial XML bytes are published.
- Working buffer budgets (`CaptureOptions::max_working_bytes`) and final byte budgets (`CaptureOptions::max_bytes`) remain strictly enforced throughout the conversion.
