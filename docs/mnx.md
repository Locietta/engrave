# MNX model

MNX import and export use an engine-owned document model in
`src/importexport/mnx/model/`, which replaces the vendored mnxdom library and
pboettch/json-schema-validator. This note records its design.

## Why a rewrite

mnxdom is a lazy, throwing view over an untyped JSON tree: every accessor
looks its value up and throws when it is missing or has the wrong type, and
everything else (ID lookups, enclosing elements, event timing) is computed by
walking JSON pointers. Its control flow for malformed input is therefore
exception-driven throughout, and malformed input is the common case for files
written by agents. The model below reads a file once, rejects anything invalid
with a located error, and afterwards cannot fail.

## Layers

1. **Document model** (`mnx_model.h`, generated). One C++ struct per object
   definition of the MNX schema, with snake_case members; optional properties
   are `std::optional`, string enums are enum structs, the `anyOf` content lists
   are `std::variant` lists (`SequenceItem`, `SystemLayoutItem`), and
   `patternProperties` maps are ordered key/value vectors. `tools/mnx_codegen.py`
   generates the header from `model/mnx-schema.json`; CI checks it is current.
   Hand-written support types (`Fraction`, `Id`, `SimpleColor`) and the glaze
   readers for the generated code are in `model_support.h`.
2. **Reading and writing** (`document.h`). `read_document(text)` is a single
   glaze read into the structs that returns `std::expected<Root, ReadError>`.
   The read enforces the whole schema: JSON syntax, types, required members,
   unknown members (`unevaluatedProperties: false`), enum and literal values,
   the `id` and colour patterns, and the integer enums. The error carries
   glaze's message with line, column and the offending text.
   `write_document(root)` is the inverse.
3. **Semantic index** (`index.h`). `build_index(root)` checks the rules the
   schema cannot express (unique and resolvable IDs, measure counts, sequence
   durations, beams, ties, slurs, ottavas, layouts) and returns either the
   diagnostics or an index of the document: ID lookups, each event's measure,
   sequence, start time, grace index and ottava shift, beam membership and
   levels, lyric line order, and the measure time and key in effect. After a
   successful build every lookup the importer makes is total.
4. **Import and export** (`internal/`). The importer reads the model through
   the index; the exporter fills the structs and writes them.

## Error handling

Reading untrusted input never throws for malformed data: syntax and schema
errors come back from `read_document` as `glz::error_ctx`-based errors, and
semantic errors from `build_index` as a list of `{pointer, message}`
diagnostics. `engrave::open_mnx` returns them as `Result` errors
(`INVALID_INPUT`). Content that is valid MNX but that the engine cannot
represent is imported with a warning, not rejected.

## Deliberate departures from the schema

- Integers must be written as JSON integers in the `int` range: `4.0`, `1e0`
  and values beyond 32 bits are rejected (JSON Schema counts `4.0` as an
  integer).
- `null` for an optional member reads as an absent member.
- The contents of comments (`_c`) and vendor extensions (`_x`) are skipped
  without type checks.
