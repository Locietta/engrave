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

#include <vector>

#include "dom/score.h"
#include "global/types/translatablestring.h"
#include "style/styledef.h"
#include "types/types.h"

namespace mu::engraving {
struct Score;
struct EngravingItem;
struct EngravingObject;
struct UndoableCommand;
enum class CommandType : signed char;

// Transitional legacy-command adapter: one forward application, inverse replay
// only on failure, then disposal. It never retains a committed batch for replay.
struct JournalBatch {
    JournalBatch() = default;
    JournalBatch(const JournalBatch &) = delete;
    JournalBatch &operator=(const JournalBatch &) = delete;
    ~JournalBatch();

    void append_command(UndoableCommand *command) { commands_.push_back(command); }

    void unwind(bool clean_up = true);

    bool replaying() const { return replaying_; }

    void cleanup(bool applied);

    const std::vector<UndoableCommand *> &commands() const { return commands_; }

    bool empty() const { return commands_.empty(); }

    // Existing playback/automation metadata, retained while command families migrate.
    using ChangesInfo = ScoreChanges;
    ChangesInfo changes_info() const;
private:
    std::vector<UndoableCommand *> commands_;
    bool replaying_ = false;
};

struct MutationJournal {
    MutationJournal() = default;
    MutationJournal(const MutationJournal &) = delete;
    MutationJournal &operator=(const MutationJournal &) = delete;
    ~MutationJournal();

    bool is_locked() const { return is_locked_; }

    void set_locked(bool locked) { is_locked_ = locked; }

    bool has_active_transaction() const { return active_transaction_ != nullptr; }

    JournalBatch *active_transaction() const { return active_transaction_; }

    // Active entries only; an idle document owns no rollback payloads.
    size_t size() const { return active_transaction_ ? active_transaction_->commands().size() : 0; }

    void begin_transaction(Score *, const muse::TranslatableString &);
    void end_transaction(bool rollback) noexcept;
    void abandon() noexcept;
private:
    JournalBatch *active_transaction_ = nullptr;
    bool is_locked_ = false;
};
} // namespace mu::engraving
