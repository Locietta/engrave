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
#include <vector>

#include "../types/types.h"
#include "cleflist.h"
#include "draw/types/color.h"
#include "engravingitem.h"
#include "groups.h"
#include "keylist.h"
#include "mscore.h"
#include "stafftypelist.h"

namespace mu::engraving {
struct BracketItem;
struct Clef;
struct Factory;
struct InstrumentTemplate;
struct KeyList;
struct Note;
struct Part;
struct Score;
struct StaffType;
struct TimeSig;

enum class Key : signed char;

//---------------------------------------------------------
//    Staff
///    Global staff data not directly related to drawing.
//---------------------------------------------------------

struct Staff final : EngravingItem {
private:
    OBJECT_ALLOCATOR(engraving, Staff)

public:
    Staff *clone() const override;

    void init(const InstrumentTemplate *, const StaffType *staff_type, staff_idx_t template_staff_idx);
    void init_from_staff_type(const StaffType *staff_type);
    void init(const Staff *);

    const ID &id() const;
    void set_id(const ID &id);

    bool is_top() const;
    String part_name() const;
    staff_idx_t rstaff() const;
    staff_idx_t idx() const;

    Part *part() const { return part_; }

    void set_part(Part *p) { part_ = p; }

    Part *origin_part(const Fraction &tick) const;

    ClefList &clef_list() { return clefs_; }

    ClefTypeList clef_type(const Fraction &) const;

    ClefTypeList default_clef_type() const { return default_clef_type_; }

    void set_default_clef_type(const ClefTypeList &l) { default_clef_type_ = l; }

    ClefType clef(const Fraction &) const;
    Fraction next_clef_tick(const Fraction &) const;
    Fraction current_clef_tick(const Fraction &) const;

    String staff_name() const;

    void set_clef(Clef *);
    void remove_clef(const Clef *);

    void add_time_sig(TimeSig *);
    void remove_time_sig(TimeSig *);
    void clear_time_sig();
    Fraction time_stretch(const Fraction &) const;
    TimeSig *time_sig(const Fraction &) const;
    TimeSig *next_time_sig(const Fraction &) const;
    Fraction current_time_sig_tick(const Fraction &) const;

    const Groups &group(const Fraction &) const;

    Interval transpose(const Fraction &tick) const;

    KeyList *key_list() { return &keys_; }

    Key key(const Fraction &tick) const { return key_sig_event(tick).key(); }

    Key concert_key(const Fraction &tick) const { return key_sig_event(tick).concert_key(); }

    KeySigEvent key_sig_event(const Fraction &) const;
    Fraction next_key_tick(const Fraction &) const;
    Fraction current_key_tick(const Fraction &) const;
    KeySigEvent prev_key(const Fraction &) const;
    void set_key(const Fraction &, KeySigEvent);
    void remove_key(const Fraction &);

    bool show() const;
    bool stemless(const Fraction &) const;

    bool cutaway() const { return cutaway_; }

    void set_cutaway(bool val) { cutaway_ = val; }

    bool show_if_entire_system_empty() const { return show_if_entire_system_empty_; }

    void set_show_if_entire_system_empty(bool val) { show_if_entire_system_empty_ = val; }

    bool hide_system_bar_line() const { return hide_system_bar_line_; }

    void set_hide_system_bar_line(bool val) { hide_system_bar_line_ = val; }

    AutoOnOff hide_when_empty() const { return hide_when_empty_; }

    void set_hide_when_empty(AutoOnOff v) { hide_when_empty_ = v; }

    AutoOnOff merge_matching_rests() const { return merge_matching_rests_; }

    void set_merge_matching_rests(AutoOnOff val) { merge_matching_rests_ = val; }

    bool should_merge_matching_rests() const;

    bool bar_line_span() const { return bar_line_span_; }

    int bar_line_from() const { return bar_line_from_; }

    int bar_line_to() const { return bar_line_to_; }

    void set_bar_line_span(const bool val) { bar_line_span_ = val; }

    void set_bar_line_from(int val) { bar_line_from_ = val; }

    void set_bar_line_to(int val) { bar_line_to_ = val; }

    double staff_height() const;
    double staff_height(const Fraction &tick) const;

    int channel(const Fraction &, voice_idx_t voice) const;

    void clear_channel_list(voice_idx_t voice) { channel_list_[voice].clear(); }

    void insert_into_channel_list(voice_idx_t voice, const Fraction &tick, int channel_id) {
        channel_list_[voice].insert({tick.ticks(), channel_id});
    }

    SwingParameters swing(const Fraction &) const;

    void clear_swing_map() { swing_map_.clear(); }

    void insert_into_swing_map(const Fraction &tick, SwingParameters sp) { swing_map_.insert({tick.ticks(), sp}); }

    const CapoParams &capo(const Fraction &) const;
    void insert_capo_params(const Fraction &tick, const CapoParams &params, bool ignore_notation_update);
    void remove_capo_params(const Fraction &tick);

    //==== staff type helper function
    const StaffType *staff_type(const Fraction & = Fraction(0, 1)) const;
    const StaffType *const_staff_type(const Fraction &) const;
    const StaffType *staff_type_for_element(const EngravingItem *) const;
    bool is_staff_type_start_from(const Fraction & = Fraction(0, 1)) const;
    void move_staff_type(const Fraction &from, const Fraction &to);
    StaffType *staff_type(const Fraction &);
    StaffType *set_staff_type(const Fraction &, const StaffType &);
    void remove_staff_type(const Fraction &);
    void staff_type_list_changed(const Fraction &);

    std::pair<int, int> staff_type_range(const Fraction &) const;

    bool is_pitched_staff(const Fraction &) const;
    bool is_tab_staff(const Fraction &) const;
    bool is_drum_staff(const Fraction &) const;

    int lines(const Fraction &) const;
    void set_lines(const Fraction &, int lines);
    double line_distance(const Fraction &) const;

    bool is_lines_invisible(const Fraction &) const;
    void set_is_lines_invisible(const Fraction &, bool val);

    void set_slash_style(const Fraction &, bool val);
    int middle_line(const Fraction &) const;
    int bottom_line(const Fraction &) const;

    double staff_mag(const Fraction &) const;
    double staff_mag(const EngravingItem *element) const;
    double spatium(const Fraction &) const;
    double spatium(const EngravingItem *) const;
    //===========

    int pitch_offset(const Fraction &tick) const;
    void update_ottava();

    std::vector<Staff *> staff_list() const;
    Staff *primary_staff() const;
    bool is_primary_staff() const;

    Spatium user_dist() const { return user_dist_; }

    void set_user_dist(Spatium val) { user_dist_ = val; }

    void set_local_spatium(double old_val, double new_val, Fraction tick);
    bool gen_key_sig();
    bool show_ledger_lines(const Fraction &) const;

    using EngravingItem::color;
    using EngravingItem::set_color;
    Color color(const Fraction &) const;
    void set_color(const Fraction &, const Color &val);
    void insert_time(const Fraction &, const Fraction &len);

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    bool playback_voice(int voice) const;
    void set_playback_voice(int voice, bool val);

    const std::array<bool, voices> &visibility_voices() const;
    bool is_voice_visible(voice_idx_t voice) const;
    bool can_disable_voice() const;

    bool reflect_transposition_in_linked_tab() const;
    void set_reflect_transposition_in_linked_tab(bool reflect);

#ifndef NDEBUG
    void dump_clefs(const char *title) const;
    void dump_keys(const char *title) const;
    void dump_time_sigs(const char *) const;
#else
    void dump_clefs(const char *) const {}

    void dump_keys(const char *) const {}

    void dump_time_sigs(const char *) const {}
#endif

    void trigger_layout() const override;
    void trigger_layout(const Fraction &tick);

    Staff *find_linked_in_score(const Score *score) const override;

    track_idx_t get_linked_track_in_staff(const Staff *linked_staff, const track_idx_t strack) const;
    bool track_has_links_in_voice_zero(track_idx_t track);

    void undo_set_show_measure_numbers(bool show);
    bool should_show_measure_numbers() const;

    bool is_last_of_score() const;
    bool is_system_object_staff() const;
    bool has_system_objects_below_bottom_staff() const;

    String individual_staff_name_long(const Fraction &tick) const;
    String individual_staff_name_short(const Fraction &tick) const;

private:

    friend struct Factory;
    Staff(Part *parent);
    Staff(const Staff &staff);

    double staff_mag(const StaffType *) const;

    friend struct Excerpt;
    void set_voice_visible(voice_idx_t voice, bool visible);
    void update_visibility_voices(const Staff *master_staff, const TracksMap &tracks);

    ID id_ = ::mu::engraving::invalid_id;
    Part *part_ = nullptr;

    ClefList clefs_;
    ClefTypeList default_clef_type_;

    KeyList keys_;
    std::map<int, TimeSig *> timesigs_;

    bool bar_line_span_ = false; // true - span barline to next staff
    int bar_line_from_ = 0;      // line of start staff to draw the barline from (0 = staff top line, ...)
    int bar_line_to_ = 0;        // line of end staff to draw the bar line to (0= staff bottom line, ...)

    bool cutaway_ = false;
    bool show_if_entire_system_empty_ = false;         // show this staff if system is empty and hideEmptyStaves is true
    bool hide_system_bar_line_ = false;                // no system barline if not preceded by staff with barline
    AutoOnOff merge_matching_rests_ = AutoOnOff::AUTO; // merge matching rests in multiple voices
    AutoOnOff hide_when_empty_ = AutoOnOff::AUTO;      // hide empty staves

    Color color_;
    Spatium user_dist_{0.0_sp}; ///< user edited extra distance

    StaffTypeList staff_type_list_;

    std::map<int, int> channel_list_[::mu::engraving::voices];
    std::map<int, SwingParameters> swing_map_;
    std::map<int, CapoParams> capo_map_;
    std::map<int, int> pitch_offset_map_;
    bool playback_voice_[::mu::engraving::voices]{true, true, true, true};
    std::array<bool, ::mu::engraving::voices> visibility_voices_{true, true, true, true};

    bool reflect_transposition_in_linked_tab_ = true;

    AutoOnOff show_measure_numbers_ = AutoOnOff::AUTO;
};
} // namespace mu::engraving
