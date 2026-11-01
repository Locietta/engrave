# Native 4.70 export, bounded conversions

`ExportOptions::native_version` selects `"5.00"` (the default) or `"4.70"`
for MSCX and MSCZ. An unknown version, or a non-default native version on
another format, returns `Unsupported`: the version may be meaningful but this
engine has no writer for that request. The 5.00 route is the existing writer.

Use `Document::export_score_with_warnings` when requesting 4.70. It returns
owned bytes and ordered `ExportWarning` values, each with a stable `code`, human
`message`, and an optional source `{kind,id}` object. On refusal, the result is
`Unsupported` and the warnings are in `Error::export_warnings`; the document is
not changed. The original byte-only `export_score` signature remains available.
It refuses a successful warning-bearing export and attaches the warnings to
its error, so its caller cannot unknowingly discard them. The engine has no
job abstraction: the host must retain `ExportOutput::warnings` and
`Error::export_warnings` in its captured job result for synchronous and
background calls alike.

The initial writer is intentionally small and fails closed. It accepts a
checked subset of the 5.00 serialization, converts `InstrumentLabel` names to
the 4.70 instrument representation, restores the part's 4.70 `trackName`,
writes a 4.70 style and header, and produces an MSCX or an MSCZ containing
that MSCX. It accepts the checked piano, pitched standard staff, treble
clef, 4/4, whole/half-note structures, bounded ties/pedal and ordinary braces
described below. Other serialized nodes, attributes,
instrument IDs and enum values are refused. It rejects excerpts, custom chord
lists and non-generated automation. A complete scan of the serialized score
also identifies the named 5.00-only features, including inside structures that
are independently refused.

Styles are especially conservative. `style470_catalog.inc` records the entire
5.00 style serialization of the independently checked 4.70 control score, and
the style names produced by the 4.7.6 writer. This slice refuses any unknown or
changed style value, including values that 4.70 might ultimately express. It
copies the known 4.70 style fields, sets `defaultsVersion` to 470, and omits
only the captured neutral 5.00-only fields. This is a guard, not a general
style conversion. It must be broadened with individually specified compatibility
rules and oracle fixtures, not by removing the comparison.

The public warning contract is `{ "code": ..., "message": ..., "object":
{ "kind": ..., "id": ... } }`. `ExportWarning::object` is an optional
`ExportWarningObject` with owned, standard-library strings `kind` and `id`.
Range and column identifiers are strings, not element handles. Serializers
must emit these two fields; `to_string(object)` is only a display convenience.
Codes and messages below are frozen. A refusal returns no score bytes; a
`*_DROPPED` warning on a refused export identifies the unsupported mapping,
not a successful projection. Additional unmapped content produces
`B470_UNSUPPORTED_FEATURE` warnings and prevents output.

| Code | Exact message | Object kind and source id |
| --- | --- | --- |
| `B470_SHARED_PART_DROPPED` | Shared-part mapping was removed; its staves and content were retained as ordinary parts. | `shared_part`, SharedPart EID (including disabled mappings) |
| `B470_STAVE_SHARING_LABEL_DROPPED` | Stave-sharing label was removed. | `stave_sharing_label`, label EID |
| `B470_PAGE_LOCK_DROPPED` | Page lock was removed. | `page_lock`, `<start-measure-EID>..<end-measure-EID>` |
| `B470_GROUP_BRACKET_DROPPED` | Group bracket was removed. | `group_bracket`, `<start-staff-EID>:<zero-based-column>` |
| `B470_INSTRUMENT_GROUP_NAME_DROPPED` | Instrument group-name override was removed. | `part`, part EID; one warning for multiple override fields |
| `B470_INVALID_TIE` | Cannot write 4.70: tied notes have different pitches. | `tie`, tie EID; refuses output |
| `B470_UNSUPPORTED_FEATURE` | Cannot write 4.70: the source feature has no defined downgrade. | Owning source object kind and EID; refuses output |
| `B470_UNSUPPORTED_TARGET` | Cannot write the requested score version. | `request`, `version:<requested value>`; refuses output |

The old `export_warning_code` C++ names remain aliases where applicable;
all values now use the frozen identities. Excerpts, automation, custom chord
lists, styles and other unmapped constructs use `B470_UNSUPPORTED_FEATURE`.
Detailed refusal reasons remain in `Error::message`.

B2b additionally converts the checked half-note ties (including paired pitch
edits), the track-0 pedal from tick 0 to 960, and ordinary braces. Connections
are translated from 5.00 endpoint EIDs / SpannerMap to 4.70 relative locations.
Unknown connection properties remain guarded and refuse output. Part names
and numeric part IDs are retained; musical EIDs are copied. The 4.70 part
format has no part EID and the frozen preservation fixture does not demand it.

Stave-sharing labels now reach the DOM and the default 5.00 writer, including
reopen. Downgrade identifies each label, but the genuine 5.00 fixture also
contains non-neutral style defaults outside the checked catalog, so that
fixture still refuses output with additional unsupported-feature warnings.
Shared parts, page locks, group brackets, instrument group names, excerpts,
other rhythms/instruments and custom styles still require real conversions.

`tests/test_write470_oracle.py` checks the parent's frozen `tests/phase_b`
manifest and all 87 preserved oracle files, then compares complete oracle
semantics for seven cases in both formats and an actual paired pitch edit.
It also rejects an actual one-end pitch edit. The oracle dump cannot prove
EIDs or brace properties: independent old-reader re-export must check those.

The control fixture in `tests/data/write470/minimal.mscx` is the preserved
4.70 header fixture, SHA-256
`64b08d861b6f7eabe0c20890c736fb38fcc691f505bc0420340c90aff741735a`.
The source of the 5.00 style catalog was an existing Windows exporter reporting
`engrave-19`'s `45ea71c` revision; its style SHA and the 4.7.6 style SHA are
recorded in the catalog. The exporter binary's SHA-256 was
`9bb30c131b5c9997f5024b41ff804b895a608d14e0d5adc1b00786c5ca56996f`;
the embedded revision and binary hash are not a clean-build attestation.
The adjacent `engrave19.mscx` and `engrave19.mscz` are captures from the
`engrave-19` Windows public exporter of the frozen
`custom_part_name.mscx.mscz` corpus input (SHA-256
`82fdedeaf524cf284fec79ad514f93f62d7b860607ccb8be36c87e0b77e66f70`).
The MSCX capture used the Windows host binary SHA-256
`e89cc4b6d23bf3965e10f89b8b11a10a30196ac33fd6c179dcb03498ac9bcd32`;
the MSCZ capture used the exporter binary above. Both report `45ea71c` in
their score XML. `test_engrave19_bytes.py` pins the capture hashes, checks
default and explicit 5.00 exports, and explains the only
normalizations: ZIP timestamps and a later build's revision stamp. At the same
revision it hashes the raw ZIP after zeroing timestamps.

Remaining work includes the actual 4.7.6 writer port and conversion rules for
other rhythms, rests, non-piano instruments and staff types, custom and newer
styles, excerpts and links, further ties and spanners, layout and image data, automation,
shared parts, further stave-sharing label/style mappings, page locks, and group brackets. Every new
accepted case needs independent expected semantics and a 4.7.6 oracle test;
the old reader's successful open alone does not validate a 5.00 body.

## B2b executed evidence

Windows and physical Linux compiled with `engrave_tests=y`; the focused
Export470 suite passed 8/8 and the public API suite passed 21/21 on each.
The preserved archive's 87 checksums and dumper hash were verified. On Linux,
14 file exports (seven cases, MSCX and MSCZ) plus the real paired pitch edit
matched the complete frozen oracle semantics. The real one-end edit refused
output with the exact frozen invalid-tie warning. Fifteen Windows exports
also matched that oracle. A hash-checked preserved Windows 4.7.6 host
(`6471b23d944dbed23844ea0785423eee781223f46ef86856e86195672256a010`)
reopened and re-exported the ID and brace outputs, retaining their required
XML properties; the semantic dumper alone does not establish those properties.

All seven preserved fixtures match their empty warning arrays exactly.
`ties_mismatched` and `unsupported_target` match their complete frozen warning
arrays exactly. The five degraded fixtures have the exact frozen feature
warnings (including kinds and compound IDs), plus additional unmapped-feature
warnings: their complete arrays do not match, and no output is produced.
`excerpts` likewise refuses rather than claiming the frozen preservation case.
These are remaining phase-B gaps, not accepted degradations or oracle passes.

The ordinary 5.00 control matched engrave-20 raw MSCX and MSCZ bytes on Linux.
On Windows the control differed only in programRevision (`84b04f2` versus
`542d326`); after the already documented revision normalization, MSCX hashes
matched. Label-bearing 5.00 scores deliberately change: both labels now
survive export/reopen. No golden was updated. Full read-suite runs against the
local historical corpus have the same three pre-4.6 fixture failures on the
baseline and candidate; the focused tests above are separately executed.
