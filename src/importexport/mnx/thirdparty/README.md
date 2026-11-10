# MNX third-party code

MNX import and export use two vendored libraries, both ported from
nlohmann::json to glaze so the engine has a single JSON library.

| Directory | Upstream | Pinned commit | License |
|---|---|---|---|
| `mnxdom/` | https://github.com/rpatters1/mnxdom (3.1.0) | `45bf46221c00411e968c851ff9a1be4e202af5c2` | MIT (`mnxdom/LICENSE`) |
| `mnxdom/schema/mnx-schema.json` | W3C MNX, as recorded by mnxdom (`third_party/w3c-mnx`) | W3C MNX `46fbe9393067221ee83ef08de209cb3c4edf0d8d`; SHA-256 `71cce118e2bb4d17d42790db2574e84e2b7ba0f0be020c866f71b8f89b638fb8` | W3C Software and Document License |
| `json-schema-validator/` | https://github.com/pboettch/json-schema-validator | `40af3ec39670e768fc3f01f935140af311d71024` | MIT (`json-schema-validator/LICENSE`) |

Only the library sources are vendored: upstream tests, examples, CMake and
documentation are not. `xmake/engrave.lua` builds them as the
`json_schema_validator` and `mnxdom` static libraries; the schema is embedded
at build time by xmake's `utils.bin2c` rule (`mnx-schema.json.h`) instead of
the upstream `mnx-schema.xxd`. Headers are included as `<mnxdom/mnxdom.h>`
and `<json-schema-validator/json-schema.hpp>`.

## The port

`json-schema-validator/json-dom.hpp` is the shared document model:
`json_schema::json` is `glz::generic_sorted_u64` (sorted object keys and
distinct unsigned, signed and floating numbers, like nlohmann::json), with an
RFC 6901 `json_pointer`, pointer navigation (`find`, `at`, `get_or_create`),
`parse`, an nlohmann-format `dump`, `equal` and `compare`. The validator moved
from `nlohmann::json_schema` to `namespace json_schema`; its logic, keyword
handling and messages are unchanged. mnxdom keeps its API: `mnx::json` and
`mnx::json_pointer` alias the json-dom types, property access goes through
`mnx::detail::fromJson`/`toJson` (nlohmann's `get<T>()` conversions, enums via
`EnumStringMapping`, the integer enums marked with `enumIsInteger`).

The port was checked against the nlohmann originals with an oracle that ran
both builds over 4808 documents (the engine's MNX test inputs, mnxdom's test
inputs and the W3C examples, plus deterministic mutations: deleted members,
type swaps, integer/float swaps, unknown members, bogus enum strings, huge
numbers, truncated, BOM-prefixed and trailing-garbage texts). Schema errors
(pointer, message and instance), semantic validation errors, exception
presence and typed DOM reads were identical except where listed below.

### Known differences from the nlohmann originals

- Exceptions raised by the JSON layer are `json_schema::parse_error`,
  `out_of_range` and `type_error` (derived from `std::invalid_argument` and
  `std::out_of_range`); their messages lack nlohmann's
  `[json.exception.*.NNN]` prefix, and parse error messages are glaze's
  (line, column, cause and the offending text, located by `glz::validate_json`).
- `hasValidDocumentRoot` on `"mnx": {}` (no version) returns false; the
  original read a missing key through nlohmann's const `operator[]`, which is
  undefined behaviour.
- `dump` formats doubles with the shortest round-trip representation
  (`std::to_chars`), where nlohmann uses Grisu2; the two can differ in rare
  digits. Invalid UTF-8 in strings is written through instead of throwing.
- Number kinds come from glaze's parser: an integer written in exponent
  form (`1e0`, `4e0`) is stored as an integer, where nlohmann stored a float,
  so the validator's `"type": "integer"` check accepts it; and a nonzero
  value below the double range (`1e-324`) is a parse error, where nlohmann
  rounded it to zero. Neither form occurs in MNX written by real tools.
- Parsing rejects a document followed by a NUL byte and nesting deeper
  than glaze's limit (256 levels); nlohmann accepted both.
- A malformed schema (not instance) can raise a different exception type, for
  example a non-string `type` or a boolean `maxLength`. The MNX schema has
  none of these.

Reproduced on purpose, since results depend on them: numbers compare across
unsigned and signed kinds the way nlohmann does (the unsigned value is cast to
`int64_t`, so 2^64-1 equals -1); booleans read as numbers (`true` is 1); and the
validator remains draft 7, so 2020-12 keywords such as `unevaluatedProperties`
are not enforced (the engine gate checks exports with a 2020-12 validator).

## Updating

Moving a pin means re-applying the port to the new upstream sources, then
rerunning the oracle comparison against the unported upstream build. Keep
`mnx-schema.json` identical to mnxdom's `third_party/w3c-mnx/mnx-schema.json`.
