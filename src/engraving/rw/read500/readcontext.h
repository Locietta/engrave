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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <vector>

#include "../infrastructure/eid.h"
#include "../types/types.h"
#include "dom/interval.h"
#include "dom/location.h"
#include "iengravingfontsprovider.h"

namespace mu::engraving {
struct Beam;
struct DummyParent;
struct EngravingObject;
struct LinkedObjects;
struct Measure;
struct MStyle;
struct Score;
struct Spanner;
struct Staff;
struct Tuplet;
} // namespace mu::engraving

namespace mu::engraving::read500 {
struct TextStyleMap {
    String name;
    TextStyleType ss;
};

struct ReadContext {

    ReadContext(Score *score, bool input_paste_mode = false);
    ~ReadContext();

    Score *score() const;
    void set_score(Score *score);

    const MStyle &style() const;
    std::shared_ptr<IEngravingFontsProvider> engraving_fonts() const;

    bool paste_mode() const { return paste_mode_value; }

    String mscore_version() const;
    int msc_version() const;

    int file_division() const;
    int file_division(int t) const;

    double spatium() const;
    void set_spatium(double v);

    void set_properties_to_skip(const PropertyIdSet &properties_to_skip) { properties_to_skip_ = properties_to_skip; }

    bool should_skip_property(Pid pid) const { return muse::contains(properties_to_skip_, pid); }

    DummyParent *dummy() const;

    Staff *staff(staff_idx_t n);

    bool is_same_score(const EngravingObject *obj) const;

    Fraction tick() const { return tick_value + tick_offset_value; }

    Fraction rtick() const;

    Fraction tick_offset() const { return tick_offset_value; }

    void set_tick(const Fraction &f);
    void inc_tick(const Fraction &f);

    void set_tick_offset(const Fraction &val) { tick_offset_value = val; }

    track_idx_t track() const { return track_value + track_offset_value; }

    void set_track_offset(int val) { track_offset_value = val; }

    int track_offset() const { return track_offset_value; }

    void set_track(track_idx_t val) { track_value = val; }

    void set_current_measure(Measure *m) { cur_measure = m; }

    Measure *current_measure() const { return cur_measure; }

    void set_last_measure(Measure *m) { last_measure_value = m; }

    Measure *last_measure() const { return last_measure_value; }

    void set_current_measure_index(int idx) { cur_measure_idx = idx; }

    int current_measure_index() const { return cur_measure_idx; }

    Interval transpose() const { return transpose_value; }

    void set_transpose_chromatic(int8_t v) { transpose_value.chromatic = v; }

    void set_transpose_diatonic(int8_t v) { transpose_value.diatonic = v; }

    TracksMap &tracks() { return tracks_value; }

    void add_part_audio_setting_compat(PartAudioSettingsCompat part_audio_setting);

    const SettingsCompat &setting_compat() { return settings_compat; }

    Fraction time_sig_for_next_measure() { return time_sig_for_next_measure_; }

    void set_time_sig_for_next_measure(Fraction time_sig) { time_sig_for_next_measure_ = time_sig; }

    Location location(bool force_abs_frac = false) const;
    void fill_location(Location &, bool force_abs_frac = false) const;
    void set_location(const Location &); // sets a new reading point, taking into account its type (absolute or relative).

    void add_mm_rest_end_measure_eid(Measure *mmrest, EID last_measure_eid);
    void set_mm_rest_end_measures();

    void register_pasted_eid(const EID &clipboard_eid, const EID &file_eid);
    EID resolve_pasted_eid(const EID &clipboard_eid) const;

    void add_note_anchored_spanner_start_el(Spanner *spanner, EID start_element_eid);
    void add_note_anchored_spanner_end_el(Spanner *spanner, EID end_element_eid);
    void connect_note_anchored_spanners();

private:
    Score *score_ = nullptr;

    bool paste_mode_value = false; // modifies read behaviour on paste operation

    Fraction tick_value{Fraction(0, 1)};
    Fraction tick_offset_value{Fraction(0, 1)};
    int int_tick = 0;

    int track_offset_value = 0;
    track_idx_t track_value = 0;

    Measure *cur_measure = nullptr;
    Measure *last_measure_value = nullptr;
    int cur_measure_idx = 0;

    Interval transpose_value;
    TracksMap tracks_value;

    SettingsCompat settings_compat;

    PropertyIdSet properties_to_skip_;

    Fraction time_sig_for_next_measure_ = Fraction(0, 1);

    std::unordered_map<Measure *, EID> mm_rest_end_measures_;

    // On pasting we must create new EIDs for the elements, as the serialized EIDs could be used elsewhere in the score
    // This map preserves the link between the old EID used to reference items within the section of the score which was copied and the newly created EID
    std::unordered_map<EID, EID> pasted_ei_ds_;

    std::map<Spanner *, std::pair<EID, EID>> incomplete_note_anchored_spanners_;
};
} // namespace mu::engraving::read500
