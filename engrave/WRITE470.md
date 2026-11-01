# Native 4.70 export, initial restricted slice

`ExportOptions::native_version` selects `"5.00"` (the default) or `"4.70"`
for MSCX and MSCZ. An unknown version, or a non-default native version on
another format, returns `Unsupported`: the version may be meaningful but this
engine has no writer for that request. The 5.00 route is the existing writer.

Use `Document::export_score_with_warnings` when requesting 4.70. It returns
owned bytes and ordered `ExportWarning` values, each with a stable `code`, human
`message`, and an optional source `ElementId`. On refusal, the result is
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
that MSCX. It accepts only the checked piano, pitched standard staff, treble
clef, 4/4 and whole-note structures. Other serialized nodes, attributes,
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

Warning identifiers currently emitted are also available as constants in
`engrave::export_warning_code`:

| Code | Meaning and object |
| --- | --- |
| `engrave.470.part_identity_omitted` | The 5.00 part EID cannot be persisted by the 4.70 part writer; the numeric 4.70 part ID remains. Successful export; source part EID. |
| `engrave.470.page_lock` | A page lock cannot be written; source first-measure EID. Export refused. |
| `engrave.470.shared_part` | A shared part or association is not converted; nearest source EID. Export refused. |
| `engrave.470.stave_sharing_label` | A stave-sharing label is not converted; source EID when available. Export refused. |
| `engrave.470.group_bracket` | A group bracket is not converted; nearest source EID. Export refused. |
| `engrave.470.excerpt` | An excerpt is not converted; excerpt score EID when available. Export refused. |
| `engrave.470.automation` | A non-generated automation curve is not converted; score EID. Export refused. |
| `engrave.470.custom_chord_list` | A custom chord list is not converted; score EID. Export refused. |
| `engrave.470.unsupported_style` | A style value lies outside the control's checked catalog; score EID. Export refused. |
| `engrave.470.unsupported_construct` | Another score node, attribute or value is outside the implemented subset; nearest source EID. Export refused. |

Warnings follow the stable score/feature traversal and style serialization
order; multiple affected objects are not collapsed. IDs identify source objects,
not generated file objects. Range locks do not have EIDs, so their first measure
identifies them.

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
styles, excerpts and links, ties and spanners, layout and image data, automation,
shared parts, stave-sharing labels, page locks, and group brackets. Every new
accepted case needs independent expected semantics and a 4.7.6 oracle test;
the old reader's successful open alone does not validate a 5.00 body.
