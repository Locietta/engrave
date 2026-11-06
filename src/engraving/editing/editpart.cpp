/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "log.h"
#include "editpart.h"
#include "editscoreproperties.h"
#include "editstaff.h"
#include "transaction/transaction.h"
#include "transpose.h"

#include "../dom/excerpt.h"
#include "../dom/factory.h"
#include "../dom/instrchange.h"
#include "../dom/instrtemplate.h"
#include "../dom/scoreorder.h"
#include "../dom/masterscore.h"
#include "../dom/mscore.h"
#include "../dom/part.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/staff.h"
#include "../dom/stafftype.h"
#include "../dom/stringtunings.h"
#include "../dom/utils.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   InsertPart
//---------------------------------------------------------

InsertPart::InsertPart(Part *p, size_t target_part_idx) {
    part_ = p;
    target_part_idx_ = target_part_idx;
}

void InsertPart::undo() { part_->score()->remove_part(part_); }

void InsertPart::redo() { part_->score()->insert_part(part_, target_part_idx_); }

void InsertPart::cleanup(bool was_done) {
    if (!was_done) {
        delete part_;
        part_ = nullptr;
    }
}

//---------------------------------------------------------
//   RemovePart
//---------------------------------------------------------

RemovePart::RemovePart(Part *p, size_t part_idx) {
    part_ = p;
    part_idx_ = part_idx;

    if (part_idx_ == muse::nidx) {
        part_idx_ = muse::index_of(part_->score()->parts(), part_);
    }
}

void RemovePart::undo() { part_->score()->insert_part(part_, part_idx_); }

void RemovePart::redo() { part_->score()->remove_part(part_); }

void RemovePart::cleanup(bool was_done) {
    if (was_done) {
        delete part_;
        part_ = nullptr;
    }
}

//---------------------------------------------------------
//   SetSoloist
//---------------------------------------------------------

SetSoloist::SetSoloist(Part *p, bool b) {
    part = p;
    soloist = b;
}

void SetSoloist::undo() { part->set_soloist(!soloist); }

void SetSoloist::redo() { part->set_soloist(soloist); }

//---------------------------------------------------------
//   ChangePart
//---------------------------------------------------------

ChangePart::ChangePart(Part *input_part, Instrument *i) {
    instrument = i;
    part = input_part;
}

void ChangePart::flip() {
    Instrument *oi = part->instrument(); //tick?
    part->set_instrument(instrument);

    part->update_harmony_channels(false);

    Score *score = part->score();
    score->master_score()->rebuild_midi_mapping();
    score->set_instruments_changed(true);

    // check if notes need to be updated
    // true if changing into or away from TAB or from one TAB type to another

    score->set_layout_all();

    instrument = oi;
}

void ChangePart::cleanup(bool) {
    delete instrument;
    instrument = nullptr;
}

//---------------------------------------------------------
//   ChangeInstrumentLong
//---------------------------------------------------------

ChangeInstrumentLong::ChangeInstrumentLong(const Fraction &input_tick, Part *p, const String &t)
    : part(p), tick(input_tick), long_name(t) {}

void ChangeInstrumentLong::flip() {
    String s = part->long_name(tick);
    part->set_long_name(long_name, tick);
    long_name = s;
    part->score()->set_layout_all();
}

//---------------------------------------------------------
//   ChangeInstrumentShort
//---------------------------------------------------------

ChangeInstrumentShort::ChangeInstrumentShort(const Fraction &input_tick, Part *p, const String &t)
    : part(p), tick(input_tick), short_name(t) {}

void ChangeInstrumentShort::flip() {
    String s = part->short_name(tick);
    part->set_short_name(short_name, tick);
    short_name = s;
    part->score()->set_layout_all();
}

//---------------------------------------------------------
//   ChangeInstrumentLong
//---------------------------------------------------------

ChangeInstrumentGroupOptions::ChangeInstrumentGroupOptions(const Fraction &input_tick, Part *p, bool use_custom, const String &long_name,
                                                           const String &short_name)
    : part(p), tick(input_tick), use_custom(use_custom), long_name(long_name), short_name(short_name) {}

void ChangeInstrumentGroupOptions::flip() {
    InstrumentLabel &label = part->instrument(tick)->instrument_label();

    bool cur_use_custom = label.use_custom_group_name();
    const String &cur_long = label.custom_name_long_group();
    const String &cur_short = label.custom_name_short_group();

    label.set_use_custom_group_name(use_custom);
    label.set_custom_name_long_group(long_name);
    label.set_custom_name_short_group(short_name);

    use_custom = cur_use_custom;
    long_name = cur_long;
    short_name = cur_short;

    part->score()->set_layout_all();
}

ChangeInstrumentNumber::ChangeInstrumentNumber(const Fraction &input_tick, Part *p, int v) : part(p), tick(input_tick), number(v) {}

void ChangeInstrumentNumber::flip() {
    int v = part->number(tick);
    part->set_number(number, tick);
    number = v;
    part->score()->set_layout_all();
}

//---------------------------------------------------------
//   ChangeDrumset
//---------------------------------------------------------

void ChangeDrumset::flip() {
    Drumset d = instrument->drumset() ? *instrument->drumset() : Drumset();
    instrument->set_drumset(&drumset);
    drumset = d;

    if (part->staves().size() > 0) {
        part->score()->set_layout(Fraction(0, 1), part->score()->end_tick(), part->staves().front()->idx(), part->staves().back()->idx());
    }
}

//---------------------------------------------------------
//   ChangeStringData
//---------------------------------------------------------

void ChangeStringData::flip() {
    const StringData *string_data = string_tunings_ ? string_tunings_->string_data() : instrument_->string_data();
    int frets = string_data->frets();
    std::vector<InstrString> string_list = string_data->string_list();

    if (string_tunings_) {
        string_tunings_->set_string_data(string_data_);
    } else {
        instrument_->set_string_data(string_data_);
    }

    string_data_.set(StringData(frets, string_list));
}

//---------------------------------------------------------
//   ChangePatch
//---------------------------------------------------------

void ChangePatch::flip() {
    MidiPatch op;
    op.prog = channel->program();
    op.bank = channel->bank();
    op.synti = channel->synti();

    channel->set_program(patch.prog);
    channel->set_bank(patch.bank);
    channel->set_synti(patch.synti);

    patch = op;

    //! TODO Needs porting for MU4
    //    if (MScore::seq == 0) {
    //        LOGW("no seq");
    //        return;
    //    }
    //
    //    NPlayEvent event;
    //    event.setType(ME_CONTROLLER);
    //    event.setChannel(channel->channel());
    //
    //    int hbank = (channel->bank() >> 7) & 0x7f;
    //    int lbank = channel->bank() & 0x7f;
    //
    //    event.setController(CTRL_HBANK);
    //    event.setValue(hbank);
    //    MScore::seq->sendEvent(event);
    //
    //    event.setController(CTRL_LBANK);
    //    event.setValue(lbank);
    //    MScore::seq->sendEvent(event);
    //
    //    event.setController(CTRL_PROGRAM);
    //    event.setValue(channel->program());
    //
    score->set_instruments_changed(true);
    //
    //    MScore::seq->sendEvent(event);
}

//---------------------------------------------------------
//   SetUserBankController
//---------------------------------------------------------

void SetUserBankController::flip() {
    bool old_val = channel->user_bank_controller();
    channel->set_user_bank_controller(val);
    val = old_val;
}

//---------------------------------------------------------
//   findInstrumentChange
//   Helper function to find InstrumentChange element at a given tick
//---------------------------------------------------------

static InstrumentChange *find_instrument_change(Score *score, const Part *part, const Fraction &tick) {
    const Segment *segment = score->tick2segment(tick, true, SegmentType::CHORD_REST);
    if (!segment) {
        return nullptr;
    }

    const TrackRange track_range = part->track_range();
    EngravingItem *item = segment->find_annotation(ElementType::INSTRUMENT_CHANGE, track_range.start_track, track_range.end_track - 1);
    return item ? to_instrument_change(item) : nullptr;
}

void EditPart::replace_part_instrument(Score *score, Part *part, const Instrument &new_instrument, const StaffType *new_staff_type) {
    if (!score || !part) {
        return;
    }

    score->undo(new ChangePart(part, new Instrument(new_instrument)));

    // Update clefs and staff type for all staves in the part
    for (staff_idx_t staff_idx = 0; staff_idx < part->nstaves(); ++staff_idx) {
        Staff *staff = part->staves().at(staff_idx);
        if (!staff) {
            continue;
        }

        // Get current staff properties
        bool visible = staff->visible();
        ClefTypeList current_clef_type = staff->default_clef_type();
        Spatium user_dist = staff->user_dist();
        bool cutaway = staff->cutaway();
        bool hide_system_bar_line = staff->hide_system_bar_line();
        AutoOnOff merge_matching_rests = staff->merge_matching_rests();
        bool reflect_transposition = staff->reflect_transposition_in_linked_tab();

        // Get new clef type from the new instrument
        ClefTypeList new_clef_type = new_instrument.clef_type(staff_idx);

        // Only update staff if clef type changes
        if (current_clef_type != new_clef_type) {
            score->undo(new ChangeStaff(staff, visible, new_clef_type, user_dist, cutaway, hide_system_bar_line, merge_matching_rests,
                                        reflect_transposition));
        }

        // Apply new staff type if provided
        if (new_staff_type) {
            score->undo(new ChangeStaffType(staff, *new_staff_type));
        }
    }
}

bool EditPart::replace_instrument_at_tick(Score *score, Part *part, const Fraction &tick, const Instrument &new_instrument) {
    if (!score || !part) {
        return false;
    }

    InstrumentChange *instrument_change = find_instrument_change(score, part, tick);
    if (!instrument_change) {
        return false;
    }

    instrument_change->set_init(true);
    instrument_change->setup_instrument(&new_instrument);
    return true;
}

void EditPart::set_part_visible(Score *score, Part *part, bool visible) {
    if (!score || !part) {
        return;
    }

    part->undo_change_property(Pid::VISIBLE, visible);
}

void EditPart::set_staff_visible(Score *score, Staff *staff, bool visible) {
    if (!score || !staff) {
        return;
    }

    staff->undo_change_property(Pid::VISIBLE, visible);
}

void EditPart::set_part_sharp_flat(Score *score, Part *part, PreferSharpFlat sharp_flat) {
    if (!score || !part) {
        return;
    }

    Interval old_transposition = part->staff(0)->transpose(Fraction(0, 1));

    part->undo_change_property(Pid::PREFER_SHARP_FLAT, int(sharp_flat));

    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();
    Transpose::transposition_changed(tx, score, part, old_transposition);
}

void EditPart::set_instrument_name(Score *score, Part *part, const Fraction &tick, const String &name) {
    if (!score || !part) {
        return;
    }

    score->undo(new ChangeInstrumentLong(tick, part, name));
}

void EditPart::set_instrument_abbreviature(Score *score, Part *part, const Fraction &tick, const String &abbreviature) {
    if (!score || !part) {
        return;
    }

    score->undo(new ChangeInstrumentShort(tick, part, abbreviature));
}

void EditPart::set_instrument_group_name_options(Score *score, Part *part, const Fraction &tick, bool use_custom, const String &long_name,
                                                 const String &short_name) {
    if (!score || !part) {
        return;
    }

    score->undo(new ChangeInstrumentGroupOptions(tick, part, use_custom, long_name, short_name));
}

void EditPart::set_staff_type(Score *score, Staff *staff, StaffTypes type_id) {
    if (!score || !staff) {
        return;
    }

    const StaffType *staff_type = StaffType::preset(type_id);
    if (!staff_type) {
        return;
    }

    score->undo(new ChangeStaffType(staff, *staff_type));
}

void EditPart::remove_parts(Score *score, const std::vector<Part *> &parts) {
    if (!score || parts.empty()) {
        return;
    }

    for (Part *part : parts) {
        score->cmd_remove_part(part);
    }

    score->set_brackets_and_barlines();
}

void EditPart::remove_staves(Score *score, const std::vector<Staff *> &staves) {
    if (!score || staves.empty()) {
        return;
    }

    for (Staff *staff : staves) {
        score->cmd_remove_staff(staff->idx());
    }

    score->set_brackets_and_barlines();
}

void EditPart::move_parts(Score *score, const std::vector<Part *> &source_parts, Part *destination_part, bool insert_after) {
    if (!score || source_parts.empty() || !destination_part) {
        return;
    }

    // Build new part order: remove source parts, then insert at destination
    std::vector<Part *> all_parts(score->parts().begin(), score->parts().end());

    // Remove source parts from the list
    for (Part *src_part : source_parts) {
        all_parts.erase(std::remove(all_parts.begin(), all_parts.end(), src_part), all_parts.end());
    }

    // Find destination index
    auto dst_it = std::find(all_parts.begin(), all_parts.end(), destination_part);
    if (dst_it == all_parts.end()) {
        return;
    }

    size_t dst_index = std::distance(all_parts.begin(), dst_it);
    if (insert_after) {
        dst_index++;
    }

    // Insert source parts at destination
    for (size_t i = 0; i < source_parts.size(); ++i) {
        all_parts.insert(all_parts.begin() + dst_index + i, source_parts[i]);
    }

    // Build staff index mapping from the new part order
    std::vector<staff_idx_t> staff_mapping;
    staff_mapping.reserve(score->nstaves());
    for (Part *part : all_parts) {
        for (Staff *staff : part->staves()) {
            staff_mapping.push_back(staff->idx());
        }
    }

    score->undo(new SortStaves(score, staff_mapping));
    score->set_brackets_and_barlines();
}

void EditPart::move_staves(Score *score, const std::vector<Staff *> &source_staves, Staff *destination_staff, bool insert_after) {
    if (!score || source_staves.empty() || !destination_staff) {
        return;
    }

    // Build new staff order: remove source staves, then insert at destination
    std::vector<Staff *> all_staves(score->staves().begin(), score->staves().end());

    // Remove source staves from the list
    for (Staff *src_staff : source_staves) {
        all_staves.erase(std::remove(all_staves.begin(), all_staves.end(), src_staff), all_staves.end());
    }

    // Find destination index
    auto dst_it = std::find(all_staves.begin(), all_staves.end(), destination_staff);
    if (dst_it == all_staves.end()) {
        return;
    }

    size_t dst_index = std::distance(all_staves.begin(), dst_it);
    if (insert_after) {
        dst_index++;
    }

    // Insert source staves at destination
    for (size_t i = 0; i < source_staves.size(); ++i) {
        all_staves.insert(all_staves.begin() + dst_index + i, source_staves[i]);
    }

    // Build index mapping
    std::vector<staff_idx_t> sorted_indexes;
    sorted_indexes.reserve(all_staves.size());
    for (const Staff *staff : all_staves) {
        sorted_indexes.push_back(staff->idx());
    }

    score->undo(new SortStaves(score, sorted_indexes));
    score->set_brackets_and_barlines();
}

void EditPart::add_system_objects(Score *score, const std::vector<Staff *> &staves) {
    if (!score || staves.empty()) {
        return;
    }

    std::vector<EngravingItem *> top_system_objects = collect_system_objects(score);

    for (Staff *staff : staves) {
        if (staff->is_system_object_staff()) {
            continue;
        }

        score->undo(new AddSystemObjectStaff(staff));

        const staff_idx_t staff_idx = staff->idx();
        for (EngravingItem *obj : top_system_objects) {
            if (obj->is_time_sig()) {
                obj->trigger_layout();
                continue;
            }
            EngravingItem *copy = obj->linked_clone();
            copy->set_staff_idx(staff_idx);
            score->undo_add_element(copy, false /*addToLinkedStaves*/);
        }
    }
}

void EditPart::remove_system_objects(Score *score, const std::vector<Staff *> &staves) {
    if (!score || staves.empty()) {
        return;
    }

    std::vector<EngravingItem *> system_objects = collect_system_objects(score, staves);

    for (Staff *staff : staves) {
        if (staff->is_system_object_staff()) {
            score->undo(new RemoveSystemObjectStaff(staff));
            if (staff->has_system_objects_below_bottom_staff()) {
                score->undo_change_style_val(Sid::SYSTEM_OBJECTS_BELOW_BOTTOM_STAFF, false);
            }
        }
    }

    for (EngravingItem *obj : system_objects) {
        if (obj->is_time_sig()) {
            obj->trigger_layout();
            continue;
        }
        obj->undo_unlink();
        score->undo_remove_element(obj, false /*removeLinked*/);
    }
}

void EditPart::move_system_objects(Score *score, Staff *source_staff, Staff *destination_staff) {
    if (!score || !source_staff || !destination_staff) {
        return;
    }

    if (!source_staff->is_system_object_staff()) {
        return;
    }

    const std::vector<EngravingItem *> system_objects = collect_system_objects(score, {source_staff, destination_staff});
    const staff_idx_t dst_staff_idx = destination_staff->idx();

    score->undo(new RemoveSystemObjectStaff(source_staff));
    if (!destination_staff->is_system_object_staff() && dst_staff_idx != 0) {
        score->undo(new AddSystemObjectStaff(destination_staff));
    } else {
        score->undo_change_style_val(Sid::SYSTEM_OBJECTS_BELOW_BOTTOM_STAFF, false);
    }

    AutoOnOff show_meas_num_on_src_staff = source_staff->get_property(Pid::SHOW_MEASURE_NUMBERS).value<AutoOnOff>();
    if (show_meas_num_on_src_staff != AutoOnOff::AUTO) {
        destination_staff->undo_change_property(Pid::SHOW_MEASURE_NUMBERS, show_meas_num_on_src_staff);
        source_staff->undo_reset_property(Pid::SHOW_MEASURE_NUMBERS);
    }

    // Remove items from destination first
    for (EngravingItem *item : system_objects) {
        if (item->is_time_sig()) {
            item->trigger_layout();
            continue;
        }

        if (item->staff() == source_staff) {
            continue;
        }
        item->undo_unlink();
        score->undo_remove_element(item, false /*removeLinked*/);
    }

    // Move items from source to destination
    for (EngravingItem *item : system_objects) {
        if (item->is_time_sig()) {
            continue;
        }

        if (item->staff() == source_staff) {
            item->undo_change_property(Pid::TRACK, staff2track(dst_staff_idx, item->voice()));
        }
    }
}

void EditPart::do_append_staff(Score *score, Staff *staff, Part *destination_part, bool create_rests) {
    if (!score || !staff || !destination_part) {
        return;
    }

    staff_idx_t staff_local_index = destination_part->nstaves();
    KeyList key_list = *destination_part->staff(staff_local_index - 1)->key_list();

    staff->set_score(score);
    staff->set_part(destination_part);

    score->undo_insert_staff(staff, staff_local_index, create_rests);

    staff_idx_t staff_global_index = staff->idx();
    score->adjust_key_sigs(staff_global_index, staff_global_index + 1, key_list);

    score->update_braces_and_barlines(destination_part, staff_local_index);

    destination_part->instrument()->set_clef_type(staff_local_index, staff->default_clef_type());
}

Staff *EditPart::append_staff(Score *score, Part *destination_part) {
    if (!score || !destination_part) {
        return nullptr;
    }

    Staff *staff = Factory::create_staff(destination_part);
    do_append_staff(score, staff, destination_part);
    return staff;
}

Staff *EditPart::append_linked_staff(Score *score, Staff *source_staff, Part *destination_part) {
    if (!score || !source_staff || !destination_part) {
        return nullptr;
    }

    Staff *staff = Factory::create_staff(destination_part);
    do_append_staff(score, staff, destination_part, false);

    staff->set_links(nullptr);
    Excerpt::clone_staff(source_staff, staff);

    return staff;
}

bool EditPart::set_voice_visible(Score *score, Staff *staff, int voice_index, bool visible) {
    if (!score || !staff) {
        return false;
    }

    if (!score->excerpt()) {
        return false;
    }

    if (!visible && !staff->can_disable_voice()) {
        return false;
    }

    score->excerpt()->set_voice_visible(staff, voice_index, visible);
    return true;
}

void EditPart::insert_part(Score *score, const InstrumentTemplate *templ, size_t index) {
    if (!score || !templ) {
        return;
    }

    Part *part = new Part(score);
    part->init_from_instr_template(templ);

    for (staff_idx_t i = 0; i < templ->staff_count; ++i) {
        Staff *staff = Factory::create_staff(part);
        StaffType *stt = staff->staff_type(Fraction(0, 1));
        score->undo_insert_staff(staff, i);
        staff->init(templ, stt, int(i));
    }

    score->undo_insert_part(part, index);
    score->master_score()->rebuild_midi_mapping();
}

void EditPart::replace_part(Score *score, Part *old_part, const InstrumentTemplate *templ) {
    if (!score || !old_part || !templ) {
        return;
    }

    size_t part_index = muse::index_of(score->parts(), old_part);
    score->cmd_remove_part(old_part);
    insert_part(score, templ, part_index);
}

void EditPart::replace_drumset(Score *score, Part *part, const Fraction &tick, const Drumset &new_drumset) {
    if (!score || !part) {
        return;
    }

    Instrument *instrument = part->instrument(tick);
    if (instrument) {
        score->undo(new ChangeDrumset(instrument, new_drumset, part));
    }
}

void EditPart::set_score_order(Score *score, const ScoreOrder &order) {
    if (!score) {
        return;
    }

    score->undo(new ChangeScoreOrder(score, order));
    ScoreOrder mutable_order = order;
    mutable_order.set_brackets_and_barlines(score);
}
