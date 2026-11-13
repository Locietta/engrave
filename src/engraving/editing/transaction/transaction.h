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

#pragma once

#include <functional>
#include <memory>

#include "global/types/translatablestring.h"
#include "sourcechange.h"

namespace mu::engraving {
struct Part;
struct Staff;
struct MStaff;
struct SharedPart;
struct StringTunings;
struct StringData;
struct Instrument;
struct Excerpt;
struct EditData;
struct BracketItem;
struct TextBase;
struct Segment;
struct Measure;
struct MeasureBase;
struct ScoreChanges;
struct MasterScore;
struct UndoableCommand;
struct JournalBatch;
struct MutationJournal;
enum class MMRestLink;
struct MMRestCreation;

// A recording view of the single active rollback journal.
struct Transaction {
    Transaction(const Transaction &) = delete;
    Transaction &operator=(const Transaction &) = delete;

    Transaction(Transaction &&) = default;
    Transaction &operator=(Transaction &&) = default;

    void push(UndoableCommand *cmd);
    void push_without_performing(UndoableCommand *cmd);
    void change_property(EngravingObject *object, Pid id, const PropertyValue &value, PropertyFlags flags = PropertyFlags::NOSTYLE,
                         bool reset_path = false);
    void change_style_values(Score &score, std::span<const StyleValue> values);
    void replace_style(Score &score, const MStyle &style, bool overlap = false);
    void change_bracket_property(BracketItem &item, Pid id, const PropertyValue &value, PropertyFlags flags);
    void change_text_line_property(EngravingObject &object, const PropertyValue &value);
    void change_text_format(TextBase &text, Pid id, const PropertyValue &value, PropertyFlags flags);
    void add_element(EngravingItem *item);
    void remove_element(EngravingItem *item);
    void replace_element(EngravingItem *before, EngravingItem *after);
    void reparent_element(EngravingItem *item, EngravingItem *parent, staff_idx_t staff);
    void reparent_segment(Segment *item, Measure *parent, Fraction tick);
    void link_objects(EngravingObject *item, EngravingObject *peer);
    void unlink_object(EngravingObject *item);
    void change_mstaff(Measure *measure, MStaff *row, staff_idx_t index, bool adding);
    void add_mstaff(Measure *measure, std::unique_ptr<MStaff> row, staff_idx_t index);
    void change_staff_tracks(Measure *measure, staff_idx_t first, staff_idx_t last, bool adding);
    void change_staff_brackets(Staff *staff, size_t level, BracketType type, size_t span, bool adding);
    void change_staff_membership(Staff *staff, staff_idx_t index, bool adding);
    void change_system_staff(Staff *staff, bool adding);
    void change_staff_order(Score *score, const std::vector<staff_idx_t> &order);
    void change_staff_hide(Measure *measure, staff_idx_t staff, AutoOnOff value);

    void change_part_membership(Part *part, size_t index, bool adding);
    void change_part_label(Fraction tick, Part *part, const String &value, bool long_name);
    void replace_part_instrument(Part *part, std::unique_ptr<Instrument> value);
    void change_string_data(StringTunings *item, const StringData &value);
    void change_shared_origin(SharedPart *shared, Part *origin, bool connecting);
    void change_excerpt_membership(Excerpt *excerpt, bool adding);

    void insert_time(Score &score, Fraction tick, Fraction length, bool unmanaged = false);
    void change_measures(MeasureBase *first, MeasureBase *last, bool adding, bool move_staff_type_changes = true);
    void change_measure_length(Measure *measure, Fraction length);
    void change_measure_repeat_count(Measure *measure, int count, staff_idx_t staff);
    void change_mm_rest_link(Measure &target, MeasureBase *value, MMRestLink link);
    MMRestCreation create_mm_rest(Measure &source, std::unique_ptr<Measure> overlay);

    /// Test hook for failure injection: when set, it is called for every command
    /// pushed to a real transaction, once before the command is appended
    /// (`appended` false) and once after it was appended and performed (`appended`
    /// true). A hook that throws on the first call owns `cmd`.
    using PushHook = void (*)(UndoableCommand *cmd, bool appended);
    static void set_push_hook(PushHook hook);

private:
    friend struct TransactionManager;
    Transaction(JournalBatch *undoable_transaction);

    static Transaction &dummy();

    JournalBatch *undoable_transaction_ = nullptr;
    bool is_dummy_ = false;

    static inline PushHook s_push_hook = nullptr;
};

struct ReplayScope;

struct TransactionManager {
    explicit TransactionManager(MasterScore *master_score);

    MutationJournal *mutation_journal() const;

    void transaction(const muse::TranslatableString &description, std::function<void(Transaction &)> func);

    void begin_transaction(const muse::TranslatableString &description);
    void end_transaction(bool rollback = false, bool keep_rolled_back_elements = false);
    ScoreChanges prepare_commit();
    void commit_prepared(const ScoreChanges &changes);

    /// Returns the current transaction, if any is active.
    /// Where possible, prefer using `transaction()` instead of this method.
    Transaction *current_transaction() const;

    Transaction &current_or_dummy_transaction();


private:
    friend struct ReplayScope;
    MasterScore *master_score_ = nullptr;

    std::unique_ptr<Transaction> current_transaction_;
    bool notification_started_ = false;
};

// Temporarily use the dummy transaction for derived-state reconstruction.
struct ReplayScope {
    explicit ReplayScope(TransactionManager &manager, bool enabled = true);
    ~ReplayScope();
    ReplayScope(const ReplayScope &) = delete;
    ReplayScope &operator=(const ReplayScope &) = delete;
private:
    TransactionManager &manager_;
    std::unique_ptr<Transaction> saved_;
    bool enabled_;
};
} // namespace mu::engraving
