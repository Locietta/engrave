"""Generate the MNX document model (C++ structs and glaze metadata) from the MNX JSON schema.

Usage: python tools/mnx_codegen.py [--check] <schema.json> <output.h>

Every object definition becomes a struct whose members are the schema properties in
snake_case (required ones plain, optional ones std::optional). String enums become enum
structs; anyOf arrays become std::variant lists whose alternatives are chosen by their
"type" key; patternProperties objects become ordered key/value vectors (glaze reads and
writes them as JSON objects). The `id` and `simple-color` patterns are the `Id` and
`SimpleColor` string types, and integer enums and enums with an empty value get checked
readers (model_support.h). Comments (`_c`) and vendor extensions (`_x`) are skipped.

Reading with model_support.h's options therefore enforces the whole schema: types,
required and unknown keys, enums, literals and patterns.
"""

import argparse
import json
import re
import sys
from pathlib import Path

# anyOf array definitions -> variant alias.
VARIANTS = {
    "sequence-content": "SequenceItem",
    "system-layout-content": "SystemLayoutItem",
}
# Definitions with a hand-written C++ type (model_support.h).
HAND_WRITTEN = {"fraction": "Fraction", "id": "Id", "simple-color": "SimpleColor"}
# Names for the values of integer enums.
INTEGER_ENUM_NAMES = {
    "ottava-amount": {1: "OCTAVE_UP", 2: "TWO_OCTAVES_UP", 3: "THREE_OCTAVES_UP", -1: "OCTAVE_DOWN", -2: "TWO_OCTAVES_DOWN",
                      -3: "THREE_OCTAVES_DOWN"},
    "time-signature-unit": {1: "WHOLE", 2: "HALF", 4: "QUARTER", 8: "EIGHTH", 16: "N16TH", 32: "N32ND", 64: "N64TH",
                            128: "N128TH"},
}
INTEGER_ENUM_NAMES["ottava-amount-or-zero"] = {0: "NONE", **INTEGER_ENUM_NAMES["ottava-amount"]}
SKIPPED_KEYS = ("_c", "_x")
# Defaults the MNX specification gives optional properties; each gets a `<name>_or_default()`
# accessor. Keyed by (definition, property); values are C++ expressions.
DEFAULTS = {
    ("support", "useAccidentalDisplay"): "false",
    ("support", "useBeams"): "false",
    ("dynamic-group", "orient"): "MultiStaffOrientation::AUTO",
    ("dynamic-group", "accentPrefix"): "DynamicPrefix::S",
    ("dynamic-group", "accentSuffix"): "DynamicSuffix::Z",
    ("fermata", "duration"): "FermataDuration::AUTO",
    ("fermata", "orient"): "Orientation::AUTO",
    ("fermata", "pointing"): "UpDownAuto::AUTO",
    ("fermata", "symbol"): "FermataSymbol::NORMAL",
    ("note-value", "dots"): "0",
    ("staff-source", "staff"): "1",
    ("staff-group", "barlineStyle"): "StaffGroupBarlineStyle::INSTRUMENT",
    ("ending", "open"): "false",
    ("arpeggio", "arrow"): "false",
    ("arpeggio", "direction"): "UpDownAuto::AUTO",
    ("clef", "octave"): "OttavaAmountOrZero::NONE",
    ("clef", "showOctave"): "true",
    ("ottava", "orient"): "Orientation::AUTO",
    ("ottava", "staff"): "1",
    ("measure-repeat-counter", "orient"): "MultiStaffOrientation::AUTO",
    ("part-transposition", "prefersWrittenPitches"): "false",
    ("positioned-clef", "staff"): "1",
    ("part", "staves"): "1",
    ("score", "useWritten"): "false",
    ("accidental-display", "force"): "false",
    ("written", "diatonicDelta"): "0",
    ("pitch", "alter"): "0",
    ("tie", "lv"): "false",
    ("event-lyric-line", "type"): "EventLyricLineType::WHOLE",
    ("grace", "graceType"): "GraceType::STEAL_PREVIOUS",
    ("grace", "slash"): "true",
    ("tuplet", "bracket"): "YesNoAuto::AUTO",
    ("tuplet", "showNumber"): "TupletDisplaySetting::INNER",
    ("tuplet", "showValue"): "TupletDisplaySetting::NO_NUMBER",
    ("sequence", "staff"): "1",
    ("strong-accent", "pointing"): "UpDownAuto::AUTO",
}
for marking in ("accent", "bow-direction", "breath-mark", "soft-accent", "spiccato", "staccatissimo", "staccato",
                "stress-marking", "strong-accent", "tenuto", "tremolo-single", "unstress-marking"):
    DEFAULTS[(marking, "orient")] = "Orientation::AUTO"
CPP_KEYWORDS = {"default", "class", "operator", "union", "template", "new", "delete", "register", "signed", "unsigned"}
NAMESPACE = "mu::iex::mnxio::model"


def camel(name):
    return "".join(part[:1].upper() + part[1:] for part in re.split(r"[-_]", name))


def snake(key):
    s = re.sub(r"(?<=[a-z0-9])([A-Z])", r"_\1", key).lower()
    return s + "_" if s in CPP_KEYWORDS else s


def enum_constant(value):
    if value == "":
        return "NONE"
    s = re.sub(r"(?<=[a-z0-9])([A-Z])", r"_\1", str(value)).upper().replace("-", "_")
    return "N" + s if s[:1].isdigit() else s


def tag_enum(literal_def):
    return camel(literal_def.removeprefix("literal-string-")) + "Tag"


class Generator:
    def __init__(self, schema):
        self.defs = schema["$defs"]
        self.root = schema["$ref"].split("/")[-1]
        self.version = int(re.search(r"/(\d+)$", schema["$id"]).group(1))
        self.structs = {}  # def name -> [(key, cpp type, required)], None for variant lists
        self.enums = {}  # C++ name -> [(json value, constant)]
        self.checked_enums = set()  # enums read through detail::read_enum
        self.untagged = {}  # variant list -> index of the alternative whose tag is optional
        self.order = []
        self.visiting = set()

    def ref_name(self, node):
        return node["$ref"].split("/")[-1]

    def alternatives(self, list_name):
        return [self.ref_name(a) for a in self.defs[list_name]["items"]["anyOf"]]

    def add_enum(self, name, values, checked):
        names = INTEGER_ENUM_NAMES.get(name)
        self.enums[camel(name)] = [(v, names[v] if names else enum_constant(v)) for v in values]
        if checked:
            self.checked_enums.add(camel(name))
        return camel(name)

    def type_of(self, node, owner, key):
        if "$ref" not in node:
            if "pattern" in node or "enum" in node or "const" in node:
                raise ValueError(f"inline constraint at {owner}.{key}")
            simple = {"integer": "int", "string": "std::string", "boolean": "bool", "number": "double"}
            if node.get("type") in simple:
                return simple[node["type"]]
            raise ValueError(f"unsupported schema node at {owner}.{key}: {node}")
        name = self.ref_name(node)
        d = self.defs[name]
        if name in VARIANTS:
            self.visit(name)
            return f"std::vector<{VARIANTS[name]}>"
        if name in HAND_WRITTEN:
            return HAND_WRITTEN[name]
        if "const" in d:
            raise ValueError(f"literal {name} outside a variant tag at {owner}.{key}")
        if "enum" in d:
            # glaze cannot name integer or empty enumerators; those get a checked reader.
            return self.add_enum(name, d["enum"], d["type"] == "integer" or "" in d["enum"])
        kind = d.get("type")
        if kind == "object":
            if "patternProperties" in d:
                return f"std::vector<std::pair<std::string, {self.type_of(d['patternProperties']['^.*$'], owner, key)}>>"
            self.visit(name)
            return camel(name)
        if kind == "array":
            return f"std::vector<{self.type_of(d['items'], owner, key)}>"
        if "pattern" in d:
            raise ValueError(f"pattern {name} has no hand-written type")
        return self.type_of({k: v for k, v in d.items() if k != "description"}, owner, key)

    def visit(self, name):
        if name in self.structs or name in self.visiting:
            return
        d = self.defs[name]
        self.visiting.add(name)
        if name in VARIANTS:
            for alt in self.alternatives(name):
                self.visit(alt)
            self.structs[name] = None
        else:
            if d.get("type") != "object" or "patternProperties" in d:
                raise ValueError(f"{name} is not a plain object")
            members = []
            required = set(d.get("required", []))
            if any(self.ref_name(a) == "global-attrs" for a in d.get("allOf", [])):
                members.append(("id", "Id", False))
            for key, prop in sorted(d.get("properties", {}).items()):
                if key == "type" and "$ref" in prop and "const" in self.defs[self.ref_name(prop)]:
                    literal = self.ref_name(prop)
                    self.enums[tag_enum(literal)] = [(self.defs[literal]["const"], "VALUE")]
                    members.append((key, tag_enum(literal), key in required))
                    continue
                members.append((key, self.type_of(prop, camel(name), key), key in required))
            self.structs[name] = members
        self.visiting.discard(name)
        self.order.append(name)

    def emit(self):
        self.visit(self.root)
        for list_name in VARIANTS:
            for index, alt in enumerate(self.alternatives(list_name)):
                if "type" not in self.defs[alt].get("required", []):
                    if list_name in self.untagged:
                        raise ValueError(f"{list_name} has two alternatives without a required type")
                    self.untagged[list_name] = index
        out = []
        w = out.append
        w("// Generated by tools/mnx_codegen.py from the MNX schema; do not edit.")
        w("// SPDX-License-Identifier: GPL-3.0-only")
        w("// clang-format off")
        w("#pragma once")
        w("")
        for header in ("array", "optional", "string", "string_view", "utility", "variant", "vector"):
            w(f"#include <{header}>")
        w("")
        w('#include "model_support.h"')
        w("")
        w(f"namespace {NAMESPACE} {{")
        w("")
        w("// The MNX schema version, the last segment of the schema $id.")
        w(f"inline constexpr int schema_version = {self.version};")
        w("")
        for name, values in sorted(self.enums.items()):
            integral = isinstance(values[0][0], int)
            w(f"enum struct {name}{' : int' if integral else ''} {{")
            for value, constant in values:
                w(f"    {constant}{f' = {value}' if integral else ''},")
            w("};")
            w("")
        for name in self.order:
            if self.structs[name] is not None:
                w(f"struct {camel(name)};")
        w("")
        for list_name, alias in VARIANTS.items():
            w(f"using {alias} = std::variant<{', '.join(camel(a) for a in self.alternatives(list_name))}>;")
        w("")
        scalar = {"int", "bool", "double"} | set(self.enums)
        used_defaults = set()
        for name in self.order:
            members = self.structs[name]
            if members is None:
                continue
            w(f"struct {camel(name)} {{")
            for key, cpp, required in members:
                if required:
                    w(f"    {cpp} {snake(key)}{'{}' if cpp in scalar else ''};")
                else:
                    w(f"    std::optional<{cpp}> {snake(key)};")
                    if (name, key) in DEFAULTS:
                        w(f"    {cpp} {snake(key)}_or_default() const {{ return {snake(key)}.value_or({DEFAULTS[(name, key)]}); }}")
                        used_defaults.add((name, key))
            w("};")
            w("")
        if set(DEFAULTS) - used_defaults:
            raise ValueError(f"defaults for missing optional properties: {sorted(set(DEFAULTS) - used_defaults)}")
        w(f"}} // namespace {NAMESPACE}")
        w("")
        for name, values in sorted(self.enums.items()):
            t = f"{NAMESPACE}::{name}"
            if name in self.checked_enums:
                value_type = "int" if isinstance(values[0][0], int) else "std::string_view"
                literal = (lambda v: str(v)) if value_type == "int" else (lambda v: f"std::string_view{{{json.dumps(v)}}}")
                entries = ", ".join(f"std::pair{{{literal(v)}, {t}::{c}}}" for v, c in values)
                w(f"template <> struct glz::from<glz::JSON, {t}> {{")
                w(f"    static constexpr std::array table{{{entries}}};")
                w("    template <auto Opts> static void op(auto &value, is_context auto &&ctx, auto &&it, auto end) {")
                w(f'        {NAMESPACE}::detail::read_enum<Opts>(value, ctx, it, end, table, "not a valid {name} value");')
                w("    }")
                w("};")
                w(f"template <> struct glz::to<glz::JSON, {t}> {{")
                w("    template <auto Opts> static void op(auto &value, is_context auto &&ctx, auto &&...args) {")
                w(f"        serialize<JSON>::op<Opts>({NAMESPACE}::detail::enum_value(value, glz::from<glz::JSON, {t}>::table), ctx, args...);")
                w("    }")
                w("};")
            else:
                w(f"template <> struct glz::meta<{t}> {{")
                w(f"    using enum {t};")
                w(f"    static constexpr auto value = glz::enumerate({', '.join(f'{json.dumps(v)}, {c}' for v, c in values)});")
                w("};")
        for list_name, alias in VARIANTS.items():
            t = f"{NAMESPACE}::{alias}"
            tags = [self.defs[self.ref_name(self.defs[a]["properties"]["type"])]["const"] for a in self.alternatives(list_name)]
            untagged = self.untagged.get(list_name)
            w(f"template <> struct glz::from<glz::JSON, {t}> {{")
            w(f"    static constexpr std::array<std::string_view, {len(tags)}> tags{{{', '.join(json.dumps(x) for x in tags)}}};")
            w("    template <auto Opts> static void op(auto &value, is_context auto &&ctx, auto &&it, auto end) {")
            w(f"        {NAMESPACE}::detail::read_tagged<Opts>(value, ctx, it, end, tags, "
              f"{'std::nullopt' if untagged is None else f'std::size_t{{{untagged}}}'});")
            w("    }")
            w("};")
        for name in self.order:
            members = self.structs[name]
            if members is None:
                continue
            t = f"{NAMESPACE}::{camel(name)}"
            fields = [f'"{key}", &T::{snake(key)}' for key, _, _ in members]
            if members and members[0][0] == "id":
                fields += [f'"{key}", glz::skip{{}}' for key in SKIPPED_KEYS]
            w(f"template <> struct glz::meta<{t}> {{")
            w(f"    using T = {t};")
            w(f"    static constexpr auto value = glz::object({', '.join(fields)});")
            w("};")
        w("")
        return "\n".join(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="fail if the output is out of date")
    parser.add_argument("schema", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    text = Generator(json.loads(args.schema.read_text(encoding="utf-8"))).emit()
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8") != text:
            print(f"{args.output} is out of date; rerun tools/mnx_codegen.py", file=sys.stderr)
            return 1
        return 0
    args.output.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
