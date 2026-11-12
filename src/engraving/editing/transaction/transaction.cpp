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

#include <limits>
#include <stdexcept>

#include "../../dom/masterscore.h"
#include "editing/editproperty.h"
#include "editing/editstyle.h"
#include "log.h"
#include "mutationjournal.h"
#include "undoablecommand.h"

using namespace mu::engraving;

static ScoreChanges build_score_changes(const CmdState &cmd_state, const JournalBatch::ChangesInfo &changes, const JournalBatch &batch) {
    int start_tick = cmd_state.start_tick().ticks();
    int end_tick = cmd_state.end_tick().ticks();
    staff_idx_t start_staff = cmd_state.start_staff();
    staff_idx_t end_staff = cmd_state.end_staff();

    for (size_t i = 0; i < batch.entry_count(); ++i) {
        std::optional<ChangedRange> range = batch.changed_range(i);
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
            std::move(changes.changed_types),
            std::move(changes.changed_property_id_set),
            std::move(changes.changed_style_id_set)};
}

//---------------------------------------------------------
// Transaction
//---------------------------------------------------------

Transaction::Transaction(JournalBatch *undoable_transaction) : undoable_transaction_(undoable_transaction) {}

Transaction &Transaction::dummy() {
    static Transaction dummy_transaction(nullptr);
    dummy_transaction.is_dummy_ = true;
    return dummy_transaction;
}

void Transaction::push(UndoableCommand *cmd) {
    if (is_dummy_ || undoable_transaction_->replaying()) {
        std::unique_ptr<UndoableCommand> owned(cmd);
        owned->redo();
        return;
    }

    if (s_push_hook) {
        s_push_hook(cmd, false);
    }

    // NOTE: append the command _before_ performing it. Some commands' redo()
    // may themselves push further commands; those must end up _after_ this
    // command in the transaction, so that undo reverses them first.
    std::unique_ptr<UndoableCommand> owned(cmd);
    undoable_transaction_->append_command(owned.get());
    owned.release();
    cmd->redo();

    if (s_push_hook) {
        s_push_hook(cmd, true);
    }
}

void Transaction::push_without_performing(UndoableCommand *cmd) {
    if (is_dummy_ || undoable_transaction_->replaying()) {
        delete cmd;
        return;
    }

    if (s_push_hook) {
        s_push_hook(cmd, false);
    }

    std::unique_ptr<UndoableCommand> owned(cmd);
    undoable_transaction_->append_command(owned.get());
    owned.release();

    if (s_push_hook) {
        s_push_hook(cmd, true);
    }
}

void Transaction::change_property(EngravingObject *object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path) {
    if (is_dummy_ || undoable_transaction_->replaying()) {
        apply_property(*object, id, value, flags, reset_path);
        return;
    }
    if (!source_object(*object)) {
        push(new ChangeProperty(object, id, value, flags, reset_path));
        return;
    }
    // Preserve the existing fault-injection seam without allocating legacy
    // command objects in production. A throwing pre-hook owns its diagnostic.
    std::unique_ptr<ChangeProperty> diagnostic;
    if (s_push_hook) {
        auto *probe = new ChangeProperty(object, id, value, flags, reset_path);
        s_push_hook(probe, false);
        diagnostic.reset(probe);
    }
    undoable_transaction_->change_property(*object, id, value, flags, reset_path);
    if (s_push_hook && diagnostic) s_push_hook(diagnostic.get(), true);
}

void Transaction::change_style_values(Score &score, std::span<const StyleValue> values) {
    if (is_dummy_ || undoable_transaction_->replaying()) {
        apply_style_values(score, values);
        return;
    }
    std::unique_ptr<ChangeStyleValues> diagnostic;
    if (s_push_hook) {
        std::unordered_map<Sid, PropertyValue> requested;
        for (const auto &value : values) requested.emplace(value.id, value.value);
        auto *probe = new ChangeStyleValues(&score, std::move(requested));
        s_push_hook(probe, false);
        diagnostic.reset(probe);
    }
    undoable_transaction_->change_style_values(score, values);
    if (s_push_hook && diagnostic) s_push_hook(diagnostic.get(), true);
}

void Transaction::set_push_hook(PushHook hook) { s_push_hook = hook; }

//---------------------------------------------------------
// TransactionManager
//---------------------------------------------------------

TransactionManager::TransactionManager(MasterScore *master_score) : master_score_(master_score) {}

ReplayScope::ReplayScope(TransactionManager &manager, bool enabled) : manager_(manager), enabled_(enabled) {
    if (enabled_) saved_ = std::move(manager_.current_transaction_);
}

ReplayScope::~ReplayScope() {
    if (enabled_) manager_.current_transaction_ = std::move(saved_);
}

MutationJournal *TransactionManager::mutation_journal() const { return master_score_->mutation_journal(); }

void TransactionManager::transaction(const muse::TranslatableString &description, std::function<void(Transaction &)> func) {
    IF_ASSERT_FAILED(!current_transaction_) {
        LOGW() << "transaction already active";
        func(*current_transaction_);
        return;
    }
    begin_transaction(description);
    try {
        func(*current_transaction_);
        end_transaction();
    } catch (...) {
        if (mutation_journal()->has_active_transaction()) end_transaction(true);
        throw;
    }
}

void TransactionManager::begin_transaction(const muse::TranslatableString &description) {
    MutationJournal *stack = mutation_journal();

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

ScoreChanges TransactionManager::prepare_commit() {
    auto *journal = mutation_journal();
    if (!journal->has_active_transaction()) throw std::logic_error("no active mutation batch");
    if (master_score_->read_only() || (MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning))
        throw std::runtime_error("the engine rejected the mutation batch");
    if (master_score_->source_revision() == std::numeric_limits<uint64_t>::max()) throw std::overflow_error("source revision exhausted");
    master_score_->update(false, NormalizeMode::TRANSACTION);
    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(true);
    const ScoreChanges provisional =
        build_score_changes(master_score_->cmd_state(), journal->active_transaction()->changes_info(), *journal->active_transaction());
    master_score_->update_automation(provisional);
    return build_score_changes(master_score_->cmd_state(), journal->active_transaction()->changes_info(), *journal->active_transaction());
}

void TransactionManager::commit_prepared(const ScoreChanges &changes) {
    auto *journal = mutation_journal();
    const bool changed = !journal->active_transaction()->empty();
    // Listener reconstruction is fallible, so keep all inverses alive until it succeeds.
    if (changed) {
        notification_started_ = true;
        master_score_->notify_changes(changes);
        if (MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning)
            throw std::runtime_error("the engine rejected batch finalization");
    }
    journal->end_transaction(false);
    current_transaction_.reset();
    master_score_->cmd_state().reset();
    if (changed) master_score_->advance_source_revision();
    notification_started_ = false;
}

void TransactionManager::end_transaction(bool rollback, bool keep_rolled_back_elements) {
    auto *journal = mutation_journal();
    if (journal->is_locked()) return;
    if (!journal->has_active_transaction()) {
        master_score_->update();
        return;
    }
    rollback |= master_score_->read_only() || (MScore::error != MsError::MS_NO_ERROR && !MScore::error_is_warning);
    if (!rollback) {
        try {
            auto changes = prepare_commit();
            commit_prepared(changes);
            return;
        } catch (...) {
            end_transaction(true, keep_rolled_back_elements);
            throw;
        }
    }
    current_transaction_.reset();
    journal->active_transaction()->unwind(!keep_rolled_back_elements);
    master_score_->update(false, NormalizeMode::REPLAY);
    master_score_->invalidate_repeat_list();
    master_score_->update_repeat_list(false);
    master_score_->init_automation();
    if (notification_started_) master_score_->reload_playback_models();
    journal->end_transaction(true);
    master_score_->cmd_state().reset();
    notification_started_ = false;
}

Transaction *TransactionManager::current_transaction() const { return current_transaction_.get(); }

Transaction &TransactionManager::current_or_dummy_transaction() {
    if (current_transaction_) {
        return *current_transaction_;
    } else {
        return Transaction::dummy();
    }
}
