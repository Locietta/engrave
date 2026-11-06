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

#include <algorithm>
#include <cmath>
#include <set>

#include "importmidi_clef.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/sig.h"
#include "engraving/dom/staff.h"
#include "importmidi_fraction.h"
#include "importmidi_meter.h"
#include "importmidi_operations.h"
#include "importmidi_tie.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::midi { namespace MidiClef {
struct AveragePitch {
    AveragePitch() : sum_pitch_(0), count_(0) {}

    AveragePitch(int sum_pitch, int count) : sum_pitch_(sum_pitch), count_(count) {}

    int pitch() const { return static_cast<int>(std::lround(sum_pitch_ * 1.0 / count_)); }

    void add_pitch(int pitch) {
        sum_pitch_ += pitch;
        ++count_;
    }

    AveragePitch &operator+=(const AveragePitch &other) {
        sum_pitch_ += other.sum_pitch_;
        count_ += other.count_;
        return *this;
    }

private:
    int sum_pitch_;
    int count_;
};

struct MinMaxPitch {
    MinMaxPitch() : min_pitch_(std::numeric_limits<int>::max()), max_pitch_(-1) {}

    MinMaxPitch(int min_pitch, int max_pitch) : min_pitch_(min_pitch), max_pitch_(max_pitch) {}

    int min_pitch() const { return min_pitch_; }

    int max_pitch() const { return max_pitch_; }

    int empty() const { return min_pitch_ == std::numeric_limits<int>::max() || max_pitch_ == -1; }

    void add_pitch(int pitch) {
        if (pitch < min_pitch_) {
            min_pitch_ = pitch;
        }
        if (pitch > max_pitch_) {
            max_pitch_ = pitch;
        }
    }

private:
    int min_pitch_;
    int max_pitch_;
};

int clef_mid_pitch() {
    static const int mid_pitch = 60;
    return mid_pitch;
}

ClefType clef_type_from_average_pitch(int average_pitch) { return average_pitch < clef_mid_pitch() ? ClefType::F : ClefType::G; }

//---------------------------------------------------------
//   createClef
//---------------------------------------------------------

static void create_clef(ClefType clef_type, Staff *staff, int tick, bool is_small = false) {
    if (tick == 0) {
        staff->set_default_clef_type(ClefTypeList(clef_type, clef_type));
    } else {
        Measure *m = staff->score()->tick2measure(Fraction::from_ticks(tick));
        Segment *seg = m->get_segment(SegmentType::CLEF, Fraction::from_ticks(tick));
        Clef *clef = Factory::create_clef(seg);
        clef->set_clef_type(clef_type);
        const track_idx_t track = staff->idx() * ::mu::engraving::voices;
        clef->set_track(track);
        clef->set_generated(false);
        clef->mutldata()->set_mag(staff->staff_mag(Fraction::from_ticks(tick)));
        clef->set_small(is_small);
        seg->add(clef);
    }
}

static AveragePitch find_average_seg_pitch(const Segment *seg, track_idx_t strack) {
    AveragePitch average_pitch;
    for (size_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        ChordRest *cr = to_chord_rest(seg->element(strack + voice));
        if (cr && cr->is_chord()) {
            Chord *chord = to_chord(cr);
            const auto &notes = chord->notes();
            for (const Note *note : notes) {
                average_pitch.add_pitch(note->pitch());
            }
        }
    }
    return average_pitch;
}

static MinMaxPitch find_min_max_seg_pitch(const Segment *seg, track_idx_t strack) {
    MinMaxPitch min_max_pitch;
    for (size_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        ChordRest *cr = to_chord_rest(seg->element(strack + voice));
        if (cr && cr->is_chord()) {
            Chord *chord = to_chord(cr);
            const auto &notes = chord->notes();
            for (const Note *note : notes) {
                min_max_pitch.add_pitch(note->pitch());
            }
        }
    }
    return min_max_pitch;
}

#ifndef NDEBUG

static bool does_clef_break_tie(const Staff *staff) {
    const track_idx_t strack = staff->idx() * ::mu::engraving::voices;

    for (size_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        bool current_tie = false;
        for (Segment *seg = staff->score()->first_segment(SegmentType::ALL); seg; seg = seg->next1()) {
            if (seg->segment_type() == SegmentType::CHORD_REST) {
                if (MidiTie::is_tied_back(seg, strack, voice)) {
                    current_tie = false;
                }
                if (MidiTie::is_tied_for(seg, strack, voice)) {
                    current_tie = true;
                }
            } else if (seg->segment_type() == SegmentType::CLEF && seg->element(strack)) {
                if (current_tie) {
                    LOGD() << "Clef breaks tie; measure number (from 1):" << seg->measure()->measure_number() + 1
                           << ", staff index (from 0):" << staff->idx();
                    return true;
                }
            }
        }
    }
    return false;
}

#endif

// clef index: 0 - treble, 1 - bass

int find_pitch_penalty_for_clef(int pitch, int clef_index) {
    const int far_pitch_penalty = 10000;
    const int very_far_pitch_penalty = 20000;
    const int approx_pitch_penalty = 1;
    const int dx = 5;

    const int mid_pitch = clef_mid_pitch();    // all notes equal or upper - better in G clef
    const int high_pitch = mid_pitch + dx;     // almost all notes equal or upper - in G clef
    const int low_pitch = mid_pitch - dx;      // almost all notes lower - in F clef
    const int highest_pitch = high_pitch + dx; // all notes equal or upper - in G clef
    const int lowest_pitch = low_pitch - dx;   // all notes lower - in F clef

    switch (clef_index) {
        case 0:
            if (pitch < lowest_pitch) {
                return very_far_pitch_penalty;
            } else if (pitch < low_pitch) {
                return far_pitch_penalty;
            } else if (pitch < mid_pitch) {
                return approx_pitch_penalty;
            }
            break;
        case 1:
            if (pitch >= highest_pitch) {
                return very_far_pitch_penalty;
            } else if (pitch >= high_pitch) {
                return far_pitch_penalty;
            } else if (pitch >= mid_pitch) {
                return approx_pitch_penalty;
            }
            break;
        default: Q_ASSERT_X(false, "MidiClef::pitchPenalty", "Unknown clef type"); break;
    }
    return 0;
}

static std::pair<ElementType, ReducedFraction> find_chord_rest(const Segment *seg, track_idx_t strack) {
    ElementType el_type = ElementType::INVALID;
    ReducedFraction new_rest_len(0, 1);
    for (size_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
        ChordRest *cr = to_chord_rest(seg->element(strack + voice));
        if (!cr) {
            continue;
        }
        if (cr->is_chord()) {
            el_type = ElementType::CHORD;
            break;
        } else if (cr->is_rest()) {
            el_type = ElementType::REST;
            new_rest_len = std::max(new_rest_len, ReducedFraction(cr->global_ticks()));
        }
    }
    return {el_type, new_rest_len};
}

int find_clef_change_penalty(int pos, int clef_index, const std::vector<std::vector<int>> &treble_bass_path, const Segment *segment,
                             const Staff *staff) {
    if (pos == 0) {
        return 0;
    }

    const int clef_change_penalty = 1000;
    const int prev_clef_change_penalty = 10001;
    const int orphan_chord_penalty = 2;
    const int notes_between_clefs = 5; // should be >= 2

    int j = pos;
    ReducedFraction total_rest_len(0, 1);
    int penalty = 0;
    const track_idx_t strack = staff->idx() * ::mu::engraving::voices;
    const auto bar_fraction = ReducedFraction(staff->score()->sigmap()->timesig(segment->tick()).timesig());
    const ReducedFraction beat_len = Meter::beat_length(bar_fraction);

    // find backward penalty
    for (const Segment *seg_prev = segment->prev1(SegmentType::CHORD_REST);; seg_prev = seg_prev->prev1(SegmentType::CHORD_REST)) {
        if (!seg_prev) {
            penalty += clef_change_penalty;
            break;
        }
        const auto el = find_chord_rest(seg_prev, strack);
        if (el.first == ElementType::CHORD) {
            --j;
            if (j == pos - notes_between_clefs) {
                break;
            }
            if (j == 0 || treble_bass_path[clef_index][j] != clef_index) {
                if (j == pos - 1) {
                    penalty += prev_clef_change_penalty;
                } else {
                    penalty += clef_change_penalty;
                }
                break;
            }
            total_rest_len = {0, 1};
        } else if (el.first == ElementType::REST) {
            total_rest_len += el.second;
            if (total_rest_len >= beat_len) {
                if (j != pos) {
                    penalty += orphan_chord_penalty;
                }
                break;
            }
        }
    }
    // find forward penalty
    int chord_counter = 0;
    for (const Segment *seg = segment;; seg = seg->next1(SegmentType::CHORD_REST)) {
        if (!seg) {
            penalty += clef_change_penalty;
            break;
        }
        const auto el = find_chord_rest(seg, strack);
        if (el.first == ElementType::CHORD) {
            ++chord_counter;
            if (chord_counter == notes_between_clefs) {
                break;
            }
            total_rest_len = {0, 1};
        } else if (el.first == ElementType::REST) {
            total_rest_len += el.second;
            if (total_rest_len >= beat_len) {
                penalty += orphan_chord_penalty;
                break;
            }
        }
    }

    return penalty;
}

ClefType clef_from_index(int index) { return (index == 0) ? ClefType::G : ClefType::F; }

void make_dynamic_programming_step(std::vector<std::vector<int>> &penalties, std::vector<std::vector<int>> &optimal_paths, int pos,
                                   MidiTie::TieStateMachine::State tie_state, const MinMaxPitch &min_max_pitch, const Segment *seg,
                                   const Staff *staff) {
    for (int clef_index = 0; clef_index != 2; ++clef_index) {
        optimal_paths[clef_index].resize(pos + 1);
    }

    for (int cur_clef = 0; cur_clef != 2; ++cur_clef) {
        const int significant_pitch = (cur_clef == 0) ? min_max_pitch.min_pitch() : min_max_pitch.max_pitch();
        const int pitch_penalty = find_pitch_penalty_for_clef(significant_pitch, cur_clef);
        int min_penalty = std::numeric_limits<int>::max();
        int min_index = 0;
        for (int prev_clef = 0; prev_clef != 2; ++prev_clef) {
            int penalty = pitch_penalty;
            if (prev_clef != cur_clef) {
                if (tie_state == MidiTie::TieStateMachine::State::TIED_BACK || tie_state == MidiTie::TieStateMachine::State::TIED_BOTH) {
                    continue; // there is a tie breakage that is incorrect
                }
                penalty += find_clef_change_penalty(pos, prev_clef, optimal_paths, seg, staff);
            }
            penalty += (pos > 0) ? penalties[prev_clef][(pos + 1) % 2] : 0;
            if ((prev_clef != cur_clef && penalty < min_penalty) || (prev_clef == cur_clef && penalty <= min_penalty)) {
                min_penalty = penalty;
                min_index = prev_clef;
            }
        }

        penalties[cur_clef][pos % 2] = min_penalty;
        if (pos > 0) {
            optimal_paths[cur_clef][pos] = min_index;
        }
    }
}

// clefs should be created from the beginning to the end of the score
// (otherwise clefs that are equal the default staff clef at the beginning
//  will be ignored),
// results of dynamic programming are collected from the end to the beginning
// so we need to store temporary results

bool create_clefs(Staff *staff, const std::vector<std::vector<int>> &optimal_paths, int last_clef, const std::vector<Segment *> &segments) {
    std::vector<std::pair<ClefType, int>> clefs_and_ticks;

    int current_clef = last_clef;
    for (size_t i = optimal_paths[0].size() - 1; i; --i) {
        const int prev_clef = optimal_paths[current_clef][i];
        if (prev_clef != current_clef) {
            clefs_and_ticks.push_back({clef_from_index(current_clef), segments[i]->tick().ticks()});
            current_clef = prev_clef;
        }
    }
    if (clefs_and_ticks.empty()) {
        return false;
    }

    create_clef(clef_from_index(current_clef), staff, 0); // main staff clef
    for (size_t i = clefs_and_ticks.size(); i; --i) {     // clef changes
        create_clef(clefs_and_ticks[i - 1].first, staff, clefs_and_ticks[i - 1].second, true);
    }

    return true;
}

static void create_main_clef_from_average_pitch(Staff *staff, track_idx_t strack) {
    AveragePitch all_average_pitch;
    for (Segment *seg = staff->score()->first_segment(SegmentType::CHORD_REST); seg; seg = seg->next1(SegmentType::CHORD_REST)) {
        all_average_pitch += find_average_seg_pitch(seg, strack);
    }
    ClefType main_clef = clef_type_from_average_pitch(all_average_pitch.pitch());
    create_clef(main_clef, staff, 0);
}

bool has_g_fclefs(const InstrumentTemplate *templ) {
    if (!templ) {
        return false;
    }
    const size_t stave_count = templ->staff_count;
    bool has_g = false;
    bool has_f = false;
    for (staff_idx_t i = 0; i != stave_count; ++i) {
        switch (templ->clef_types[i].concert_clef) {
            case ClefType::G: has_g = true; break;
            case ClefType::F: has_f = true; break;
            default: break;
        }
    }
    return has_g && has_f;
}

void create_clefs(Staff *staff, int index_of_operation, bool is_drum_track) {
    const auto &opers = midi_import_operations.data()->track_opers;
    const auto &track_instr_list = opers.ms_instr_list.value(index_of_operation);
    const int ms_instr_index = opers.ms_instr_index.value(index_of_operation);
    const bool has_instrument = !track_instr_list.empty() && track_instr_list[ms_instr_index];

    if (is_drum_track && !has_instrument) {
        create_clef(ClefType::PERC, staff, 0);
        return;
    }

    const track_idx_t strack = staff->idx() * ::mu::engraving::voices;
    bool main_clef_was_set = false;
    const bool can_change_clef = !has_instrument || has_g_fclefs(track_instr_list[ms_instr_index]);

    if (opers.change_clef.value(index_of_operation) && can_change_clef) {
        MidiTie::TieStateMachine tie_tracker;

        // find optimal clef changes via dynamic programming
        std::vector<std::vector<int>> penalties(2); // 0 - treble, 1 - bass
        // remember only 2 last positions to save memory
        for (size_t i = 0; i != penalties.size(); ++i) {
            penalties[i].resize(2); // 2 = current + prev
        }
        std::vector<std::vector<int>> optimal_paths(2); // first col is unused
        std::vector<Segment *> segments;

        int pos = 0;
        for (Segment *seg = staff->score()->first_segment(SegmentType::CHORD_REST); seg; seg = seg->next1(SegmentType::CHORD_REST)) {
            const auto min_max_pitch = find_min_max_seg_pitch(seg, strack);
            if (min_max_pitch.empty()) { // no chords
                continue;
            }
            tie_tracker.add_seg(seg, strack);
            segments.push_back(seg);

            make_dynamic_programming_step(penalties, optimal_paths, pos, tie_tracker.state(), min_max_pitch, seg, staff);
            ++pos;
        }

        if (!optimal_paths[0].empty()) {
            const int last_clef = (penalties[1][(pos - 1) % 2] < penalties[0][(pos - 1) % 2]) ? 1 : 0;
            // get the optimal clef changes found via dynamic programming
            if (create_clefs(staff, optimal_paths, last_clef, segments)) {
                main_clef_was_set = true;
            }
        }
    }
    if (!main_clef_was_set && can_change_clef) {
        create_main_clef_from_average_pitch(staff, strack);
    }
#ifndef NDEBUG
    Q_ASSERT_X(!does_clef_break_tie(staff), "MidiClef::createClefs", "Clef breaks the tie");
#endif
}
}} // namespace mu::iex::midi::MidiClef
