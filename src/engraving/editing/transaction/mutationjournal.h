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

#include "derivedchange.h"
#include "dom/score.h"
#include "elementchange.h"
#include "global/types/translatablestring.h"
#include "sourcechange.h"
#include "textchange.h"
#include "timingchange.h"
#include "undoablecommand.h"

namespace mu::engraving {
// Closed family dispatch; source payloads are stored separately by family.
enum class JournalEntryKind : uint8_t {
    LEGACY,
    PROPERTY,
    AUTHORED_OVERRIDE,
    STYLE_VALUES,
    BRACKET_PROPERTY,
    TEXT_LINE_PROPERTY,
    TEXT_FORMAT,
    STYLE_REPLACEMENT,
    ELEMENT_ADD,
    ELEMENT_REMOVE,
    ELEMENT_REPLACE,
    ELEMENT_REPARENT,
    SEGMENT_REPARENT,
    LINK,
    TIME_INSERTION,
    MEASURE_MEMBERSHIP,
    MEASURE_LENGTH,
    MEASURE_REPEAT_COUNT
};

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
    void remember_authored_overrides(EngravingObject &object, Pid id);
    void change_style_values(Score &score, std::span<const StyleValue> values);
    void replace_style(Score &score, const MStyle &style, bool overlap);
    void change_bracket_property(BracketItem &item, Pid id, const PropertyValue &value, PropertyFlags flags);
    void change_text_line_property(EngravingObject &object, const PropertyValue &value);
    void change_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags);
    void change_element_membership(EngravingItem &item, bool adding);
    void replace_element(EngravingItem &before, EngravingItem &after);
    void reparent_element(EngravingItem &item, EngravingItem &parent, staff_idx_t staff);
    void reparent_segment(Segment &item, Measure &parent, Fraction tick);
    void change_link(EngravingObject &item, EngravingObject *peer);
    void insert_time(Score &score, Fraction tick, Fraction length, bool unmanaged = false);
    void change_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes = true);
    void change_measure_length(Measure *measure, Fraction length);
    void change_measure_repeat_count(Measure *measure, int count, staff_idx_t staff);
    void unwind(bool clean_up = true);

    bool replaying() const { return replaying_; }

    void cleanup(bool applied);

    size_t entry_count() const { return entries_.size(); }

    bool empty() const { return entries_.empty(); }

    bool structural_since(size_t mark) const;
    std::optional<ChangedRange> changed_range(size_t index) const;

    size_t property_entries() const { return source_.properties.size(); }

    size_t authored_override_entries() const { return source_.authored_overrides.size(); }

    size_t style_entries() const { return source_.styles.size(); }

    size_t style_replacement_entries() const { return source_.style_replacements.size(); }

    size_t legacy_entries() const { return legacy_.size(); }

    size_t derived_entries() const { return brackets_.size() + text_lines_.size(); }

    size_t text_format_entries() const { return text_formats_.size(); }

    size_t compound_entries() const {
        return elements_.size() + replacements_.size() + parents_.size() + segment_parents_.size() + links_.size();
    }

    size_t timing_entries() const { return times_.size() + measures_.size() + measure_lengths_.size() + measure_repeats_.size(); }

    // Existing playback/automation metadata, retained while command families migrate.
    using ChangesInfo = ScoreChanges;
    ChangesInfo changes_info() const;
private:
    amc::vector<JournalEntry> entries_;
    amc::vector<UndoableCommand *> legacy_;
    SourceChangePayloads source_;
    amc::vector<BracketPropertyInverse> brackets_;
    amc::vector<TextLinePropertyInverse> text_lines_;
    amc::vector<TextFormatInverse> text_formats_;
    amc::vector<ElementMembershipInverse> elements_;
    amc::vector<ElementReplacementInverse> replacements_;
    amc::vector<ElementParentInverse> parents_;
    amc::vector<SegmentParentInverse> segment_parents_;
    amc::vector<LinkInverse> links_;
    amc::vector<TimeInsertionInverse> times_;
    amc::vector<MeasureMembershipInverse> measures_;
    amc::vector<MeasureLengthInverse> measure_lengths_;
    amc::vector<MeasureRepeatCountInverse> measure_repeats_;
    void dispose_entry(JournalEntry entry, bool committed);
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
