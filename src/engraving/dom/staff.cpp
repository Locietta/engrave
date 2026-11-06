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

#include "staff.h"

#include "../editing/editcapo.h"
#include "../editing/editstaffbrackets.h"
#include "../editing/transpose.h"
#include "barline.h"
#include "clef.h"
#include "cleflist.h"
#include "containers.h"
#include "excerpt.h"
#include "factory.h"
#include "iengravingconfiguration.h" // IWYU pragma: keep
#include "instrtemplate.h"
#include "linkedobjects.h"
#include "masterscore.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"
#include "ottava.h"
#include "part.h"
#include "score.h"
#include "segment.h"
#include "stafftype.h"
#include "timesig.h"
#include "types/typesconv.h"
#include "utils.h"

// #define DEBUG_CLEFS

#include "log.h"

using namespace mu;
using namespace mu::engraving;

#ifdef DEBUG_CLEFS
#define DUMP_CLEFS(s) dump_clefs(s)
#else
#define DUMP_CLEFS(s)
#endif

namespace mu::engraving {
//---------------------------------------------------------
//   Staff
//---------------------------------------------------------

Staff::Staff(Part *parent) : EngravingItem(ElementType::STAFF, parent) {
    color_ = configuration()->default_color();
    init_from_staff_type(0);
}

//---------------------------------------------------------
//   copy ctor
//---------------------------------------------------------

Staff::Staff(const Staff &staff) : EngravingItem(staff) {
    init(&staff);
    part_ = staff.part_;
}

//---------------------------------------------------------
//   clone
//---------------------------------------------------------

Staff *Staff::clone() const { return new Staff(*this); }

//---------------------------------------------------------
//   idx
//---------------------------------------------------------

staff_idx_t Staff::idx() const { return muse::index_of(score()->staves(), (Staff *) this); }

Part *Staff::origin_part(const Fraction &tick) const {
    // Hides EngravingItem::originPart, whose track and tick are meaningless for a staff
    // Always returns part of voice 0
    if (!part_ || !part_->is_shared_part()) {
        return part_;
    }

    return part_->origin_part(staff2track(idx(), 0), tick);
}

//---------------------------------------------------------
//   triggerLayout
//---------------------------------------------------------

void Staff::trigger_layout() const { score()->set_layout_all(idx(), this); }

void Staff::trigger_layout(const Fraction &tick) { score()->set_layout(tick, idx()); }

Staff *Staff::find_linked_in_score(const Score *score) const {
    for (Staff *staff : score->staves()) {
        if (staff->id() == id()) {
            return staff;
        }
    }

    return nullptr;
}

track_idx_t Staff::get_linked_track_in_staff(const Staff *linked_staff, const track_idx_t original_track) const {
    IF_ASSERT_FAILED(linked_staff && original_track != muse::nidx) { return muse::nidx; }

    Score *this_score = score();
    Score *linked_staff_score = linked_staff->score();
    staff_idx_t linked_staff_idx = linked_staff->idx();

    if (this_score == linked_staff_score) {
        // For staves linked within the same score, voices are always mapped 1 to 1
        voice_idx_t voice = track2voice(original_track);
        return staff2track(linked_staff_idx, voice);
    }

    // NOTE 1: if linkedStaff has a different id than *this, it means that there isn't direct voice mapping between
    // the two. Example: if we have guitar+TAB in the score, with corresponding guitar+TAB in the part, *this
    // may be the notation-staff of the score and linkedStaff may be the TAB-staff of the part. In that case, the
    // correct voice mapping must be obtained by looking for the staff with same id in thisScore.

    track_idx_t ref_track = original_track;
    if (linked_staff->id() != id()) {
        Staff *corresponding_staff_in_this_score = linked_staff->find_linked_in_score(this_score);
        if (!corresponding_staff_in_this_score) {
            return muse::nidx;
        }
        voice_idx_t original_voice = track2voice(original_track);
        ref_track = staff2track(corresponding_staff_in_this_score->idx(), original_voice);
    }

    // NOTE 2: TracksMap is a map from a track in the *score* to track(s) in the *part*.
    // If we are in the master score, refTrack corresponds to one of the keys in the map, so we can retrieve
    // the linked tracks by simply querying the map by key. However, if we are in a part, we need to search for
    // refTrack among the *values* of the map, and the corresponding key gives us the linked track in the score.
    // If linkedStaff is *also* in a part, we need to do it in two steps: first find the linked track in the score,
    // then use it to find the linked track in linkeStaff's part.

    if (this_score->is_master()) {
        const TracksMap &tracks_map = linked_staff_score->excerpt()->tracks_mapping();
        std::vector<track_idx_t> linked_tracks = muse::values(tracks_map, ref_track);
        for (track_idx_t track : linked_tracks) {
            if (track2staff(track) == linked_staff_idx) {
                return track;
            }
        }

        return muse::nidx;
    }

    const TracksMap &this_tracks_map = this_score->excerpt()->tracks_mapping();
    track_idx_t linked_track_in_score = muse::nidx;
    for (auto pair : this_tracks_map) {
        track_idx_t track_in_score = pair.first;
        std::vector<track_idx_t> tracks_in_part = muse::values(this_tracks_map, track_in_score);
        for (track_idx_t track_in_part : tracks_in_part) {
            if (track_in_part == ref_track) {
                linked_track_in_score = track_in_score;
                break;
            }
        }
        if (linked_track_in_score != muse::nidx) {
            break;
        }
    }

    if (linked_staff_score->is_master() || linked_track_in_score == muse::nidx) {
        return linked_track_in_score;
    }

    const TracksMap &linked_tracks_map = linked_staff_score->excerpt()->tracks_mapping();
    std::vector<track_idx_t> linked_tracks = muse::values(linked_tracks_map, linked_track_in_score);
    for (track_idx_t track : linked_tracks) {
        if (track2staff(track) == linked_staff_idx) {
            return track;
        }
    }

    return muse::nidx;
}

bool Staff::track_has_links_in_voice_zero(track_idx_t track) {
    for (Staff *linked_staff : staff_list()) {
        track_idx_t linked_track = get_linked_track_in_staff(linked_staff, track);
        if (linked_track != muse::nidx && track2voice(linked_track) == 0) {
            return true;
        }
    }

    return false;
}

void Staff::undo_set_show_measure_numbers(bool show) {
    bool is_top_stave = score()->staves().front() == this;
    if (show) {
        undo_change_property(Pid::SHOW_MEASURE_NUMBERS, is_top_stave ? AutoOnOff::AUTO : AutoOnOff::ON);
    } else {
        undo_change_property(Pid::SHOW_MEASURE_NUMBERS, is_top_stave ? AutoOnOff::OFF : AutoOnOff::AUTO);
    }
}

bool Staff::should_show_measure_numbers() const {
    MeasureNumberPlacement placement_mode = style().style_v(Sid::MEASURE_NUMBER_PLACEMENT_MODE).value<MeasureNumberPlacement>();
    switch (placement_mode) {
        case MeasureNumberPlacement::ABOVE_SYSTEM: return score()->staves().front() == this;
        case MeasureNumberPlacement::BELOW_SYSTEM: return score()->staves().back() == this;
        case MeasureNumberPlacement::ON_SYSTEM_OBJECT_STAVES: {
            bool is_top_stave = score()->staves().front() == this;
            return (is_top_stave && show_measure_numbers_ != AutoOnOff::OFF) ||
                   (is_system_object_staff() && show_measure_numbers_ == AutoOnOff::ON);
        }
        case MeasureNumberPlacement::ON_ALL_STAVES: return show();
    }

    return false;
}

bool Staff::is_last_of_score() const { return score()->staves().empty() ? false : score()->staves().back() == this; }

bool Staff::is_system_object_staff() const { return score() && muse::contains(score()->system_object_staves(), const_cast<Staff *>(this)); }

bool Staff::has_system_objects_below_bottom_staff() const {
    return is_system_object_staff() && is_last_of_score() && style().style_b(Sid::SYSTEM_OBJECTS_BELOW_BOTTOM_STAFF);
}

String Staff::individual_staff_name_long(const Fraction &tick) const {
    IF_ASSERT_FAILED(part_) { return String(); }

    return staff_type(tick)->long_name();
}

String Staff::individual_staff_name_short(const Fraction &tick) const {
    IF_ASSERT_FAILED(part_) { return String(); }

    return staff_type(tick)->short_name();
}

bool Staff::playback_voice(int voice) const { return playback_voice_[voice]; }

void Staff::set_playback_voice(int voice, bool val) { playback_voice_[voice] = val; }

const std::array<bool, voices> &Staff::visibility_voices() const { return visibility_voices_; }

bool Staff::is_voice_visible(voice_idx_t voice) const {
    if (voice >= ::mu::engraving::voices) {
        return false;
    }

    return visibility_voices_[voice];
}

void Staff::set_voice_visible(voice_idx_t voice, bool visible) {
    if (voice >= ::mu::engraving::voices) {
        return;
    }

    visibility_voices_[voice] = visible;
}

bool Staff::can_disable_voice() const {
    auto voices = visibility_voices();
    int count_of_visible_voices = 0;
    for (size_t i = 0; i < voices.size(); ++i) {
        if (voices[i]) {
            count_of_visible_voices++;
        }
    }

    return count_of_visible_voices > 1;
}

void Staff::update_visibility_voices(const Staff *master_staff, const TracksMap &tracks) {
    if (tracks.empty()) {
        visibility_voices_ = {true, true, true, true};
        return;
    }

    std::array<bool, ::mu::engraving::voices> voices{false, false, false, false};

    staff_idx_t master_staff_idx = master_staff->idx();
    staff_idx_t staff_idx = idx();

    voice_idx_t voice_index = 0;
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; voice++) {
        std::vector<track_idx_t> master_staff_tracks =
            muse::values(tracks, master_staff_idx * ::mu::engraving::voices + voice % ::mu::engraving::voices);
        bool is_voice_visible =
            muse::contains(master_staff_tracks, staff_idx * ::mu::engraving::voices + voice_index % ::mu::engraving::voices);
        if (is_voice_visible) {
            voices[voice] = true;
            voice_index++;
        }
    }

    visibility_voices_ = voices;
}

bool Staff::reflect_transposition_in_linked_tab() const { return reflect_transposition_in_linked_tab_; }

void Staff::set_reflect_transposition_in_linked_tab(bool reflect) { reflect_transposition_in_linked_tab_ = reflect; }

//---------------------------------------------------------
//   partName
//---------------------------------------------------------

String Staff::part_name() const { return part_->part_name(); }

//---------------------------------------------------------
//   Staff::clefType
//---------------------------------------------------------

ClefTypeList Staff::clef_type(const Fraction &tick) const {
    ClefTypeList ct = clefs_.clef(tick.ticks());
    if (ct.concert_clef == ClefType::INVALID) {
        // Clef compatibility based on instrument (override StaffGroup)
        StaffGroup staff_group = staff_type(tick)->group();
        if (staff_group != StaffGroup::TAB) {
            staff_group = origin_part(tick)->instrument(tick)->use_drumset() ? StaffGroup::PERCUSSION : StaffGroup::STANDARD;
        }

        switch (staff_group) {
            case StaffGroup::TAB: {
                ClefType sct = ClefType(style().style_i(Sid::TAB_CLEF));
                ct = staff_type(tick)->lines() <= 4 ? ClefTypeList(sct == ClefType::TAB ? ClefType::TAB4 : ClefType::TAB4_SERIF) :
                                                      ClefTypeList(sct == ClefType::TAB ? ClefType::TAB : ClefType::TAB_SERIF);
            } break;
            case StaffGroup::STANDARD: ct = default_clef_type(); break;
            case StaffGroup::PERCUSSION: ct = ClefTypeList(ClefType::PERC); break;
        }
    }
    return ct;
}

//---------------------------------------------------------
//   Staff::clef
//---------------------------------------------------------

ClefType Staff::clef(const Fraction &tick) const {
    ClefTypeList c = clef_type(tick);
    return style().style_b(Sid::CONCERT_PITCH) ? c.concert_clef : c.transposing_clef;
}

//---------------------------------------------------------
//   Staff::nextClefTick
//
//    return the tick of next clef after tick
//    return last tick of score if not found
//---------------------------------------------------------

Fraction Staff::next_clef_tick(const Fraction &tick) const {
    Fraction t = Fraction::from_ticks(clefs_.next_clef_tick(tick.ticks()));
    return t != Fraction(-1, 1) ? t : score()->end_tick();
}

//---------------------------------------------------------
//   Staff::currentClefTick
//
//    return the tick position of the clef currently
//    in effect at tick
//    return 0, if no such clef
//---------------------------------------------------------

Fraction Staff::current_clef_tick(const Fraction &tick) const { return Fraction::from_ticks(clefs_.current_clef_tick(tick.ticks())); }

String Staff::staff_name() const { return TConv::translated_user_name(clef_type(Fraction()).transposing_clef); }

#ifndef NDEBUG
//---------------------------------------------------------
//   dumpClef
//---------------------------------------------------------

void Staff::dump_clefs(const char *title) const {
    LOGD("(%zd): %s", clefs_.size(), title);
    for (auto &i : clefs_) {
        LOGD("  %d: %d %d", i.first, int(i.second.concert_clef), int(i.second.transposing_clef));
    }
}

//---------------------------------------------------------
//   dumpKeys
//---------------------------------------------------------

void Staff::dump_keys(const char *title) const {
    LOGD("(%zd): %s", keys_.size(), title);
    for (auto &i : keys_) {
        LOGD("  %d: %d", i.first, int(i.second.key()));
    }
}

//---------------------------------------------------------
//   dumpTimeSigs
//---------------------------------------------------------

void Staff::dump_time_sigs(const char *title) const {
    LOGD("size (%zd) staffIdx %zu: %s", timesigs_.size(), idx(), title);
    for (auto &i : timesigs_) {
        LOGD("  %d: %d/%d", i.first, i.second->sig().numerator(), i.second->sig().denominator());
    }
}

#endif

//---------------------------------------------------------
//   setClef
//---------------------------------------------------------

void Staff::set_clef(Clef *clef) {
    if (clef->generated()) {
        return;
    }
    Fraction tick = clef->segment()->tick();
    for (Segment *s = clef->segment()->next1(); s && s->tick() == tick; s = s->next1()) {
        if ((s->segment_type() == SegmentType::CLEF || s->segment_type() == SegmentType::HEADER_CLEF) && s->element(clef->track()) &&
            !s->element(clef->track())->generated()) {
            // adding this clef has no effect on the clefs list
            return;
        }
    }
    clefs_.set_clef(clef->segment()->tick().ticks(), clef->clef_type_list());
    DUMP_CLEFS("setClef");
}

//---------------------------------------------------------
//   removeClef
//---------------------------------------------------------

void Staff::remove_clef(const Clef *clef) {
    if (clef->generated()) {
        return;
    }
    Fraction tick = clef->segment()->tick();
    for (Segment *s = clef->segment()->next1(); s && s->tick() == tick; s = s->next1()) {
        if ((s->segment_type() == SegmentType::CLEF || s->segment_type() == SegmentType::HEADER_CLEF) && s->element(clef->track()) &&
            !s->element(clef->track())->generated()) {
            // removal of this clef has no effect on the clefs list
            return;
        }
    }
    clefs_.erase(clef->segment()->tick().ticks());
    for (Segment *s = clef->segment()->prev1(); s && s->tick() == tick; s = s->prev1()) {
        if ((s->segment_type() == SegmentType::CLEF || s->segment_type() == SegmentType::HEADER_CLEF) && s->element(clef->track()) &&
            !s->element(clef->track())->generated()) {
            // a previous clef at the same tick position gets valid
            clefs_.set_clef(tick.ticks(), to_clef(s->element(clef->track()))->clef_type_list());
            break;
        }
    }
    DUMP_CLEFS("removeClef");
}

//---------------------------------------------------------
//   timeStretch
//---------------------------------------------------------

Fraction Staff::time_stretch(const Fraction &tick) const {
    TimeSig *timesig = time_sig(tick);
    return timesig ? timesig->stretch() : Fraction(1, 1);
}

//---------------------------------------------------------
//   timeSig
//    lookup time signature before or at tick
//---------------------------------------------------------

TimeSig *Staff::time_sig(const Fraction &tick) const {
    auto i = timesigs_.upper_bound(tick.ticks());
    if (i != timesigs_.begin()) {
        --i;
    }
    if (i == timesigs_.end()) {
        return 0;
    } else if (tick < Fraction::from_ticks(i->first)) {
        return 0;
    }
    return i->second;
}

//---------------------------------------------------------
//   nextTimeSig
//    lookup time signature at tick or after
//---------------------------------------------------------

TimeSig *Staff::next_time_sig(const Fraction &tick) const {
    auto i = timesigs_.lower_bound(tick.ticks());
    return (i == timesigs_.end()) ? 0 : i->second;
}

//---------------------------------------------------------
//   currentTimeSigTick
//
//    return the tick position of the time sig currently
//    in effect at tick
//---------------------------------------------------------

Fraction Staff::current_time_sig_tick(const Fraction &tick) const {
    if (timesigs_.empty()) {
        return Fraction(0, 1);
    }
    auto i = timesigs_.upper_bound(tick.ticks());
    if (i == timesigs_.begin()) {
        return Fraction(0, 1);
    }
    --i;
    return Fraction::from_ticks(i->first);
}

//---------------------------------------------------------
//   group
//---------------------------------------------------------

const Groups &Staff::group(const Fraction &tick) const {
    TimeSig *ts = time_sig(tick);
    if (ts) {
        if (!ts->groups().empty()) {
            return ts->groups();
        }
        return Groups::endings(ts->sig());
    }
    Measure *m = score()->tick2measure(tick);
    return Groups::endings(m ? m->timesig() : Fraction(4, 4));
}

//---------------------------------------------------------
//   addTimeSig
//---------------------------------------------------------

void Staff::add_time_sig(TimeSig *timesig) {
    if (timesig->segment()->segment_type() == SegmentType::TIME_SIG) {
        timesigs_[timesig->segment()->tick().ticks()] = timesig;
    }
    //      dumpTimeSigs("after addTimeSig");
}

//---------------------------------------------------------
//   removeTimeSig
//---------------------------------------------------------

void Staff::remove_time_sig(TimeSig *timesig) {
    if (timesig->segment()->segment_type() == SegmentType::TIME_SIG) {
        if (timesigs_[timesig->segment()->tick().ticks()] == timesig) {
            timesigs_.erase(timesig->segment()->tick().ticks());
        }
    }
    //      dumpTimeSigs("after removeTimeSig");
}

//---------------------------------------------------------
//   clearTimeSig
//---------------------------------------------------------

void Staff::clear_time_sig() { timesigs_.clear(); }

//---------------------------------------------------------
//   Staff::transpose
//
//    actual staff transposioton at tick
//    (taking key into account)
//---------------------------------------------------------

Interval Staff::transpose(const Fraction &tick) const {
    // get real transposition

    const Part *p = origin_part(tick);
    Interval v = p->instrument(tick)->transpose();
    if (v.is_zero()) {
        return v;
    }
    Key c_key = concert_key(tick);
    v.flip();
    Key t_key = Transpose::transpose_key(c_key, v, p->prefer_sharp_flat());
    v.flip();

    int chromatic = (7 * (static_cast<int>(c_key) - static_cast<int>(t_key))) % 12;
    if (chromatic < 0) {
        chromatic += 12;
    }
    int diatonic = (4 * (static_cast<int>(c_key) - static_cast<int>(t_key))) % 7;
    if (diatonic < 0) {
        diatonic += 7;
    }

    if (v.chromatic < 0 || v.diatonic < 0) {
        chromatic -= 12;
        diatonic -= 7;
    }

    v.chromatic = v.chromatic - (v.chromatic % 12) + (chromatic % 12);
    v.diatonic = v.diatonic - (v.diatonic % 7) + (diatonic % 7);

    return v;
}

//---------------------------------------------------------
//   Staff::keySigEvent
//
//    locates the key sig currently in effect at tick
//---------------------------------------------------------

KeySigEvent Staff::key_sig_event(const Fraction &tick) const { return keys_.key(tick.ticks()); }

//---------------------------------------------------------
//   setKey
//---------------------------------------------------------

void Staff::set_key(const Fraction &tick, KeySigEvent k) { keys_.set_key(tick.ticks(), k); }

//---------------------------------------------------------
//   removeKey
//---------------------------------------------------------

void Staff::remove_key(const Fraction &tick) { keys_.erase(tick.ticks()); }

//---------------------------------------------------------
//   prevkey
//---------------------------------------------------------

KeySigEvent Staff::prev_key(const Fraction &tick) const { return keys_.prev_key(tick.ticks()); }

//---------------------------------------------------------
//   Staff::nextKeyTick
//
//    return the tick at which the key sig after tick is located
//    return 0, if no such a key sig
//---------------------------------------------------------

Fraction Staff::next_key_tick(const Fraction &tick) const {
    Fraction t = Fraction::from_ticks(keys_.next_key_tick(tick.ticks()));
    return t != Fraction(-1, 1) ? t : score()->end_tick();
}

//---------------------------------------------------------
//   Staff::currentKeyTick
//
//    return the tick position of the key currently
//    in effect at tick
//    return 0, if no such a key sig
//---------------------------------------------------------

Fraction Staff::current_key_tick(const Fraction &tick) const { return Fraction::from_ticks(keys_.current_key_tick(tick.ticks())); }

//---------------------------------------------------------
//   height
//---------------------------------------------------------

double Staff::staff_height() const {
    Fraction tick = Fraction(0, 1);
    return (lines(tick) - 1) * spatium(tick) * staff_type(tick)->line_distance().val();
}

double Staff::staff_height(const Fraction &tick) const {
    return (lines(tick) - 1) * spatium(tick) * staff_type(tick)->line_distance().val();
}

//---------------------------------------------------------
//   spatium
//---------------------------------------------------------

double Staff::spatium(const Fraction &tick) const { return style().spatium() * staff_mag(tick); }

double Staff::spatium(const EngravingItem *e) const { return style().spatium() * staff_mag(e); }

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Staff::staff_mag(const StaffType *stt) const {
    return (stt->is_small() ? style().style_d(Sid::SMALL_STAFF_MAG) : 1.0) * stt->user_mag();
}

double Staff::staff_mag(const Fraction &tick) const { return staff_mag(staff_type(tick)); }

double Staff::staff_mag(const EngravingItem *element) const { return staff_mag(staff_type_for_element(element)); }

//---------------------------------------------------------
//   swing
//---------------------------------------------------------

SwingParameters Staff::swing(const Fraction &tick) const {
    SwingParameters sp;
    int swing_unit = 0;
    muse::ByteArray ba = style().style_st(Sid::SWING_UNIT).to_ascii();
    DurationType unit = TConv::from_xml(ba.const_char(), DurationType::V_INVALID);
    int swing_ratio = style().style_i(Sid::SWING_RATIO);
    if (unit == DurationType::V_EIGHTH) {
        swing_unit = Constants::division / 2;
    } else if (unit == DurationType::V_16TH) {
        swing_unit = Constants::division / 4;
    } else if (unit == DurationType::V_ZERO) {
        swing_unit = 0;
    }
    sp.swing_ratio = swing_ratio;
    sp.swing_unit = swing_unit;
    if (swing_map_.empty()) {
        return sp;
    }

    auto it = muse::find_less_or_equal(swing_map_, tick.ticks());
    return it == swing_map_.cend() ? sp : it->second;
}

const CapoParams &Staff::capo(const Fraction &tick) const {
    static const CapoParams dummy;
    if (capo_map_.empty()) {
        return dummy;
    }

    auto it = muse::find_less_or_equal(capo_map_, tick.ticks());
    return it == capo_map_.cend() ? dummy : it->second;
}

void Staff::insert_capo_params(const Fraction &tick, const CapoParams &params, bool ignore_notation_update) {
    if (ignore_notation_update) {
        capo_map_.insert_or_assign(tick.ticks(), params);
        return;
    }
    auto is_need_update = [](const CapoParams &old_params, const CapoParams &new_params) -> bool {
        return !(old_params.active == new_params.active && old_params.transpose_mode == new_params.transpose_mode &&
                 old_params.fret_position == new_params.fret_position && old_params.ignored_strings == new_params.ignored_strings);
    };

    int start_tick = tick.ticks();
    int end_tick = -1;

    if (auto it = capo_map_.find(start_tick); it == capo_map_.end()) {
        auto result = capo_map_.insert({start_tick, params});
        if (const auto next_it = std::next(result.first); next_it != capo_map_.end()) {
            end_tick = next_it->first;
        }
        if (result.first != capo_map_.begin()) {
            const auto prev_it = std::prev(result.first);
            CapoParams old_params = prev_it->second;
            // We don't need to apply any changes if the previous capo is inactive
            if (old_params.active) {
                EditCapo::update_notation_for_capo_change(old_params, params, this, start_tick, end_tick);
            }
            // This is an undo action
        } else if (CapoParams::TransposeMode::PLAYBACK_ONLY != params.transpose_mode && params.active) {
            CapoParams old_params;
            old_params.transpose_mode = CapoParams::TransposeMode::PLAYBACK_ONLY;
            old_params.fret_position = params.fret_position;
            old_params.active = params.active;
            old_params.ignored_strings = params.ignored_strings;
            EditCapo::update_notation_for_capo_change(old_params, params, this, start_tick, end_tick);
        }
    } else {
        CapoParams old_params = it->second;
        if (!is_need_update(old_params, params)) {
            return;
        }
        auto result = capo_map_.insert_or_assign(start_tick, params);
        if (const auto next_it = std::next(result.first); next_it != capo_map_.end()) {
            end_tick = next_it->first;
        }
        EditCapo::update_notation_for_capo_change(old_params, params, this, start_tick, end_tick);
    }
}

void Staff::remove_capo_params(const mu::engraving::Fraction &tick) {
    const int start_tick = tick.ticks();

    const auto it = capo_map_.find(start_tick);
    IF_ASSERT_FAILED(it != capo_map_.end()) {
        LOGE() << "Key must exist in capo map!";
        return;
    }

    const auto next_capo_it = std::next(it);
    const int end_tick = next_capo_it == capo_map_.end() ? -1 : next_capo_it->first;

    CapoParams revert_params;
    revert_params.fret_position = it->second.fret_position;

    CapoParams old_params = it->second;
    // If this capo is inactive, it is treated as PLAYBACK_ONLY mode capo
    if (!old_params.active) {
        old_params.transpose_mode = CapoParams::TransposeMode::PLAYBACK_ONLY;
    }

    if (it != capo_map_.begin()) {
        revert_params = std::prev(it)->second;
        // If not active, treat as PLAYBACK_ONLY
        if (!revert_params.active) {
            revert_params.transpose_mode = CapoParams::TransposeMode::PLAYBACK_ONLY;
        }
    }
    EditCapo::update_notation_for_capo_change(old_params, revert_params, this, start_tick, end_tick);

    capo_map_.erase(start_tick);
}

bool Staff::should_merge_matching_rests() const {
    return merge_matching_rests() == AutoOnOff::ON ||
           (merge_matching_rests() == AutoOnOff::AUTO && style().value(Sid::MERGE_MATCHING_RESTS).to_bool());
}

//---------------------------------------------------------
//   channel
//---------------------------------------------------------

int Staff::channel(const Fraction &tick, voice_idx_t voice) const {
    const std::map<int, int> &map = channel_list_[voice];
    if (map.empty()) {
        return 0;
    }

    auto it = muse::find_less_or_equal(map, tick.ticks());
    return it == map.cend() ? 0 : it->second;
}

//---------------------------------------------------------
//   middleLine
//    returns logical line number of middle staff line
//---------------------------------------------------------

int Staff::middle_line(const Fraction &tick) const { return staff_type(tick)->middle_line(); }

//---------------------------------------------------------
//   bottomLine
//    returns logical line number of bottom staff line
//---------------------------------------------------------

int Staff::bottom_line(const Fraction &tick) const { return staff_type(tick)->bottom_line(); }

//---------------------------------------------------------
//   stemless
//---------------------------------------------------------

bool Staff::stemless(const Fraction &tick) const { return staff_type(tick)->stemless(); }

//---------------------------------------------------------
//   setSlashStyle
//---------------------------------------------------------

void Staff::set_slash_style(const Fraction &tick, bool val) { staff_type(tick)->set_stemless(val); }

//---------------------------------------------------------
//   isPrimaryStaff
///   if there are linked staves, the primary staff is
///   the one who is played back and it's not a tab staff
///   because we don't have enough information  to play
///   e.g ornaments. NOTE: it's not necessarily the top staff!
//---------------------------------------------------------

bool Staff::is_primary_staff() const {
    if (!links_) {
        return true;
    }

    std::vector<const Staff *> linked_staves_in_this_score;
    std::vector<const Staff *> linked_non_tab_staves_in_this_score;

    for (const EngravingObject *linked : *links_) {
        const Staff *staff = to_staff(linked);

        if (staff->score() == score()) {
            linked_staves_in_this_score.push_back(staff);

            if (!staff->is_tab_staff(Fraction(0, 1))) {
                linked_non_tab_staves_in_this_score.push_back(staff);
            }
        }
    }

    IF_ASSERT_FAILED(!linked_staves_in_this_score.empty()) { return true; }

    if (!linked_non_tab_staves_in_this_score.empty()) {
        return linked_non_tab_staves_in_this_score.front() == this;
    }

    return linked_staves_in_this_score.front() == this;
}

//---------------------------------------------------------
//   staffType
//---------------------------------------------------------

const StaffType *Staff::staff_type(const Fraction &tick) const { return &staff_type_list_.staff_type(tick); }

const StaffType *Staff::const_staff_type(const Fraction &tick) const { return &staff_type_list_.staff_type(tick); }

StaffType *Staff::staff_type(const Fraction &tick) { return &staff_type_list_.staff_type(tick); }

const StaffType *Staff::staff_type_for_element(const EngravingItem *e) const {
    if (staff_type_list_.unique_staff_type()) {
        // if one staff type spans for the entire staff, optimize by omitting a call to `tick()`
        return &staff_type_list_.staff_type({0, 1});
    }
    // Handle items at the last tick of measures as StaffTypeList::staffType rounds up
    const Measure *measure = e->find_measure();
    const Fraction tick = measure ? measure->tick() : e->tick();
    return &staff_type_list_.staff_type(tick);
}

bool Staff::is_staff_type_start_from(const Fraction &tick) const { return staff_type_list_.is_staff_type_start_from(tick); }

void Staff::move_staff_type(const Fraction &from, const Fraction &to) {
    staff_type_list_.move_staff_type(from, to);
    staff_type_list_changed(from);
}

std::pair<int, int> Staff::staff_type_range(const Fraction &tick) const { return staff_type_list_.staff_type_range(tick); }

//---------------------------------------------------------
//   staffTypeListChanged
//    Signal that the staffTypeList has changed at
//    position tick. Update layout range.
//---------------------------------------------------------

void Staff::staff_type_list_changed(const Fraction &tick) {
    std::pair<int, int> range = staff_type_list_.staff_type_range(tick);

    if (range.first < 0) {
        trigger_layout(Fraction(0, 1));
    } else {
        trigger_layout(Fraction::from_ticks(range.first));
    }

    if (range.second < 0) {
        // When reading a score and there is a Staff Change on the first
        // measure, there are no measures yet and nothing to layout.
        if (score()->last_measure()) {
            trigger_layout(score()->last_measure()->end_tick());
        }
    } else {
        trigger_layout(Fraction::from_ticks(range.second));
    }
}

//---------------------------------------------------------
//   setStaffType
//---------------------------------------------------------

StaffType *Staff::set_staff_type(const Fraction &tick, const StaffType &nst) {
    StaffType *stt = staff_type_list_.set_staff_type(tick, nst);
    stt->set_score(score());
    return stt;
}

//---------------------------------------------------------
//   setStaffType
//---------------------------------------------------------

void Staff::remove_staff_type(const Fraction &tick) {
    double old = spatium(tick);
    const bool removed = staff_type_list_.remove_staff_type(tick);
    if (!removed) {
        return;
    }
    set_local_spatium(old, spatium(tick), tick);
    staff_type_list_changed(tick);
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void Staff::init(const InstrumentTemplate *t, const StaffType *staff_type, staff_idx_t template_staff_idx) {
    // Initialises a staff from an InstrumentTemplate and StaffType. The staff MUST be added to the score already.
    assert(idx() != muse::nidx);
    // set staff-type-independent parameters
    const StaffType *pst = staff_type ? staff_type : t->staff_type_preset;
    if (!pst) {
        pst = StaffType::get_default_preset(t->staff_group);
    }

    const staff_idx_t staff_idx = idx() != muse::nidx ? idx() : score()->nstaves();

    StaffType *stt = set_staff_type(Fraction(0, 1), *pst);
    if (template_staff_idx >= ::mu::engraving::max_staves) {
        stt->set_small(false);
    } else {
        stt->set_small(t->small_staff[template_staff_idx]);
        EditStaffBrackets::set_bracket_type(score(), staff_idx, 0, t->bracket[template_staff_idx]);
        EditStaffBrackets::set_bracket_span(score(), staff_idx, 0, t->bracket_span[template_staff_idx]);
        set_bar_line_span(t->barline_span[template_staff_idx]);
    }
    set_default_clef_type(t->clef_type(template_staff_idx));
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void Staff::init(const Staff *s) {
    id_ = s->id_;

    set_staff_type(Fraction(0, 1), s->staff_type_list_.staff_type(Fraction(0, 1)));
    for (const auto &st_pair : s->staff_type_list_.staff_type_changes()) {
        const StaffType &st = st_pair.second;
        StaffType new_staff_type(st);
        set_staff_type(Fraction::from_ticks(st_pair.first), new_staff_type);
    }

    set_default_clef_type(s->default_clef_type());
    bar_line_from_ = s->bar_line_from_;
    bar_line_to_ = s->bar_line_to_;
    hide_when_empty_ = s->hide_when_empty_;
    cutaway_ = s->cutaway_;
    show_if_entire_system_empty_ = s->show_if_entire_system_empty_;
    hide_system_bar_line_ = s->hide_system_bar_line_;
    merge_matching_rests_ = s->merge_matching_rests_;
    color_ = s->color_;
    user_dist_ = s->user_dist_;
    visibility_voices_ = s->visibility_voices_;
    capo_map_ = s->capo_map_;
}

const ID &Staff::id() const { return id_; }

void Staff::set_id(const ID &id) { id_ = id; }

//---------------------------------------------------------
//   initFromStaffType
//---------------------------------------------------------

void Staff::init_from_staff_type(const StaffType *staff_type) {
    // get staff type if given (if none, get default preset for default staff group)
    if (!staff_type) {
        staff_type = StaffType::get_default_preset(StaffGroup::STANDARD);
    }

    // use selected staff type
    set_staff_type(Fraction(0, 1), *staff_type);
}

//---------------------------------------------------------
//   show
//---------------------------------------------------------

bool Staff::show() const { return part_->show() && visible(); }

//---------------------------------------------------------
//   genKeySig
//---------------------------------------------------------

bool Staff::gen_key_sig() {
    if (const_staff_type(Fraction(0, 1))->group() == StaffGroup::TAB) {
        return false;
    } else {
        return const_staff_type(Fraction(0, 1))->gen_keysig();
    }
}

//---------------------------------------------------------
//   showLedgerLines
//---------------------------------------------------------

bool Staff::show_ledger_lines(const Fraction &tick) const { return staff_type(tick)->show_ledger_lines(); }

//---------------------------------------------------------
//   color
//---------------------------------------------------------

Color Staff::color(const Fraction &tick) const { return staff_type(tick)->color(); }

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void Staff::set_color(const Fraction &tick, const Color &val) { staff_type(tick)->set_color(val); }

//---------------------------------------------------------
//   updateOttava
//---------------------------------------------------------

void Staff::update_ottava() {
    staff_idx_t staff_idx = idx();
    pitch_offset_map_.clear();
    for (auto i : score()->spanner()) {
        const Spanner *s = i.second;
        if (s->is_ottava() && s->staff_idx() == staff_idx && s->play_spanner()) {
            const Ottava *o = to_ottava(s);
            pitch_offset_map_.insert_or_assign(o->tick().ticks(), o->pitch_shift());
            pitch_offset_map_.insert_or_assign(o->tick2().ticks(), 0);
        }
    }
}

int Staff::pitch_offset(const Fraction &tick) const {
    if (pitch_offset_map_.empty()) {
        return 0;
    }

    const auto it = muse::find_less_or_equal(pitch_offset_map_, tick.ticks());
    return it == pitch_offset_map_.cend() ? 0 : it->second;
}

//---------------------------------------------------------
//   insertTime
//---------------------------------------------------------

void Staff::insert_time(const Fraction &tick, const Fraction &len) {
    if (len.is_zero()) {
        return;
    }

    // move all keys and clefs >= tick

    if (len < Fraction(0, 1)) {
        // remove entries between tickpos >= tick and tickpos < (tick+len)
        keys_.erase(keys_.lower_bound(tick.ticks()), keys_.lower_bound((tick - len).ticks()));
        clefs_.erase(clefs_.lower_bound(tick.ticks()), clefs_.lower_bound((tick - len).ticks()));
    }

    KeyList kl2;
    for (auto i = keys_.lower_bound(tick.ticks()); i != keys_.end();) {
        KeySigEvent kse = i->second;
        Fraction t = Fraction::from_ticks(i->first);
        if (t + len < score()->end_tick()) {
            keys_.erase(i++);
            kl2[(t + len).ticks()] = kse;
        } else {
            ++i;
        }
    }
    keys_.insert(kl2.begin(), kl2.end());

    // check if there is a clef at the end of measure
    // before tick
    Clef *clef = 0;
    Measure *m = score()->tick2measure(tick);
    if (m && (m->tick() == tick) && (m->prev_measure())) {
        m = m->prev_measure();
        Segment *s = m->find_segment(SegmentType::CLEF, tick);
        if (s) {
            track_idx_t track = idx() * ::mu::engraving::voices;
            clef = to_clef(s->element(track));
        }
    }

    ClefList cl2;
    for (auto i = clefs_.lower_bound(tick.ticks()); i != clefs_.end();) {
        ClefTypeList ctl = i->second;
        Fraction t = Fraction::from_ticks(i->first);
        if (clef && tick == t) {
            ++i;
            continue;
        }
        clefs_.erase(i++);
        cl2.set_clef((t + len).ticks(), ctl);
    }
    clefs_.insert(cl2.begin(), cl2.end());

    // check if there is a clef at the end of measure
    // before tick: do not remove from clefs list

    if (clef) {
        set_clef(clef);
    }

    update_ottava();
    DUMP_CLEFS("  insertTime");
}

//---------------------------------------------------------
//   staffList
//    return list of linked staves
//---------------------------------------------------------

std::vector<Staff *> Staff::staff_list() const {
    std::vector<Staff *> staff_list;
    if (links_) {
        staff_list.reserve(links_->size());
        for (EngravingObject *e : *links_) {
            staff_list.push_back(to_staff(e));
        }
    } else {
        staff_list.push_back(const_cast<Staff *>(this));
    }
    return staff_list;
}

Staff *Staff::primary_staff() const {
    const LinkedObjects *linked_elements = links();
    if (!linked_elements) {
        return nullptr;
    }

    for (auto linked_element : *linked_elements) {
        Staff *staff = to_staff(linked_element);
        if (staff && staff->is_primary_staff()) {
            return staff;
        }
    }

    return nullptr;
}

//---------------------------------------------------------
//   rstaff
//---------------------------------------------------------

staff_idx_t Staff::rstaff() const { return muse::index_of(part_->staves(), this); }

//---------------------------------------------------------
//   isTop
//---------------------------------------------------------

bool Staff::is_top() const {
    if (part_->staves().empty()) {
        return false;
    }

    return part_->staves().front() == this;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Staff::get_property(Pid id) const {
    switch (id) {
        case Pid::SMALL: return staff_type(Fraction(0, 1))->is_small();
        case Pid::MAG: return staff_type(Fraction(0, 1))->user_mag();
        case Pid::LINE_DISTANCE: return staff_type(Fraction(0, 1))->line_distance();
        case Pid::STAFF_INVISIBLE: return staff_type(Fraction(0, 1))->invisible();
        case Pid::HIDE_WHEN_EMPTY: return hide_when_empty_;
        case Pid::STAFF_COLOR: return PropertyValue::from_value(staff_type(Fraction(0, 1))->color());
        case Pid::PLAYBACK_VOICE1: return playback_voice(0);
        case Pid::PLAYBACK_VOICE2: return playback_voice(1);
        case Pid::PLAYBACK_VOICE3: return playback_voice(2);
        case Pid::PLAYBACK_VOICE4: return playback_voice(3);
        case Pid::STAFF_BARLINE_SPAN: return bar_line_span();
        case Pid::STAFF_BARLINE_SPAN_FROM: return bar_line_from();
        case Pid::STAFF_BARLINE_SPAN_TO: return bar_line_to();
        case Pid::STAFF_USERDIST: return user_dist();
        case Pid::GENERATED: return false;
        case Pid::SHOW_MEASURE_NUMBERS: return show_measure_numbers_;
        case Pid::SHOW_IF_ENTIRE_SYSTEM_EMPTY: return show_if_entire_system_empty_;
        case Pid::VISIBLE: return visible();
        case Pid::STAFF_CUTAWAY: return cutaway();
        case Pid::STAFF_HIDE_SYSTEM_BARLINE: return hide_system_bar_line();
        case Pid::STAFF_MERGE_MATCHING_RESTS: return int(merge_matching_rests());
        case Pid::STAFF_REFLECT_TRANSPOSITION: return reflect_transposition_in_linked_tab();
        default: LOGD("unhandled id <%s>", property_name(id)); return PropertyValue();
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Staff::set_property(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::SMALL: {
            double spatium_value = spatium(Fraction(0, 1));
            staff_type(Fraction(0, 1))->set_small(v.to_bool());
            set_local_spatium(spatium_value, spatium(Fraction(0, 1)), Fraction(0, 1));
            break;
        }
        case Pid::MAG: {
            double spatium_value = spatium(Fraction(0, 1));
            staff_type(Fraction(0, 1))->set_user_mag(v.to_real());
            set_local_spatium(spatium_value, spatium(Fraction(0, 1)), Fraction(0, 1));
            break;
        }
        case Pid::LINE_DISTANCE: {
            staff_type(Fraction(0, 1))->set_line_distance(v.value<Spatium>());
            break;
        }
        case Pid::HIDE_WHEN_EMPTY: set_hide_when_empty(v.value<AutoOnOff>()); break;
        case Pid::STAFF_COLOR: set_color(Fraction(0, 1), v.value<Color>()); break;
        case Pid::PLAYBACK_VOICE1: set_playback_voice(0, v.to_bool()); break;
        case Pid::PLAYBACK_VOICE2: set_playback_voice(1, v.to_bool()); break;
        case Pid::PLAYBACK_VOICE3: set_playback_voice(2, v.to_bool()); break;
        case Pid::PLAYBACK_VOICE4: set_playback_voice(3, v.to_bool()); break;
        case Pid::STAFF_BARLINE_SPAN: {
            set_bar_line_span(v.to_bool());
            // update non-generated barlines
            track_idx_t track = idx() * ::mu::engraving::voices;
            std::vector<EngravingItem *> bl_list;
            for (Measure *m = score()->first_measure(); m; m = m->next_measure()) {
                Segment *s = m->get_segment_r(SegmentType::END_BAR_LINE, m->ticks());
                if (s && s->element(track)) {
                    bl_list.push_back(s->element(track));
                }
                if (Measure *mm = m->mm_rest()) {
                    Segment *ss = mm->get_segment_r(SegmentType::END_BAR_LINE, mm->ticks());
                    if (ss && ss->element(track)) {
                        bl_list.push_back(ss->element(track));
                    }
                }
            }
            for (EngravingItem *e : bl_list) {
                if (e && e->is_bar_line() && !e->generated()) {
                    to_bar_line(e)->set_span_staff(bar_line_span());
                }
            }
        } break;
        case Pid::STAFF_BARLINE_SPAN_FROM: set_bar_line_from(v.to_int()); break;
        case Pid::STAFF_BARLINE_SPAN_TO: set_bar_line_to(v.to_int()); break;
        case Pid::STAFF_USERDIST: set_user_dist(v.value<Spatium>()); break;
        case Pid::SHOW_MEASURE_NUMBERS: show_measure_numbers_ = v.value<AutoOnOff>(); break;
        case Pid::SHOW_IF_ENTIRE_SYSTEM_EMPTY: show_if_entire_system_empty_ = v.to_bool(); break;
        case Pid::VISIBLE:
            set_visible(v.to_bool());
            master_score()->rebuild_midi_mapping();
            break;
        case Pid::STAFF_CUTAWAY: set_cutaway(v.to_bool()); break;
        case Pid::STAFF_HIDE_SYSTEM_BARLINE: set_hide_system_bar_line(v.to_bool()); break;
        case Pid::STAFF_MERGE_MATCHING_RESTS: set_merge_matching_rests(AutoOnOff(v.to_int())); break;
        case Pid::STAFF_REFLECT_TRANSPOSITION: set_reflect_transposition_in_linked_tab(v.to_bool()); break;
        default: LOGD("unhandled id <%s>", property_name(id)); break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Staff::property_default(Pid id) const {
    switch (id) {
        case Pid::SMALL: return false;
        case Pid::MAG: return 1.0;
        case Pid::HIDE_WHEN_EMPTY: return AutoOnOff::AUTO;
        case Pid::STAFF_COLOR: return PropertyValue::from_value(configuration()->default_color());
        case Pid::PLAYBACK_VOICE1:
        case Pid::PLAYBACK_VOICE2:
        case Pid::PLAYBACK_VOICE3:
        case Pid::PLAYBACK_VOICE4: return true;
        case Pid::STAFF_BARLINE_SPAN: return false;
        case Pid::STAFF_BARLINE_SPAN_FROM:
        case Pid::STAFF_BARLINE_SPAN_TO: return 0;
        case Pid::STAFF_USERDIST: return 0.0_sp;
        case Pid::SHOW_MEASURE_NUMBERS: return AutoOnOff::AUTO;
        case Pid::SHOW_IF_ENTIRE_SYSTEM_EMPTY: return false;
        case Pid::VISIBLE: return true;
        case Pid::STAFF_CUTAWAY: return false;
        case Pid::STAFF_HIDE_SYSTEM_BARLINE: return false;
        case Pid::STAFF_MERGE_MATCHING_RESTS: return int(AutoOnOff::AUTO);
        case Pid::STAFF_REFLECT_TRANSPOSITION: return true;
        default: LOGD("unhandled id <%s>", property_name(id)); return PropertyValue();
    }
}

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void Staff::set_local_spatium(double old_val, double new_val, Fraction tick) {
    const int int_end_tick = staff_type_list_.staff_type_range(tick).second;
    const Fraction etick = (int_end_tick == -1) ? score()->last_measure()->end_tick() : Fraction::from_ticks(int_end_tick);

    staff_idx_t staff_idx = idx();
    track_idx_t start_track = staff_idx * ::mu::engraving::voices;
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    for (Segment *s = score()->tick2right_segment(tick); s && s->tick() < etick; s = s->next1()) {
        for (EngravingItem *e : s->annotations()) {
            if (e->track() >= start_track && e->track() < end_track) {
                e->local_spatium_changed(old_val, new_val);
            }
        }
        for (track_idx_t track = start_track; track < end_track; ++track) {
            if (s->element(track)) {
                s->element(track)->local_spatium_changed(old_val, new_val);
            }
        }
    }
    auto spanners = score()->spanner_map().find_contained(tick.ticks(), etick.ticks());
    for (auto interval : spanners) {
        Spanner *spanner = interval.value;
        if (spanner->staff_idx() == staff_idx) {
            for (auto k : spanner->spanner_segments()) {
                k->local_spatium_changed(old_val, new_val);
            }
        }
    }
}

//---------------------------------------------------------
//   isPitchedStaff
//---------------------------------------------------------

bool Staff::is_pitched_staff(const Fraction &tick) const {
    //return staffType(tick)->group() == StaffGroup::STANDARD;
    return staff_type(tick)->group() != StaffGroup::TAB && !origin_part(tick)->instrument(tick)->use_drumset();
}

//---------------------------------------------------------
//   isTabStaff
//---------------------------------------------------------

bool Staff::is_tab_staff(const Fraction &tick) const { return staff_type(tick)->group() == StaffGroup::TAB; }

//---------------------------------------------------------
//   isDrumStaff
//---------------------------------------------------------

bool Staff::is_drum_staff(const Fraction &tick) const {
    //check for instrument instead of staffType (for pitched to unpitched instr. changes)
    return origin_part(tick)->instrument(tick)->use_drumset();
}

//---------------------------------------------------------
//   lines
//---------------------------------------------------------

int Staff::lines(const Fraction &tick) const { return staff_type(tick)->lines(); }

//---------------------------------------------------------
//   setLines
//---------------------------------------------------------

void Staff::set_lines(const Fraction &tick, int val) { staff_type(tick)->set_lines(val); }

//---------------------------------------------------------
//   lineDistance
//    distance between staff lines
//---------------------------------------------------------

double Staff::line_distance(const Fraction &tick) const { return staff_type(tick)->line_distance().val(); }

//---------------------------------------------------------
//   invisible
//---------------------------------------------------------

bool Staff::is_lines_invisible(const Fraction &tick) const { return staff_type(tick)->invisible(); }

//---------------------------------------------------------
//   setInvisible
//---------------------------------------------------------

void Staff::set_is_lines_invisible(const Fraction &tick, bool val) { staff_type(tick)->set_invisible(val); }
} // namespace mu::engraving
