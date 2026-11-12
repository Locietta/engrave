/* SPDX-License-Identifier: GPL-3.0-only */
#include "sourcechange.h"

#include <algorithm>
#include <stdexcept>

#include "dom/arpeggio.h"
#include "dom/beam.h"
#include "dom/masterscore.h"
#include "dom/slurtie.h"
#include "dom/staff.h"

namespace mu::engraving {
namespace {
SpatiumItemHook spatium_hook = nullptr;

bool appearance_property(const EngravingObject &object, Pid id) {
    return object.is_chord_line() && (std::ranges::find(ChordLine::appearance_properties, id) != ChordLine::appearance_properties.end() ||
                                      id == Pid::APPEARANCE_LINKED_TO_MASTER);
}

uint32_t payload_index(size_t size) {
    if (size >= no_source_payload) throw std::length_error("journal source payload space exhausted");
    return static_cast<uint32_t>(size);
}

SpatiumSources capture_spatium_sources(Score &score) {
    SpatiumSources result;
    const auto capture = [&](EngravingItem *item) {
        if (!item->offset().is_null()) result.offsets.push_back({JournalTarget::capture(*item), item->offset()});
        if (item->is_arpeggio() || item->is_chord_bracket()) {
            auto *arpeggio = to_arpeggio(item);
            if (arpeggio->user_len1() != 0.0 || arpeggio->user_len2() != 0.0)
                result.arpeggios.push_back({JournalTarget::capture(*item), arpeggio->user_len1(), arpeggio->user_len2()});
        }
        if (item->is_beam()) {
            auto *beam = to_beam(item);
            if (beam->user_modified()) {
                const auto target = JournalTarget::capture(*item);
                const auto direction = beam->direction_idx();
                const auto &fragments = beam->beam_fragments();
                for (size_t i = 0; i < fragments.size(); ++i)
                    result.beams.push_back(
                        {target, payload_index(i), direction, fragments[i]->py1[direction], fragments[i]->py2[direction]});
            }
        }
        if (item->is_spanner_segment()) {
            auto *segment = to_spanner_segment(item);
            if (!segment->user_off2().is_null()) result.spanner_ends.push_back({JournalTarget::capture(*item), segment->user_off2()});
        }
        if (item->is_slur_tie_segment()) {
            auto *segment = to_slur_tie_segment(item);
            SlurGripsBefore grips{{}, {}};
            bool modified = false;
            for (size_t i = 0; i < grips.values.size(); ++i) {
                grips.values[i] = segment->ups(static_cast<Grip>(i)).off;
                modified |= !grips.values[i].is_null();
            }
            if (modified) {
                grips.target = JournalTarget::capture(*item);
                result.slurs.push_back(std::move(grips));
            }
        }
        if (item->is_chord_line() && to_chord_line(item)->has_source_offsets())
            result.chord_lines.push_back({JournalTarget::capture(*item), to_chord_line(item)->source_offsets()});
    };
    score.scan_elements(capture);
    for (auto *staff : score.staves()) capture(staff);
    return result;
}
} // namespace

void set_spatium_item_hook(SpatiumItemHook hook) { spatium_hook = hook; }

SpatiumItemHook spatium_item_hook() { return spatium_hook; }

JournalTarget JournalTarget::capture(EngravingObject &object) {
    auto *master = object.master_score();
    return {master, master->eid_register()->enroll(object)};
}

EngravingObject &JournalTarget::object() const {
    if (!document) throw std::logic_error("a journal target has no document");
    auto *result = document->eid_register()->item_from_handle(handle);
    if (!result) throw std::logic_error("a journal source target was destroyed before rollback");
    return *result;
}

PropertyInverse capture_property(EngravingObject &object, Pid id, SourceChangePayloads &payloads) {
    PropertyInverse result{JournalTarget::capture(object), id, object.get_property(id), object.property_flags(id)};
    if (appearance_property(object, id)) {
        result.appearance = payload_index(payloads.appearances.size());
        payloads.appearances.push_back(to_chord_line(&object)->appearance_state());
    }
    return result;
}

void apply_property(EngravingObject &object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path) {
    if (reset_path && id == Pid::PATH && appearance_property(object, id))
        object.reset_property(id);
    else
        object.set_property(id, value);
    object.set_property_flags(id, flags);
}

void restore_property(const PropertyInverse &inverse, const SourceChangePayloads &payloads) {
    auto &object = inverse.target.object();
    if (inverse.appearance != no_source_payload)
        to_chord_line(&object)->restore_appearance(payloads.appearances[inverse.appearance]);
    else
        object.set_property(inverse.id, inverse.before);
    object.set_property_flags(inverse.id, inverse.flags);
}

StyleValuesInverse capture_style_values(Score &score, std::span<const StyleValue> values, SourceChangePayloads &payloads) {
    const auto begin = payload_index(payloads.style_values.size());
    if (values.size() >= no_source_payload - begin) throw std::length_error("journal style payload space exhausted");
    for (const auto &value : values) payloads.style_values.push_back({value.id, score.style().style_v(value.id)});
    StyleValuesInverse result{JournalTarget::capture(score), begin, static_cast<uint32_t>(values.size())};
    if (std::ranges::any_of(
            values, [&](const auto &value) { return value.id == Sid::SPATIUM && value.value != score.style().style_v(Sid::SPATIUM); })) {
        result.spatium_sources = payload_index(payloads.spatium_sources.size());
        payloads.spatium_sources.push_back(capture_spatium_sources(score));
    }
    return result;
}

void restore_style_values(const StyleValuesInverse &inverse, const SourceChangePayloads &payloads) {
    if (!inverse.count) return;
    const auto *spatium = inverse.spatium_sources == no_source_payload ? nullptr : &payloads.spatium_sources[inverse.spatium_sources];
    apply_style_values(*to_score(&inverse.target.object()),
                       std::span<const StyleValue>(payloads.style_values.data() + inverse.begin, inverse.count), spatium);
}

void restore_spatium_sources(const SpatiumSources &sources) {
    for (const auto &before : sources.offsets) to_engraving_item(&before.target.object())->roffset() = before.value;
    for (const auto &before : sources.spanner_ends) to_spanner_segment(&before.target.object())->set_user_off2(before.value);
    for (const auto &before : sources.arpeggios) {
        auto *arpeggio = to_arpeggio(&before.target.object());
        arpeggio->set_user_len1(before.first);
        arpeggio->set_user_len2(before.second);
    }
    for (const auto &before : sources.beams) {
        auto &fragments = to_beam(&before.target.object())->beam_fragments();
        if (before.fragment >= fragments.size()) throw std::logic_error("a source beam fragment disappeared during style rollback");
        fragments[before.fragment]->py1[before.direction] = before.first;
        fragments[before.fragment]->py2[before.direction] = before.second;
    }
    for (const auto &before : sources.slurs) {
        auto *segment = to_slur_tie_segment(&before.target.object());
        for (size_t i = 0; i < before.values.size(); ++i) segment->set_slur_offset(static_cast<Grip>(i), before.values[i]);
    }
    for (const auto &before : sources.chord_lines) to_chord_line(&before.target.object())->read_source_offsets(before.values, 1.0);
}
} // namespace mu::engraving
