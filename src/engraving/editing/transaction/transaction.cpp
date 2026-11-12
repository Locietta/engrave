/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "transaction.h"

#include "../../dom/masterscore.h"
#include "log.h"
#include "undoablecommand.h"
#include "undostack.h"

using namespace mu::engraving;

static ScoreChanges build_score_changes(const CmdState &cmd_state, const UndoableTransaction::ChangesInfo &changes,
                                        const std::vector<UndoableCommand *> &commands) {
    int start_tick = cmd_state.start_tick().ticks();
    int end_tick = cmd_state.end_tick().ticks();
    staff_idx_t start_staff = cmd_state.start_staff();
    staff_idx_t end_staff = cmd_state.end_staff();

    for (const UndoableCommand *cmd : commands) {
        std::optional<ChangedRange> range = cmd->changed_range();
        if (!range) {
            continue;
        }

        int tick_from = range->tick_from.ticks();
        int tick_to = range->tick_to.ticks();

        if (start_tick == -1 || start_tick > tick_from) {
            start_tick = tick_from;
        }
        if (end_tick == -1 || end_tick < tick_to) {
            end_tick = tick_to;
        }

        if (range->staff_idx_from != muse::nidx && (start_staff == muse::nidx || start_staff > range->staff_idx_from)) {
            start_staff = range->staff_idx_from;
        }
        if (range->staff_idx_to != muse::nidx && (end_staff == muse::nidx || end_staff < range->staff_idx_to)) {
            end_staff = range->staff_idx_to;
        }
    }

    for (const auto &pair : changes.changed_objects) {
        if (!pair.first->is_engraving_item()) {
            continue;
        }

        int tick = to_engraving_item(pair.first)->tick().ticks();

        if (start_tick > tick) {
            start_tick = tick;
        }

        if (end_tick < tick) {
            end_tick = tick;
        }
    }

    return {start_tick,
            end_tick,
            start_staff,
            end_staff,
            changes.is_text_editing,
            std::move(changes.changed_objects),
            std::move(changes.changed_object_types),
            std::move(changes.changed_property_id_set),
            std::move(changes.changed_style_id_set)};
}

//---------------------------------------------------------
// Transaction
//---------------------------------------------------------

Transaction::Transaction(UndoableTransaction *undoable_transaction) : undoable_transaction_(undoable_transaction) {}

Transaction &Transaction::dummy() {
    static Transaction dummy_transaction(nullptr);
    dummy_transaction.is_dummy_ = true;
    return dummy_transaction;
}

void Transaction::push(UndoableCommand *cmd) {
    if (is_dummy_) {
        cmd->redo();
        delete cmd;
        return;
    }

    if (s_push_hook) {
        s_push_hook(cmd, false);
    }

    // NOTE: append the command _before_ performing it. Some commands' redo()
    // may themselves push further commands; those must end up _after_ this
    // command in the transaction, so that undo reverses them first.
    undoable_transaction_->append_command(cmd);
    cmd->redo();

    if (s_push_hook) {
        s_push_hook(cmd, true);
    }
}

void Transaction::push_without_performing(UndoableCommand *cmd) {
    if (is_dummy_) {
        delete cmd;
        return;
    }

    if (s_push_hook) {
        s_push_hook(cmd, false);
    }

    undoable_transaction_->append_command(cmd);

    if (s_push_hook) {
        s_push_hook(cmd, true);
    }
}

void Transaction::set_push_hook(PushHook hook) { s_push_hook = hook; }

//---------------------------------------------------------
// TransactionManager
//---------------------------------------------------------

TransactionManager::TransactionManager(MasterScore *master_score) : master_score_(master_score) {}

UndoStack *TransactionManager::undo_stack() const { return master_score_->undo_stack(); }

void TransactionManager::transaction(const muse::TranslatableString &description, std::function<void(Transaction &)> func) {
    IF_ASSERT_FAILED(!current_transaction_) {
        LOGW() << "transaction already active";
        func(*current_transaction_);
        return;
    }
    begin_transaction(description);
    func(*current_transaction_);
    end_transaction();
}

void TransactionManager::begin_transaction(const muse::TranslatableString &description) {
    UndoStack *stack = undo_stack();

    if (stack->is_locked()) {
        return;
    }

    if (stack->has_active_transaction()) {
        LOGD() << "cmd already active";
        return;
    }

    MScore::set_error(MsError::MS_NO_ERROR);

    master_score_->cmd_state().reset();
    stack->begin_transaction(master_score_, description);
    current_transaction_ = std::unique_ptr<Transaction>(new Transaction(stack->active_transaction()));
}

void TransactionManager::end_transaction(bool rollback, bool layout_all_parts, bool keep_rolled_back_elements) {
    UndoStack *stack = undo_stack();

    if (stack->is_locked()) {
        return;
    }

    if (!stack->has_active_transaction()) {
        LOGW() << "no command active";
        master_score_->update();
        return;
    }

    if (master_score_->read_only() || (MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning)) {
        rollback = true;
    }

    if (rollback) {
        /* When keepRolledBackElements is true, unwind removes the rolled-back elements from
         * the score but does not free them: an external owner is responsible for the deferred
         * deletion, since the elements are still referenced after this returns. */
        stack->active_transaction()->unwind(!keep_rolled_back_elements);
    }

    // NOTE: perform update and re-layout within the transaction, so that
    // undoable commands generated by layout are recorded correctly.
    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(false);
    master_score_->update(false, layout_all_parts);

    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(!rollback);

    ScoreChanges changes;
    if (!rollback) {
        const ScoreChanges provisional = build_score_changes(master_score_->cmd_state(), stack->active_transaction()->changes_info(),
                                                             stack->active_transaction()->commands());
        master_score_->update_automation(provisional);
        changes = build_score_changes(master_score_->cmd_state(), stack->active_transaction()->changes_info(),
                                      stack->active_transaction()->commands());
    } else {
        master_score_->init_automation();
    }

    LOGD() << "Undo stack current transaction commands count: " << stack->active_transaction()->commands().size();

    const bool is_current_transaction_empty = stack->active_transaction()->empty(); // nothing to undo?
    stack->end_transaction(is_current_transaction_empty);
    current_transaction_.reset();

    master_score_->cmd_state().reset();

    if (!is_current_transaction_empty && !rollback) {
        master_score_->notify_changes(changes);
    }
}

void TransactionManager::reopen() {
    UndoStack *stack = undo_stack();

    if (stack->is_locked()) {
        return;
    }

    if (stack->has_active_transaction()) {
        LOGD() << "cmd already active";
        return;
    }

    stack->reopen();
    current_transaction_ = std::unique_ptr<Transaction>(new Transaction(stack->active_transaction()));
}

Transaction *TransactionManager::current_transaction() const { return current_transaction_.get(); }

Transaction &TransactionManager::current_or_dummy_transaction() {
    if (current_transaction_) {
        return *current_transaction_;
    } else {
        return Transaction::dummy();
    }
}

void TransactionManager::undo_redo(bool undo, EditData *edit_data) {
    if (master_score_->read_only()) {
        return;
    }

    UndoStack *stack = undo_stack();

    IF_ASSERT_FAILED(!stack->has_active_transaction()) {
        LOGW() << "cannot undo/redo while transaction is active";
        return;
    }

    if (undo) {
        IF_ASSERT_FAILED(stack->can_undo()) {
            LOGW() << "cannot undo";
            return;
        }
    } else {
        IF_ASSERT_FAILED(stack->can_redo()) {
            LOGW() << "cannot redo";
            return;
        }
    }

    master_score_->cmd_state().reset();

    //! NOTE: the order of operations is very important here
    //! 1. for the undo operation, the list of changed elements is available before undo()
    //! 2. for the redo operation, the list of changed elements will be available after redo()
    UndoableTransaction::ChangesInfo changes;
    UndoableTransaction *transaction = nullptr;

    if (undo) {
        transaction = stack->last();
        changes = transaction->changes_info(true);
        stack->undo(edit_data);
    } else {
        stack->redo();
        transaction = stack->last();
        changes = transaction->changes_info(false);
    }

    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(false);
    master_score_->update(false);
    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(false);
    master_score_->update_selection();

    ScoreChanges result = build_score_changes(master_score_->cmd_state(), changes, transaction->commands());
    master_score_->update_automation(result);
    master_score_->notify_changes(result);
}
