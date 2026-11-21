# MuseScore 4.70 Format Export Contract

`engrave` provides a bounded backward-compatibility exporter for MuseScore 4.70
(interoperable with MuseScore 4.7.0–4.7.6). This document defines the API
contract, supported subset, degraded features, refusal rules, and style guard.

## 1. Activation and API Contract

### Requesting 4.70 Export

Set `ExportOptions::native_version = "4.70"` for `MSCX` or `MSCZ` exports:

```cpp
struct ExportOptions {
    Format format = Format::MSCZ;        // MSCX or MSCZ
    std::string native_version = "4.70"; // default is "5.00"
    ScoreSel score;                      // master by default, or specific excerpt
    CaptureOptions capture;
};
```

Any version string other than `"5.00"` or `"4.70"`, or a non-default version on
any format other than `MSCX`/`MSCZ`, returns `ErrorCode::UNSUPPORTED`.

### Warning Structure and Ordering

Use `Document::export_score_with_warnings(ExportOptions)` to receive exported
bytes alongside ordered compatibility warnings:

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

- **Frozen warning contract**: Every `B470_*` warning has a frozen `code`,
  human-readable `message`, and non-empty object identity `{ kind, id }`
  (warnings are never anonymous; the unsupported-target path supplies
  `{ "request", "version:<requested value>" }`). Host schemas must require
  `{ kind, id }` on every `B470_*` warning and allow both EIDs and non-EID
  composite/locator forms.
- **Ordering on success**: `convert_degradations` first collects serialized XML
  degradations (`B470_SHARED_PART_DROPPED`, `B470_STAVE_SHARING_LABEL_DROPPED`,
  `B470_PAGE_LOCK_DROPPED`, `B470_GROUP_BRACKET_DROPPED`) in pre-order XML
  traversal, then a pass over source scores and parts appends
  `B470_INSTRUMENT_GROUP_NAME_DROPPED`.
- **Ordering on refusal**: When refusal reaches post-serialization checks,
  degradation warnings precede unsupported-feature warnings. An invalid tie
  returns early before degradation and style scans run, so only
  `B470_INVALID_TIE` is reported until it is fixed.
- **Deduplication and excerpt indices**: Identical warnings are deduplicated in
  first-occurrence order. Non-EID warning locators retain document-wide score
  indices (master score `0`, then excerpts in source order) even when exporting
  a single excerpt.

### Fail-Closed Behavior

- **Byte-only export**: `Document::export_score(ExportOptions)` fails closed —
  if the export produces any compatibility warnings, it returns
  `ErrorCode::UNSUPPORTED` with the warnings attached to
  `Error::export_warnings` so callers cannot silently drop degradations.
- **Refused export**: No bytes are produced (`bytes` is empty), and
  `Error::export_warnings` reports the blocking elements.

## 2. Supported Score Features

- **Instruments and staves.** The writer preserves instrument IDs, ranges,
  transposition, MIDI channels and actions, drum maps, string tunings, and
  pitched, percussion, and tablature staves. Instrument changes and initial
  instruments use legacy long and short names. The adapter renders effective
  transposition and numbering into the label text and places individual staff
  labels with the legacy placement indices where those indices exist.
- **Notation.** Shared clefs, keys, time signatures, chords, rests, tuplets,
  beams, text, lyrics, harmony, ornaments, and measure-level markings retain
  their legacy representation. Generated, user-authored, and courtesy
  accidentals use 4.70 accidental symbols. Harmony cases use the legacy
  integer encoding, and note-parenthesis groups use chord-local note indices.
  Legacy single- and two-chord tremolos are supported. Active newer tremolo
  subtypes and authored chord-line control offsets refuse the downgrade.
- **Structure.** Score order, system objects, locks and dividers, layout
  frames, repeats, and layout breaks are supported.
- **Ties and spanners.** Paired connectors preserve note, chord/rest and
  segment/measure endpoints, including cross-staff and cross-voice connections,
  grace notes, and the final barline. The writer preserves inline partial ties,
  laissez vibrer ties, tapping half-slurs and authored lyrics lines. Relocated
  starts retain their element IDs and authored segment edits. Line segment
  overrides use positional entries, with neutral entries filling gaps in the
  sequence. Links point to the first member written in the master or selected
  excerpt. Note connector offsets and parenthesis indices follow the reader's
  chord ordering, including after a staff-type change.
- **Brackets.** Normal brackets, braces, square brackets and lines use
  legacy types 0 through 3.
- **Identities**: Serialized musical, staff, and score EIDs supported by the
  mapping, plus legacy numeric part IDs. MuseScore 4.70 has no part EID
  representation, so part EIDs are omitted.
- **Excerpt selections**: Selecting an excerpt via `ExportOptions::score`
  exports a standalone 4.70 score preserving its parts, style, and linked
  elements. Master exports retain attached excerpts with their names, track
  maps, initial part IDs, and effective styles.
- **Custom chord lists**: MSCX stores the definitions in `Style/ChordList`.
  MSCZ stores the top-level definitions in a versioned `chordlist.xml`
  sidecar and keeps each attached excerpt's custom definitions in its own
  style.

## 3. Degraded Features (Exported with Warnings)

Five feature families are converted to 4.70 structures with structured warnings:

| Code                                 | Exact Frozen Message                                                                     | Object Kind & Source ID                                                     |
| ------------------------------------ | ---------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- |
| `B470_SHARED_PART_DROPPED`           | Shared-part mapping was removed; its staves and content were retained as ordinary parts. | `kind: "shared_part"`, `id: <SharedPart-EID>` (including disabled mappings) |
| `B470_STAVE_SHARING_LABEL_DROPPED`   | Stave-sharing label was removed.                                                         | `kind: "stave_sharing_label"`, `id: <label-EID>`                            |
| `B470_PAGE_LOCK_DROPPED`             | Page lock was removed.                                                                   | `kind: "page_lock"`, `id: "<start-measure-EID>..<end-measure-EID>"`         |
| `B470_GROUP_BRACKET_DROPPED`         | Group bracket was removed.                                                               | `kind: "group_bracket"`, `id: "<start-staff-EID>:<zero-based-column>"`      |
| `B470_INSTRUMENT_GROUP_NAME_DROPPED` | Instrument group-name override was removed.                                              | `kind: "part"`, `id: <part-EID>` (one warning for multiple override fields) |

## 4. Refused Features

Output is refused with `ErrorCode::UNSUPPORTED` when constructs have no defined
4.70 downgrade:

| Code                       | Exact Frozen Message                                            | Object Kind & Source ID                                                                                                     |
| -------------------------- | --------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `B470_INVALID_TIE`         | Cannot write 4.70: tied notes have different pitches.           | `kind: "tie"` with tie EID; else nearest addressable ancestor's kind and EID; else `kind: "tie"` with deterministic locator |
| `B470_UNSUPPORTED_FEATURE` | Cannot write 4.70: the source feature has no defined downgrade. | Owning source object kind and EID                                                                                           |
| `B470_UNSUPPORTED_TARGET`  | Cannot write the requested score version.                       | `kind: "request"`, `id: "version:<requested value>"`                                                                        |

Refusals include unmappable endpoints, unknown spanner types, text-line size
overrides outside the legacy hairpin, ottava, pedal, and volta readers, slur
or tie masking, active newer tremolo and chord-line
properties, and authored automation curves. The reader reconstructs generated
automation curves from the score. Custom chord lists require a root render
program and representable render actions. Static staff labels are projected into
instrument names. Staff type changes that alter those labels require a refusal.

4.7.6 has no Jianpu (numbered) notation, so these refuse the downgrade:

- A Jianpu staff type, initial or in a `StaffTypeChange`
  (`StaffType[@group="jianpu"]`). 4.7.6 would read a pitched staff without
  lines.
- A key signature with custom Jianpu numbering (`KeySig/jianpuNumbering`).
  Under the default numbering, which follows the score key, the kept
  `jianpuTonicKey` and `jianpuTonicMode` are inactive and are dropped.
- A reference to the `jianpu_number` text style, from a text item
  (`style`) or from tablature frets (`StaffType/fretTextStyle`), in the master
  or an excerpt. 4.7.6 would draw it in another style.
- Active `jianpu*` styles (section 5).

### Dropped Stave-Sharing Data

Without the shared part (`B470_SHARED_PART_DROPPED`), the authored maps
(`SharedPart/StaveSharingState`) and the generated map indicators
(`StaveSharingChange`) are removed with no warning of their own, as is the
`staveSharingChangeMinDistance` style.

### Tie Warning Identities and Locators

An invalid-tie warning never stringifies an invalid-EID sentinel:

1. If the tie has a valid EID, emit `kind: "tie"` and the tie EID.
2. Otherwise, walk to the nearest addressable ancestor with a valid EID and emit
   its lowercase type name (for example `kind: "note"`) and EID.
3. If no addressable owner exists, emit `kind: "tie"` with a deterministic
   non-EID locator:

```text
locator:score:<s>/measure:<m>/tick:<t>/track:<k>/note:<n>/tieFor
```

Here `<s>`, `<m>`, and `<n>` are zero-based score, measure, and in-chord note
indices (master score `0`, then excerpts in source order); `<t>` is the absolute
engine tick (480 per quarter); and `<k>` is the staff/voice track index. The
locator is deterministic for a revision, is not a query handle, and must carry
all five components plus the trailing `tieFor` field.

## 5. Style Compatibility Guard

The writer saves each score's full effective style, including each embedded
excerpt's style, and sets `defaultsVersion` to `470`. Canonical 4.7.6
properties retain their values. This includes explicit instrument-name
alignments, geometry, typography, and header and footer settings. The style
catalog rejects unknown or repeated tags from the intermediate writer.

Settings for dropped group brackets and stave sharing are omitted. Instrument
name format settings are reflected in the emitted labels. Other 5.00-only
settings are omitted when neutral or when the affected notation is absent: the
24 `jianpu*` styles are inactive in a score or excerpt without a Jianpu staff.
Unspecified slur and tie masking uses the 4.70 default. An authored active
setting without a legacy representation refuses export and names the owning
score or excerpt.

An omitted setting takes its line break with it. The 5.00-only settings that
the writer omitted before the upstream 5.0 ports still leave a blank line, so
earlier 4.70 output keeps its bytes; the style catalog marks them.

## 6. Cooperative Cancellation and Budgets

The 4.70 writer polls `CaptureOptions::cancelled` at traversal steps during
preflight and conversion, and at intervals of at most 4 KiB in chunked
serialization and output loops. Score, style, and chord-list XML each use a
single `pugi::load_buffer` call. The writer polls and charges their input
against the working-byte budget before parsing, and resumes polling after each
call returns.

- Observed cancellation returns `ErrorCode::CANCELLED` and publishes no bytes.
- `CaptureOptions::max_working_bytes` and `CaptureOptions::max_bytes` remain
  enforced throughout conversion.
