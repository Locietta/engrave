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
#include "editing/editstyle.h"
#include "editing/textedit.h"
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
    try {
        source_.styles.push_back(capture_style_values(score, values, source_));
        entries_.push_back({JournalEntryKind::STYLE_VALUES, index});
    } catch (...) {
        source_.styles.resize(index);
        source_.style_values.resize(value_mark);
        source_.spatium_sources.resize(spatium_mark);
        throw;
    }
    apply_style_values(score, values);
}

void JournalBatch::cleanup(bool applied) {
    for (const auto entry : entries_)
        if (entry.kind == JournalEntryKind::LEGACY) legacy_[entry.payload]->cleanup(applied);
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
    entries_.clear();
}

bool JournalBatch::structural_since(size_t mark) const {
    for (size_t i = mark; i < entries_.size(); ++i) {
        const auto entry = entries_[i];
        if (entry.kind != JournalEntryKind::LEGACY) continue;
        switch (legacy_[entry.payload]->type()) {
            case CommandType::CHANGE_PROPERTY:
            case CommandType::CHANGE_STYLE:
            case CommandType::CHANGE_STYLE_VALUES: break;
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
        } else if (type == CommandType::CHANGE_STYLE_VALUES) {
            for (const auto &pair : static_cast<const ChangeStyleValues *>(command)->values())
                result.changed_style_id_set.insert(pair.first);
        } else if (type == CommandType::CHANGE_STYLE) {
            const auto ids = static_cast<const ChangeStyle *>(command)->changed_ids();
            result.changed_style_id_set.insert(ids.begin(), ids.end());
        } else if (type == CommandType::TEXT_EDIT) {
            result.is_text_editing |= static_cast<const TextEditUndoableCommand *>(command)->cursor().editing();
        }
        for (auto *object : command->object_items()) collect_object(result, object, type);
    }
    return result;
}
