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

#include "dom/box.h"
#include "dom/select.h"

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

static const std::unordered_map<CommandType, CommandType> COMMAND_TYPE_INVERSION{
    {CommandType::AddElement, CommandType::RemoveElement},
    {CommandType::RemoveElement, CommandType::AddElement},

    {CommandType::AddBracket, CommandType::RemoveBracket},
    {CommandType::RemoveBracket, CommandType::AddBracket},

    {CommandType::AddExcerpt, CommandType::RemoveExcerpt},
    {CommandType::RemoveExcerpt, CommandType::AddExcerpt},

    {CommandType::AddSystemObjectStaff, CommandType::RemoveSystemObjectStaff},
    {CommandType::RemoveSystemObjectStaff, CommandType::AddSystemObjectStaff},

    {CommandType::InsertMeasures, CommandType::RemoveMeasures},
    {CommandType::RemoveMeasures, CommandType::InsertMeasures},

    {CommandType::InsertStaff, CommandType::RemoveStaff},
    {CommandType::RemoveStaff, CommandType::InsertStaff},

    {CommandType::InsertPart, CommandType::RemovePart},
    {CommandType::RemovePart, CommandType::InsertPart},

    {CommandType::Link, CommandType::Unlink},
    {CommandType::Unlink, CommandType::Link},

    {CommandType::AddNoteParenthesesInfo, CommandType::RemoveNoteParenthesesInfo},
    {CommandType::RemoveNoteParenthesesInfo, CommandType::AddNoteParenthesesInfo},

    {CommandType::ConnectSharedPart, CommandType::DisconnectSharedPart},
    {CommandType::DisconnectSharedPart, CommandType::ConnectSharedPart},
};

//---------------------------------------------------------
//   UndoStack
//---------------------------------------------------------

UndoStack::UndoStack() {
    activeTransaction_ = nullptr;
    currentIndex_ = 0;
    cleanState_ = 0;
    states_.push_back(cleanState_);
    nextState_ = 1;
}

UndoStack::~UndoStack() {
    size_t idx = 0;
    for (auto c : transactions_) {
        c->cleanup(idx++ < currentIndex_);
    }
    muse::DeleteAll(transactions_);
}

bool UndoStack::isLocked() const { return isLocked_; }

void UndoStack::setLocked(bool locked) { isLocked_ = locked; }

void UndoStack::beginTransaction(Score *score, const TranslatableString &actionName) {
    if (isLocked_) {
        return;
    }

    IF_ASSERT_FAILED(!activeTransaction_) {
        LOGE() << "Transaction already in progress";
        return;
    }

    activeTransaction_ = new UndoableTransaction(score, actionName);
}

//---------------------------------------------------------
//   clearHistory
//---------------------------------------------------------

bool UndoStack::clearHistory() {
    if (isLocked_ || activeTransaction_) {
        return false;
    }
    const bool wasClean = isClean();
    const int currentState = states_[currentIndex_];
    remove(0);
    // Note: states_ is always one longer than transactions_ (initial state at index 0).
    // After remove(0), transactions_ is empty and states_ has size 1 (currentIndex_ == 0).
    // Preserve the current document state at index 0 and keep isClean() semantics intact:
    // if the document was clean before clearing history, it remains clean (cleanState_ = currentState);
    // if it was dirty, it remains dirty (currentState != cleanState_).
    states_[0] = currentState;
    if (wasClean) {
        cleanState_ = currentState;
    }
    return true;
}

void UndoStack::remove(size_t idx) {
    assert(idx <= currentIndex_);
    assert(currentIndex_ != muse::nidx);
    // remove redo stack
    while (transactions_.size() > currentIndex_) {
        UndoableTransaction *transaction = muse::takeLast(transactions_);
        states_.pop_back();
        transaction->cleanup(false); // delete elements for which UndoableCommand() holds ownership
        delete transaction;
        //            --curIdx;
    }
    while (transactions_.size() > idx) {
        UndoableTransaction *transaction = muse::takeLast(transactions_);
        states_.pop_back();
        transaction->cleanup(true);
        delete transaction;
    }
    currentIndex_ = idx;
}

void UndoStack::mergeTransactions(size_t startIdx) {
    assert(startIdx <= currentIndex_);

    if (startIdx >= transactions_.size()) {
        return;
    }

    UndoableTransaction *startTransaction = transactions_[startIdx];

    for (size_t idx = startIdx + 1; idx < currentIndex_; ++idx) {
        startTransaction->append(std::move(*transactions_[idx]));
    }
    remove(startIdx + 1); // TODO: remove from startIdx to curIdx only
}

void UndoStack::endTransaction(bool rollback) {
    if (isLocked_) {
        return;
    }

    IF_ASSERT_FAILED(activeTransaction_) {
        LOGE() << "No transaction in progress";
        return;
    }

    if (rollback) {
        delete activeTransaction_;
    } else {
        // remove redo stack
        while (transactions_.size() > currentIndex_) {
            UndoableTransaction *transaction = muse::takeLast(transactions_);
            states_.pop_back();
            transaction->cleanup(false); // delete elements for which UndoableCommand() holds ownership
            delete transaction;
        }
        transactions_.push_back(activeTransaction_);
        states_.push_back(nextState_++);
        ++currentIndex_;
    }
    activeTransaction_ = nullptr;
}

void UndoStack::reopen() {
    if (isLocked_) {
        return;
    }

    LOG_UNDO() << "curIdx: " << currentIndex_ << ", size: " << transactions_.size();
    assert(activeTransaction_ == nullptr);
    assert(currentIndex_ > 0);
    --currentIndex_;
    activeTransaction_ = muse::takeAt(transactions_, currentIndex_);
    states_.erase(states_.begin() + currentIndex_ + 1);
    for (auto i : activeTransaction_->commands()) {
        LOG_UNDO() << "   " << i->name();
    }
}

void UndoStack::undo(EditData *ed) {
    LOG_UNDO() << "called";
    // Are we currently editing text?
    if (ed && ed->element && ed->element->isTextBase()) {
        TextEditData *ted = dynamic_cast<TextEditData *>(ed->getData(ed->element).get());
        if (ted && ted->startUndoIdx == currentIndex_) {
            // No edits to undo, so do nothing
            return;
        }
    }
    if (currentIndex_) {
        --currentIndex_;
        assert(currentIndex_ < transactions_.size());
        transactions_[currentIndex_]->undo();
    }
}

void UndoStack::redo() {
    LOG_UNDO() << "called";
    if (canRedo()) {
        transactions_[currentIndex_++]->redo();
    }
}

//---------------------------------------------------------
//   UndoableTransaction
//---------------------------------------------------------

UndoableTransaction::UndoableTransaction(Score *s, const TranslatableString &actionName)
    : undoInputState_(s->inputState()), actionName_(actionName), score_(s) {
    fillSelectionInfo(undoSelectionInfo_, s->selection());
}

UndoableTransaction::~UndoableTransaction() {
    for (UndoableCommand *command : commands_) {
        delete command;
    }
}

void UndoableTransaction::cleanup(bool wasDone) {
    for (UndoableCommand *command : commands_) {
        command->cleanup(wasDone);
    }
}

void UndoableTransaction::unwind(bool cleanUp) {
    // Undo everything first, without freeing: commands pushed later (e.g. AddElement for a part's linked
    // clone) may still be referenced by earlier commands' undo() (e.g. Link::undo()):
    std::vector<UndoableCommand *> undoneCommands;
    undoneCommands.reserve(commands_.size());
    while (!commands_.empty()) {
        UndoableCommand *command = muse::takeLast(commands_);
        LOG_UNDO() << "unwind: " << command->name();
        command->undo();
        undoneCommands.push_back(command);
    }
    // Now we can free everything:
    for (UndoableCommand *command : undoneCommands) {
        if (cleanUp) {
            command->cleanup(false);
        }
        delete command;
    }
}

void UndoableTransaction::appendCommands(UndoableTransaction &other) {
    commands_.insert(commands_.end(), other.commands_.cbegin(), other.commands_.cend());
    other.commands_.clear();
}

bool UndoableTransaction::hasCommandsMatchingFilter(UndoableCommandFilter f, const EngravingItem *target) const {
    for (UndoableCommand *command : commands_) {
        if (command->matchesFilter(f, target)) {
            return true;
        }
    }
    return false;
}

bool UndoableTransaction::hasCommandsNotMatchingFilters(const std::vector<UndoableCommandFilter> &filters,
                                                        const EngravingItem *target) const {
    for (UndoableCommand *command : commands_) {
        bool filtered = false;
        for (UndoableCommandFilter f : filters) {
            if (command->matchesFilter(f, target)) {
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

void UndoableTransaction::removeCommandsMatchingFilter(UndoableCommandFilter f, EngravingItem *target) {
    std::vector<UndoableCommand *> acceptedList;
    for (UndoableCommand *command : commands_) {
        if (command->matchesFilter(f, target)) {
            delete command;
        } else {
            acceptedList.push_back(command);
        }
    }
    commands_ = std::move(acceptedList);
}

bool UndoableTransaction::canRecordSelectedElement(const EngravingItem *e) {
    if (e->generated()) {
        return false;
    }

    return e->isNote() || (e->isChordRest() && !e->isChord()) ||
           (e->isTextBase() && !e->isInstrumentName() && !e->isHammerOnPullOffText()) || e->isFretDiagram() || e->isSoundFlag();
}

void UndoableTransaction::fillSelectionInfo(SelectionInfo &info, const Selection &sel) {
    info.staffStart = info.staffEnd = muse::nidx;
    info.startBox = info.endBox = nullptr;
    info.elements.clear();

    if (sel.isList()) {
        for (EngravingItem *e : sel.elements()) {
            if (canRecordSelectedElement(e)) {
                info.elements.push_back(e);
            } else {
                // don't remember selection we are unable to restore
                info.elements.clear();
                return;
            }
        }
    } else if (sel.isRange()) {
        info.staffStart = sel.staffStart();
        info.staffEnd = sel.staffEnd();
        info.tickStart = sel.tickStart();
        info.tickEnd = sel.tickEnd();
        info.startBox = sel.startBox();
        info.endBox = sel.endBox();
    }
}

void UndoableTransaction::applySelectionInfo(const SelectionInfo &info, Selection &sel) {
    if (!info.elements.empty()) {
        for (EngravingItem *e : info.elements) {
            sel.add(e);
        }
    } else if (info.staffStart != muse::nidx) {
        sel.setRangeTicks(info.tickStart, info.tickEnd, info.staffStart, info.staffEnd, info.startBox, info.endBox);
    }
}

void UndoableTransaction::undo() {
    redoInputState_ = score_->inputState();
    fillSelectionInfo(redoSelectionInfo_, score_->selection());
    score_->deselectAll();

    for (auto it = commands_.rbegin(); it != commands_.rend(); ++it) {
        LOG_UNDO() << "<" << (*it)->name() << ">";
        (*it)->undo();
    }

    score_->setInputState(undoInputState_);
    if (undoSelectionInfo_.isValid()) {
        score_->deselectAll();
        applySelectionInfo(undoSelectionInfo_, score_->selection());
    }
}

void UndoableTransaction::redo() {
    undoInputState_ = score_->inputState();
    fillSelectionInfo(undoSelectionInfo_, score_->selection());
    score_->deselectAll();

    for (UndoableCommand *command : commands_) {
        LOG_UNDO() << "<" << command->name() << ">";
        command->redo();
    }

    score_->setInputState(redoInputState_);
    if (redoSelectionInfo_.isValid()) {
        score_->deselectAll();
        applySelectionInfo(redoSelectionInfo_, score_->selection());
    }
}

void UndoableTransaction::append(UndoableTransaction &&other) {
    appendCommands(other);
    if (score_ == other.score_) {
        redoInputState_ = std::move(other.redoInputState_);
        redoSelectionInfo_ = std::move(other.redoSelectionInfo_);
    }
}

const InputState &UndoableTransaction::undoInputState() const { return undoInputState_; }

const InputState &UndoableTransaction::redoInputState() const { return redoInputState_; }

void UndoableTransaction::excludeElementFromSelectionInfo(EngravingItem *element) {
    if (undoSelectionInfo_.isValid()) {
        muse::remove(undoSelectionInfo_.elements, element);
    }

    if (redoSelectionInfo_.isValid()) {
        muse::remove(redoSelectionInfo_.elements, element);
    }
}

const UndoableTransaction::SelectionInfo &UndoableTransaction::undoSelectionInfo() const { return undoSelectionInfo_; }

const UndoableTransaction::SelectionInfo &UndoableTransaction::redoSelectionInfo() const { return redoSelectionInfo_; }

UndoableTransaction::ChangesInfo UndoableTransaction::changesInfo(bool undo) const {
    ChangesInfo result;

    for (const UndoableCommand *command : commands()) {
        CommandType type = command->type();

        if (type == CommandType::ChangeProperty) {
            auto changeProperty = static_cast<const ChangeProperty *>(command);
            result.changedPropertyIdSet.insert(changeProperty->getId());
        } else if (type == CommandType::ChangeStyleValues) {
            auto changeStyle = static_cast<const ChangeStyleValues *>(command);
            for (const auto &pair : changeStyle->values()) {
                result.changedStyleIdSet.insert(pair.first);
            }
        } else if (type == CommandType::ChangeStyle) {
            auto changeStyle = static_cast<const ChangeStyle *>(command);
            const StyleIdSet styleIds = changeStyle->changedIds();
            result.changedStyleIdSet.insert(styleIds.cbegin(), styleIds.cend());
        } else if (type == CommandType::TextEdit) {
            result.isTextEditing |= static_cast<const TextEditUndoableCommand *>(command)->cursor().editing();
        }

        if (undo) {
            auto it = COMMAND_TYPE_INVERSION.find(type);
            if (it != COMMAND_TYPE_INVERSION.end()) {
                type = it->second;
            }
        }

        for (EngravingObject *object : command->objectItems()) {
            if (!object) {
                continue;
            }

            result.changedObjectTypes.insert(object->type());
            result.changedObjects[object].insert(type);
        }
    }

    return result;
}

const TranslatableString &UndoableTransaction::actionName() const { return actionName_; }
} // namespace mu::engraving
