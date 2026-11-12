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

#include "editing/editproperty.h"
#include "editing/editstyle.h"
#include "editing/textedit.h"
#include "editing/transaction/undoablecommand.h"
#include "log.h"

using namespace mu::engraving;

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
    for (UndoableCommand *command : commands_) {
        delete command;
    }
}

void JournalBatch::cleanup(bool was_done) {
    for (UndoableCommand *command : commands_) {
        command->cleanup(was_done);
    }
}

void JournalBatch::unwind(bool clean_up) {
    // Some music helpers unwind the active journal directly. Suppress recording
    // through every Transaction view, including references held by those helpers.
    struct ReplayGuard {
        bool &flag;

        explicit ReplayGuard(bool &value) : flag(value) { flag = true; }

        ~ReplayGuard() { flag = false; }
    } replay(replaying_);

    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) (*it)->undo();
    // All inverses finish before any disposal; retain the legacy reverse cleanup
    // order so children are disposed before the enclosing command's payload.
    if (clean_up)
        for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) (*it)->cleanup(false);
    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) delete *it;
    commands_.clear();
}

JournalBatch::ChangesInfo JournalBatch::changes_info() const {
    ChangesInfo result;

    for (const UndoableCommand *command : commands()) {
        CommandType type = command->type();

        if (type == CommandType::CHANGE_PROPERTY) {
            auto change_property = static_cast<const ChangeProperty *>(command);
            result.changed_property_id_set.insert(change_property->get_id());
        } else if (type == CommandType::CHANGE_STYLE_VALUES) {
            auto change_style = static_cast<const ChangeStyleValues *>(command);
            for (const auto &pair : change_style->values()) {
                result.changed_style_id_set.insert(pair.first);
            }
        } else if (type == CommandType::CHANGE_STYLE) {
            auto change_style = static_cast<const ChangeStyle *>(command);
            const StyleIdSet style_ids = change_style->changed_ids();
            result.changed_style_id_set.insert(style_ids.cbegin(), style_ids.cend());
        } else if (type == CommandType::TEXT_EDIT) {
            result.is_text_editing |= static_cast<const TextEditUndoableCommand *>(command)->cursor().editing();
        }

        for (EngravingObject *object : command->object_items()) {
            if (!object) {
                continue;
            }

            result.changed_types.insert(object->type());
            result.changed_objects[object].insert(type);
        }
    }

    return result;
}
