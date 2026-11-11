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

/**
 \file
 Implementation of undo functions.

 The undo system requires calling startUndo() when starting a GUI command
 and calling endUndo() when ending the command. All changes to a score
 in response to a GUI command must be undoable/redoable by executing
 a sequence of low-level undo operations. This sequence is built by the code
 handling the command, by calling one or more undoOp()'s
 between startUndo() and endUndo().
*/

#include "undostack.h"

#include "containers.h"
#include "dom/score.h"
#include "editing/editproperty.h"
#include "editing/editstyle.h"
#include "editing/textedit.h"
#include "editing/transaction/undoablecommand.h"
#include "log.h"

#define LOG_UNDO() \
    if constexpr (false) LOGD()

using namespace mu::engraving;

namespace mu::engraving {
extern Measure *tick2measure(int tick);

static const std::unordered_map<CommandType, CommandType> command_type_inversion{
    {CommandType::ADD_ELEMENT, CommandType::REMOVE_ELEMENT},
    {CommandType::REMOVE_ELEMENT, CommandType::ADD_ELEMENT},

    {CommandType::ADD_BRACKET, CommandType::REMOVE_BRACKET},
    {CommandType::REMOVE_BRACKET, CommandType::ADD_BRACKET},

    {CommandType::ADD_EXCERPT, CommandType::REMOVE_EXCERPT},
    {CommandType::REMOVE_EXCERPT, CommandType::ADD_EXCERPT},

    {CommandType::ADD_SYSTEM_OBJECT_STAFF, CommandType::REMOVE_SYSTEM_OBJECT_STAFF},
    {CommandType::REMOVE_SYSTEM_OBJECT_STAFF, CommandType::ADD_SYSTEM_OBJECT_STAFF},

    {CommandType::INSERT_MEASURES, CommandType::REMOVE_MEASURES},
    {CommandType::REMOVE_MEASURES, CommandType::INSERT_MEASURES},

    {CommandType::INSERT_STAFF, CommandType::REMOVE_STAFF},
    {CommandType::REMOVE_STAFF, CommandType::INSERT_STAFF},

    {CommandType::INSERT_PART, CommandType::REMOVE_PART},
    {CommandType::REMOVE_PART, CommandType::INSERT_PART},

    {CommandType::LINK, CommandType::UNLINK},
    {CommandType::UNLINK, CommandType::LINK},

    {CommandType::ADD_NOTE_PARENTHESES_INFO, CommandType::REMOVE_NOTE_PARENTHESES_INFO},
    {CommandType::REMOVE_NOTE_PARENTHESES_INFO, CommandType::ADD_NOTE_PARENTHESES_INFO},

    {CommandType::CONNECT_SHARED_PART, CommandType::DISCONNECT_SHARED_PART},
    {CommandType::DISCONNECT_SHARED_PART, CommandType::CONNECT_SHARED_PART},
};

//---------------------------------------------------------
//   UndoStack
//---------------------------------------------------------

UndoStack::UndoStack() {
    active_transaction_ = nullptr;
    current_index_ = 0;
    clean_state_ = 0;
    states_.push_back(clean_state_);
    next_state_ = 1;
}

UndoStack::~UndoStack() {
    size_t idx = 0;
    for (auto c : transactions_) {
        c->cleanup(idx++ < current_index_);
    }
    muse::delete_all(transactions_);
}

bool UndoStack::is_locked() const { return is_locked_; }

void UndoStack::set_locked(bool locked) { is_locked_ = locked; }

void UndoStack::begin_transaction(Score *score, const TranslatableString &action_name) {
    if (is_locked_) {
        return;
    }

    IF_ASSERT_FAILED(!active_transaction_) {
        LOGE() << "Transaction already in progress";
        return;
    }

    active_transaction_ = new UndoableTransaction(score, action_name);
}

//---------------------------------------------------------
//   clearHistory
//---------------------------------------------------------

bool UndoStack::clear_history() {
    if (is_locked_ || active_transaction_) {
        return false;
    }
    const bool was_clean = is_clean();
    const int current_state = states_[current_index_];
    remove(0);
    // Note: states_ is always one longer than transactions_ (initial state at index 0).
    // After remove(0), transactions_ is empty and states_ has size 1 (currentIndex_ == 0).
    // Preserve the current document state at index 0 and keep isClean() semantics intact:
    // if the document was clean before clearing history, it remains clean (cleanState_ = currentState);
    // if it was dirty, it remains dirty (currentState != cleanState_).
    states_[0] = current_state;
    if (was_clean) {
        clean_state_ = current_state;
    }
    return true;
}

void UndoStack::remove(size_t idx) {
    assert(idx <= current_index_);
    assert(current_index_ != muse::nidx);
    // remove redo stack
    while (transactions_.size() > current_index_) {
        UndoableTransaction *transaction = muse::take_last(transactions_);
        states_.pop_back();
        transaction->cleanup(false); // delete elements for which UndoableCommand() holds ownership
        delete transaction;
        //            --curIdx;
    }
    while (transactions_.size() > idx) {
        UndoableTransaction *transaction = muse::take_last(transactions_);
        states_.pop_back();
        transaction->cleanup(true);
        delete transaction;
    }
    current_index_ = idx;
}

void UndoStack::merge_transactions(size_t start_idx) {
    assert(start_idx <= current_index_);

    if (start_idx >= transactions_.size()) {
        return;
    }

    UndoableTransaction *start_transaction = transactions_[start_idx];

    for (size_t idx = start_idx + 1; idx < current_index_; ++idx) {
        start_transaction->append(std::move(*transactions_[idx]));
    }
    remove(start_idx + 1); // TODO: remove from startIdx to curIdx only
}

void UndoStack::end_transaction(bool rollback) {
    if (is_locked_) {
        return;
    }

    IF_ASSERT_FAILED(active_transaction_) {
        LOGE() << "No transaction in progress";
        return;
    }

    if (rollback) {
        delete active_transaction_;
    } else {
        // remove redo stack
        while (transactions_.size() > current_index_) {
            UndoableTransaction *transaction = muse::take_last(transactions_);
            states_.pop_back();
            transaction->cleanup(false); // delete elements for which UndoableCommand() holds ownership
            delete transaction;
        }
        transactions_.push_back(active_transaction_);
        states_.push_back(next_state_++);
        ++current_index_;
    }
    active_transaction_ = nullptr;
}

void UndoStack::reopen() {
    if (is_locked_) {
        return;
    }

    LOG_UNDO() << "curIdx: " << current_index_ << ", size: " << transactions_.size();
    assert(active_transaction_ == nullptr);
    assert(current_index_ > 0);
    --current_index_;
    active_transaction_ = muse::take_at(transactions_, current_index_);
    states_.erase(states_.begin() + current_index_ + 1);
    for (auto i : active_transaction_->commands()) {
        LOG_UNDO() << "   " << i->name();
    }
}

void UndoStack::undo(EditData *ed) {
    LOG_UNDO() << "called";
    // Are we currently editing text?
    if (ed && ed->element && ed->element->is_text_base()) {
        TextEditData *ted = dynamic_cast<TextEditData *>(ed->get_data(ed->element).get());
        if (ted && ted->start_undo_idx == current_index_) {
            // No edits to undo, so do nothing
            return;
        }
    }
    if (current_index_) {
        --current_index_;
        assert(current_index_ < transactions_.size());
        transactions_[current_index_]->undo();
    }
}

void UndoStack::redo() {
    LOG_UNDO() << "called";
    if (can_redo()) {
        transactions_[current_index_++]->redo();
    }
}

//---------------------------------------------------------
//   UndoableTransaction
//---------------------------------------------------------

UndoableTransaction::UndoableTransaction(Score *s, const TranslatableString &action_name) : action_name_(action_name), score_(s) {}

UndoableTransaction::~UndoableTransaction() {
    for (UndoableCommand *command : commands_) {
        delete command;
    }
}

void UndoableTransaction::cleanup(bool was_done) {
    for (UndoableCommand *command : commands_) {
        command->cleanup(was_done);
    }
}

void UndoableTransaction::unwind(bool clean_up) {
    // Undo everything first, without freeing: commands pushed later (e.g. AddElement for a part's linked
    // clone) may still be referenced by earlier commands' undo() (e.g. Link::undo()):
    std::vector<UndoableCommand *> undone_commands;
    undone_commands.reserve(commands_.size());
    while (!commands_.empty()) {
        UndoableCommand *command = muse::take_last(commands_);
        LOG_UNDO() << "unwind: " << command->name();
        command->undo();
        undone_commands.push_back(command);
    }
    // Now we can free everything:
    for (UndoableCommand *command : undone_commands) {
        if (clean_up) {
            command->cleanup(false);
        }
        delete command;
    }
}

void UndoableTransaction::append_commands(UndoableTransaction &other) {
    commands_.insert(commands_.end(), other.commands_.cbegin(), other.commands_.cend());
    other.commands_.clear();
}

bool UndoableTransaction::has_commands_matching_filter(UndoableCommandFilter f, const EngravingItem *target) const {
    for (UndoableCommand *command : commands_) {
        if (command->matches_filter(f, target)) {
            return true;
        }
    }
    return false;
}

bool UndoableTransaction::has_commands_not_matching_filters(const std::vector<UndoableCommandFilter> &filters,
                                                            const EngravingItem *target) const {
    for (UndoableCommand *command : commands_) {
        bool filtered = false;
        for (UndoableCommandFilter f : filters) {
            if (command->matches_filter(f, target)) {
                filtered = true;
                break;
            }
        }
        if (!filtered) {
            return true;
        }
    }
    return false;
}

void UndoableTransaction::remove_commands_matching_filter(UndoableCommandFilter f, EngravingItem *target) {
    std::vector<UndoableCommand *> accepted_list;
    for (UndoableCommand *command : commands_) {
        if (command->matches_filter(f, target)) {
            delete command;
        } else {
            accepted_list.push_back(command);
        }
    }
    commands_ = std::move(accepted_list);
}

void UndoableTransaction::undo() {
    score_->deselect_all();

    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        LOG_UNDO() << "<" << (*it)->name() << ">";
        (*it)->undo();
    }
}

void UndoableTransaction::redo() {
    score_->deselect_all();

    for (UndoableCommand *command : commands_) {
        LOG_UNDO() << "<" << command->name() << ">";
        command->redo();
    }
}

void UndoableTransaction::append(UndoableTransaction &&other) { append_commands(other); }

UndoableTransaction::ChangesInfo UndoableTransaction::changes_info(bool undo) const {
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

        if (undo) {
            auto it = ::mu::engraving::command_type_inversion.find(type);
            if (it != ::mu::engraving::command_type_inversion.end()) {
                type = it->second;
            }
        }

        for (EngravingObject *object : command->object_items()) {
            if (!object) {
                continue;
            }

            result.changed_object_types.insert(object->type());
            result.changed_objects[object].insert(type);
        }
    }

    return result;
}

const TranslatableString &UndoableTransaction::action_name() const { return action_name_; }
} // namespace mu::engraving
