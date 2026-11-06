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
#include "importmidi_tuplet_tonotes.h"

#include "importmidi_tuplet.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/masterscore.h"
#include "engraving/types/fraction.h"
#include "engraving/dom/durationelement.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/chordrest.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiTuplet {
void add_element_to_tuplet(int voice, const ReducedFraction &on_time, const ReducedFraction &len, DurationElement *el,
                           std::multimap<ReducedFraction, TupletData> &tuplets) {
    const auto found_tuplets = find_tuplets_for_time_range(voice, on_time, len, tuplets, true);

#ifndef NDEBUG
    if (found_tuplets.size() > 1) {
        LOGD() << "Measure number (from 1):" << el->measure()->measure_number() + 1 << ", staff index (from 0):" << el->staff()->idx();

        Q_ASSERT_X(false, "MidiTuplet::addElementToTuplet", "More than one tuplet contains specified duration");
    }
#endif

    if (!found_tuplets.empty()) {
        auto &tuplet = const_cast<TupletData &>(found_tuplets.front()->second);
        tuplet.elements.push_back(el); // add chord/rest to the tuplet
    }
}

void create_tuplet_notes(Staff *staff, const std::multimap<ReducedFraction, TupletData> &tuplets) {
    Score *score = staff->score();
    const track_idx_t track = staff->idx() * ::mu::engraving::voices;

    for (const auto &tuplet_event : tuplets) {
        const auto &tuplet_data = tuplet_event.second;
        if (tuplet_data.elements.empty()) {
            continue;
        }

        Measure *measure = score->tick2measure(tuplet_data.on_time.fraction());
        Tuplet *tuplet = new Tuplet(measure);
        const auto &tuplet_ratio = tuplet_limits(tuplet_data.tuplet_number).ratio;
        tuplet->set_ratio(tuplet_ratio.fraction());

        tuplet->set_ticks(tuplet_data.len.fraction());
        const TDuration base_len = tuplet_data.len.fraction() / Fraction(tuplet_ratio.denominator(), 1);
        tuplet->set_base_len(base_len);

        tuplet->set_track(track);
        tuplet->set_tick(tuplet_data.on_time.fraction());
        tuplet->set_voice(tuplet_data.voice);

        for (DurationElement *el : tuplet_data.elements) {
            tuplet->add(el);
            el->set_tuplet(tuplet);
        }
    }
}

#ifndef NDEBUG

static void print_invalid_tuplet_location(int measure_index, staff_idx_t staff_index) {
    LOGD() << "Tuplet is invalid; measure number (from 1):" << measure_index + 1 << ", staff index (from 0):" << staff_index;
}

bool have_tuplets_enough_elements(const Staff *staff) {
    const track_idx_t strack = staff->idx() * ::mu::engraving::voices;

    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        for (Segment *seg = staff->score()->first_segment(SegmentType::ALL); seg; seg = seg->next1()) {
            if (seg->segment_type() == SegmentType::CHORD_REST) {
                const ChordRest *cr = to_chord_rest(seg->element(strack + voice));
                if (!cr) {
                    continue;
                }
                const Tuplet *tuplet = cr->tuplet();
                if (tuplet) {
                    if (tuplet->elements().size() <= 1) {
                        print_invalid_tuplet_location(seg->measure()->measure_number(), staff->idx());
                        return false;
                    }
                    int chord_count = 0;
                    for (const auto &e : tuplet->elements()) {
                        const ChordRest *cr1 = to_chord_rest(e);
                        if (cr1 && cr1->is_chord()) {
                            ++chord_count;
                        }
                    }
                    if (chord_count == 0) {
                        print_invalid_tuplet_location(seg->measure()->measure_number(), staff->idx());
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

#endif
}} // namespace mu::iex::midi::MidiTuplet
