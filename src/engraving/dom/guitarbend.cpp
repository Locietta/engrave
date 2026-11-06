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

#include "log.h"
#include "../iengravingconfiguration.h" // IWYU pragma: keep

#include "../editing/editdata.h"
#include "../editing/elementeditdata.h"
#include "../editing/editnote.h"
#include "../editing/navigation.h"
#include "../editing/noteinput.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"

#include "accidental.h"
#include "actionicon.h"
#include "chord.h"
#include "guitarbend.h"
#include "note.h"
#include "part.h"
#include "rest.h"
#include "score.h"
#include "staff.h"
#include "tie.h"
#include "utils.h"
#include "whammybar.h"

namespace mu::engraving {
/****************************************
 *              GuitarBend
 * **************************************/

GuitarBend::GuitarBend(EngravingObject *parent) : SLine(ElementType::GUITAR_BEND, parent, ElementFlag::MOVABLE) {}

GuitarBend::GuitarBend(const GuitarBend &g) : SLine(g) {
    bend_type_ = g.bend_type();
    bend_amount_in_quarter_tones_value = g.bend_amount_in_quarter_tones();
}

GuitarBend::~GuitarBend() {
    if (hold_line_) {
        delete hold_line_;
    }
}

LineSegment *GuitarBend::create_line_segment() {
    GuitarBendSegment *seg = new GuitarBendSegment(this);
    seg->set_track(track());
    seg->set_color(line_color());
    return seg;
}

Note *GuitarBend::start_note() const {
    EngravingItem *start_el = start_element();
    IF_ASSERT_FAILED(start_el && start_el->is_note()) { return nullptr; }
    return to_note(start_el);
}

Note *GuitarBend::start_note_of_chain() const {
    Note *start_of_chain = start_note();
    GuitarBend *prev_bend = find_preceding_bend();
    while (start_of_chain && prev_bend) {
        start_of_chain = prev_bend->start_note();
        prev_bend = prev_bend->find_preceding_bend();
    }

    return start_of_chain;
}

Note *GuitarBend::end_note() const {
    EngravingItem *end_el = end_element();
    IF_ASSERT_FAILED(end_el && end_el->is_note()) { return nullptr; }
    return to_note(end_el);
}

void GuitarBend::change_bend_amount(int end_bend_amount, int start_bend_amount) {
    if (end_bend_amount == ::mu::engraving::GuitarBend::slack_bend_amount) {
        undo_change_property(Pid::GUITAR_DIVE_IS_SLACK, true);
        if (end_note()) {
            end_note()->undo_change_property(Pid::HEAD_GROUP, NoteHeadGroup::HEAD_CROSS);
        }
        return;
    } else if (is_slack()) {
        undo_reset_property(Pid::GUITAR_DIVE_IS_SLACK);
        if (end_note()) {
            end_note()->undo_reset_property(Pid::HEAD_GROUP);
        }
    }

    if (bend_type() == GuitarBendType::DIP) {
        // Dips and slack have no end note so set bend amount directly
        undo_change_property(Pid::GUITAR_BEND_AMOUNT, end_bend_amount);
        return;
    }

    if (GuitarBend *overlapping = overlapping_bend_or_dive()) {
        int cur_total_bend_amount = bend_amount_in_quarter_tones() + overlapping->bend_amount_in_quarter_tones();
        int new_bend_amount_for_this = end_bend_amount - start_bend_amount;
        if (is_dive()) {
            undo_change_property(Pid::GUITAR_BEND_AMOUNT, new_bend_amount_for_this);
        } else {
            overlapping->undo_change_property(Pid::GUITAR_BEND_AMOUNT, cur_total_bend_amount - new_bend_amount_for_this);
        }
        return;
    }

    // All other bends: set bend amount by transposing end note appropriately
    int pitch = end_bend_amount / 2 + start_note_of_chain()->pitch();
    int quarter_off = end_bend_amount % 2;
    if (pitch == start_note()->pitch() && quarter_off == 1) {
        // Because a flat second is more readable than a sharp unison
        pitch += 1;
        quarter_off = -1;
    }

    set_end_note_pitch(pitch, quarter_off);
}

void GuitarBend::set_end_note_pitch(int pitch, int quarter_tone_offset) {
    Note *note = end_note();
    IF_ASSERT_FAILED(note) { return; }

    Fraction tick = note->tick();
    Staff *staff = note->staff();

    Key key = staff->key(tick);
    Interval interval = staff->transpose(tick);
    interval.flip();

    int target_tpc1 = pitch2tpc(pitch, key, Prefer::NEAREST);
    int target_tpc2 = Transpose::transpose_tpc(target_tpc1, interval, true);

    auto do_change_end_note_pitch = [&]() {
        EditNote::undo_change_pitch(score(), note, pitch, target_tpc1, target_tpc2);
        Note *tied_note = note->tie_for() ? note->tie_for()->end_note() : nullptr;
        while (tied_note) {
            EditNote::undo_change_pitch(score(), tied_note, pitch, target_tpc1, target_tpc2);
            tied_note = tied_note->tie_for() ? tied_note->tie_for()->end_note() : nullptr;
        }
    };

    Note *linked_note_on_notation_staff = nullptr;
    for (EngravingObject *linked : note->link_list()) {
        if (!to_note(linked)->staff_type()->is_tab_staff()) {
            linked_note_on_notation_staff = to_note(linked);
            break;
        }
    }

    if (linked_note_on_notation_staff) {
        // Manage microtonal by setting appropriate microtonal accidentals, which will propagate to TAB staff too
        AccidentalType accidental_type = Accidental::value2_microtonal_subtype(tpc2alter(target_tpc1), quarter_tone_offset);
        if (Accidental::is_microtonal(accidental_type)) {
            do_change_end_note_pitch();
            linked_note_on_notation_staff->update_line();
            EditNote::change_accidental(score(), linked_note_on_notation_staff, accidental_type);
        } else {
            linked_note_on_notation_staff->update_line();
            EditNote::change_accidental(score(), linked_note_on_notation_staff, accidental_type);
            do_change_end_note_pitch();
        }
    } else {
        // Accidental logic doesn't work on TAB, so set cents offset directly
        do_change_end_note_pitch();
        note->undo_change_property(Pid::CENT_OFFSET, quarter_tone_offset * 50.0);
    }

    compute_bend_amount();

    trigger_layout();
}

bool GuitarBend::is_release_bend() const { return bend_amount_in_quarter_tones() < 0; }

bool GuitarBend::is_full_release() const { return is_release_bend() && tot_bend_amount_including_preceding_bends() == 0; }

bool GuitarBend::angled_pre_bend() const {
    Note *end_n = end_note();
    Chord *end_chord = end_note()->chord();
    return bend_type() == GuitarBendType::PRE_BEND && end_n->string() > end_chord->up_string();
}

bool GuitarBend::is_dive() const {
    switch (bend_type()) {
        case GuitarBendType::DIVE:
        case GuitarBendType::PRE_DIVE:
        case GuitarBendType::DIP:
        case GuitarBendType::SCOOP: return true;
        default: return false;
    }
}

bool GuitarBend::is_full_release_dive() const { return is_dive() && tot_bend_amount_including_preceding_bends() == 0; }

void GuitarBend::fix_notes_fretting_for_standard_bend(Note *start_note, Note *end_note) {
    Staff *cur_staff = start_note->staff();
    Part *cur_part = start_note->origin_part();
    if (!cur_staff || !cur_part) {
        return;
    }

    const StringData *string_data = cur_part->string_data(start_note->tick(), cur_staff->idx());
    int start_fret = start_note->fret();
    int start_string = start_note->string();
    if (start_fret == 0) {
        // Bend can't start from empty string
        start_string += 1;
        start_fret = string_data->fret(start_note->pitch() + cur_staff->pitch_offset(start_note->tick()), start_string, cur_staff);
        start_note->undo_change_property(Pid::STRING, start_string);
        start_note->undo_change_property(Pid::FRET, start_fret);
    }

    if (end_note->string() != start_string) {
        end_note->undo_change_property(Pid::STRING, start_string);
        int end_fret = string_data->fret(end_note->pitch() + cur_staff->pitch_offset(end_note->tick()), start_string, cur_staff);
        end_note->undo_change_property(Pid::FRET, end_fret);
    }
}

Note *GuitarBend::create_end_note(Note *start_note, GuitarBendType bend_type) {
    track_idx_t track = start_note->track();
    Chord *start_chord = start_note->chord();
    Segment *start_segment = start_chord->segment();

    Segment *end_segment = start_segment->next_cr(track);
    if (!end_segment) {
        return nullptr;
    }

    EngravingItem *item = end_segment->element(track);
    if (!item) {
        return nullptr;
    }

    Score *score = start_note->score();
    NoteVal note_val = start_note->note_val();
    Note *end_note = nullptr;

    if (item->is_rest()) {
        Rest *rest = to_rest(item);
        Fraction duration = std::min(start_chord->ticks(), rest->ticks());

        end_segment = score->set_note_rest(end_segment, track, note_val, duration);
        Chord *end_chord = end_segment ? to_chord(end_segment->element(track)) : nullptr;
        end_note = end_chord ? end_chord->up_note() : nullptr;
    } else if (item->is_chord()) {
        Chord *chord = to_chord(item);
        end_note = NoteInput::add_note(score->transaction_manager()->current_or_dummy_transaction(), score, chord, note_val);
    }

    if (end_note) {
        end_note->transpose_diatonic(bend_type == GuitarBendType::BEND ? 1 : -1, true, false);
    }

    return end_note;
}

void GuitarBend::fix_notes_fretting_for_grace_bend(Note *grace, Note *main) {
    // The start grace-note of bend must be on the same string as the main note
    int main_string = main->string();
    const StringData *string_data = main->origin_part()->string_data(main->tick(), main->staff()->idx());
    int staff_pitch_offset = main->staff()->pitch_offset(main->tick());
    int grace_fret = string_data->fret(grace->pitch() + staff_pitch_offset, main_string, main->staff());
    if (grace_fret > 0) {
        // There is valid fretting
        grace->undo_change_property(Pid::STRING, main_string);
        grace->undo_change_property(Pid::FRET, grace_fret);
    } else {
        // No valid fretting on this string, so it must be played on the lower string
        main_string += 1;
        main->undo_change_property(Pid::STRING, main_string);
        int main_fret = string_data->fret(main->pitch() + staff_pitch_offset, main_string, main->staff());
        main->undo_change_property(Pid::FRET, main_fret);

        grace->undo_change_property(Pid::STRING, main_string);
        grace_fret = string_data->fret(grace->pitch() + staff_pitch_offset, main_string, main->staff());
        grace->undo_change_property(Pid::FRET, grace_fret);
    }
}

PropertyValue GuitarBend::get_property(Pid id) const {
    switch (id) {
        case Pid::DIRECTION: return direction();
        case Pid::BEND_SHOW_HOLD_LINE: return static_cast<int>(show_hold_line());
        case Pid::BEND_START_TIME_FACTOR: return start_time_factor();
        case Pid::BEND_TARGET_TIME_FACTOR: return target_time_factor_.has_value() ? target_time_factor_.value() : PropertyValue();
        case Pid::BEND_END_TIME_FACTOR: return end_time_factor();
        case Pid::GUITAR_DIVE_TAB_POS: return is_dive() ? dive_tab_pos_ : DirectionV::AUTO;
        case Pid::GUITAR_BEND_AMOUNT: return bend_amount_in_quarter_tones();
        case Pid::VIBRATO_LINE_TYPE: return dip_vibrato_type();
        case Pid::GUITAR_DIVE_IS_SLACK: return is_slack();
        default: return SLine::get_property(id);
    }
}

bool GuitarBend::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::DIRECTION: set_direction(v.value<DirectionV>()); break;
        case Pid::BEND_SHOW_HOLD_LINE: set_show_hold_line(static_cast<GuitarBendShowHoldLine>(v.to_int())); break;
        case Pid::BEND_START_TIME_FACTOR: set_start_time_factor(v.to_real()); break;
        case Pid::BEND_TARGET_TIME_FACTOR:
            if (v.is_valid()) {
                set_target_time_factor(v.to_real());
            } else {
                target_time_factor_ = std::nullopt;
            }
            break;
        case Pid::BEND_END_TIME_FACTOR: set_end_time_factor(v.to_real()); break;
        case Pid::COLOR:
            set_color(v.value<Color>());
            set_line_color(v.value<Color>());
            break;
        case Pid::GUITAR_DIVE_TAB_POS: {
            IF_ASSERT_FAILED(is_dive()) { return false; }
            set_dive_tab_pos(v.value<DirectionV>());
            break;
        }
        case Pid::GUITAR_BEND_AMOUNT: set_bend_amount_in_quarter_tones(v.to_int()); break;
        case Pid::VIBRATO_LINE_TYPE: {
            IF_ASSERT_FAILED(bend_type() == GuitarBendType::DIP) { return false; }
            set_dip_vibrato_type(v.value<VibratoType>());
            break;
        }
        case Pid::GUITAR_DIVE_IS_SLACK: set_is_slack(v.to_bool()); break;
        default: return SLine::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

PropertyValue GuitarBend::property_default(Pid id) const {
    switch (id) {
        case Pid::DIRECTION: return DirectionV::AUTO;
        case Pid::BEND_SHOW_HOLD_LINE: return static_cast<int>(GuitarBendShowHoldLine::AUTO);
        case Pid::BEND_START_TIME_FACTOR: return 0.f;
        case Pid::BEND_TARGET_TIME_FACTOR:
            if (bend_type_ == GuitarBendType::DIP) {
                return 0.25f;
            }
            return {};
        case Pid::BEND_END_TIME_FACTOR:
            if (bend_type_ == GuitarBendType::GRACE_NOTE_BEND) {
                return 0.25f;
            }
            if (bend_type_ == GuitarBendType::SCOOP) {
                return 0.25f;
            }
            if (bend_type_ == GuitarBendType::DIP) {
                return 0.5f;
            }
            return 1.f;
        case Pid::GUITAR_DIVE_TAB_POS: return DirectionV::AUTO;
        case Pid::GUITAR_BEND_AMOUNT: return bend_type_ == GuitarBendType::DIP ? -2 : 0;
        case Pid::VIBRATO_LINE_TYPE: return VibratoType::NONE;
        case Pid::GUITAR_DIVE_IS_SLACK: return false;
        default: return SLine::property_default(id);
    }
}

void GuitarBend::compute_bend_amount() {
    if (is_slack()) {
        int prev_amount = 0;
        if (GuitarBend *prev_bend = find_preceding_bend()) {
            prev_amount = prev_bend->tot_bend_amount_including_preceding_bends();
        }
        set_bend_amount_in_quarter_tones(::mu::engraving::GuitarBend::slack_bend_amount - prev_amount);
        compute_bend_text();
        return;
    }

    if (bend_type() == GuitarBendType::DIP) {
        compute_bend_text();
        return;
    }

    if (bend_type() == GuitarBendType::SCOOP) {
        set_bend_amount_in_quarter_tones(-2);
        compute_bend_text();
        return;
    }

    if (bend_type() == GuitarBendType::SLIGHT_BEND) {
        set_bend_amount_in_quarter_tones(1);
        compute_bend_text();
        return;
    }

    if (is_dive() && overlapping_bend_or_dive()) {
        compute_bend_text();
        return;
    }

    GuitarBend *prev_bend = find_preceding_bend();
    GuitarBend *prev_slack = prev_bend && prev_bend->is_slack() ? prev_bend : nullptr;

    Note *start_n = prev_slack ? prev_slack->start_note() : start_note();
    Note *end_n = end_note();

    int start_pitch = start_n->pitch();
    int end_pitch = end_n->pitch();

    int pitch_diff_in_quarter_tones = 2 * (end_pitch - start_pitch);

    int start_off_in_quarter_tones = round(start_n->cent_offset() / 50);
    int end_off_in_quarter_tones = round(end_n->cent_offset() / 50);

    pitch_diff_in_quarter_tones += end_off_in_quarter_tones - start_off_in_quarter_tones;

    if (prev_slack) {
        pitch_diff_in_quarter_tones -= ::mu::engraving::GuitarBend::slack_bend_amount;
    }

    GuitarBend *overlapping_dive = overlapping_bend_or_dive();
    if (overlapping_dive) {
        pitch_diff_in_quarter_tones -= overlapping_dive->bend_amount_in_quarter_tones();
    }

    set_bend_amount_in_quarter_tones(pitch_diff_in_quarter_tones);

    compute_bend_text();

    compute_is_invalid_or_needs_warning();
}

int GuitarBend::tot_bend_amount_including_preceding_bends() const {
    int bend_amount = bend_amount_in_quarter_tones();
    if (bend_type() == GuitarBendType::PRE_DIVE || bend_type() == GuitarBendType::PRE_BEND || bend_type() == GuitarBendType::DIP ||
        bend_type() == GuitarBendType::SCOOP) {
        return bend_amount;
    }

    GuitarBend *prev_bend = find_preceding_bend();
    while (prev_bend) {
        bend_amount += prev_bend->bend_amount_in_quarter_tones();
        if (prev_bend->bend_type() == GuitarBendType::PRE_DIVE || prev_bend->bend_type() == GuitarBendType::PRE_BEND) {
            return bend_amount;
        }
        prev_bend = prev_bend->find_preceding_bend();
    }

    return bend_amount;
}

void GuitarBend::compute_bend_text() {
    if (is_slack()) {
        mutldata()->set_bend_digit(String(u"slack"));
        return;
    }

    if (bend_type() == GuitarBendType::PRE_DIVE || bend_type() == GuitarBendType::PRE_BEND) {
        GuitarBend *prev_bend = find_preceding_bend();
        if (prev_bend && prev_bend->hold_line()) {
            mutldata()->set_bend_digit(u"");
            return;
        }
    }

    int quarters = tot_bend_amount_including_preceding_bends();

    int fulls = quarters / 4;
    int quarts = quarters % 4;
    if (fulls == 0 && quarters == 0) {
        mutldata()->set_bend_digit(String());
        return;
    }

    String string = bend_amount_to_string(fulls, quarts, style().style_b(Sid::USE_FRACTION_CHARACTERS));

    if (is_dive()) {
        if (quarters > 0) {
            string.insert(0, u"+");
        } else if (quarters < 0 && fulls == 0) {
            string.insert(0, u"-");
        }
    }

    if (string == u"1" && style().style_b(Sid::GUITAR_BEND_USE_FULL)) {
        string = String(u"full");
    }

    mutldata()->set_bend_digit(string);
}

void GuitarBend::compute_is_invalid_or_needs_warning() {
    int tot_bend_amount = tot_bend_amount_including_preceding_bends();
    static constexpr int unplayable_threshold = 14;
    static constexpr int warning_threshold = 8;
    static constexpr int dive_max = 16;
    static constexpr int dive_min = -32;

    if (is_dive()) {
        is_invalid_ = bend_amount_in_quarter_tones() == 0 || tot_bend_amount < dive_min || tot_bend_amount > dive_max;
        is_borderline_unplayable_ = false;
    } else {
        is_invalid_ = tot_bend_amount < 0 || tot_bend_amount > unplayable_threshold || bend_amount_in_quarter_tones() == 0;
        is_borderline_unplayable_ = tot_bend_amount > warning_threshold && tot_bend_amount <= unplayable_threshold;
    }
}

GuitarBend *GuitarBend::overlapping_bend_or_dive() const {
    GuitarBend *overlapping_bend_or_dive = is_dive() ? start_note()->bend_for() : start_note()->dive_for();

    if (overlapping_bend_or_dive && overlapping_bend_or_dive != this && overlapping_bend_or_dive->end_note() == end_note()) {
        return overlapping_bend_or_dive;
    }

    return nullptr;
}

GuitarBend *GuitarBend::find_preceding_bend() const {
    Note *start_n = start_note();
    while (start_n->tie_back() && start_n->tie_back()->start_note()) {
        start_n = start_n->tie_back()->start_note();
    }

    auto is_valid_type = [](GuitarBendType t) {
        switch (t) {
            case GuitarBendType::SLIGHT_BEND:
            case GuitarBendType::DIP:
            case GuitarBendType::SCOOP: return false;
            default: return true;
        }
    };

    // If this is a bend[dive], backtrack on the chain of previous dives[bends] as if they were ties
    GuitarBend *bend_or_dive_back = is_dive() ? start_n->bend_back() : start_n->dive_back();
    while (bend_or_dive_back && bend_or_dive_back->is_dive() != is_dive() && is_valid_type(bend_or_dive_back->bend_type()) &&
           !bend_or_dive_back->overlapping_bend_or_dive()) {
        start_n = bend_or_dive_back->start_note();
        bend_or_dive_back = is_dive() ? start_n->bend_back() : start_n->dive_back();
    }

    GuitarBend *preceding_bend = is_dive() ? start_n->dive_back() : start_n->bend_back();
    if (preceding_bend && preceding_bend->is_dive() == is_dive() && is_valid_type(preceding_bend->bend_type())) {
        return preceding_bend;
    }

    if (bend_type() == GuitarBendType::PRE_DIVE || bend_type() == GuitarBendType::PRE_BEND) {
        ChordRest *prev_cr = Navigation::prev_chord_rest(start_n->chord());
        if (prev_cr && prev_cr->is_rest() && is_dive()) {
            WhammyBar *whammy_bar = find_overlapping_whammy_bar(prev_cr->tick(), tick2());
            if (whammy_bar) {
                while (prev_cr && prev_cr->is_rest() && prev_cr->tick() > whammy_bar->tick()) {
                    prev_cr = Navigation::prev_chord_rest(prev_cr);
                }
            }
        }

        if (!prev_cr || !prev_cr->is_chord()) {
            return nullptr;
        }

        Chord *prev_chord = to_chord(prev_cr);
        for (Note *note : prev_chord->notes()) {
            while (note->tie_back() && note->tie_back()->start_note()) {
                note = note->tie_back()->start_note();
            }
            GuitarBend *prev_bend = note->bend_back();
            bool is_valid = false;
            if (prev_bend && prev_bend->is_dive() == is_dive() && is_valid_type(prev_bend->bend_type()) &&
                prev_bend->tot_bend_amount_including_preceding_bends() == bend_amount_in_quarter_tones()) {
                is_valid = true;
            }
            if (is_valid) {
                if (bend_type() == GuitarBendType::PRE_BEND) {
                    Note *sn = prev_bend->start_note();
                    is_valid &= sn->fret() == start_note()->fret() && sn->string() == start_note()->string();
                }
                if (is_valid) {
                    return prev_bend;
                }
            }
        }
    }

    return nullptr;
}

GuitarBend *GuitarBend::find_following_pre_bend_or_dive() const {
    Note *end_n = end_note();
    while (end_n->tie_for() && end_n->tie_for()->end_note()) {
        end_n = end_n->tie_for()->end_note();
    }

    ChordRest *next_cr = Navigation::next_chord_rest(end_n->chord());
    if (is_dive() && next_cr && next_cr->is_rest()) {
        WhammyBar *whammy_bar = find_overlapping_whammy_bar(tick(), next_cr->end_tick());
        if (whammy_bar) {
            while (next_cr && next_cr->is_rest() && next_cr->tick() < whammy_bar->tick2()) {
                next_cr = Navigation::next_chord_rest(next_cr);
            }
        }
    }

    if (!next_cr || !next_cr->is_chord()) {
        return nullptr;
    }

    Chord *next_chord = to_chord(next_cr);
    for (Note *note : next_chord->notes()) {
        while (note->tie_for() && note->tie_for()->end_note()) {
            note = note->tie_for()->end_note();
        }
        GuitarBend *bend = note->bend_for();
        bool is_valid = false;
        if (bend && bend->is_dive() == is_dive() &&
            (bend->bend_type() == GuitarBendType::PRE_DIVE || bend->bend_type() == GuitarBendType::PRE_BEND) &&
            bend->bend_amount_in_quarter_tones() == tot_bend_amount_including_preceding_bends()) {
            is_valid = true;
        }
        if (is_valid) {
            if (bend->bend_type() == GuitarBendType::PRE_BEND) {
                Note *start_n = bend->start_note();
                is_valid &= start_n->fret() == start_note()->fret() && start_n->string() == start_note()->string();
            }
            if (is_valid) {
                return bend;
            }
        }
    }

    return nullptr;
}

WhammyBar *GuitarBend::find_overlapping_whammy_bar(Fraction start_tick, Fraction end_tick) const {
    const auto &spanners = score()->spanner_map().find_overlapping(start_tick.ticks(), end_tick.ticks());
    for (const auto &item : spanners) {
        Spanner *spanner = item.value;
        if (spanner->is_whammy_bar() && spanner->tick() < start_tick && spanner->tick2() >= end_tick) {
            Staff *spanner_staff = spanner->staff();
            Staff *this_staff = staff();
            if (spanner_staff == this_staff || spanner_staff->is_linked(this_staff)) {
                return to_whammy_bar(spanner);
            }
        }
    }

    return nullptr;
}

void GuitarBend::update_hold_line() {
    Note *start_of_hold = nullptr;
    Note *end_of_hold = nullptr;

    bool is_dip_with_vibrato = bend_type() == GuitarBendType::DIP && dip_vibrato_type() != VibratoType::NONE;
    bool can_have_hold_line = is_dip_with_vibrato || (staff_type()->is_tab_staff() && !is_full_release() &&
                                                      bend_type() != GuitarBendType::SCOOP && bend_type() != GuitarBendType::DIP);
    if (is_dive()) {
        can_have_hold_line &= start_note() && start_note() == start_note()->chord()->up_note();
    }

    bool needs_hold_line = false;
    if (can_have_hold_line) {
        start_of_hold = end_note();
        end_of_hold = start_of_hold;

        GuitarBend *following_pre_dive = find_following_pre_bend_or_dive();
        end_of_hold = following_pre_dive ? following_pre_dive->start_note() : start_of_hold;

        if (end_of_hold == start_of_hold) {
            while (end_of_hold->tie_for() && end_of_hold->tie_for()->end_note()) {
                end_of_hold = end_of_hold->tie_for()->end_note();
            }
        }

        if (is_dive()) {
            GuitarBend *bend_f = end_of_hold->bend_for();
            while (bend_f && !bend_f->is_dive() && !bend_f->overlapping_bend_or_dive() && bend_f->end_note() != end_of_hold) {
                end_of_hold = bend_f->end_note();
                bend_f = end_of_hold->bend_for();
            }
        } else {
            GuitarBend *dive_f = end_of_hold->dive_for();
            while (dive_f && !dive_f->overlapping_bend_or_dive() && dive_f->end_note() != end_of_hold) {
                end_of_hold = dive_f->end_note();
                dive_f = end_of_hold->dive_for();
            }
        }

        if (show_hold_line() == GuitarBendShowHoldLine::AUTO) {
            needs_hold_line = end_of_hold != start_of_hold;
        } else {
            needs_hold_line = show_hold_line() == GuitarBendShowHoldLine::SHOW;
        }
        needs_hold_line |= is_dip_with_vibrato;
    }

    if (!needs_hold_line) {
        if (hold_line_) {
            if (!hold_line_->segments_empty()) {
                hold_line_->erase_spanner_segments();
            }
            delete hold_line_;
            hold_line_ = nullptr;
        }
        return;
    }

    if (end_of_hold == start_of_hold) {
        Chord *guessed_end_chord = start_of_hold->chord()->next();
        if (guessed_end_chord) {
            for (Note *note : guessed_end_chord->notes()) {
                if (note->is_pre_bend_or_dive_start()) {
                    end_of_hold = note;
                    break;
                }
            }
        }
    }

    if (!hold_line_) {
        hold_line_ = new GuitarBendHold(this);
    }

    hold_line_->set_start_element(start_of_hold);
    hold_line_->set_end_element(end_of_hold);
    hold_line_->set_tick(start_of_hold->tick());
    hold_line_->set_tick2(end_of_hold->tick());
    hold_line_->set_track(track());
    hold_line_->set_track2(track());
    hold_line_->set_generated(true);
    hold_line_->set_ownership_parent(this);
}

double GuitarBend::line_width() const {
    if (is_dive()) {
        return (staff_type() && staff_type()->is_tab_staff()) ? style().style_absolute(Sid::GUITAR_DIVE_LINE_WIDTH_TAB) :
                                                                style().style_absolute(Sid::GUITAR_DIVE_LINE_WIDTH);
    }

    return (staff_type() && staff_type()->is_tab_staff()) ? style().style_absolute(Sid::GUITAR_BEND_LINE_WIDTH_TAB) :
                                                            style().style_absolute(Sid::GUITAR_BEND_LINE_WIDTH);
}

/****************************************
 *              GuitarBendSegment
 * **************************************/

GuitarBendSegment::GuitarBendSegment(GuitarBend *sp) : LineSegment(ElementType::GUITAR_BEND_SEGMENT, sp, ElementFlag::MOVABLE) {
    text_ = new GuitarBendText(this);
    set_flag(ElementFlag::ON_STAFF, true);
}

GuitarBendSegment::GuitarBendSegment(const GuitarBendSegment &s) : LineSegment(s) {
    vertex_point_off_ = s.vertex_point_off_;
    text_ = new GuitarBendText(this);
}

GuitarBendSegment::~GuitarBendSegment() { delete text_; }

PropertyValue GuitarBendSegment::get_property(Pid id) const {
    switch (id) {
        case Pid::BEND_VERTEX_OFF: return vertex_point_off();
        default: return LineSegment::get_property(id);
    }
}

bool GuitarBendSegment::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::BEND_VERTEX_OFF: set_vertex_point_off(v.value<PointF>()); break;
        default: return LineSegment::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

PropertyValue GuitarBendSegment::property_default(Pid id) const {
    switch (id) {
        case Pid::BEND_VERTEX_OFF: return PointF();
        default: return LineSegment::property_default(id);
    }
}

EngravingObject *GuitarBendSegment::property_delegate(Pid id) const {
    switch (id) {
        case Pid::DIRECTION:
        case Pid::BEND_SHOW_HOLD_LINE:
        case Pid::POSITION_LINKED_TO_MASTER:
        case Pid::APPEARANCE_LINKED_TO_MASTER:
        case Pid::GUITAR_DIVE_TAB_POS:
        case Pid::VIBRATO_LINE_TYPE: return guitar_bend();
        default: return LineSegment::property_delegate(id);
    }
}

void GuitarBendSegment::reset() {
    undo_reset_property(Pid::DIRECTION);
    undo_reset_property(Pid::BEND_VERTEX_OFF);
    LineSegment::reset();
}

double GuitarBendSegment::line_width() const { return guitar_bend()->line_width(); }

void GuitarBendSegment::scan_elements(std::function<void(EngravingItem *)> func) {
    func(text_);
    LineSegment::scan_elements(func);
}

bool GuitarBendSegment::is_user_modified() const {
    bool modified = !vertex_point_off().is_null() || (text_ && text_->is_user_modified());
    return modified || LineSegment::is_user_modified();
}

Color GuitarBend::cur_color(const rendering::PaintOptions &opt) const {
    if (!opt.is_printing && MScore::warn_guitar_bends) {
        if (is_invalid_) {
            return selected() ? configuration()->critical_selected_color() : configuration()->critical_color();
        }

        if (is_borderline_unplayable_) {
            return selected() ? configuration()->warning_selected_color() : configuration()->warning_color();
        }
    }

    return EngravingItem::cur_color(opt);
}

void GuitarBend::adapt_bends_from_tab_to_standard_staff(const Staff *staff) {
    // On tabs, bends force end notes to be invisible. When switching to
    // normal staff we need to turn all the end notes visible again.

    auto process_bends = [](Chord *chord) {
        for (Note *note : chord->notes()) {
            GuitarBend *bend = note->bend_for();
            if (!bend) {
                continue;
            }
            if (!bend->start_note()->override_bend_visibility_rules()) {
                bend->start_note()->set_visible(true);
            }
            if (!bend->end_note()->override_bend_visibility_rules()) {
                bend->end_note()->set_visible(true);
            }
        }
    };

    staff_idx_t staff_idx = staff->idx();
    track_idx_t start_track = staff2track(staff_idx);
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    for (Segment *segment = staff->score()->first_segment(SegmentType::CHORD_REST); segment; segment = segment->next1()) {
        if (!segment->is_chord_rest_type()) {
            continue;
        }
        for (track_idx_t track = start_track; track < end_track; ++track) {
            EngravingItem *item = segment->element(track);
            if (!item || !item->is_chord()) {
                continue;
            }
            Chord *chord = to_chord(item);
            process_bends(chord);
            for (Chord *grace : chord->grace_notes()) {
                process_bends(grace);
            }
        }
    }
}

GuitarBendType GuitarBend::bend_type_from_action_icon(ActionIconType action_icon_type) {
    switch (action_icon_type) {
        case ActionIconType::STANDARD_BEND: return GuitarBendType::BEND;
        case ActionIconType::PRE_BEND: return GuitarBendType::PRE_BEND;
        case ActionIconType::GRACE_NOTE_BEND: return GuitarBendType::GRACE_NOTE_BEND;
        case ActionIconType::SLIGHT_BEND: return GuitarBendType::SLIGHT_BEND;
        case ActionIconType::DIVE: return GuitarBendType::DIVE;
        case ActionIconType::PRE_DIVE: return GuitarBendType::PRE_DIVE;
        case ActionIconType::DIP: return GuitarBendType::DIP;
        case ActionIconType::SCOOP: return GuitarBendType::SCOOP;
        default: return GuitarBendType::BEND;
    }
}

void GuitarBend::set_bend_type(GuitarBendType t) {
    bend_type_ = t;

    reset_property(Pid::GUITAR_BEND_AMOUNT);
    reset_property(Pid::BEND_START_TIME_FACTOR);
    reset_property(Pid::BEND_END_TIME_FACTOR);
    reset_property(Pid::BEND_TARGET_TIME_FACTOR);
}

void GuitarBend::set_target_time_factor(float f) {
    IF_ASSERT_FAILED(muse::real_is_equal_or_less(f, end_time_factor())) { return; }

    target_time_factor_ = f;
}

/****************************************
 *            GuitarBendHold
 * **************************************/

GuitarBendHold::GuitarBendHold(GuitarBend *parent) : SLine(ElementType::GUITAR_BEND_HOLD, parent, ElementFlag::MOVABLE) {
    reset_property(Pid::LINE_STYLE);
}

GuitarBendHold::GuitarBendHold(const GuitarBendHold &h) : SLine(h) {}

LineSegment *GuitarBendHold::create_line_segment() {
    GuitarBendHoldSegment *seg = new GuitarBendHoldSegment(this);
    seg->set_track(track());
    seg->set_color(line_color());
    return seg;
}

PropertyValue GuitarBendHold::property_default(Pid id) const {
    switch (id) {
        case Pid::LINE_STYLE: return LineType::DASHED;
        default: return SLine::property_default(id);
    }
}

Note *GuitarBendHold::start_note() const {
    EngravingItem *start_el = start_element();
    assert(start_el && start_el->is_note());
    return to_note(start_el);
}

Note *GuitarBendHold::end_note() const {
    EngravingItem *end_el = end_element();
    assert(end_el && end_el->is_note());
    return to_note(end_el);
}

double GuitarBendHold::line_width() const {
    return style().style_absolute(parent() && to_guitar_bend(parent())->is_dive() ? Sid::GUITAR_DIVE_LINE_WIDTH_TAB :
                                                                                    Sid::GUITAR_BEND_LINE_WIDTH_TAB);
}

/****************************************
 *         GuitarBendHoldSegment
 * **************************************/

GuitarBendHoldSegment::GuitarBendHoldSegment(GuitarBendHold *sp)
    : LineSegment(ElementType::GUITAR_BEND_HOLD_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    set_flag(ElementFlag::ON_STAFF, true);
}

/****************************************
 *         GuitarBendText
 * **************************************/

static const ElementStyle guitar_bend_style{};

GuitarBendText::GuitarBendText(GuitarBendSegment *parent) : TextBase(ElementType::GUITAR_BEND_TEXT, parent, TextStyleType::BEND) {
    init_element_style(&::mu::engraving::guitar_bend_style);
}
} // namespace mu::engraving
