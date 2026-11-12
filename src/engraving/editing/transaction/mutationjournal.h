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

#include <cstdint>
#include <vector>

#include <amc/vector.hpp>

#include "dom/score.h"
#include "global/types/translatablestring.h"
#include "sourcechange.h"
#include "undoablecommand.h"

namespace mu::engraving {
enum class JournalEntryKind : uint8_t { LEGACY, PROPERTY, STYLE_VALUES };

struct JournalEntry {
    JournalEntryKind kind;
    uint32_t payload;
};

// One forward application, failure-only inverse replay, then disposal.
// Converted families own flat payloads; remaining commands use the legacy adapter.
struct JournalBatch {
    JournalBatch() = default;
    JournalBatch(const JournalBatch &) = delete;
    JournalBatch &operator=(const JournalBatch &) = delete;
    ~JournalBatch();

    void append_command(UndoableCommand *command);
    void change_property(EngravingObject &object, Pid id, const PropertyValue &value, PropertyFlags flags, bool reset_path);
    void change_style_values(Score &score, std::span<const StyleValue> values);
    void unwind(bool clean_up = true);

    bool replaying() const { return replaying_; }

    void cleanup(bool applied);

    size_t entry_count() const { return entries_.size(); }

    bool empty() const { return entries_.empty(); }

    bool structural_since(size_t mark) const;
    std::optional<ChangedRange> changed_range(size_t index) const;

    size_t property_entries() const { return source_.properties.size(); }

    size_t style_entries() const { return source_.styles.size(); }

    size_t legacy_entries() const { return legacy_.size(); }

    // Existing playback/automation metadata, retained while command families migrate.
    using ChangesInfo = ScoreChanges;
    ChangesInfo changes_info() const;
private:
    amc::vector<JournalEntry> entries_;
    amc::vector<UndoableCommand *> legacy_;
    SourceChangePayloads source_;
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

    size_t size() const { return active_transaction_ ? active_transaction_->entry_count() : 0; }

    void begin_transaction(Score *, const muse::TranslatableString &);
    void end_transaction(bool rollback) noexcept;
    void abandon() noexcept;
private:
    JournalBatch *active_transaction_ = nullptr;
    bool is_locked_ = false;
};
} // namespace mu::engraving
