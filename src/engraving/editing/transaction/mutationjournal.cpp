/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mutationjournal.h"

#include <stdexcept>

#include "dom/excerpt.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/sharedpart.h"
#include "dom/stringtunings.h"
#include "editing/addremoveelement.h"
#include "editing/editmeasures.h"
#include "editing/editproperty.h"
#include "editing/inserttime.h"
#include "log.h"

using namespace mu::engraving;

namespace {
uint32_t entry_index(size_t size) {
    if (size >= no_source_payload) throw std::length_error("journal entry space exhausted");
    return static_cast<uint32_t>(size);
}

void collect_object(ScoreChanges &changes, EngravingObject *object, CommandType type) {
    if (!object) return;
    changes.changed_types.insert(object->type());
    changes.changed_objects[object].insert(type);
}
} // namespace

MutationJournal::~MutationJournal() { abandon(); }

void MutationJournal::begin_transaction(Score *, const muse::TranslatableString &) {
    if (is_locked_) return;
    IF_ASSERT_FAILED(!active_transaction_) { return; }
    active_transaction_ = new JournalBatch;
}

void MutationJournal::end_transaction(bool rollback) noexcept {
    if (!active_transaction_) return;
    if (!rollback) active_transaction_->cleanup(true);
    delete active_transaction_;
    active_transaction_ = nullptr;
}

void MutationJournal::abandon() noexcept {
    // Snapshot replacement owns the DOM. Do not run unsafe inverse/disposal hooks.
    delete active_transaction_;
    active_transaction_ = nullptr;
}

JournalBatch::~JournalBatch() {
    for (auto *command : legacy_) delete command;
    dispose_excerpts();
}

void JournalBatch::append_command(UndoableCommand *command) {
    const auto index = entry_index(legacy_.size());
    legacy_.push_back(command);
    try {
        entries_.push_back({JournalEntryKind::LEGACY, index});
    } catch (...) {
        legacy_.pop_back();
        throw;
    }
}

void JournalBatch::change_property(EngravingObject &object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path) {
    remember_authored_overrides(object, id);
    const auto index = entry_index(source_.properties.size());
    const auto appearance_mark = source_.appearances.size();
    try {
        source_.properties.push_back(capture_property(object, id, source_));
        entries_.push_back({JournalEntryKind::PROPERTY, index});
    } catch (...) {
        source_.properties.resize(index);
        source_.appearances.resize(appearance_mark);
        throw;
    }
    // The inverse is fully owned before a setter or a nested edit can throw.
    apply_property(object, id, value, flags, reset_path);
}

void JournalBatch::remember_authored_overrides(EngravingObject &object, Pid id) {
    auto before = capture_authored_overrides(object, id);
    if (!before) return;
    const auto index = entry_index(source_.authored_overrides.size());
    source_.authored_overrides.push_back(std::move(*before));
    try {
        entries_.push_back({JournalEntryKind::AUTHORED_OVERRIDE, index});
    } catch (...) {
        source_.authored_overrides.pop_back();
        throw;
    }
}

void JournalBatch::change_style_values(Score &score, std::span<const StyleValue> values) {
    const auto index = entry_index(source_.styles.size());
    const auto value_mark = source_.style_values.size();
    const auto spatium_mark = source_.spatium_sources.size();
    const auto chord_mark = source_.chord_lists.size();
    try {
        source_.styles.push_back(capture_style_values(score, values, source_));
        entries_.push_back({JournalEntryKind::STYLE_VALUES, index});
    } catch (...) {
        source_.styles.resize(index);
        source_.style_values.resize(value_mark);
        source_.spatium_sources.resize(spatium_mark);
        source_.chord_lists.resize(chord_mark);
        throw;
    }
    apply_style_values(score, values);
}

void JournalBatch::replace_style(Score &score, const MStyle &style, bool overlap) {
    const auto index = entry_index(source_.style_replacements.size());
    const auto spatium_mark = source_.spatium_sources.size();
    const auto chord_mark = source_.chord_lists.size();
    try {
        source_.style_replacements.push_back(capture_style_replacement(score, style, source_));
        entries_.push_back({JournalEntryKind::STYLE_REPLACEMENT, index});
    } catch (...) {
        source_.style_replacements.resize(index);
        source_.spatium_sources.resize(spatium_mark);
        source_.chord_lists.resize(chord_mark);
        throw;
    }
    apply_style_replacement(score, style, overlap);
}

void JournalBatch::cleanup(bool applied) {
    for (const auto entry : entries_) dispose_entry(entry, applied);
}

void JournalBatch::dispose_entry(JournalEntry entry, bool committed) {
    switch (entry.kind) {
        case JournalEntryKind::LEGACY: legacy_[entry.payload]->cleanup(committed); break;
        case JournalEntryKind::PART_MEMBERSHIP: dispose_part_membership(parts_[entry.payload], committed); break;
        case JournalEntryKind::ELEMENT_ADD: dispose_element_membership(elements_[entry.payload], true, committed); break;
        case JournalEntryKind::ELEMENT_REMOVE: dispose_element_membership(elements_[entry.payload], false, committed); break;
        default: break;
    }
}

void JournalBatch::change_element_membership(EngravingItem &item, bool adding) {
    const auto index = entry_index(elements_.size());
    elements_.push_back({JournalTarget::capture(item)});
    try {
        entries_.push_back({adding ? JournalEntryKind::ELEMENT_ADD : JournalEntryKind::ELEMENT_REMOVE, index});
    } catch (...) {
        elements_.pop_back();
        throw;
    }
    if (adding)
        apply_element_add(item);
    else
        apply_element_remove(item);
}

void JournalBatch::replace_element(EngravingItem &before, EngravingItem &after) {
    if (before.master_score() != after.master_score()) throw std::logic_error("replacement cannot span documents");
    const auto index = entry_index(replacements_.size());
    replacements_.push_back({JournalTarget::capture(before), JournalTarget::capture(after)});
    try {
        entries_.push_back({JournalEntryKind::ELEMENT_REPLACE, index});
    } catch (...) {
        replacements_.pop_back();
        throw;
    }
    apply_element_replace(before, after);
}

void JournalBatch::reparent_element(EngravingItem &item, EngravingItem &parent, staff_idx_t staff) {
    if (item.master_score() != parent.master_score()) throw std::logic_error("reparent cannot span documents");
    const auto index = entry_index(parents_.size());
    parents_.push_back({JournalTarget::capture(item), JournalTarget::capture(*item.ownership_parent_item()), item.track()});
    try {
        entries_.push_back({JournalEntryKind::ELEMENT_REPARENT, index});
    } catch (...) {
        parents_.pop_back();
        throw;
    }
    apply_element_reparent(item, parent, staff * voices + item.voice());
}

void JournalBatch::reparent_segment(Segment &item, Measure &parent, Fraction tick) {
    if (item.master_score() != parent.master_score()) throw std::logic_error("segment reparent cannot span documents");
    const auto index = entry_index(segment_parents_.size());
    segment_parents_.push_back({JournalTarget::capture(item), JournalTarget::capture(*item.measure()), item.rtick()});
    try {
        entries_.push_back({JournalEntryKind::SEGMENT_REPARENT, index});
    } catch (...) {
        segment_parents_.pop_back();
        throw;
    }
    apply_segment_reparent(item, parent, tick);
}

void JournalBatch::change_link(EngravingObject &item, EngravingObject *peer) {
    const auto index = entry_index(links_.size());
    links_.push_back(capture_link(item, peer));
    try {
        entries_.push_back({JournalEntryKind::LINK, index});
    } catch (...) {
        links_.pop_back();
        throw;
    }
    apply_link(links_[index]);
}

void JournalBatch::change_bracket_property(BracketItem &item, Pid id, const PropertyValue &value, PropertyFlags flags) {
    const auto index = entry_index(brackets_.size());
    brackets_.push_back(capture_bracket_property(item, id));
    try {
        entries_.push_back({JournalEntryKind::BRACKET_PROPERTY, index});
    } catch (...) {
        brackets_.pop_back();
        throw;
    }
    apply_bracket_property(brackets_[index], value, flags);
}

void JournalBatch::change_text_line_property(EngravingObject &object, const PropertyValue &value) {
    const auto index = entry_index(text_lines_.size());
    text_lines_.push_back(capture_text_line_property(object));
    try {
        entries_.push_back({JournalEntryKind::TEXT_LINE_PROPERTY, index});
    } catch (...) {
        text_lines_.pop_back();
        throw;
    }
    apply_text_line_property(object, value);
}

void JournalBatch::change_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags) {
    remember_authored_overrides(text, id);
    // Tuplet number font properties delegate to the typed source style. Restoring
    // just the text cursor/markup would leave those source values changed.
    if (auto *owner = text.ownership_parent();
        owner && owner->is_tuplet() && (id == Pid::FONT_FACE || id == Pid::FONT_SIZE || id == Pid::FONT_STYLE)) {
        const auto index = entry_index(source_.properties.size());
        source_.properties.push_back(capture_property(*owner, id, source_));
        try {
            entries_.push_back({JournalEntryKind::PROPERTY, index});
        } catch (...) {
            source_.properties.pop_back();
            throw;
        }
    }
    const auto index = entry_index(text_formats_.size());
    text_formats_.push_back(capture_text_format(text, id));
    try {
        entries_.push_back({JournalEntryKind::TEXT_FORMAT, index});
    } catch (...) {
        text_formats_.pop_back();
        throw;
    }
    apply_text_format(text, id, value, flags);
}

void JournalBatch::insert_time(Score &score, Fraction tick, Fraction length, bool unmanaged) {
    const auto index = entry_index(times_.size());
    times_.push_back({JournalTarget::capture(score), tick, length, unmanaged});
    try {
        entries_.push_back({JournalEntryKind::TIME_INSERTION, index});
    } catch (...) {
        times_.pop_back();
        throw;
    }
    apply_time_insertion(score, tick, length, unmanaged);
}

void JournalBatch::change_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes) {
    if (first->master_score() != last->master_score()) throw std::logic_error("measure chain cannot span documents");
    const auto index = entry_index(measures_.size());
    measures_.push_back({JournalTarget::capture(*first), JournalTarget::capture(*last), adding, move_staff_type_changes});
    try {
        entries_.push_back({JournalEntryKind::MEASURE_MEMBERSHIP, index});
    } catch (...) {
        measures_.pop_back();
        throw;
    }
    if (adding)
        insert_measure_chain(first, last, move_staff_type_changes);
    else
        remove_measure_chain(first, last, move_staff_type_changes);
}

void JournalBatch::change_measure_length(Measure *measure, Fraction length) {
    const auto index = entry_index(measure_lengths_.size());
    measure_lengths_.push_back({JournalTarget::capture(*measure), measure->ticks()});
    try {
        entries_.push_back({JournalEntryKind::MEASURE_LENGTH, index});
    } catch (...) {
        measure_lengths_.pop_back();
        throw;
    }
    apply_measure_length(measure, length);
}

void JournalBatch::change_measure_repeat_count(Measure *measure, int count, staff_idx_t staff) {
    const auto index = entry_index(measure_repeats_.size());
    measure_repeats_.push_back({JournalTarget::capture(*measure), measure->measure_repeat_count(staff), staff});
    try {
        entries_.push_back({JournalEntryKind::MEASURE_REPEAT_COUNT, index});
    } catch (...) {
        measure_repeats_.pop_back();
        throw;
    }
    apply_measure_repeat_count(measure, count, staff);
}

void JournalBatch::change_part_membership(Part *part, size_t index, bool adding) {
    if (!adding && index == muse::nidx) index = muse::index_of(part->score()->parts(), part);
    const auto i = entry_index(parts_.size());
    parts_.push_back({JournalTarget::capture(*part), index, adding});
    try {
        entries_.push_back({JournalEntryKind::PART_MEMBERSHIP, i});
    } catch (...) {
        parts_.pop_back();
        throw;
    }
    apply_part_membership(*part, index, adding);
}

void JournalBatch::change_part_label(Fraction tick, Part *part, const String &value, bool long_name) {
    const auto i = entry_index(labels_.size());
    labels_.push_back({JournalTarget::capture(*part), tick, long_name ? part->long_name(tick) : part->short_name(tick), long_name});
    try {
        entries_.push_back({JournalEntryKind::PART_LABEL, i});
    } catch (...) {
        labels_.pop_back();
        throw;
    }
    apply_part_label(*part, tick, value, long_name);
}

void JournalBatch::replace_part_instrument(Part *part, std::unique_ptr<Instrument> value) {
    const auto i = entry_index(instruments_.size());
    instruments_.push_back({JournalTarget::capture(*part), std::move(value)});
    try {
        entries_.push_back({JournalEntryKind::PART_INSTRUMENT, i});
    } catch (...) {
        instruments_.pop_back();
        throw;
    }
    exchange_part_instrument(*part, instruments_[i].displaced);
    instruments_[i].applied = true;
    refresh_part_instrument(*part);
}

void JournalBatch::change_string_data(StringTunings *item, const StringData &value) {
    const auto i = entry_index(string_data_.size());
    string_data_.push_back({JournalTarget::capture(*item), *item->string_data()});
    try {
        entries_.push_back({JournalEntryKind::STRING_DATA, i});
    } catch (...) {
        string_data_.pop_back();
        throw;
    }
    item->set_string_data(value);
}

void JournalBatch::change_shared_origin(SharedPart *shared, Part *origin, bool connecting) {
    const auto &members = shared->origin_parts();
    const auto position = muse::index_of(members, origin);
    const auto i = entry_index(origins_.size());
    origins_.push_back({JournalTarget::capture(*shared), JournalTarget::capture(*origin), position, position != muse::nidx});
    try {
        entries_.push_back({JournalEntryKind::SHARED_ORIGIN, i});
    } catch (...) {
        origins_.pop_back();
        throw;
    }
    if (connecting)
        shared->add_origin_part(origin);
    else
        shared->remove_origin_part(origin);
}

uint32_t JournalBatch::retain_excerpt(Excerpt &excerpt) {
    if (excerpt.retaining_journal_) {
        if (excerpt.retaining_journal_ != this) throw std::logic_error("excerpt belongs to another active journal");
        return excerpt.retained_index_;
    }
    const auto index = entry_index(excerpts_.size());
    excerpts_.push_back({&excerpt, JournalTarget::capture(*excerpt.master_score())});
    excerpt.retaining_journal_ = this;
    excerpt.retained_index_ = index;
    return index;
}

void JournalBatch::forget_excerpt(uint32_t reference) { excerpts_[reference].object = nullptr; }

void JournalBatch::dispose_excerpts() {
    for (auto &reference : excerpts_) {
        auto *excerpt = reference.object;
        if (!excerpt) continue;
        excerpt->retaining_journal_ = nullptr;
        reference.object = nullptr;
        auto &master = *to_score(&reference.master.object())->master_score();
        if (reference.retained && !muse::contains(master.excerpts(), excerpt)) delete excerpt;
    }
}

void JournalBatch::change_excerpt_membership(Excerpt *excerpt, bool adding) {
    auto &master = *excerpt->master_score();
    const auto reference = retain_excerpt(*excerpt);
    const auto index = muse::index_of(master.excerpts(), excerpt);
    const auto i = entry_index(excerpt_memberships_.size());
    excerpt_memberships_.push_back({reference, index, adding});
    try {
        entries_.push_back({JournalEntryKind::EXCERPT_MEMBERSHIP, i});
    } catch (...) {
        excerpt_memberships_.pop_back();
        throw;
    }
    excerpts_[reference].retained = true;
    if (adding)
        master.add_excerpt(excerpt);
    else
        master.remove_excerpt(excerpt);
}

void JournalBatch::unwind(bool clean_up) {
    struct ReplayGuard {
        bool &flag;

        explicit ReplayGuard(bool &value) : flag(value) { flag = true; }

        ~ReplayGuard() { flag = false; }
    } replay(replaying_);

    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        switch (it->kind) {
            case JournalEntryKind::PART_MEMBERSHIP: {
                const auto &r = parts_[it->payload];
                apply_part_membership(journal_part(r.target), r.index, !r.adding);
            } break;
            case JournalEntryKind::PART_LABEL: {
                const auto &r = labels_[it->payload];
                apply_part_label(journal_part(r.target), r.tick, r.before, r.long_name);
            } break;
            case JournalEntryKind::PART_INSTRUMENT: restore_part_instrument(instruments_[it->payload]); break;
            case JournalEntryKind::STRING_DATA: {
                const auto &r = string_data_[it->payload];
                to_string_tunings(&r.target.object())->set_string_data(r.before);
            } break;
            case JournalEntryKind::SHARED_ORIGIN: restore_shared_origin(origins_[it->payload]); break;
            case JournalEntryKind::EXCERPT_MEMBERSHIP: {
                const auto &r = excerpt_memberships_[it->payload];
                auto &reference = excerpts_[r.reference];
                if (!reference.object) throw std::logic_error("excerpt lifetime ended before rollback");
                auto &master = *to_score(&reference.master.object())->master_score();
                if (r.adding)
                    master.remove_excerpt(reference.object);
                else
                    master.add_excerpt(reference.object, r.index);
            } break;
            case JournalEntryKind::LEGACY: legacy_[it->payload]->undo(); break;
            case JournalEntryKind::PROPERTY: restore_property(source_.properties[it->payload], source_); break;
            case JournalEntryKind::AUTHORED_OVERRIDE: restore_authored_overrides(source_.authored_overrides[it->payload]); break;
            case JournalEntryKind::STYLE_VALUES: restore_style_values(source_.styles[it->payload], source_); break;
            case JournalEntryKind::STYLE_REPLACEMENT: restore_style_replacement(source_.style_replacements[it->payload], source_); break;
            case JournalEntryKind::BRACKET_PROPERTY: restore_bracket_property(brackets_[it->payload]); break;
            case JournalEntryKind::TEXT_LINE_PROPERTY: restore_text_line_property(text_lines_[it->payload]); break;
            case JournalEntryKind::TEXT_FORMAT: restore_text_format(text_formats_[it->payload]); break;
            case JournalEntryKind::ELEMENT_ADD: revert_element_add(*to_engraving_item(&elements_[it->payload].target.object())); break;
            case JournalEntryKind::ELEMENT_REMOVE:
                revert_element_remove(*to_engraving_item(&elements_[it->payload].target.object()));
                break;
            case JournalEntryKind::ELEMENT_REPLACE: {
                const auto &entry = replacements_[it->payload];
                apply_element_replace(*to_engraving_item(&entry.after.object()), *to_engraving_item(&entry.before.object()));
            } break;
            case JournalEntryKind::ELEMENT_REPARENT: {
                const auto &entry = parents_[it->payload];
                apply_element_reparent(*to_engraving_item(&entry.target.object()), *to_engraving_item(&entry.parent.object()), entry.track);
            } break;
            case JournalEntryKind::SEGMENT_REPARENT: {
                const auto &entry = segment_parents_[it->payload];
                apply_segment_reparent(*to_segment(&entry.target.object()), *to_measure(&entry.parent.object()), entry.tick);
            } break;
            case JournalEntryKind::LINK: restore_link(links_[it->payload]); break;
            case JournalEntryKind::TIME_INSERTION: {
                const auto &record = times_[it->payload];
                apply_time_insertion(*to_score(&record.target.object()), record.tick, -record.length, record.unmanaged);
            } break;
            case JournalEntryKind::MEASURE_MEMBERSHIP: {
                const auto &record = measures_[it->payload];
                auto *first = to_measure_base(&record.first.object());
                auto *last = to_measure_base(&record.last.object());
                if (record.adding)
                    remove_measure_chain(first, last, record.move_staff_type_changes);
                else
                    insert_measure_chain(first, last, record.move_staff_type_changes);
            } break;
            case JournalEntryKind::MEASURE_LENGTH: {
                const auto &record = measure_lengths_[it->payload];
                apply_measure_length(to_measure(&record.target.object()), record.before);
            } break;
            case JournalEntryKind::MEASURE_REPEAT_COUNT: {
                const auto &record = measure_repeats_[it->payload];
                apply_measure_repeat_count(to_measure(&record.target.object()), record.before, record.staff);
            } break;
        }
    }
    if (clean_up)
        for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) dispose_entry(*it, false);
    for (auto it = legacy_.rbegin(); it != legacy_.rend(); ++it) delete *it;
    legacy_.clear();
    source_.properties.clear();
    source_.authored_overrides.clear();
    source_.appearances.clear();
    source_.styles.clear();
    source_.style_values.clear();
    source_.spatium_sources.clear();
    source_.style_replacements.clear();
    source_.chord_lists.clear();
    brackets_.clear();
    text_lines_.clear();
    text_formats_.clear();
    elements_.clear();
    replacements_.clear();
    parents_.clear();
    segment_parents_.clear();
    links_.clear();
    times_.clear();
    measures_.clear();
    measure_lengths_.clear();
    measure_repeats_.clear();
    parts_.clear();
    labels_.clear();
    instruments_.clear();
    string_data_.clear();
    origins_.clear();
    excerpt_memberships_.clear();
    entries_.clear();
}

bool JournalBatch::structural_since(size_t mark) const {
    for (size_t i = mark; i < entries_.size(); ++i) {
        const auto entry = entries_[i];
        switch (entry.kind) {
            case JournalEntryKind::TEXT_FORMAT:
            case JournalEntryKind::ELEMENT_ADD:
            case JournalEntryKind::ELEMENT_REMOVE:
            case JournalEntryKind::ELEMENT_REPLACE:
            case JournalEntryKind::ELEMENT_REPARENT:
            case JournalEntryKind::SEGMENT_REPARENT:
            case JournalEntryKind::LINK:
            case JournalEntryKind::TIME_INSERTION:
            case JournalEntryKind::MEASURE_MEMBERSHIP:
            case JournalEntryKind::MEASURE_LENGTH:
            case JournalEntryKind::PART_MEMBERSHIP:
            case JournalEntryKind::PART_LABEL:
            case JournalEntryKind::PART_INSTRUMENT:
            case JournalEntryKind::STRING_DATA:
            case JournalEntryKind::SHARED_ORIGIN:
            case JournalEntryKind::EXCERPT_MEMBERSHIP:
            case JournalEntryKind::MEASURE_REPEAT_COUNT: return true;
            default: break;
        }
        if (entry.kind != JournalEntryKind::LEGACY) continue;
        switch (legacy_[entry.payload]->type()) {
            case CommandType::CHANGE_PROPERTY: break;
            default: return true;
        }
    }
    return false;
}

std::optional<ChangedRange> JournalBatch::changed_range(size_t index) const {
    const auto entry = entries_[index];
    return entry.kind == JournalEntryKind::LEGACY ? legacy_[entry.payload]->changed_range() : std::nullopt;
}

JournalBatch::ChangesInfo JournalBatch::changes_info() const {
    ChangesInfo result;
    for (const auto entry : entries_) {
        if (entry.kind == JournalEntryKind::TIME_INSERTION) {
            const auto &record = times_[entry.payload];
            collect_object(result, &record.target.object(),
                           record.unmanaged ? CommandType::INSERT_TIME_UNMANAGED_SPANNER : CommandType::INSERT_TIME);
            continue;
        }
        if (entry.kind == JournalEntryKind::MEASURE_MEMBERSHIP) {
            const auto &record = measures_[entry.payload];
            const auto type = record.adding ? CommandType::INSERT_MEASURES : CommandType::REMOVE_MEASURES;
            collect_object(result, &record.first.object(), type);
            collect_object(result, &record.last.object(), type);
            continue;
        }
        if (entry.kind == JournalEntryKind::MEASURE_LENGTH) {
            collect_object(result, &measure_lengths_[entry.payload].target.object(), CommandType::CHANGE_MEASURE_LEN);
            continue;
        }
        if (entry.kind == JournalEntryKind::MEASURE_REPEAT_COUNT) {
            collect_object(result, &measure_repeats_[entry.payload].target.object(), CommandType::CHANGE_MEASURE_REPEAT_COUNT);
            continue;
        }
        if (entry.kind == JournalEntryKind::PART_MEMBERSHIP) {
            const auto &r = parts_[entry.payload];
            collect_object(result, &r.target.object(), r.adding ? CommandType::INSERT_PART : CommandType::REMOVE_PART);
            continue;
        }
        if (entry.kind == JournalEntryKind::PART_LABEL) {
            const auto &r = labels_[entry.payload];
            collect_object(result, &r.target.object(),
                           r.long_name ? CommandType::CHANGE_INSTRUMENT_LONG : CommandType::CHANGE_INSTRUMENT_SHORT);
            continue;
        }
        if (entry.kind == JournalEntryKind::PART_INSTRUMENT) {
            collect_object(result, &instruments_[entry.payload].target.object(), CommandType::CHANGE_PART);
            continue;
        }
        if (entry.kind == JournalEntryKind::STRING_DATA || entry.kind == JournalEntryKind::SHARED_ORIGIN) continue;
        if (entry.kind == JournalEntryKind::EXCERPT_MEMBERSHIP) {
            const auto &r = excerpt_memberships_[entry.payload];
            collect_object(result, &excerpts_[r.reference].master.object(),
                           r.adding ? CommandType::ADD_EXCERPT : CommandType::REMOVE_EXCERPT);
            continue;
        }
        if (entry.kind == JournalEntryKind::AUTHORED_OVERRIDE) continue;
        if (entry.kind == JournalEntryKind::ELEMENT_ADD || entry.kind == JournalEntryKind::ELEMENT_REMOVE) {
            const auto type = entry.kind == JournalEntryKind::ELEMENT_ADD ? CommandType::ADD_ELEMENT : CommandType::REMOVE_ELEMENT;
            for (auto *object : compound_objects(&elements_[entry.payload].target.object())) collect_object(result, object, type);
            continue;
        }
        if (entry.kind == JournalEntryKind::ELEMENT_REPLACE) {
            collect_object(result, &replacements_[entry.payload].before.object(), CommandType::CHANGE_ELEMENT);
            collect_object(result, &replacements_[entry.payload].after.object(), CommandType::CHANGE_ELEMENT);
            continue;
        }
        if (entry.kind == JournalEntryKind::ELEMENT_REPARENT) {
            collect_object(result, &parents_[entry.payload].target.object(), CommandType::CHANGE_PARENT);
            collect_object(result, &parents_[entry.payload].parent.object(), CommandType::CHANGE_PARENT);
            continue;
        }
        if (entry.kind == JournalEntryKind::SEGMENT_REPARENT) {
            collect_object(result, &segment_parents_[entry.payload].target.object(), CommandType::CHANGE_PARENT);
            continue;
        }
        if (entry.kind == JournalEntryKind::LINK) continue;
        if (entry.kind == JournalEntryKind::STYLE_REPLACEMENT) {
            const auto &replacement = source_.style_replacements[entry.payload];
            auto &score = *to_score(&replacement.target.object());
            for (int i = 0; i < static_cast<int>(Sid::STYLES); ++i) {
                const auto id = static_cast<Sid>(i);
                if (score.style().style_v(id) != replacement.before.style_v(id)) result.changed_style_id_set.insert(id);
            }
            collect_object(result, &score, CommandType::CHANGE_STYLE);
            continue;
        }
        if (entry.kind == JournalEntryKind::TEXT_FORMAT) {
            collect_object(result, &text_formats_[entry.payload].target.object(), CommandType::TEXT_EDIT);
            continue;
        }
        if (entry.kind == JournalEntryKind::BRACKET_PROPERTY) {
            result.changed_property_id_set.insert(brackets_[entry.payload].id);
            continue;
        }
        if (entry.kind == JournalEntryKind::TEXT_LINE_PROPERTY) {
            const auto &property = text_lines_[entry.payload].property;
            result.changed_property_id_set.insert(property.id);
            for (auto *object : compound_objects(&property.target.object())) collect_object(result, object, CommandType::CHANGE_PROPERTY);
            continue;
        }
        if (entry.kind == JournalEntryKind::PROPERTY) {
            const auto &property = source_.properties[entry.payload];
            result.changed_property_id_set.insert(property.id);
            for (auto *object : compound_objects(&property.target.object())) collect_object(result, object, CommandType::CHANGE_PROPERTY);
            continue;
        }
        if (entry.kind == JournalEntryKind::STYLE_VALUES) {
            const auto &style = source_.styles[entry.payload];
            for (uint32_t i = 0; i < style.count; ++i) result.changed_style_id_set.insert(source_.style_values[style.begin + i].id);
            collect_object(result, &style.target.object(), CommandType::CHANGE_STYLE_VALUES);
            continue;
        }
        const auto *command = legacy_[entry.payload];
        const auto type = command->type();
        if (type == CommandType::CHANGE_PROPERTY) {
            result.changed_property_id_set.insert(static_cast<const ChangeProperty *>(command)->get_id());
        }
        for (auto *object : command->object_items()) collect_object(result, object, type);
    }
    return result;
}
