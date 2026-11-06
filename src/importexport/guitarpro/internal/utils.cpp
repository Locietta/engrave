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
#include "utils.h"

#include "realfn.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/note.h"
#include "engraving/dom/parenthesis.h"
#include "engraving/dom/playcounttext.h"
#include "engraving/dom/score.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/style/styledef.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro::utils {
int harmonic_overtone(Note *note, float harmonic_value, int harmonic_type) {
    int result{0};

    if (muse::real_is_equal(harmonic_value, 12.0f)) {
        result = 12;
    } else if (muse::real_is_equal(harmonic_value, 7.0f) || muse::real_is_equal(harmonic_value, 19.0f)) {
        result = 19;
    } else if (muse::real_is_equal(harmonic_value, 5.0f) || muse::real_is_equal(harmonic_value, 24.0f)) {
        result = 24;
    } else if (muse::real_is_equal(harmonic_value, 3.9f) || muse::real_is_equal(harmonic_value, 4.0f) ||
               muse::real_is_equal(harmonic_value, 9.0f) || muse::real_is_equal(harmonic_value, 16.0f)) {
        result = 28;
    } else if (muse::real_is_equal(harmonic_value, 3.2f)) {
        result = 31;
    } else if (muse::real_is_equal(harmonic_value, 2.7f) || muse::real_is_equal(harmonic_value, 5.8f) ||
               muse::real_is_equal(harmonic_value, 9.6f) || muse::real_is_equal(harmonic_value, 14.7f) ||
               muse::real_is_equal(harmonic_value, 21.7f)) {
        result = 34;
    } else if (muse::real_is_equal(harmonic_value, 2.3f) || muse::real_is_equal(harmonic_value, 2.4f) ||
               muse::real_is_equal(harmonic_value, 8.2f) || muse::real_is_equal(harmonic_value, 17.0f)) {
        result = 36;
    } else if (muse::real_is_equal(harmonic_value, 2.0f)) {
        result = 38;
    } else if (muse::real_is_equal(harmonic_value, 1.8f)) {
        result = 40;
    }

    return harmonic_type == 1 ? result : (result + note->fret());
}

std::vector<int> standard_tuning_for(int midi_program, int strings_count) {
    static const std::map<std::pair<int, /* program */ int /* strings count */>, std::vector<int>> tunings{
        // Guitars 6 strings
        {{25, 6}, {40, 45, 50, 55, 59, 64}},
        {{26, 6}, {40, 45, 50, 55, 59, 64}},
        {{27, 6}, {40, 45, 50, 55, 59, 64}},
        {{28, 6}, {40, 45, 50, 55, 59, 64}},
        {{29, 6}, {40, 45, 50, 55, 59, 64}},
        {{30, 6}, {40, 45, 50, 55, 59, 64}},
        {{31, 6}, {40, 45, 50, 55, 59, 64}},
        {{32, 6}, {40, 45, 50, 55, 59, 64}},
        // Guitars 7 strings
        {{25, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{26, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{27, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{28, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{29, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{30, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{31, 7}, {35, 40, 45, 50, 55, 59, 64}},
        {{32, 7}, {35, 40, 45, 50, 55, 59, 64}},
        // Basses 4 strings
        {{33, 4}, {28, 33, 38, 43}},
        {{34, 4}, {28, 33, 38, 43}},
        {{35, 4}, {28, 33, 38, 43}},
        {{36, 4}, {28, 33, 38, 43}},
        {{37, 4}, {28, 33, 38, 43}},
        {{38, 4}, {28, 33, 38, 43}},
        // Basses 5 strings
        {{33, 5}, {23, 28, 33, 38, 43}},
        {{34, 5}, {23, 28, 33, 38, 43}},
        {{35, 5}, {23, 28, 33, 38, 43}},
        {{36, 5}, {23, 28, 33, 38, 43}},
        {{37, 5}, {23, 28, 33, 38, 43}},
        {{38, 5}, {23, 28, 33, 38, 43}},
    };
    std::pair<int, int> key{midi_program, strings_count};

    if (tunings.find(key) == tunings.end()) {
        return tunings.at({25, 6});
    }
    return tunings.at(key);
}

bool is_standard_tuning(int midi_program, const std::vector<int> &tuning) {
    const auto &standard_tuning = standard_tuning_for(midi_program, (int) tuning.size());

    for (size_t i = 0; i < standard_tuning.size(); ++i) {
        if (tuning[i] != standard_tuning[i]) {
            return false;
        }
    }

    return true;
}

Chord *get_located_chord(mu::engraving::Score *score, Fraction tick_fr, track_idx_t track) {
    const Measure *measure = score->tick2measure(tick_fr);
    if (!measure) {
        LOGE() << "bend import error: no valid measure for track " << track << ", tick " << tick_fr.ticks();
        return nullptr;
    }

    Chord *chord = measure->find_chord(tick_fr, track);
    if (!chord) {
        LOGE() << "bend import error: no valid chord for track " << track << ", tick " << tick_fr.ticks();
        return nullptr;
    }

    return chord;
}

static void add_parentheses_to_notes(Chord *ch, const std::vector<Note *> &notes) {
    auto create_paren = [ch](DirectionH dir) {
        Parenthesis *p = Factory::create_parenthesis(ch);
        p->set_track(ch->track());
        p->set_direction(dir);
        return p;
    };

    NoteParenthesisInfo *paren_info = new NoteParenthesisInfo(create_paren(DirectionH::LEFT), create_paren(DirectionH::RIGHT), notes);
    ch->add_note_parenthesis_info(paren_info);

    for (Note *n : notes) {
        n->set_property(Pid::HAS_PARENTHESES, PropertyValue::from_value(ParenthesesMode::BOTH));
        n->set_hide_generated_parens(true);
    }
}

void create_ghost_note_paren_groups(Chord *ch) {
    if (!ch->note_parentheses().empty()) {
        return;
    }

    const std::vector<Note *> &notes = ch->notes();
    const size_t count = notes.size();
    std::vector<Note *> current_group;

    for (size_t i = 0; i <= count; ++i) {
        if (i < count && notes[i]->ghost()) {
            current_group.push_back(notes[i]);
        } else if (!current_group.empty()) {
            add_parentheses_to_notes(ch, current_group);
            current_group.clear();
        }
    }
}

void add_play_count_texts(Score *score) {
    const bool show_text = score->style().style_b(Sid::REPEAT_PLAY_COUNT_SHOW);
    const bool single_repeats = score->style().style_b(Sid::REPEAT_PLAY_COUNT_SHOW_SINGLE_REPEATS);
    for (Measure *m = score->first_measure(); m; m = m->next_measure()) {
        if (!m->repeat_end()) {
            continue;
        }
        const int play_count = m->repeat_count();
        if (!show_text || (play_count == 2 && !single_repeats)) {
            continue;
        }
        Segment *end_bar_seg = m->last(SegmentType::BAR_LINE_TYPES);
        if (!end_bar_seg || end_bar_seg->find_annotation(ElementType::PLAY_COUNT_TEXT, 0, 0)) {
            continue;
        }
        PlayCountText *pct = Factory::create_play_count_text(end_bar_seg);
        pct->set_track(0);
        score->add_element(pct);
    }
}
} // namespace mu::iex::guitarpro::utils
