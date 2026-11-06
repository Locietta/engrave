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

#include "importmidi_tie.h"

#include "engraving/dom/engravingitem.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/note.h"

#ifndef NDEBUG
#include "engraving/dom/staff.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#endif

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiTie {
static bool is_tied(const Segment *seg, track_idx_t strack, voice_idx_t voice, mu::engraving::Tie *(Note::*tie_func)() const) {
    ChordRest *cr = to_chord_rest(seg->element(strack + voice));
    if (cr && cr->is_chord()) {
        Chord *chord = to_chord(cr);
        const auto &notes = chord->notes();
        for (const Note *note : notes) {
            if ((note->*tie_func)()) {
                return true;
            }
        }
    }
    return false;
}

bool is_tied_for(const Segment *seg, track_idx_t strack, voice_idx_t voice) { return is_tied(seg, strack, voice, &Note::tie_for); }

bool is_tied_back(const Segment *seg, track_idx_t strack, voice_idx_t voice) { return is_tied(seg, strack, voice, &Note::tie_back); }

void TieStateMachine::add_seg(const Segment *seg, track_idx_t strack) {
    bool is_chord = false;
    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        ChordRest *cr = to_chord_rest(seg->element(strack + voice));
        if (!cr || !cr->is_chord()) {
            continue;
        }
        if (!is_chord) {
            is_chord = true;
        }

        bool tied_for = is_tied_for(seg, strack, voice);
        bool tied_back = is_tied_back(seg, strack, voice);

        if (tied_for && !tied_back) {
            tied_voices.insert(voice);
        } else if (!tied_for && tied_back) {
            tied_voices.erase(voice);
        }
    }
    if (!is_chord) {
        return;
    }

    if (tied_voices.empty() && (state_ == State::TIED_FOR || state_ == State::TIED_BOTH)) {
        state_ = State::TIED_BACK;
    } else if (tied_voices.empty() && state_ == State::TIED_BACK) {
        state_ = State::UNTIED;
    } else if (!tied_voices.empty() && (state_ == State::TIED_BACK || state_ == State::UNTIED)) {
        state_ = State::TIED_FOR;
    } else if (!tied_voices.empty() && state_ == State::TIED_FOR) {
        state_ = State::TIED_BOTH;
    }
}

#ifndef NDEBUG

static void print_inconsistent_tie_location(int measure_index, staff_idx_t staff_index) {
    LOGD() << "Ties are inconsistent; measure number (from 1):" << measure_index + 1 << ", staff index (from 0):" << staff_index;
}

bool are_ties_consistent(const Staff *staff) {
    const track_idx_t strack = staff->idx() * ::mu::engraving::voices;

    for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        bool is_tie = false;
        for (Segment *seg = staff->score()->first_segment(SegmentType::ALL); seg; seg = seg->next1()) {
            if (seg->segment_type() == SegmentType::CHORD_REST) {
                ChordRest *cr = to_chord_rest(seg->element(strack + voice));

                if (cr && cr->is_rest() && is_tie) {
                    print_inconsistent_tie_location(seg->measure()->measure_number(), staff->idx());
                    return false;
                }
                if (is_tied_back(seg, strack, voice)) {
                    if (!is_tie) {
                        print_inconsistent_tie_location(seg->measure()->measure_number(), staff->idx());
                        return false;
                    }
                    is_tie = false;
                }
                if (is_tied_for(seg, strack, voice)) {
                    if (is_tie) {
                        print_inconsistent_tie_location(seg->measure()->measure_number(), staff->idx());
                        return false;
                    }
                    is_tie = true;
                }
            }
        }
        if (is_tie) {
            return false;
        }
    }
    return true;
}

#endif
}} // namespace mu::iex::midi::MidiTie
