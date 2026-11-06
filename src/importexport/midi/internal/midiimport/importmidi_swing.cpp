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
#include "importmidi_swing.h"

#include "engraving/dom/chordrest.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tuplet.h"
#include "importmidi_fraction.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace Swing {
struct SwingDetector {
    SwingDetector(MidiOperations::Swing st);

    void add(ChordRest *cr);

    bool was_swing_applied() const { return swing_applied; }

private:
    std::vector<ChordRest *> elements;
    ReducedFraction sum_len;
    const ReducedFraction full_len = {1, 4};
    MidiOperations::Swing swing_type;
    bool swing_applied = false;

    void reset();
    void append(ChordRest *cr);
    void check_normal_swing();
    void check_shuffle();
    void apply_swing();
    bool are_all_tuplets() const;
    bool are_all_non_tuplets() const;
};

SwingDetector::SwingDetector(MidiOperations::Swing st) : swing_type(st) {}

void SwingDetector::add(ChordRest *cr) {
    if (elements.empty()) {
        if (ReducedFraction(cr->global_ticks()) >= full_len) {
            return;
        }
        const int tick_in_bar = (cr->tick() - cr->measure()->tick()).ticks();
        if (tick_in_bar % Constants::division == 0) {
            append(cr);
        }
    } else {
        if (sum_len + ReducedFraction(cr->global_ticks()) > full_len) {
            reset();
            return;
        }
        append(cr);
        if (sum_len == full_len) {
            // check for swing patterns
            switch (swing_type) {
                case MidiOperations::Swing::SWING: check_normal_swing(); break;
                case MidiOperations::Swing::SHUFFLE: check_shuffle(); break;
                default: break;
            }
            reset();
        }
    }
}

void SwingDetector::reset() {
    elements.clear();
    sum_len = ReducedFraction(Fraction(0, 1));
}

void SwingDetector::append(ChordRest *cr) {
    if (cr->is_chord() || cr->is_rest()) {
        elements.push_back(cr);
        sum_len += ReducedFraction(cr->global_ticks());
    }
}

void SwingDetector::check_normal_swing() {
    if (elements.size() == 2 && are_all_tuplets() && (elements[0]->is_chord() || elements[1]->is_chord()) &&
        elements[0]->ticks().reduced() == Fraction(1, 4) && elements[1]->ticks().reduced() == Fraction(1, 8)) {
        // swing with two 8th notes
        // or 8th note + 8th rest
        // or 8th rest + 8th note
        apply_swing();
    } else if (elements.size() == 3 && elements[0]->is_chord() && elements[1]->is_rest() && elements[2]->is_chord() &&
               elements[0]->ticks().reduced() == Fraction(1, 8) && elements[1]->ticks().reduced() == Fraction(1, 8) &&
               elements[2]->ticks().reduced() == Fraction(1, 8)) {
        // swing with two 8th notes
        apply_swing();
    }
}

void SwingDetector::check_shuffle() {
    if (elements.size() == 2 && are_all_non_tuplets() && elements[0]->is_chord() && (elements[1]->is_chord() || elements[1]->is_rest()) &&
        elements[0]->ticks().reduced() == Fraction(3, 16) // dotted 8th
        && elements[1]->ticks().reduced() == Fraction(1, 16)) {
        // swing with two 8th notes
        // or 8th note + 8th rest
        apply_swing();
    }
}

void SwingDetector::apply_swing() {
    if (elements.size() != 2 && elements.size() != 3) {
        return;
    }

    Tuplet *tuplet = nullptr;
    for (ChordRest *el : elements) {
        el->set_duration_type(DurationType::V_EIGHTH);
        el->set_ticks(Fraction(1, 8));
        el->set_dots(0);
        if (el->tuplet()) {
            if (!tuplet) {
                tuplet = el->tuplet();
            }
            tuplet->remove(el);
            el->set_tuplet(nullptr);
        }
    }

    const ChordRest *first = elements.front();
    const int start_tick = first->segment()->tick().ticks();
    ChordRest *last = elements.back();
    last->segment()->remove(last);
    Segment *s = last->measure()->get_segment(SegmentType::CHORD_REST, Fraction::from_ticks(start_tick + Constants::division / 2));
    s->add(last);

    if (elements.size() == 3) {
        // remove central rest
        ChordRest *cr = elements[1];
        cr->score()->remove_element(cr);
        delete cr;
    }

    if (tuplet) {
        // delete tuplet
        delete tuplet;
        tuplet = nullptr;
    }
    if (!swing_applied) {
        swing_applied = true;
    }
}

bool SwingDetector::are_all_tuplets() const {
    for (const auto &el : elements) {
        if (!el->tuplet()) {
            return false;
        }
    }
    return true;
}

bool SwingDetector::are_all_non_tuplets() const {
    for (const auto &el : elements) {
        if (el->tuplet()) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------

muse::String swing_caption(MidiOperations::Swing swing_type) {
    muse::String caption;
    switch (swing_type) {
        case MidiOperations::Swing::SWING: caption = u"Swing"; break;
        case MidiOperations::Swing::SHUFFLE: caption = u"Shuffle"; break;
        case MidiOperations::Swing::NONE: break;
    }
    return caption;
}

void detect_swing(Staff *staff, MidiOperations::Swing swing_type) {
    Score *score = staff->score();
    const track_idx_t strack = staff->idx() * ::mu::engraving::voices;
    SwingDetector swing_detector(swing_type);

    for (Segment *seg = score->first_segment(SegmentType::CHORD_REST); seg; seg = seg->next1(SegmentType::CHORD_REST)) {
        for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
            ChordRest *cr = to_chord_rest(seg->element(strack + voice));
            if (!cr) {
                continue;
            }
            swing_detector.add(cr);
        }
    }
    if (swing_detector.was_swing_applied()) {
        // add swing label to the score
        Segment *seg = score->first_segment(SegmentType::CHORD_REST);
        StaffText *st = new StaffText(seg, TextStyleType::STAFF);
        st->set_plain_text(swing_caption(swing_type));
        st->set_track(strack); // voice == 0
        score->add_element(st);
    }
}
}} // namespace mu::iex::midi::Swing
