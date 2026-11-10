# MNX model

MNX import and export use an engine-owned document model in
`src/importexport/mnx/model/`. It replaced the vendored mnxdom library and
pboettch/json-schema-validator; this note records its design.

## Why a rewrite

mnxdom was a lazy, throwing view over an untyped JSON tree: every accessor
looked its value up and threw when it was missing or had the wrong type, and
everything else (ID lookups, enclosing elements, event timing) was computed by
walking JSON pointers. Malformed input, the common case for files written by
agents, was therefore handled by exceptions throughout. The model reads a file
once, rejects anything invalid with a located error, and afterwards cannot
fail.

## Layers

1. **Document model** (`mnx_model.h`, generated). One C++ struct per object
   definition of the MNX schema, with snake_case members; optional properties
   are `std::optional` (with `<name>_or_default()` accessors where the MNX
   specification gives a default), string enums are enum structs, the `anyOf`
   content lists are `std::variant` lists (`SequenceItem`, `SystemLayoutItem`),
   and `patternProperties` maps are ordered key/value vectors.
   `tools/mnx_codegen.py` generates the header from `model/mnx-schema.json`; CI
   checks it is current. Hand-written support types (`Fraction`, `Id`,
   `SimpleColor`) and the glaze readers for the generated code are in
   `model_support.h`.
2. **Reading and writing** (`document.h`). `read_document(text)` is a single
   glaze read into the structs. It enforces the whole schema: JSON syntax,
   types, required members, unknown members (`unevaluatedProperties: false`),
   enum and literal values, the `id` and colour patterns, and the integer
   enums; the error carries glaze's message with line, column and the
   offending text. `write_document(root)` is the inverse.
3. **Semantic index** (`index.h`). `build_index(root)` checks the rules the
   schema cannot express (unique and resolvable IDs, measure counts, sequence
   and tuplet durations, staff numbers, beams, ties, slurs, ottavas, endings,
   layouts, scores) and returns either the diagnostics or an index: ID
   lookups, each event's part, measure, sequence, container, start time,
   duration, grace index and ottava shift, beam membership and levels, the
   lyric line order, and the measure whose time and key apply at each global
   measure. `load_document(text)` runs both steps and returns a `Document`
   (the root, its index and the warnings) or a `LoadError`.
4. **Helpers**: `walk.h` walks a sequence with MNX timing, `music.h` computes
   durations and written pitches, `layout.h` flattens layouts into staff and
   group spans.
5. **Import and export** (`internal/`). The importer reads a `Document` through
   its index; the exporter fills a `Root` and writes it.

The index, walk, layout and pitch code are adapted from mnxdom (MIT), which
also supplies `thirdparty/music_theory/music_theory.hpp` unchanged.

## Error handling

Malformed input never throws. Syntax and schema errors come back from
`read_document`, semantic errors from `build_index` as `{pointer, message}`
diagnostics, and `engrave::open_mnx` returns either as an `INVALID_INPUT`
error. A missing initial clef is a warning: the importer gives the staff a
default clef. Exceptions remain only for engine bugs and allocation failures,
which `Engine::open` still catches as a last resort.

## Deliberate departures from the schema and from mnxdom

- Integers must be JSON numbers with an integral value in the `int` range:
  `4.0`, `5e-1` and values beyond 32 bits are rejected, while an exponent form
  of an integer (`1e2`) is read as that integer. JSON Schema counts `4.0` as an
  integer.
- `null` for an optional member reads as an absent member. A repeated key keeps
  its last value in a fixed object and is an error in a map (kit, sounds,
  lyric lines, line metadata).
- The contents of comments (`_c`) and vendor extensions (`_x`) are skipped
  without type checks, and the exporter writes neither (mnxdom wrote its own
  provenance under `_x.mnxdom`).
- mnxdom's validator ignored `unevaluatedProperties`; unknown members are now
  errors.
- `build_index` rejects what the importer cannot handle safely though the
  schema allows it: staff numbers outside the part, more than 64 staves, zero
  or negative denominators and multiples, fractions finer than 1/65536 or
  above 2^26, time signature counts outside 1 to 1024, scores longer than a
  million whole notes, measure numbers above 2^20, staff positions beyond
  ±1024, ties to a note of their own event, slurs that end before they start,
  tempos, repeat counts and measure repeats below 1, negative measure numbers, endings and
  multimeasure rests past the last measure, more than 16 dots, sub-beam
  events outside their parent beam, beams across parts, octaves beyond ±20,
  `diatonicDelta` beyond ±7, keys beyond ±30 fifths and transposition
  intervals beyond 70 steps. Durations and positions whose exact value needs
  more than 31-bit terms are rejected, which keeps every later rational
  operation within 64 bits.
- The importer imports nothing from the measures a measure repeat covers (the
  repeat replaces their content), imports the events of a tuplet MuseScore
  cannot represent without the tuplet, and spells a gap that no single note
  value fits with several rests.
- An ottava that ends at the end of a measure without its own time signature
  uses the time signature in effect (mnxdom used a whole note), and without
  any time signature a measure is 4/4 for layout changes as for durations.