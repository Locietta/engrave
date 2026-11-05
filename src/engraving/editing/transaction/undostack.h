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
class EngravingObject;
class Score;
class Segment;
class Box;

class EditData;

class UndoableCommand;
enum class UndoableCommandFilter : unsigned char;
enum class CommandType : signed char;

class UndoableTransaction {
public:
    UndoableTransaction(Score *s, const muse::TranslatableString &actionName);
    ~UndoableTransaction();

    void undo();
    void redo();

    void appendCommand(UndoableCommand *cmd) { commands_.push_back(cmd); }

    void append(UndoableTransaction &&other);

    void unwind(bool cleanUp = true);
    void cleanup(bool wasDone);

    const std::vector<UndoableCommand *> &commands() const { return commands_; }

    bool empty() const { return commands_.empty(); }

    bool hasCommandsMatchingFilter(UndoableCommandFilter, const EngravingItem *target) const;
    bool hasCommandsNotMatchingFilters(const std::vector<UndoableCommandFilter> &filters, const EngravingItem *target) const;
    void removeCommandsMatchingFilter(UndoableCommandFilter f, EngravingItem *target);

    const InputState &undoInputState() const;
    const InputState &redoInputState() const;

    struct SelectionInfo {
        std::vector<EngravingItem *> elements;
        Fraction tickStart;
        Fraction tickEnd;
        staff_idx_t staffStart = muse::nidx;
        staff_idx_t staffEnd = muse::nidx;
        Box *startBox = nullptr;
        Box *endBox = nullptr;

        bool isValid() const { return !elements.empty() || staffStart != muse::nidx; }
    };

    const SelectionInfo &undoSelectionInfo() const;
    const SelectionInfo &redoSelectionInfo() const;

    void excludeElementFromSelectionInfo(EngravingItem *element);
    static bool canRecordSelectedElement(const EngravingItem *e);

    struct ChangesInfo {
        ElementTypeSet changedObjectTypes;
        std::map<EngravingObject *, std::unordered_set<CommandType>> changedObjects;
        StyleIdSet changedStyleIdSet;
        PropertyIdSet changedPropertyIdSet;
        bool isTextEditing = false;
    };

    ChangesInfo changesInfo(bool undo = false) const;
    const muse::TranslatableString &actionName() const;

private:
    void appendCommands(UndoableTransaction &other);

    std::vector<UndoableCommand *> commands_;

    InputState undoInputState_;
    InputState redoInputState_;
    SelectionInfo undoSelectionInfo_;
    SelectionInfo redoSelectionInfo_;
    muse::TranslatableString actionName_;

    Score *score_ = nullptr;

    static void fillSelectionInfo(SelectionInfo &, const Selection &);
    static void applySelectionInfo(const SelectionInfo &, Selection &);
};

class UndoStack {
public:
    UndoStack();
    ~UndoStack();

    bool isLocked() const;
    void setLocked(bool locked);

    bool hasActiveTransaction() const { return activeTransaction_ != nullptr; }

    void beginTransaction(Score *, const muse::TranslatableString &actionName);
    void endTransaction(bool rollback);

    bool canUndo() const { return currentIndex_ > 0; }

    bool canRedo() const { return currentIndex_ < transactions_.size(); }

    bool isClean() const { return cleanState_ == states_[currentIndex_]; }

    size_t size() const { return transactions_.size(); }

    size_t currentIndex() const { return currentIndex_; }

    UndoableTransaction *activeTransaction() const { return activeTransaction_; }

    UndoableTransaction *last() const { return currentIndex_ > 0 ? transactions_[currentIndex_ - 1] : nullptr; }

    UndoableTransaction *prev() const { return currentIndex_ > 1 ? transactions_[currentIndex_ - 2] : nullptr; }

    UndoableTransaction *next() const { return canRedo() ? transactions_[currentIndex_] : nullptr; }

    /// Returns the transaction that led to the state with the given `idx`.
    /// For further discussion of the indices involved in UndoStack, see:
    /// https://github.com/musescore/MuseScore/pull/25389#discussion_r1825782176
    UndoableTransaction *lastAtIndex(size_t idx) const {
        return idx > 0 && idx - 1 < transactions_.size() ? transactions_[idx - 1] : nullptr;
    }

    void undo(EditData *);
    void redo();
    void reopen();

    void mergeTransactions(size_t startIdx);

    void cleanRedoStack() { remove(currentIndex_); }

    // Retire history without changing the currently applied score state.
    bool clearHistory();

private:
    void remove(size_t idx);

    UndoableTransaction *activeTransaction_ = nullptr;
    std::vector<UndoableTransaction *> transactions_;
    std::vector<int> states_;
    int nextState_ = 0;
    int cleanState_ = 0;
    size_t currentIndex_ = 0;
    bool isLocked_ = false;
};
} // namespace mu::engraving
