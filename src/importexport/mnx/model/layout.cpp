// SPDX-License-Identifier: GPL-3.0-only
#include "layout.h"

#include <algorithm>
#include <tuple>
#include <utility>
#include <variant>

namespace mu::iex::mnxio::model {

std::optional<std::vector<StaffKey>> staff_keys(const Staff &staff) {
    if (staff.sources.empty()) return std::nullopt;

    struct State {
        StaffKey key;
        std::size_t count = 0;
        bool missing_voice = false;
        std::vector<std::string> voices;
    };

    std::vector<State> states;
    for (const auto &source : staff.sources) {
        const StaffKey key{source.part, source.staff_or_default()};
        auto it = std::ranges::find(states, key, &State::key);
        if (it == states.end()) {
            states.push_back(State{.key = key});
            it = states.end() - 1;
        }
        ++it->count;
        if (!source.voice) {
            it->missing_voice = true;
            continue;
        }
        if (source.voice->empty() || std::ranges::contains(it->voices, *source.voice)) return std::nullopt;
        it->voices.push_back(*source.voice);
    }
    std::vector<StaffKey> keys;
    for (const auto &state : states) {
        if (state.count > 1 && (state.missing_voice || state.voices.size() != state.count)) return std::nullopt;
        keys.push_back(state.key);
    }
    return keys;
}

std::vector<const Staff *> flatten_staves(const SystemLayout &layout) {
    std::vector<const Staff *> staves;
    const auto walk = [&](const auto &self, const std::vector<SystemLayoutItem> &content) -> void {
        for (const auto &item : content) {
            if (const auto *group = std::get_if<StaffGroup>(&item))
                self(self, group->content);
            else
                staves.push_back(&std::get<Staff>(item));
        }
    };
    walk(walk, layout.content);
    return staves;
}

bool is_part_group(const StaffGroup &group) {
    std::optional<std::string> part;
    bool mixed = false;
    const auto walk = [&](const auto &self, const std::vector<SystemLayoutItem> &content) -> void {
        for (const auto &item : content) {
            if (const auto *inner = std::get_if<StaffGroup>(&item)) {
                self(self, inner->content);
                continue;
            }
            for (const auto &source : std::get<Staff>(item).sources) {
                if (!part)
                    part = source.part;
                else if (*part != source.part)
                    mixed = true;
            }
        }
    };
    walk(walk, group.content);
    return part && !mixed;
}

BarlineOverride barline_override(const StaffGroup &group) {
    switch (group.barline_style_or_default()) {
        case StaffGroupBarlineStyle::INDIVIDUAL: return BarlineOverride::NONE;
        case StaffGroupBarlineStyle::INSTRUMENT: return is_part_group(group) ? BarlineOverride::UNIFIED : BarlineOverride::NONE;
        case StaffGroupBarlineStyle::UNIFIED: return BarlineOverride::UNIFIED;
        case StaffGroupBarlineStyle::MENSURSTRICH: return BarlineOverride::MENSURSTRICH;
    }
    return BarlineOverride::NONE;
}

std::vector<LayoutSpan> layout_spans(const SystemLayout &layout) {
    struct Tagged {
        LayoutSpan span;
        std::size_t encounter;
    };

    std::vector<Tagged> tagged;
    std::size_t staff_index = 0;
    // Returns the first and last staff index under `content`, if it has staves.
    const auto walk = [&](const auto &self, const std::vector<SystemLayoutItem> &content,
                          std::size_t depth) -> std::optional<std::pair<std::size_t, std::size_t>> {
        std::optional<std::size_t> first;
        std::optional<std::size_t> last;
        for (const auto &item : content) {
            if (const auto *staff = std::get_if<Staff>(&item)) {
                const std::size_t i = staff_index++;
                LayoutSpan span;
                span.kind = LayoutSpan::Kind::STAFF;
                span.depth = depth + 1; // staff spans sort after the groups starting with them
                span.start = span.end = i;
                span.symbol = staff->symbol;
                span.label = staff->label;
                span.labelref = staff->labelref;
                span.sources = staff_keys(*staff);
                tagged.push_back({std::move(span), tagged.size()});
                first = first.value_or(i);
                last = i;
                continue;
            }
            const auto &group = std::get<StaffGroup>(item);
            const auto range = self(self, group.content, depth + 1);
            if (!range) continue; // a group without staves
            LayoutSpan span;
            span.kind = LayoutSpan::Kind::GROUP;
            span.depth = depth;
            span.start = range->first;
            span.end = range->second;
            span.symbol = group.symbol;
            span.label = group.label;
            span.barline = barline_override(group);
            tagged.push_back({std::move(span), tagged.size()});
            first = first.value_or(range->first);
            last = range->second;
        }
        if (!first) return std::nullopt;
        return std::pair{*first, *last};
    };
    std::ignore = walk(walk, layout.content, 0);
    std::ranges::stable_sort(tagged, [](const Tagged &a, const Tagged &b) {
        if (a.span.start != b.span.start) return a.span.start < b.span.start;
        if (a.span.depth != b.span.depth) return a.span.depth < b.span.depth;
        return a.encounter < b.encounter;
    });
    std::vector<LayoutSpan> spans;
    spans.reserve(tagged.size());
    for (auto &t : tagged) spans.push_back(std::move(t.span));
    return spans;
}

std::vector<LayoutSpan> default_layout_spans(const Root &root) {
    std::vector<LayoutSpan> spans;
    std::size_t staff_index = 0;
    for (const auto &part : root.parts) {
        const int staves = part.staves_or_default();
        if (staves < 1) continue;
        const auto count = static_cast<std::size_t>(staves);
        if (count > 1) {
            LayoutSpan group;
            group.kind = LayoutSpan::Kind::GROUP;
            group.depth = 0;
            group.symbol = StaffSymbol::BRACE;
            group.start = staff_index;
            group.end = staff_index + count - 1;
            group.label = part.name;
            group.barline = BarlineOverride::UNIFIED;
            spans.push_back(std::move(group));
        }
        for (std::size_t s = 0; s < count; ++s) {
            LayoutSpan staff;
            staff.kind = LayoutSpan::Kind::STAFF;
            staff.depth = 1;
            staff.start = staff.end = staff_index++;
            if (count == 1) staff.label = part.name;
            spans.push_back(std::move(staff));
        }
    }
    return spans;
}

const SystemLayout *full_score_layout(const Root &root) {
    if (!root.layouts) return nullptr;
    std::vector<StaffKey> expected;
    for (const auto &part : root.parts) {
        if (!part.id) return nullptr; // a part without an id cannot appear in any layout
        for (int s = 1; s <= part.staves_or_default(); ++s) expected.push_back({*part.id, s});
    }
    for (const auto &layout : *root.layouts) {
        const auto staves = flatten_staves(layout);
        if (staves.size() != expected.size()) continue;
        bool matches = true;
        for (std::size_t i = 0; i < staves.size() && matches; ++i) {
            const auto keys = staff_keys(*staves[i]);
            matches = keys && keys->size() == 1 && keys->front() == expected[i];
        }
        // Matching the expected order one to one also makes every part staff unique.
        if (matches) return &layout;
    }
    return nullptr;
}

} // namespace mu::iex::mnxio::model
