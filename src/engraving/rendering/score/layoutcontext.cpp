/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#include "layoutcontext.h"

#include "dom/chord.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/spanner.h"
#include "editing/addremoveelement.h"
#include "editing/editpagelocks.h"
#include "editing/editsystemlocks.h"
#include "editing/transaction/transaction.h"
#include "log.h"
#include "style/defaultstyle.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

// ================================================
// LayoutConfiguration
// ================================================

LayoutConfiguration::LayoutConfiguration(LayoutContext *s) : get_score_(s) {}

const Score *LayoutConfiguration::score() const {
    const Score *score = get_score_->score();
    IF_ASSERT_FAILED(score) { return nullptr; }
    return score;
}

const LayoutOptions &LayoutConfiguration::options() const { return score()->layout_options(); }

bool LayoutConfiguration::is_printing_mode() const {
    IF_ASSERT_FAILED(score()) { return false; }

    return score()->printing();
}

std::shared_ptr<const EngravingFont> LayoutConfiguration::engraving_font() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->engraving_font();
}

const MStyle &LayoutConfiguration::style() const {
    IF_ASSERT_FAILED(score()) { return DefaultStyle::default_style(); }
    return score()->style();
}

bool LayoutConfiguration::is_show_invisible() const {
    IF_ASSERT_FAILED(score()) { return false; }
    return score()->is_show_invisible();
}

int LayoutConfiguration::page_number_offset() const {
    IF_ASSERT_FAILED(score()) { return 0; }
    return score()->page_number_offset();
}

bool LayoutConfiguration::is_vertical_spread_enabled() const {
    return style_b(Sid::ENABLE_VERTICAL_SPREAD) && (view_mode() != LayoutMode::SYSTEM);
}

double LayoutConfiguration::max_system_distance() const {
    if (is_vertical_spread_enabled()) {
        return style().style_absolute(Sid::MAX_SYSTEM_SPREAD);
    } else {
        return style().style_absolute(Sid::MAX_SYSTEM_DISTANCE);
    }
}

bool LayoutConfiguration::is_show_instrument_names() const { return score()->show_instrument_names(); }

// ================================================
// DomAccessor
// ================================================

DomAccessor::DomAccessor(LayoutContext *s) : get_score_(s) {}

const Score *DomAccessor::score() const { return get_score_->score(); }

Score *DomAccessor::score() { return get_score_->score(); }

const std::vector<Part *> &DomAccessor::parts() const {
    IF_ASSERT_FAILED(score()) {
        static const std::vector<Part *> dummy;
        return dummy;
    }
    return score()->parts();
}

size_t DomAccessor::visible_part_count() const {
    IF_ASSERT_FAILED(score()) { return 0; }
    return score()->visible_part_count();
}

std::vector<Part *> DomAccessor::visible_parts() const {
    IF_ASSERT_FAILED(score()) { return {}; }
    return score()->visible_parts();
}

size_t DomAccessor::npages() const {
    IF_ASSERT_FAILED(score()) { return 0; }
    return score()->npages();
}

const std::vector<Page *> &DomAccessor::pages() const {
    IF_ASSERT_FAILED(score()) {
        static const std::vector<Page *> dummy;
        return dummy;
    }
    return score()->pages();
}

std::vector<Page *> &DomAccessor::pages() {
    IF_ASSERT_FAILED(score()) {
        static std::vector<Page *> dummy;
        return dummy;
    }
    return score()->pages();
}

const std::vector<System *> &DomAccessor::systems() const {
    IF_ASSERT_FAILED(score()) {
        static const std::vector<System *> dummy;
        return dummy;
    }
    return score()->systems();
}

std::vector<System *> &DomAccessor::systems() {
    IF_ASSERT_FAILED(score()) {
        static std::vector<System *> dummy;
        return dummy;
    }
    return score()->systems();
}

size_t DomAccessor::nstaves() const {
    IF_ASSERT_FAILED(score()) { return 0; }
    return score()->nstaves();
}

size_t DomAccessor::visible_staff_count() const {
    IF_ASSERT_FAILED(score()) { return 0; }

    return score()->visible_staves_count();
}

const std::vector<Staff *> &DomAccessor::staves() const {
    IF_ASSERT_FAILED(score()) {
        static const std::vector<Staff *> dummy;
        return dummy;
    }
    return score()->staves();
}

const Staff *DomAccessor::staff(staff_idx_t idx) const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->staff(idx);
}

bool DomAccessor::all_staves_invisible() const {
    IF_ASSERT_FAILED(score()) { return false; }
    return score()->all_staves_invisible();
}

size_t DomAccessor::ntracks() const {
    IF_ASSERT_FAILED(score()) { return 0; }
    return score()->ntracks();
}

size_t DomAccessor::nmeasures() const {
    IF_ASSERT_FAILED(score()) { return 0; }
    return score()->nmeasures();
}

const Measure *DomAccessor::tick2measure(const Fraction &tick) const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->tick2measure(tick);
}

const Measure *DomAccessor::tick2measure_mm(const Fraction &tick) const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->tick2measure_mm(tick);
}

const Measure *DomAccessor::first_measure() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->first_measure();
}

Measure *DomAccessor::first_measure() {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->first_measure();
}

const Measure *DomAccessor::last_measure() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->last_measure();
}

const Measure *DomAccessor::last_measure_mm() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->last_measure_mm();
}

Measure *DomAccessor::last_measure() {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->last_measure();
}

Measure *DomAccessor::last_measure_mm() {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->last_measure_mm();
}

Measure *DomAccessor::tick2measure(const Fraction &tick) {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->tick2measure(tick);
}

Measure *DomAccessor::tick2measure_mm(const Fraction &tick) {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->tick2measure_mm(tick);
}

const SpannerMap &DomAccessor::spanner_map() const {
    IF_ASSERT_FAILED(score()) {
        static const SpannerMap dummy;
        return dummy;
    }
    return score()->spanner_map();
}

const Segment *DomAccessor::last_segment() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->last_segment();
}

const ChordRest *DomAccessor::find_cr(Fraction tick, track_idx_t track) const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->find_cr(tick, track);
}

const RangeLocks *DomAccessor::system_locks() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->system_locks();
}

const RangeLocks *DomAccessor::page_locks() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->page_locks();
}

const PaddingTable &DomAccessor::padding_table() const { return score()->padding_tables().horizontal_padding_table(); }

const std::vector<BracketItem *> &DomAccessor::brackets(staff_idx_t staff_idx) const { return score()->brackets(staff_idx); }

ChordRest *DomAccessor::find_cr(Fraction tick, track_idx_t track) {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->find_cr(tick, track);
}

MeasureBase *DomAccessor::first() {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->first();
}

DummyParent *DomAccessor::dummy_parent() const {
    IF_ASSERT_FAILED(score()) { return nullptr; }
    return score()->dummy();
}

void DomAccessor::do_undo_add_element(EngravingItem *item) {
    if (item->generated()) {
        add_element(item);
    } else {
        undo(new AddElement(item));
    }
}

void DomAccessor::undo_add_element(EngravingItem *item, bool add_to_linked_staves, bool ctrl_modifier) {
    IF_ASSERT_FAILED(score()) { return; }
    score()->undo_add_element(item, add_to_linked_staves, ctrl_modifier);
}

void DomAccessor::do_undo_remove_element(EngravingItem *item) {
    if (item->generated()) {
        remove_element(item);
        item->delete_later();
    } else {
        undo(new RemoveElement(item));
    }
}

void DomAccessor::undo_remove_element(EngravingItem *item) {
    IF_ASSERT_FAILED(score()) { return; }
    score()->undo_remove_element(item);
}

void DomAccessor::undo(UndoableCommand *cmd) const {
    IF_ASSERT_FAILED(score()) { return; }
    score()->undo(cmd);
}

void DomAccessor::add_element(EngravingItem *item) {
    IF_ASSERT_FAILED(score()) { return; }
    score()->add_element(item);
}

void DomAccessor::remove_element(EngravingItem *item) {
    IF_ASSERT_FAILED(score()) { return; }
    score()->remove_element(item);
}

void DomAccessor::update_locks_on_create_mm_rest(Measure *first, Measure *last) {
    IF_ASSERT_FAILED(score()) { return; }
    Transaction &tx = score()->transaction_manager()->current_or_dummy_transaction();
    EditPageLocks::update_page_locks_on_create_mm_rests(tx, score(), first, last);
    EditSystemLocks::update_system_locks_on_create_mm_rests(tx, score(), first, last);
}

void DomAccessor::undo_change_parent(EngravingItem *element, EngravingItem *parent, staff_idx_t staff, bool change_links_parents) {
    IF_ASSERT_FAILED(score()) { return; }
    score()->undo_change_parent(element, parent, staff, change_links_parents);
}

void DomAccessor::add_unmanaged_spanner(Spanner *s) {
    IF_ASSERT_FAILED(score()) { return; }
    score()->add_unmanaged_spanner(s);
}

const std::set<Spanner *> &DomAccessor::unmanaged_spanners() const {
    IF_ASSERT_FAILED(score()) {
        static const std::set<Spanner *> dummy;
        return dummy;
    }
    return score()->unmanaged_spanners();
}

// =============================================================
// LayoutContext
// =============================================================

LayoutContext::LayoutContext(Score *score) : score_(score), configuration_(this), dom_(this) {
    if (score) {
        state_.set_first_system_indent(score->style().style_b(Sid::ENABLE_INDENTATION_ON_FIRST_SYSTEM));
    }
}

LayoutContext::~LayoutContext() {
    if (!grace_groups_.empty()) {
        clear_grace_attachments(score_);
    }
}

GraceNotesGroup &LayoutContext::update_grace_group(Chord *chord, bool after) {
    auto &group = grace_groups_[chord][after ? 1 : 0];
    if (!group) {
        group = std::make_unique<GraceNotesGroup>(chord);
    }
    const auto notes = after ? chord->grace_notes_after() : chord->grace_notes_before();
    group->assign(notes.begin(), notes.end());
    return *group;
}

const GraceNotesGroup *LayoutContext::grace_group(const Chord *chord, bool after) const {
    const auto it = grace_groups_.find(chord);
    return it == grace_groups_.end() ? nullptr : it->second[after ? 1 : 0].get();
}

void LayoutContext::clear_grace_attachments(EngravingObject *object) const {
    if (!object) {
        return;
    }
    if (object->is_segment()) {
        Segment *segment = to_segment(object);
        for (track_idx_t track = 0; track < segment->pre_appended_items().size(); ++track) {
            EngravingItem *item = segment->pre_appended_item(track);
            if (!item || !item->is_grace_notes_group()) {
                continue;
            }
            const GraceNotesGroup *group = to_grace_notes_group(item);
            if (grace_group(group->parent(), false) == group || grace_group(group->parent(), true) == group) {
                segment->clear_pre_appended(track);
            }
        }
    }
    // Traverse live ownership, not saved Segment pointers: layout may have moved
    // or deleted an attached segment. This also reaches retained undo objects.
    for (EngravingObject *child : object->children()) {
        clear_grace_attachments(child);
    }
}

bool LayoutContext::is_valid() const { return score_ != nullptr; }

EngravingFontPtr LayoutContext::engraving_font() const {
    IF_ASSERT_FAILED(score_) { return nullptr; }
    return score_->engraving_font();
}

const DomAccessor &LayoutContext::dom() const { return dom_; }

DomAccessor &LayoutContext::mut_dom() { return dom_; }

const LayoutState &LayoutContext::state() const { return state_; }

LayoutState &LayoutContext::mut_state() { return state_; }

const Selection &LayoutContext::selection() const {
    IF_ASSERT_FAILED(score_) {
        static const Selection dummy;
        return dummy;
    }
    return score_->selection();
}

void LayoutContext::select(EngravingItem *item, SelectType type, staff_idx_t staff) {
    IF_ASSERT_FAILED(score_) { return; }
    score_->select(item, type, staff);
}

void LayoutContext::deselect(EngravingItem *el) {
    IF_ASSERT_FAILED(score_) { return; }
    score_->deselect(el);
}
