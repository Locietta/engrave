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
#include "importmidi_tuplet_filter.h"

#include <algorithm>
#include <set>

#include "engraving/dom/mscore.h"
#include "importmidi_chord.h"
#include "importmidi_inner.h"
#include "importmidi_quant.h"
#include "importmidi_tuplet.h"
#include "importmidi_tuplet_voice.h"

namespace mu::iex::midi { namespace MidiTuplet {
bool is_more_tuplet_voices_allowed(int voices_in_use, int available_voices) {
    return !(voices_in_use >= available_voices || voices_in_use >= tuplet_voice_limit());
}

struct TupletErrorResult {
    TupletErrorResult(double t = 0.0, double rel_places = 0.0, const ReducedFraction &r = ReducedFraction(0, 1), size_t vc = 0,
                      size_t tc = 0)
        : tuplet_average_error(t), relative_used_chord_places(rel_places), sum_length_of_rests(r), voice_count(vc), tuplet_count(tc) {}

    bool is_initialized() const { return tuplet_count != 0; }

    bool operator<(const TupletErrorResult &er) const {
        double value = div(tuplet_average_error, er.tuplet_average_error) - div(relative_used_chord_places, er.relative_used_chord_places) +
                       div(sum_length_of_rests.numerator() * 1.0 / sum_length_of_rests.denominator(),
                           er.sum_length_of_rests.numerator() * 1.0 / er.sum_length_of_rests.denominator());
        if (value == 0) {
            value = div(voice_count, er.voice_count) + div(tuplet_count, er.tuplet_count);
        }
        return value < 0;
    }

private:
    static double div(double val1, double val2) {
        if (val1 == val2) {
            return 0;
        }
        return (val1 - val2) / std::max(val1, val2);
    }

    double tuplet_average_error;
    double relative_used_chord_places;
    ReducedFraction sum_length_of_rests;
    size_t voice_count;
    size_t tuplet_count;
};

bool have_common_chords(int i, int j, const std::vector<TupletInfo> &tuplets) {
    if (tuplets.empty()) {
        return false;
    }
    std::set<std::pair<const ReducedFraction, MidiChord> *> used_chords;
    for (const auto &chord : tuplets[i].chords) {
        used_chords.insert(&*chord.second);
    }
    for (const auto &chord : tuplets[j].chords) {
        if (used_chords.find(&*chord.second) != used_chords.end()) {
            return true;
        }
    }
    return false;
}

// remove overlapping tuplets with the same tuplet number
// when tuplet with bigger length contains the same notes

void remove_useless_tuplets(std::vector<TupletInfo> &tuplets) {
    struct {
        bool operator()(const TupletInfo &t1, const TupletInfo &t2) {
            if (t1.tuplet_number != t2.tuplet_number) {
                return t1.tuplet_number < t2.tuplet_number;
            }
            return t1.len < t2.len;
        }
    } comparator;

    std::sort(tuplets.begin(), tuplets.end(), comparator);

    size_t beg = 0;
    while (beg < tuplets.size()) {
        size_t end = beg + 1;
        while (tuplets.size() > end && tuplets[end].tuplet_number == tuplets[beg].tuplet_number) {
            ++end;
        }
        for (size_t i = beg; i < end - 1; ++i) {
            const auto &t1 = tuplets[i];
            for (size_t j = i + 1; j < end; ++j) {
                const auto &t2 = tuplets[j];
                if (t1.on_time >= t2.on_time && t1.on_time + t1.len <= t2.on_time + t2.len) {
                    // check onTimes
                    if (t2.chords.rbegin()->first < t1.on_time + t1.len && t2.chords.begin()->first >= t1.on_time) {
                        // remove larger tuplet
                        tuplets.erase(tuplets.begin() + j);
                        --j;
                        --end;
                    }
                }
            }
        }
        beg = end;
    }
}

std::set<int> find_longest_uncommon_group(const std::vector<TupletInfo> &tuplets, const ReducedFraction &basic_quant) {
    struct TInfo {
        bool operator<(const TInfo &other) const {
            if (off_time < other.off_time) {
                return true;
            } else if (off_time > other.off_time) {
                return false;
            } else {
                return on_time > other.on_time;
            }
        }

        bool operator==(const TInfo &other) const { return off_time == other.off_time; }

        ReducedFraction on_time;
        ReducedFraction off_time;
        int index;
    };

    std::vector<TInfo> info;
    for (size_t i = 0; i != tuplets.size(); ++i) {
        const auto &tuplet = tuplets[i];
        const auto interval = tuplet_interval(tuplet, basic_quant);
        info.push_back({interval.first, interval.second, static_cast<int>(i)});
    }

    std::sort(info.begin(), info.end());
    info.erase(std::unique(info.begin(), info.end()), info.end());

    // check for overlapping tuplets
    // and check for rare case: because of tol when detecting tuplets
    // non-overlapping tuplets can have common chords

    std::set<int> indexes;
    size_t last_selected = 0;
    for (size_t i = 0; i != info.size(); ++i) {
        if (i > 0 && info[i].on_time < info[last_selected].off_time) {
            continue;
        }
        if (have_common_chords(info[last_selected].index, info[i].index, tuplets)) {
            continue;
        }
        last_selected = i;
        indexes.insert(info[i].index);
    }

    return indexes;
}

struct TupletCommon {
    // indexes of tuplets that have common chords with the tuplet with tupletIndex
    std::set<int> common_indexes;
};

bool are_in_commons(const TupletInfo &t1, const TupletInfo &t2) {
    for (auto it1 = t1.chords.begin(); it1 != t1.chords.end(); ++it1) {
        for (auto it2 = t2.chords.begin(); it2 != t2.chords.end(); ++it2) {
            if (&*it1->second != &*it2->second) {
                continue;
            }
            if (t1.first_chord_index != 0 || t2.first_chord_index != 0 || it1 != t1.chords.begin() || it2 != t2.chords.begin() ||
                !is_more_tuplet_voices_allowed(1, it1->second->second.notes.size())) {
                return true;
            }
        }
    }
    return false;
}

std::vector<TupletCommon> find_tuplet_commons(const std::vector<TupletInfo> &tuplets) {
    std::vector<TupletCommon> tuplet_commons(tuplets.size());

    for (size_t i = 0; i != tuplets.size() - 1; ++i) {
        for (size_t j = i + 1; j != tuplets.size(); ++j) {
            if (are_in_commons(tuplets[i], tuplets[j])) {
                tuplet_commons[i].common_indexes.insert(int(j));
            }
        }
    }
    return tuplet_commons;
}

bool is_in_common_indexes(int index_to_check, const std::vector<int> &selected_tuplets, const std::vector<TupletCommon> &tuplet_commons) {
    for (size_t i = 0; i != selected_tuplets.size(); ++i) {
        const int tuplet_index = selected_tuplets[i];

        Q_ASSERT_X(index_to_check != tuplet_index, "MidiTuplet::isInCommonIndexes",
                   "Checked indexes are the same but they should be different");

        if (index_to_check > tuplet_index) {
            const auto &indexes = tuplet_commons[tuplet_index].common_indexes;
            if (indexes.find(index_to_check) != indexes.end()) {
                return true;
            }
        } else {
            const auto &indexes = tuplet_commons[index_to_check].common_indexes;
            if (indexes.find(tuplet_index) != indexes.end()) {
                return true;
            }
        }
    }
    return false;
}

TupletErrorResult find_tuplet_error(const std::vector<int> &tuplet_indexes, const std::vector<TupletInfo> &tuplets, size_t voice_count,
                                    const ReducedFraction &basic_quant) {
    ReducedFraction sum_error{0, 1};
    ReducedFraction sum_length_of_rests{0, 1};
    size_t sum_chord_count = 0;
    int sum_chord_places = 0;
    std::set<std::pair<const ReducedFraction, MidiChord> *> used_chords;
    std::vector<char> used_indexes(tuplets.size(), 0);

    for (int i : tuplet_indexes) {
        const auto &tuplet = tuplets[i];

        sum_error += tuplet.tuplet_sum_error;
        sum_length_of_rests += tuplet.sum_length_of_rests;
        sum_chord_count += tuplet.chords.size();
        sum_chord_places += tuplet.tuplet_number;

        used_indexes[i] = 1;
        for (const auto &chord : tuplet.chords) {
            used_chords.insert(&*chord.second);
        }
    }
    // add quant error of all chords excluded from tuplets
    for (size_t i = 0; i != tuplets.size(); ++i) {
        if (used_indexes[i]) {
            continue;
        }
        const auto &tuplet = tuplets[i];
        for (const auto &chord : tuplet.chords) {
            if (used_chords.find(&*chord.second) != used_chords.end()) {
                continue;
            }
            sum_error += Quantize::find_on_time_quant_error(*chord.second, basic_quant);
        }
    }

    return TupletErrorResult{sum_error.numerator() * 1.0 / (sum_error.denominator() * sum_chord_count),
                             sum_chord_count * 1.0 / sum_chord_places, sum_length_of_rests, voice_count, tuplet_indexes.size()};
}

#ifndef NDEBUG

bool are_commons_different(const std::vector<int> &selected_commons) {
    std::set<int> commons;
    for (int i : selected_commons) {
        if (commons.find(i) != commons.end()) {
            return false;
        }
        commons.insert(i);
    }
    return true;
}

bool are_commons_uncommon(const std::vector<int> &selected_commons, const std::vector<TupletCommon> &tuplet_commons) {
    std::set<int> commons;
    for (int i : selected_commons) {
        for (int j : tuplet_commons[i].common_indexes) {
            commons.insert(j);
        }
    }
    for (int i : selected_commons) {
        if (commons.find(i) != commons.end()) {
            return false;
        }
    }
    return true;
}

#endif

int find_available_voice(size_t tuplet_index, const std::vector<std::pair<ReducedFraction, ReducedFraction>> &tuplet_intervals,
                         const std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &voice_intervals) {
    int voice = 0;
    while (true) {
        const auto it = voice_intervals.find(voice);
        if (it != voice_intervals.end() && have_intersection(tuplet_intervals[tuplet_index], it->second)) {
            ++voice;
            continue;
        }
        break;
    }

    return voice;
}

std::map<std::pair<const ReducedFraction, MidiChord> *, int> prepare_used_first_chords(const std::vector<int> &selected_tuplets,
                                                                                       const std::vector<TupletInfo> &tuplets) {
    std::map<std::pair<const ReducedFraction, MidiChord> *, int> used_first_chords;
    for (int i : selected_tuplets) {
        if (tuplets[i].first_chord_index != 0) {
            continue;
        }
        const auto first_chord = tuplets[i].chords.begin();
        const auto it = used_first_chords.find(&*first_chord->second);
        if (it != used_first_chords.end()) {
            ++(it->second);
        } else {
            used_first_chords.insert({&*first_chord->second, 1});
        }
    }

    return used_first_chords;
}

std::vector<int> find_unused_indexes(const std::vector<int> &selected_tuplets) {
    std::vector<int> unused_indexes;
    int k = 0;
    for (int i = 0; i != selected_tuplets.back(); ++i) {
        if (i == selected_tuplets[k]) {
            ++k;
            continue;
        }
        unused_indexes.push_back(i);
    }
    return unused_indexes;
}

bool can_use_index(int index_to_check, const std::vector<TupletInfo> &tuplets,
                   const std::vector<std::pair<ReducedFraction, ReducedFraction>> &tuplet_intervals,
                   const std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &voice_intervals,
                   const std::map<std::pair<const ReducedFraction, MidiChord> *, int> &used_first_chords) {
    const auto &tuplet = tuplets[index_to_check];
    // check tuplets for common 1st chord
    if (tuplet.first_chord_index == 0) {
        const auto first_chord = tuplet.chords.begin();
        const auto it = used_first_chords.find(&*first_chord->second);
        if (it != used_first_chords.end() && !is_more_tuplet_voices_allowed(it->second, it->first->second.notes.size())) {
            return false;
        }
    }
    // check tuplets for resulting voice count
    const int voice = find_available_voice(index_to_check, tuplet_intervals, voice_intervals);
    const int voice_count = std::max((int) voice_intervals.size(), voice + 1); // index + 1 = count
    if (voice_count > 1 && (int) tuplet.chords.size() < tuplet_limits(tuplet.tuplet_number).min_note_count_add_voice) {
        return false;
    }
    return true;
}

#ifndef NDEBUG

bool are_tuplet_chords_empty(const std::vector<TupletInfo> &tuplets) {
    for (const auto &tuplet : tuplets) {
        if (tuplet.chords.empty()) {
            return true;
        }
    }
    return false;
}

template <typename Iter>
bool validate_selected_tuplets(Iter begin_it, Iter end_it, const std::vector<TupletInfo> &tuplets) {
    // <chord address, used voices>
    std::map<std::pair<const ReducedFraction, MidiChord> *, size_t> used_chords;
    for (auto index_it = begin_it; index_it != end_it; ++index_it) {
        const auto &tuplet = tuplets[*index_it];
        const auto &chords = tuplet.chords;
        for (auto it = chords.begin(); it != chords.end(); ++it) {
            bool is_first_chord = (tuplet.first_chord_index == 0 && it == tuplet.chords.begin());
            const auto fit = used_chords.find(&*(it->second));
            if (fit == used_chords.end()) {
                used_chords.insert({&*(it->second), is_first_chord ? 1 : engraving::voices});
            } else {
                if (!is_first_chord) {
                    return false;
                }
                if (!is_more_tuplet_voices_allowed(static_cast<int>(fit->second), it->second->second.notes.size())) {
                    return false;
                }
                ++(fit->second);
            }
        }
    }
    return true;
}

#endif

void try_update_best_indexes(std::vector<int> &best_tuplet_indexes, TupletErrorResult &min_current_error,
                             const std::vector<int> &selected_tuplets, const std::vector<TupletInfo> &tuplets,
                             const std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> &voice_intervals,
                             const ReducedFraction &basic_quant) {
    const size_t voice_count = voice_intervals.size();
    const auto error = find_tuplet_error(selected_tuplets, tuplets, voice_count, basic_quant);
    if (!min_current_error.is_initialized() || error < min_current_error) {
        min_current_error = error;
        best_tuplet_indexes = selected_tuplets;
    }
}

std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>>
prepare_voice_intervals(const std::vector<int> &selected_tuplets,
                        const std::vector<std::pair<ReducedFraction, ReducedFraction>> &tuplet_intervals) {
    // <voice, intervals>
    std::map<int, std::vector<std::pair<ReducedFraction, ReducedFraction>>> voice_intervals;
    for (int i : selected_tuplets) {
        int voice = find_available_voice(i, tuplet_intervals, voice_intervals);
        voice_intervals[voice].push_back(tuplet_intervals[i]);
    }
    return voice_intervals;
}

struct ValidTuplets {
    ValidTuplets(int tuplets_size) : indexes_(tuplets_size), first_(0) {
        for (int i = 0; i != (int) indexes_.size(); ++i) {
            indexes_[i] = {i - 1, i + 1};
        }
    }

    int first() const { return first_; }

    bool is_valid(int index) const { return index >= first_ && index < (int) indexes_.size(); }

    int next(int index) const { return indexes_[index].second; }

    bool empty() const { return first_ >= (int) indexes_.size(); }

    int exclude(int index) {
        if (index < first_) {
            return index;
        }
        int prev = indexes_[index].first;
        int next = indexes_[index].second;
        indexes_[index].first = -1;
        indexes_[index].second = int(indexes_.size());
        if (prev >= first_) {
            indexes_[prev].second = next;
        }
        if (next < (int) indexes_.size()) {
            indexes_[next].first = prev;
        }
        if (index == first_) {
            first_ = next;
        }
        return next;
    }

    std::vector<std::pair<int, int>> save() {
        std::vector<std::pair<int, int>> indexes(indexes_.size() - first_);
        for (size_t i = first_; i != indexes_.size(); ++i) {
            indexes[i - first_] = indexes_[i];
        }
        return indexes;
    }

    void restore(const std::vector<std::pair<int, int>> &indexes) {
        first_ = int(indexes_.size() - indexes.size());
        for (size_t i = 0; i < indexes.size(); ++i) {
            indexes_[i + first_] = indexes[i];
        }
    }

private:
    std::vector<std::pair<int, int>> indexes_; // pair<prev, next>
    int first_;
};

void find_next_tuplet(std::vector<int> &selected_tuplets, ValidTuplets &valid_tuplets, std::vector<int> &best_tuplet_indexes,
                      TupletErrorResult &min_current_error, const std::vector<TupletCommon> &tuplet_commons,
                      const std::vector<TupletInfo> &tuplets,
                      const std::vector<std::pair<ReducedFraction, ReducedFraction>> &tuplet_intervals, size_t commons_size,
                      const ReducedFraction &basic_quant) {
    while (!valid_tuplets.empty()) {
        size_t index = valid_tuplets.first();

        bool is_common_group_begins = (selected_tuplets.empty() && index == commons_size);
        if (is_common_group_begins) { // first level
            for (size_t i = index; i < tuplets.size(); ++i) {
                selected_tuplets.push_back(int(i));
            }
        } else {
            selected_tuplets.push_back(int(index));
        }
#ifndef NDEBUG
        Q_ASSERT_X(validate_selected_tuplets(selected_tuplets.begin(), selected_tuplets.end(), tuplets), "MIDI tuplets::findNextTuplet",
                   "Tuplets have common chords but they shouldn't");
#endif

        const auto voice_intervals = prepare_voice_intervals(selected_tuplets, tuplet_intervals);
        const auto used_first_chords = prepare_used_first_chords(selected_tuplets, tuplets);
#ifndef NDEBUG
        Q_ASSERT_X(are_commons_different(selected_tuplets), "MidiTuplet::findNextTuplet", "There are duplicates in selected commons");
        Q_ASSERT_X(are_commons_uncommon(selected_tuplets, tuplet_commons), "MidiTuplet::findNextTuplet", "Incompatible selected commons");
#endif

        if (is_common_group_begins) {
            bool can_add_more_indexes = false;
            for (size_t i = 0; i != commons_size; ++i) {
                if (!is_in_common_indexes(int(i), selected_tuplets, tuplet_commons) &&
                    can_use_index(int(i), tuplets, tuplet_intervals, voice_intervals, used_first_chords)) {
                    can_add_more_indexes = true;
                    break;
                }
            }
            if (!can_add_more_indexes) {
                try_update_best_indexes(best_tuplet_indexes, min_current_error, selected_tuplets, tuplets, voice_intervals, basic_quant);
            }
            return;
        }

        valid_tuplets.exclude(int(index));
        const auto saved_tuplets = valid_tuplets.save();
        // check tuplets for compatibility
        if (!valid_tuplets.empty()) {
            for (int i : tuplet_commons[index].common_indexes) {
                valid_tuplets.exclude(i);
                if (valid_tuplets.empty()) {
                    break;
                }
            }
        }
        for (int i = valid_tuplets.first(); valid_tuplets.is_valid(i);) {
            if (!can_use_index(i, tuplets, tuplet_intervals, voice_intervals, used_first_chords)) {
                i = valid_tuplets.exclude(i);
                continue;
            }
            i = valid_tuplets.next(i);
        }
        if (valid_tuplets.empty()) {
            const auto unused_indexes = find_unused_indexes(selected_tuplets);
            bool can_add_more_indexes = false;
            for (int i : unused_indexes) {
                if (!is_in_common_indexes(i, selected_tuplets, tuplet_commons) &&
                    can_use_index(i, tuplets, tuplet_intervals, voice_intervals, used_first_chords)) {
                    can_add_more_indexes = true;
                    break;
                }
            }
            if (!can_add_more_indexes) {
                try_update_best_indexes(best_tuplet_indexes, min_current_error, selected_tuplets, tuplets, voice_intervals, basic_quant);
            }
        } else {
            find_next_tuplet(selected_tuplets, valid_tuplets, best_tuplet_indexes, min_current_error, tuplet_commons, tuplets,
                             tuplet_intervals, commons_size, basic_quant);
        }

        selected_tuplets.pop_back();
        valid_tuplets.restore(saved_tuplets);
    }
}

void move_uncommon_tuplets_to_end(std::vector<TupletInfo> &tuplets, std::set<int> &uncommons) {
    int swap_with = int(tuplets.size()) - 1;
    for (int i = swap_with; i >= 0; --i) {
        auto it = uncommons.find(i);
        if (it != uncommons.end()) {
            if (i != swap_with) {
                std::swap(tuplets[i], tuplets[swap_with]);
            }
            --swap_with;
            uncommons.erase(it);
        }
    }

    Q_ASSERT_X(uncommons.empty(), "MidiTuplet::moveUncommonTupletsToEnd", "Untested uncommon tuplets remaining");
}

std::vector<int> find_best_tuplets(const std::vector<TupletCommon> &tuplet_commons, const std::vector<TupletInfo> &tuplets,
                                   size_t commons_size, const ReducedFraction &basic_quant) {
    std::vector<int> best_tuplet_indexes;
    std::vector<int> selected_tuplets;
    TupletErrorResult min_current_error;
    const auto tuplet_intervals = find_tuplet_intervals(tuplets, basic_quant);

    ValidTuplets valid_tuplets(int(tuplets.size()));

    find_next_tuplet(selected_tuplets, valid_tuplets, best_tuplet_indexes, min_current_error, tuplet_commons, tuplets, tuplet_intervals,
                     commons_size, basic_quant);

    return best_tuplet_indexes;
}

void remove_extra_tuplets(std::vector<TupletInfo> &tuplets) {
    const size_t max_tuplets = 17; // found empirically

    if (tuplets.size() <= max_tuplets) {
        return;
    }

    std::map<TupletErrorResult, size_t> errors;
    for (size_t i = 0; i != tuplets.size(); ++i) {
        auto tuplet_error = TupletErrorResult{
            tuplets[i].tuplet_sum_error.numerator() * 1.0 / (tuplets[i].tuplet_sum_error.denominator() * tuplets[i].chords.size()),
            tuplets[i].chords.size() * 1.0 / tuplets[i].tuplet_number, tuplets[i].sum_length_of_rests, 1, 1};
        errors.insert({tuplet_error, i});
    }
    std::vector<TupletInfo> new_tuplets;
    size_t count = 0;
    for (const auto &e : errors) {
        ++count;
        new_tuplets.push_back(tuplets[e.second]);
        if (count == max_tuplets) {
            break;
        }
    }

    std::swap(tuplets, new_tuplets);
}

// first chord in tuplet may belong to other tuplet at the same time
// in the case if there are enough notes in this first chord
// to be split into different voices

void filter_tuplets(std::vector<TupletInfo> &tuplets, const ReducedFraction &basic_quant) {
    if (tuplets.empty()) {
        return;
    }
#ifndef NDEBUG
    Q_ASSERT_X(!are_tuplet_chords_empty(tuplets), "MIDI tuplets: filterTuplets", "Tuplet has no chords but it should");
#endif
    remove_useless_tuplets(tuplets);
    remove_extra_tuplets(tuplets);

    std::set<int> uncommons = find_longest_uncommon_group(tuplets, basic_quant);
#ifndef NDEBUG
    Q_ASSERT_X(validate_selected_tuplets(uncommons.begin(), uncommons.end(), tuplets), "MIDI tuplets: filterTuplets",
               "Uncommon tuplets have common chords but they shouldn't");
#endif
    size_t commons_size = tuplets.size();
    if (uncommons.size() > 1) {
        commons_size -= uncommons.size();
        move_uncommon_tuplets_to_end(tuplets, uncommons);
    }
    const auto tuplet_commons = find_tuplet_commons(tuplets);

    const std::vector<int> best_indexes = find_best_tuplets(tuplet_commons, tuplets, commons_size, basic_quant);
#ifndef NDEBUG
    Q_ASSERT_X(validate_selected_tuplets(best_indexes.begin(), best_indexes.end(), tuplets), "MIDI tuplets: filterTuplets",
               "Tuplets have common chords but they shouldn't");
#endif
    std::vector<TupletInfo> new_tuplets;
    for (int i : best_indexes) {
        new_tuplets.push_back(tuplets[i]);
    }

    std::swap(tuplets, new_tuplets);
}
}} // namespace mu::iex::midi::MidiTuplet
