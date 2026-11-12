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

#include "editing/editproperty.h"
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
    for (const auto entry : entries_)
        if (entry.kind == JournalEntryKind::LEGACY) legacy_[entry.payload]->cleanup(applied);
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

void JournalBatch::unwind(bool clean_up) {
    struct ReplayGuard {
        bool &flag;

        explicit ReplayGuard(bool &value) : flag(value) { flag = true; }

        ~ReplayGuard() { flag = false; }
    } replay(replaying_);

    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
        switch (it->kind) {
            case JournalEntryKind::LEGACY: legacy_[it->payload]->undo(); break;
            case JournalEntryKind::PROPERTY: restore_property(source_.properties[it->payload], source_); break;
            case JournalEntryKind::STYLE_VALUES: restore_style_values(source_.styles[it->payload], source_); break;
            case JournalEntryKind::STYLE_REPLACEMENT: restore_style_replacement(source_.style_replacements[it->payload], source_); break;
            case JournalEntryKind::BRACKET_PROPERTY: restore_bracket_property(brackets_[it->payload]); break;
            case JournalEntryKind::TEXT_LINE_PROPERTY: restore_text_line_property(text_lines_[it->payload]); break;
            case JournalEntryKind::TEXT_FORMAT: restore_text_format(text_formats_[it->payload]); break;
        }
    }
    if (clean_up)
        for (auto it = entries_.rbegin(); it != entries_.rend(); ++it)
            if (it->kind == JournalEntryKind::LEGACY) legacy_[it->payload]->cleanup(false);
    for (auto it = legacy_.rbegin(); it != legacy_.rend(); ++it) delete *it;
    legacy_.clear();
    source_.properties.clear();
    source_.appearances.clear();
    source_.styles.clear();
    source_.style_values.clear();
    source_.spatium_sources.clear();
    source_.style_replacements.clear();
    source_.chord_lists.clear();
    brackets_.clear();
    text_lines_.clear();
    text_formats_.clear();
    entries_.clear();
}

bool JournalBatch::structural_since(size_t mark) const {
    for (size_t i = mark; i < entries_.size(); ++i) {
        const auto entry = entries_[i];
        if (entry.kind == JournalEntryKind::TEXT_FORMAT) return true;
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
