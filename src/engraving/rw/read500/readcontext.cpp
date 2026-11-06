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

#include "readcontext.h"

#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/score.h"
#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read500;

ReadContext::ReadContext(Score *score, bool paste_mode) : score_(score), paste_mode_value(paste_mode) {}

ReadContext::~ReadContext() {
    IF_ASSERT_FAILED(incomplete_note_anchored_spanners_.empty()) {
        for (auto &[spanner, start_end_ei_ds] : incomplete_note_anchored_spanners_) {
            delete spanner;
        }

        incomplete_note_anchored_spanners_.clear();
    }
}

Score *ReadContext::score() const { return score_; }

void ReadContext::set_score(Score *score) { score_ = score; }

const MStyle &ReadContext::style() const { return score()->style(); }

std::shared_ptr<IEngravingFontsProvider> ReadContext::engraving_fonts() const { return score()->engraving_fonts(); }

String ReadContext::mscore_version() const { return score_->mscore_version(); }

int ReadContext::msc_version() const { return score_->msc_version(); }

int ReadContext::file_division() const { return score_->file_division(); }

int ReadContext::file_division(int t) const { return score_->file_division(t); }

double ReadContext::spatium() const { return score_->style().spatium(); }

void ReadContext::set_spatium(double v) { score_->style().set(Sid::SPATIUM, v); }

DummyParent *ReadContext::dummy() const { return score_->dummy(); }

Staff *ReadContext::staff(staff_idx_t n) { return score_->staff(n); }

bool ReadContext::is_same_score(const EngravingObject *obj) const { return obj->score() == score_; }

Fraction ReadContext::rtick() const { return cur_measure ? tick_value - cur_measure->tick() : tick_value; }

void ReadContext::set_tick(const Fraction &f) {
    tick_value = f.reduced();
    int_tick = tick_value.ticks();
}

void ReadContext::inc_tick(const Fraction &f) {
    tick_value += f;
    tick_value.reduce();
    int_tick += f.ticks();
}

Location ReadContext::location(bool force_abs_frac) const {
    Location l = Location::absolute();
    fill_location(l, force_abs_frac);
    return l;
}

//---------------------------------------------------------
//   fillLocation
//    fills location fields which have default values with
//    values relevant for the current reader's position.
//    When in paste mode (or forceAbsFrac is true) absolute
//    fraction values are used and measure number is set to
//    zero.
//---------------------------------------------------------

void ReadContext::fill_location(Location &l, bool force_abs_frac) const {
    constexpr Location defaults = Location::absolute();
    const bool abs_frac = (paste_mode() || force_abs_frac);
    if (l.track() == defaults.track()) {
        l.set_track(static_cast<int>(track()));
    }
    if (l.frac() == defaults.frac()) {
        l.set_frac(abs_frac ? tick() : rtick());
    }
    if (l.measure() == defaults.measure()) {
        l.set_measure(abs_frac ? 0 : current_measure_index());
    }
}

//---------------------------------------------------------
//   setLocation
//    sets a new reading location, taking into account its
//    type (absolute or relative).
//---------------------------------------------------------

void ReadContext::set_location(const Location &l) {
    if (l.is_relative()) {
        Location new_loc = l;
        new_loc.to_absolute(location());
        int int_ticks = l.frac().ticks();
        if (tick_value == Fraction::from_ticks(int_tick + int_ticks)) {
            int_tick += int_ticks;
            set_track(new_loc.track() - track_offset_value);
            return;
        }
        set_location(new_loc); // recursion
        return;
    }
    set_track(l.track() - track_offset_value);
    set_tick(l.frac() - tick_offset_value);
    if (!paste_mode()) {
        assert(l.measure() == current_measure_index());
        inc_tick(current_measure()->tick());
    }
}

void ReadContext::add_part_audio_setting_compat(PartAudioSettingsCompat part_audio_setting) {
    if (settings_compat.audio_settings.count(part_audio_setting.instrument_id.part_id) == 0) {
        settings_compat.audio_settings.insert({part_audio_setting.instrument_id.part_id, part_audio_setting});
    }
}

void ReadContext::add_mm_rest_end_measure_eid(Measure *mmrest, EID last_measure_eid) {
    DO_ASSERT(last_measure_eid.is_valid());
    mm_rest_end_measures_.emplace(mmrest, last_measure_eid);
}

void ReadContext::set_mm_rest_end_measures() {
    EIDRegister *eid_register = score()->master_score()->eid_register();
    for (auto &[mmrest, last_measure_eid] : mm_rest_end_measures_) {
        EngravingObject *linked_element = eid_register->item_from_eid(last_measure_eid);
        IF_ASSERT_FAILED(linked_element && linked_element->is_measure()) {
            LOGE() << "No valid end measure found for MMRest at " << mmrest->tick().to_string();
            continue;
        }

        Measure *last_measure = to_measure(linked_element);
        MeasureBase *next_mb = last_measure->next();
        mmrest->set_next(next_mb);
    }
}

void ReadContext::register_pasted_eid(const EID &clipboard_eid, const EID &file_eid) { pasted_ei_ds_.emplace(clipboard_eid, file_eid); }

EID ReadContext::resolve_pasted_eid(const EID &clipboard_eid) const {
    if (!paste_mode_value) {
        return clipboard_eid;
    }

    auto it = pasted_ei_ds_.find(clipboard_eid);
    IF_ASSERT_FAILED(it != pasted_ei_ds_.end()) { return clipboard_eid; }

    return it->second;
}

void ReadContext::add_note_anchored_spanner_start_el(Spanner *spanner, EID start_element_eid) {
    auto it = incomplete_note_anchored_spanners_.find(spanner);
    if (it != incomplete_note_anchored_spanners_.end()) {
        it->second.first = start_element_eid;
    } else {
        incomplete_note_anchored_spanners_.emplace(spanner, std::make_pair(start_element_eid, EID::invalid()));
    }
}

void ReadContext::add_note_anchored_spanner_end_el(Spanner *spanner, EID end_element_eid) {
    auto it = incomplete_note_anchored_spanners_.find(spanner);
    if (it != incomplete_note_anchored_spanners_.end()) {
        it->second.second = end_element_eid;
    } else {
        incomplete_note_anchored_spanners_.emplace(spanner, std::make_pair(EID::invalid(), end_element_eid));
    }
}

void ReadContext::connect_note_anchored_spanners() {
    EIDRegister *eid_register = score()->master_score()->eid_register();
    for (auto &[spanner, start_end_ei_ds] : incomplete_note_anchored_spanners_) {
        EID start_element_eid = start_end_ei_ds.first;
        EID end_element_eid = start_end_ei_ds.second;
        EngravingItem *start_element = to_engraving_item(eid_register->item_from_eid(start_element_eid));
        EngravingItem *end_element = to_engraving_item(eid_register->item_from_eid(end_element_eid));
        IF_ASSERT_FAILED(start_element && start_element->is_note()) {
            LOGD() << "Could not find note element for " << spanner->type_name();
            delete spanner;
            continue;
        }
        IF_ASSERT_FAILED(end_element && end_element->is_note()) {
            LOGD() << "Could not find end note for " << spanner->type_name();
            delete spanner;
            continue;
        }

        Note *start_note = to_note(start_element);
        Note *end_note = to_note(end_element);
        spanner->set_note_span(start_note, end_note);

        if (spanner->is_tie()) {
            Tie *tie = to_tie(spanner);
            start_note->set_tie_for(tie);
            end_note->set_tie_back(tie);
        } else {
            score()->add_element(spanner);
        }
    }
    incomplete_note_anchored_spanners_.clear();
}
