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
instrument IDs and enum values are refused. It preserves the frozen linked Flute excerpt and rejects custom chord
lists and non-generated automation. A complete scan of the serialized score
also identifies the named 5.00-only features, including inside structures that
are independently refused.

Styles are especially conservative. `style470_catalog.inc` records the entire
5.00 style serialization of the independently checked 4.70 control score, and
the style names produced by the 4.7.6 writer. Unknown or changed style values are refused except for the individually
bounded B5 mappings below. It
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
all values now use the frozen identities. Automation, custom chord
lists, unmapped styles and other unmapped constructs use `B470_UNSUPPORTED_FEATURE`.
Detailed refusal reasons remain in `Error::message`.

B2b additionally converts the checked half-note ties (including paired pitch
edits), the track-0 pedal from tick 0 to 960, and ordinary braces. Connections
are translated from 5.00 endpoint EIDs / SpannerMap to 4.70 relative locations.
Unknown connection properties remain guarded and refuse output. Part names
and numeric part IDs are retained; musical EIDs are copied. The 4.70 part
format has no part EID and the frozen preservation fixture does not demand it.

B5 converts the five frozen degradation families in `convert_degradations`:
SharedPart becomes an ordinary Part with its staff, visibility and content;
shared references are omitted. Labels, page-lock ranges and group brackets
are omitted with their frozen source identities. Ordinary braces remain.
InstrumentLabel group overrides are omitted once per owning part; ordinary
long/short names are translated to the legacy Instrument representation.
Only identical warning objects are deduplicated; unsupported properties still
refuse the entire export, retaining detailed reasons in the error message.

The frozen excerpt uses a pitched flute, quarter notes, half rests, C-major
key signatures and same-voice same-measure ties. Nested scores undergo the
same structural, connection and full-style checks as their master. Links on
staff and musical objects (including ties) retain their source EIDs. All
master/excerpt note graphs are checked for unequal-pitch ties before writing.
This does not establish general instrument/rhythm/spanner compatibility.

## Selected excerpt export (engrave-25 candidate)

An excerpt selected through `ExportOptions::score` is now converted from the
same standalone 5.00 serialization used by the default native exporter. Only
that excerpt is emitted. The serializer retains links between emitted objects
and omits references to master objects outside the standalone file. The
selected score supplies its own style and part names. The master export path
and the 5.00 writer are unchanged.

Selection additionally accepts the legacy instrument-excerpt title VBox,
eighth durations, appoggiaturas and eighth after-graces used by the full
`fixtures/golden/linked_parts.mscx` excerpt. Ties use the existing engine
`Location` calculation, including grace indices, rather than a new musical
timing algorithm. Unrecognized tie properties and other constructs still
refuse output with the frozen warnings. Invalid-tie scans cover emitted
scores; non-EID warning locators retain their document-wide score indices.

`tests/test_write470_excerpts.py` captures both native formats for the parent's
`tests/phase_b/excerpts.mscx` and full linked-parts fixture. It checks empty
warning arrays and representable EIDs/link edges against standalone 5.00,
then compares every field in the independently dumped source excerpt body
with the standalone 4.70 result. `--verify-only` permits Windows captures to
be checked on physical Linux. `Export470.SelectedExcerptIsStandaloneAndBounded`
also checks reopening, output limits, in-process default/explicit 5.00 byte
equality and unsupported-style refusal.

EXECUTED on Windows and physical `cachy`: native builds with
`engrave_tests=y`, 15/15 Export470 tests, 21/21 API tests, and both fixture
selections in MSCX/MSCZ with exactly `[]` warnings. The archived 4.7.6 reader
opens all eight Windows/Linux 4.70 captures and matches the complete source
excerpt bodies (parts/names, measures, signatures, notes/ties and all other
dumped fields). Each oracle run verifies all 87 SHA256SUMS entries and sets
its subprocess LD_LIBRARY_PATH solely to the oracle's `lib/`. The full Linux
bounded oracle suite also passes 26 exports, the real paired edit and frozen
refusals; focused gap checks pass on both hosts. The frozen engrave-19
default/explicit 5.00 byte checks pass on both hosts with their documented
revision/ZIP-time normalization. The byte runner now accepts the probe's
structured empty warning array instead of requiring empty stdout.

The archive/manifest-verified Windows 4.7.6 host independently re-exported
all four Windows selection outputs to both formats: all eight re-exports
retain complete oracle semantics, element EIDs and serialized links. A
separate master-with-excerpt control measures the historical MSCX limitation:
that old host discards the separately archived nested excerpt when extracting
its MSCX output, while its MSCZ output retains it. Standalone selections put
their own content in the main score and retain it through both old formats.
This does not establish general old-host MSCX excerpt retention.

Retained evidence is in the parent's ignored `notes/phase-b/e25/`, with remote
captures at `cachy:~/OSS/engrave-e25/`. The parent owns the full two-host
engine gate, hash-pinned golden comparison, host MCP route tests, final pin/tag
and independent Terra/Astra review. No full engine gate, pin/tag, push or
phase acceptance is claimed here.

The five 5.00-only style differences have explicit projections:
`windsNameByGroup` and `vocalsNameByGroup` boolean values are omitted with
the group-name behavior; ordinary instrument names are retained.
`lyricsStackingOrder`, `maskSlurs` and `maskTies` boolean values may be omitted
only when the affected lyrics/slurs/ties are absent (or the catalog's neutral
value already matches). Excerpts also copy legacy `headerFirstPage` boolean
values, empty header/footer text and the legacy `$C` center-footer macro.
All other catalog, uniqueness, attribute and value checks remain in place.
The complete style of each excerpt is validated independently.

### Warning identities and bounded identity preservation

Invalid EID sentinels are never warning identities. An unequal-pitch tie uses
its own valid EID, otherwise its start note's EID, otherwise the nearest
addressable ancestor (with that ancestor's kind). If none exists, `kind:"tie"`
uses the explicitly non-EID locator
`locator:score:<s>/measure:<m>/tick:<t>/track:<k>/note:<n>/tieFor`.
Score, measure and note indices are zero-based in document, measure and chord
order; tick is the absolute engine tick (480 per quarter), track is the engine
staff/voice track. The master is score zero, followed by excerpts in source
order. This source-revision locator is deterministic and is not a query handle.
Only the complete numeric tie-locator shape above is accepted (all five
components and the final `tieFor` field are required). Other malformed/empty
warning identities fail as invalid source arguments;
they cannot become successful anonymous warnings. Composite range/bracket keys
must contain valid EIDs and a nonempty numeric column where applicable.
Host schemas must require `{kind,id}` for every B470 warning and describe both
EIDs and these non-EID locator/composite forms; `id` is not uniformly an EID.

Enabled wind/vocal group naming is projected off by the old reader. If a score
contains multiple parts with the same applicable instrument, each affected part
receives the frozen `B470_INSTRUMENT_GROUP_NAME_DROPPED` message and its source
part EID. This extends that identity's trigger to the enabled group style;
no warning code or message is added. Potential groups are checked even when
hidden. A single applicable instrument has no current group behavior to lose.

The 4.70 identity promise is limited to the serialized musical, staff and score
EIDs supported by the bounded mapping, plus legacy numeric part IDs. Part EIDs
(ordinary and shared) have no interoperable 4.70 field and are omitted; current
host reopen assigns fresh ones. A numeric part ID is not a retained part EID.
The parent must explicitly approve this format limitation in B1's contract;
this implementation introduces no new warning code or fixture expectation to
authorize that product decision. If preservation of part EIDs is required,
the current 4.70 mapping cannot satisfy it and phase acceptance remains blocked.

`tests/test_write470_oracle.py` checks the parent's frozen `tests/phase_b`
manifest and all 87 preserved oracle files, then compares complete oracle
semantics and warning arrays for thirteen successful cases in both formats,
an actual paired pitch edit and the two refused cases. No warnings are
filtered. The oracle dump cannot prove EIDs, links or brace properties:
independent old-reader re-export must check those at phase acceptance.

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

Remaining work includes general rhythms/instruments, arbitrary styles,
further ties and spanners, layout/image data, automation and custom chords.
Every newly accepted mapping needs independent semantics and oracle evidence.
The following B2b evidence is historical; its refused B5 cases are superseded
by the B5 implementation and its separately retained execution report.

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

## B5 executed evidence

Windows and physical Linux compiled with `engrave_tests=y`. On each, the
focused Export470 suite passed 9/9 and the public API suite passed 21/21.
All thirteen successful frozen cases matched complete preserved-oracle JSON
and exact warning arrays in MSCX and MSCZ: 26 Linux and 26 Windows outputs.
This includes preservation of `excerpts` with no warnings and all five
warned degradations. The actual paired edit also matched its oracle JSON;
the one-end edit and unsupported target refused with exact frozen warnings.
The runner verified all 87 SHA256SUMS entries and the dumper digest, and
limited each oracle subprocess's LD_LIBRARY_PATH to the preserved `lib/`.

A Windows default-5.00 label-bearing export matched the baseline after only
the documented programRevision normalization (`542d326` versus `56aabbb`);
raw cross-build bytes therefore differ in that build stamp. Default versus
explicit 5.00 bytes matched in both formats in the focused tests. The 5.00
writer source was not changed. No golden or frozen expectation was changed.
The parent owns the full engine gate, golden comparison, old-reader identity
and brace-association checks, host/schema fixes, Terra re-review and final
tag/pin. These focused results do not establish full phase-B acceptance.
