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

#pragma once

#include <map>

#include "global/containers.h"
#include "global/types/translatablestring.h"

#include "../../dom/input.h"

namespace mu::engraving {
struct EngravingObject;
struct Score;
struct Segment;
struct Box;

struct EditData;

struct UndoableCommand;
enum class UndoableCommandFilter : unsigned char;
enum class CommandType : signed char;

struct UndoableTransaction {
    UndoableTransaction(Score *s, const muse::TranslatableString &action_name);
    ~UndoableTransaction();

    void undo();
    void redo();

    void append_command(UndoableCommand *cmd) { commands_.push_back(cmd); }

    void append(UndoableTransaction &&other);

    void unwind(bool clean_up = true);
    void cleanup(bool was_done);

    const std::vector<UndoableCommand *> &commands() const { return commands_; }

    bool empty() const { return commands_.empty(); }

    bool has_commands_matching_filter(UndoableCommandFilter, const EngravingItem *target) const;
    bool has_commands_not_matching_filters(const std::vector<UndoableCommandFilter> &filters, const EngravingItem *target) const;
    void remove_commands_matching_filter(UndoableCommandFilter f, EngravingItem *target);

    const InputState &undo_input_state() const;
    const InputState &redo_input_state() const;

    struct SelectionInfo {
        std::vector<EngravingItem *> elements;
        Fraction tick_start;
        Fraction tick_end;
        staff_idx_t staff_start = muse::nidx;
        staff_idx_t staff_end = muse::nidx;
        Box *start_box = nullptr;
        Box *end_box = nullptr;

        bool is_valid() const { return !elements.empty() || staff_start != muse::nidx; }
    };

    const SelectionInfo &undo_selection_info() const;
    const SelectionInfo &redo_selection_info() const;

    void exclude_element_from_selection_info(EngravingItem *element);
    static bool can_record_selected_element(const EngravingItem *e);

    struct ChangesInfo {
        ElementTypeSet changed_object_types;
        std::map<EngravingObject *, std::unordered_set<CommandType>> changed_objects;
        StyleIdSet changed_style_id_set;
        PropertyIdSet changed_property_id_set;
        bool is_text_editing = false;
    };

    ChangesInfo changes_info(bool undo = false) const;
    const muse::TranslatableString &action_name() const;

private:
    void append_commands(UndoableTransaction &other);

    std::vector<UndoableCommand *> commands_;

    InputState undo_input_state_;
    InputState redo_input_state_;
    SelectionInfo undo_selection_info_;
    SelectionInfo redo_selection_info_;
    muse::TranslatableString action_name_;

    Score *score_ = nullptr;

    static void fill_selection_info(SelectionInfo &, const Selection &);
    static void apply_selection_info(const SelectionInfo &, Selection &);
};

struct UndoStack {
    UndoStack();
    ~UndoStack();

    bool is_locked() const;
    void set_locked(bool locked);

    bool has_active_transaction() const { return active_transaction_ != nullptr; }

    void begin_transaction(Score *, const muse::TranslatableString &action_name);
    void end_transaction(bool rollback);

    bool can_undo() const { return current_index_ > 0; }

    bool can_redo() const { return current_index_ < transactions_.size(); }

    bool is_clean() const { return clean_state_ == states_[current_index_]; }

    size_t size() const { return transactions_.size(); }

    size_t current_index() const { return current_index_; }

    UndoableTransaction *active_transaction() const { return active_transaction_; }

    UndoableTransaction *last() const { return current_index_ > 0 ? transactions_[current_index_ - 1] : nullptr; }

    UndoableTransaction *prev() const { return current_index_ > 1 ? transactions_[current_index_ - 2] : nullptr; }

    UndoableTransaction *next() const { return can_redo() ? transactions_[current_index_] : nullptr; }

    /// Returns the transaction that led to the state with the given `idx`.
    /// For further discussion of the indices involved in UndoStack, see:
    /// https://github.com/musescore/MuseScore/pull/25389#discussion_r1825782176
    UndoableTransaction *last_at_index(size_t idx) const {
        return idx > 0 && idx - 1 < transactions_.size() ? transactions_[idx - 1] : nullptr;
    }

    void undo(EditData *);
    void redo();
    void reopen();

    void merge_transactions(size_t start_idx);

    void clean_redo_stack() { remove(current_index_); }

    // Retire history without changing the currently applied score state.
    bool clear_history();

private:
    void remove(size_t idx);

    UndoableTransaction *active_transaction_ = nullptr;
    std::vector<UndoableTransaction *> transactions_;
    std::vector<int> states_;
    int next_state_ = 0;
    int clean_state_ = 0;
    size_t current_index_ = 0;
    bool is_locked_ = false;
};
} // namespace mu::engraving
