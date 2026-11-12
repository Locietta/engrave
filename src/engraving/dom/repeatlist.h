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

#ifndef MU_ENGRAVING_REPEATLIST_H
#define MU_ENGRAVING_REPEATLIST_H

#include <set>
#include <vector>

#include "types/string.h"

namespace mu::engraving {
struct Score;
struct Measure;
struct Volta;
struct Jump;
struct RepeatListElement;
using RepeatListElementList = std::vector<RepeatListElement *>;

//---------------------------------------------------------
//   RepeatSegment
//---------------------------------------------------------

struct RepeatSegment {
public:
    int tick; // start tick
    int utick;
    double pause;
    int playback_count;

    RepeatSegment(int playback_count);

    void add_measure(Measure const *const);
    void add_measures(Measure const *const);
    bool contains_measure(Measure const *const) const;
    bool ends_with_measure(Measure const *const) const;
    bool starts_with_measure(Measure const *const) const;
    bool is_empty() const;
    int len() const;
    int end_tick() const;
    void pop_measure();

    Measure const *first_measure() const { return measure_list_.empty() ? nullptr : measure_list_.front(); }

    Measure const *last_measure() const { return measure_list_.empty() ? nullptr : measure_list_.back(); }

    const std::vector<const Measure *> &measure_list() const;

    friend bool operator==(const RepeatSegment &lhs, const RepeatSegment &rhs);

    inline friend bool operator!=(const RepeatSegment &lhs, const RepeatSegment &rhs) { return !(lhs == rhs); }

    friend struct RepeatList;
private:
    std::vector<const Measure *> measure_list_;
};

struct RepeatSegmentInfo {
    int tick = 0;
    int end_tick = 0;
    int utick = 0;
};

//---------------------------------------------------------
//   RepeatList
//---------------------------------------------------------

struct RepeatList : std::vector<RepeatSegment *> {

public:
    RepeatList(Score *s);
    RepeatList(const RepeatList &) = delete;
    RepeatList &operator=(const RepeatList &) = delete;
    ~RepeatList();

    // Returns whether the published repeat structure changed.
    bool update(bool expand);

    void set_score_changed() { score_changed_ = true; }

    const Score *score() const { return score_; }

    int utick2tick(int tick) const;
    int tick2utick(int tick) const;
    void update_uticks();
    int ticks() const;

    std::vector<RepeatSegment *>::const_iterator find_repeat_segment_from_u_tick(int utick) const;
    std::vector<RepeatSegmentInfo> segment_info_list() const;

private:
    void collect_repeat_list_elements();
    std::pair<std::vector<RepeatListElementList>::const_iterator, RepeatListElementList::const_iterator>
    find_marker(muse::String label, std::vector<RepeatListElementList>::const_iterator reference_section_it,
                RepeatListElementList::const_iterator reference_repeat_list_element_it) const;

    void perform_jump(std::vector<RepeatListElementList>::const_iterator section_it,
                      RepeatListElementList::const_iterator repeat_list_element_target_it, bool with_repeats, int *const playback_count,
                      Volta const **const active_volta, RepeatListElement const **const start_repeat_reference) const;
    void unwind();
    void flatten();

    Score *score_ = nullptr;

    bool expanded_ = false;
    bool score_changed_ = true;

    std::set<std::pair<Jump const *const, int>> jumps_taken_; // take the jumps only once, so track them during unwind
    std::vector<RepeatListElementList> rl_elements_;          // all elements of the score that influence the RepeatList
};
} // namespace mu::engraving
#endif
