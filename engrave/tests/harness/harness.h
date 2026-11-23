#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engrave/src/api/batch.h"
#include "engrave/src/score/json.h"
#include "global/flatmap.h"
#include "global/fn.h"
#include "global/unique_fn.h"
#include <engrave/engrave.h>

namespace mu::engraving {
struct EngravingObject;
struct MasterScore;
} // namespace mu::engraving

namespace engrave::harness {

using music::array_push;
using music::Json;
using music::json_equal;
using music::make_array;
using music::make_object;
using music::parse_json;

// Native interop observations around a committed edit.
struct Fingerprint {
    muse::FlatMap<std::string, std::string> files; // every file of an in-memory MSCZ: scores, excerpts, styles
    std::string semantic;                          // the semantic dump (no layout, no ids)
    std::vector<std::string> ids;                  // "<id> <type> <score>/<tick>/<track>", sorted
    std::uint64_t revision = 0;
    int engine_error = 0;
    std::vector<std::string> layout;                 // per laid-out score: its pages and segment positions,
                                                     // equal to within a unit of 1/1200 inch
    std::string externals;                           // MIDI mapping, repeats, meta tags, selection, input state
    std::size_t registered = 0;                      // entries in the id register
    muse::FlatMap<std::string, std::size_t> objects; // what the document owns (not layout's), per type
};

Fingerprint fingerprint(Document &document);

// The fields in which two fingerprints differ, by name; `ignore` lists fields to skip.
std::vector<std::string> differences(const Fingerprint &before, const Fingerprint &after, const std::vector<std::string_view> &ignore = {});
// The first difference in the text fields, with some context, for a failure message.
std::string explain(const Fingerprint &before, const Fingerprint &after);

struct OpCase {
    std::string name;    // unique across all groups
    std::string fixture; // a corpus file name
    // The batch, built against the freshly opened fixture.
    muse::UniqueFn<Batch(Document &) const> make;
    // After the commit: what is wrong with the result, or "" when it is right.
    muse::UniqueFn<std::string(Document &after, const Fingerprint &before, const Fingerprint &committed) const> expect;
};

// Registers a group's cases; call it from a namespace-scope initializer.
bool register_cases(std::span<OpCase> cases);

template <size_t N>
bool register_cases(OpCase (&&cases)[N]) {
    return register_cases(std::span<OpCase>(cases));
}

const std::vector<OpCase> &registered_cases();

std::string run_case(Engine &engine, const OpCase &op_case, const std::filesystem::path &fixture);

// --- helpers for cases ------------------------------------------------------

mu::engraving::MasterScore &master_of(Document &document);
// The id of the first element of an MSCX type (in document order) that has one
// and matches; throws when there is none, so a case fails on a fixture that lacks it.
ElementId find_id(Document &document, std::string_view type, muse::FnRef<bool(const mu::engraving::EngravingObject &)> match = {});
// The element an id names; throws when it names nothing live.
mu::engraving::EngravingObject &element(Document &document, const ElementId &id);
// A property of an element, by its catalogue name.
Value property(Document &document, const ElementId &id, std::string_view name);
// The index-th excerpt of the score.
ScoreSel excerpt(Document &document, std::size_t index);

} // namespace engrave::harness
