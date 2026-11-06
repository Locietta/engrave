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

#include <climits>

#include "location.h"

#include "chord.h"
#include "engravingitem.h"
#include "measure.h"
#include "mscore.h"
#include "note.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static constexpr Location abs_defaults = Location::absolute();

//---------------------------------------------------------
//   Location::track
//---------------------------------------------------------

int Location::track() const {
    if ((staff_ == ::mu::engraving::abs_defaults.staff_) || (voice_ == ::mu::engraving::abs_defaults.voice_)) {
        return INT_MIN;
    }
    return static_cast<int>(::mu::engraving::voices) * staff_ + voice_;
}

//---------------------------------------------------------
//   Location::setTrack
//---------------------------------------------------------

void Location::set_track(int track) {
    staff_ = track / ::mu::engraving::voices;
    voice_ = track % ::mu::engraving::voices;
}

//---------------------------------------------------------
//   Location::toAbsolute
//---------------------------------------------------------

void Location::to_absolute(const Location &ref) {
    if (is_absolute()) {
        return;
    }
    staff_ += ref.staff_;
    voice_ += ref.voice_;
    measure_ += ref.measure_;
    frac_ += ref.frac_;
    note_ += ref.note_;
    rel_ = false;
}

//---------------------------------------------------------
//   Location::toRelative
//---------------------------------------------------------

void Location::to_relative(const Location &ref) {
    if (is_relative()) {
        return;
    }
    staff_ -= ref.staff_;
    voice_ -= ref.voice_;
    measure_ -= ref.measure_;
    frac_ -= ref.frac_;
    note_ -= ref.note_;
    rel_ = true;
}

//---------------------------------------------------------
//   Location::fillPositionForElement
//    Fills default fields of Location by values relevant
//    for the given EngravingItem. This function fills only
//    position values, not dealing with parameters specific
//    for Chords and Notes, like grace index.
//---------------------------------------------------------

void Location::fill_position_for_element(const EngravingItem *e, bool absfrac) {
    assert(is_absolute());
    if (!e) {
        LOGW("Location::fillPositionForElement: element is nullptr");
        return;
    }
    if (track() == ::mu::engraving::abs_defaults.track()) {
        set_track(track(e));
    }
    if (frac() == ::mu::engraving::abs_defaults.frac()) {
        set_frac(absfrac ? e->tick() : e->rtick());
    }
    if (measure() == ::mu::engraving::abs_defaults.measure()) {
        set_measure(absfrac ? 0 : measure(e));
    }
}

//---------------------------------------------------------
//   Location::fillForElement
//    Fills default fields of Location by values relevant
//    for the given EngravingItem, including parameters specific
//    for Chords and Notes.
//---------------------------------------------------------

void Location::fill_for_element(const EngravingItem *e, bool absfrac) {
    assert(is_absolute());
    if (!e) {
        LOGW("Location::fillForElement: element is nullptr");
        return;
    }

    fill_position_for_element(e, absfrac);
    set_grace_index(grace_index(e));
    set_note(note(e));
}

//---------------------------------------------------------
//   Location::forElement
//---------------------------------------------------------

Location Location::for_element(const EngravingItem *e, bool absfrac) {
    Location i = Location::absolute();
    i.fill_for_element(e, absfrac);
    return i;
}

//---------------------------------------------------------
//   Location::positionForElement
//---------------------------------------------------------

Location Location::position_for_element(const EngravingItem *e, bool absfrac) {
    Location i = Location::absolute();
    i.fill_position_for_element(e, absfrac);
    return i;
}

//---------------------------------------------------------
//   Location::track
//---------------------------------------------------------

int Location::track(const EngravingItem *e) {
    int track = static_cast<int>(e->track());
    if (track < 0) {
        const MeasureBase *mb = e->find_measure_base();
        if (mb && !mb->is_measure()) {
            // Such elements are written in the first staff,
            // see writeMeasure() in rw/write/measurewrite.cpp
            track = 0;
        }
    }
    return track;
}

//---------------------------------------------------------
//   Location::measure
//---------------------------------------------------------

int Location::measure(const EngravingItem *e) {
    const Measure *m = to_measure(e->find_measure());
    if (m) {
        return m->measure_index();
    }
    LOGW("Location::measure: cannot find element's measure (%s)", e->type_name());
    return 0;
}

//---------------------------------------------------------
//   Location::graceIndex
//---------------------------------------------------------

int Location::grace_index(const EngravingItem *e) {
    if (e->is_chord() || (e->ownership_parent() && e->ownership_parent()->is_chord())) {
        const Chord *ch = e->is_chord() ? to_chord(e) : to_chord(e->ownership_parent());
        if (ch->is_grace()) {
            return static_cast<int>(ch->grace_index());
        }
    }
    return ::mu::engraving::abs_defaults.grace_index();
}

//---------------------------------------------------------
//   Location::note
//---------------------------------------------------------

int Location::note(const EngravingItem *e) {
    if (e->is_note()) {
        const Note *n = to_note(e);
        const std::vector<Note *> &notes = n->chord()->notes();
        if (notes.size() == 1) {
            return 0;
        }
        size_t note_idx;
        for (note_idx = 0; note_idx < notes.size(); ++note_idx) {
            if (n == notes.at(note_idx)) {
                break;
            }
        }
        return static_cast<int>(note_idx);
    }
    return ::mu::engraving::abs_defaults.note();
}

//---------------------------------------------------------
//   Location::getLocationProperty
//---------------------------------------------------------

PropertyValue Location::get_location_property(Pid pid, const EngravingItem *start, const EngravingItem *end) {
    if (!start || !end) {
        return 0;
    }
    switch (pid) {
        case Pid::LOCATION_STAVES: return (track(start) / ::mu::engraving::voices) - (track(end) / ::mu::engraving::voices);
        case Pid::LOCATION_VOICES: return (track(start) % ::mu::engraving::voices) - (track(end) / ::mu::engraving::voices);
        case Pid::LOCATION_MEASURES: return measure(end) - measure(start);
        case Pid::LOCATION_FRACTIONS: return end->rtick() - start->rtick();
        case Pid::LOCATION_GRACE: return grace_index(start) - grace_index(end);
        case Pid::LOCATION_NOTE: return note(start) - note(end);
        default: return PropertyValue();
    }
}

//---------------------------------------------------------
//   Location::operator==
//---------------------------------------------------------

bool Location::operator==(const Location &pi2) const {
    const Location &pi1 = *this;
    return (pi1.frac_ == pi2.frac_) && (pi1.measure_ == pi2.measure_) && (pi1.voice_ == pi2.voice_) && (pi1.staff_ == pi2.staff_) &&
           (pi1.grace_index_ == pi2.grace_index_) && (pi1.note_ == pi2.note_);
}
} // namespace mu::engraving
