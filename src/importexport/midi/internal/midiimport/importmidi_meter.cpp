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
#include "importmidi_meter.h"

#include <algorithm>
#include <deque>

#include "engraving/dom/durationtype.h"
#include "engraving/dom/mscore.h"
#include "importmidi_chord.h"
#include "importmidi_fraction.h"
#include "importmidi_inner.h"
#include "importmidi_tuplet.h"

namespace mu::iex::midi { namespace Meter {
bool is_simple(const ReducedFraction &bar_fraction) // 2/2, 3/4, 4/4, ...
{
    return bar_fraction.numerator() < 5;
}

bool is_compound(const ReducedFraction &bar_fraction) // 6/8, 12/4, ...
{
    return bar_fraction.numerator() % 3 == 0 && bar_fraction.numerator() > 3;
}

bool is_complex(const ReducedFraction &bar_fraction) // 5/4, 7/8, ...
{
    return bar_fraction.numerator() == 5 || bar_fraction.numerator() == 7;
}

bool is_duple(const ReducedFraction &bar_fraction) // 2/2, 6/8, ...
{
    return bar_fraction.numerator() == 2 || bar_fraction.numerator() == 6;
}

bool is_triple(const ReducedFraction &bar_fraction) // 3/4, 9/4, ...
{
    return bar_fraction.numerator() == 3 || bar_fraction.numerator() == 9;
}

bool is_quadruple(const ReducedFraction &bar_fraction) // 4/4, 12/8, ...
{
    return bar_fraction.numerator() % 4 == 0;
}

bool is_quintuple(const ReducedFraction &bar_fraction) // 5/4, 15/8, ...
{
    return bar_fraction.numerator() % 5 == 0;
}

bool is_septuple(const ReducedFraction &bar_fraction) // 7/8, 21/8, ...
{
    return bar_fraction.numerator() % 7 == 0;
}

// list of bar division lengths in ticks (whole bar len, half bar len, ...)
// and its corresponding levels
// tuplets are not taken into account here

DivisionInfo metric_divisions_of_bar(const ReducedFraction &bar_fraction) {
    DivisionInfo bar_div_info;
    bar_div_info.len = bar_fraction;
    // first value of each element in list is a length (in ticks) of every part of bar
    // on which bar is subdivided on each level
    // the level value is a second value of each element
    auto &div_lengths = bar_div_info.div_lengths;
    int level = 0;
    div_lengths.push_back({bar_fraction, level});
    // pulse-level division
    if (is_duple(bar_fraction)) {
        div_lengths.push_back({bar_fraction / 2, --level});
    } else if (is_triple(bar_fraction)) {
        div_lengths.push_back({bar_fraction / 3, --level});
    } else if (is_quadruple(bar_fraction)) {
        div_lengths.push_back({bar_fraction / 2, --level}); // additional central accent
        div_lengths.push_back({bar_fraction / 4, --level});
    } else if (is_quintuple(bar_fraction)) {
        div_lengths.push_back({bar_fraction / 5, --level});
    } else if (is_septuple(bar_fraction)) {
        div_lengths.push_back({bar_fraction / 7, --level});
    } else { // if other - complex meter
        div_lengths.push_back({bar_fraction / bar_fraction.numerator(), --level});
    }

    if (is_compound(bar_fraction)) {
        --level; // additional min level for pulse divisions
                 // subdivide pulse of compound meter into 3 parts
        div_lengths.push_back({div_lengths.back().len / 3, --level});
    }

    while (div_lengths.back().len >= MChord::min_allowed_duration() * 2) {
        div_lengths.push_back({div_lengths.back().len / 2, --level});
    }

    return bar_div_info;
}

DivisionInfo metric_divisions_of_tuplet(const MidiTuplet::TupletData &tuplet, int tuplet_start_level) {
    DivisionInfo tuplet_div_info;
    tuplet_div_info.on_time = tuplet.on_time;
    tuplet_div_info.len = tuplet.len;
    tuplet_div_info.is_tuplet = true;
    tuplet_div_info.div_lengths.push_back({tuplet.len, ::mu::iex::midi::Meter::tuplet_boundary_level});
    const auto div_len = tuplet.len / tuplet.tuplet_number;
    tuplet_div_info.div_lengths.push_back({div_len, tuplet_start_level--});
    while (tuplet_div_info.div_lengths.back().len >= MChord::min_allowed_duration() * 2) {
        tuplet_div_info.div_lengths.push_back({tuplet_div_info.div_lengths.back().len / 2, tuplet_start_level--});
    }
    return tuplet_div_info;
}

ReducedFraction beat_length(const ReducedFraction &bar_fraction) {
    auto beat_len = bar_fraction / 4;
    if (is_duple(bar_fraction)) {
        beat_len = bar_fraction / 2;
    } else if (is_triple(bar_fraction)) {
        beat_len = bar_fraction / 3;
    } else if (is_quadruple(bar_fraction)) {
        beat_len = bar_fraction / 4;
    } else if (is_complex(bar_fraction)) {
        beat_len = bar_fraction / bar_fraction.numerator();
    }
    return beat_len;
}

std::vector<ReducedFraction> divisions_of_bar_for_tuplets(const ReducedFraction &bar_fraction) {
    const DivisionInfo info = metric_divisions_of_bar(bar_fraction);
    std::vector<ReducedFraction> div_lengths;
    const auto beat_len = beat_length(bar_fraction);
    for (const auto &i : info.div_lengths) {
        // in compound or complex meter
        // tuplet starts from beat level, not the whole bar
        if ((is_compound(bar_fraction) || is_complex(bar_fraction)) && i.len > beat_len) {
            continue;
        }
        div_lengths.push_back(i.len);
    }
    return div_lengths;
}

// result in vector: first elements - all tuplets info, one at the end - bar division info

std::vector<DivisionInfo> division_info(const ReducedFraction &bar_fraction, const std::vector<MidiTuplet::TupletData> &tuplets_in_bar) {
    std::vector<DivisionInfo> divs_info;

    const auto bar_division_info = metric_divisions_of_bar(bar_fraction);
    for (const auto &tuplet : tuplets_in_bar) {
        int tuplet_start_level = 0;
        for (const auto &div_len_info : bar_division_info.div_lengths) {
            if (div_len_info.len == tuplet.len) {
                tuplet_start_level = div_len_info.level;
                break;
            }
        }
        divs_info.push_back(metric_divisions_of_tuplet(tuplet, --tuplet_start_level));
    }
    divs_info.push_back(bar_division_info);

    return divs_info;
}

// tick is counted from the beginning of bar

int level_of_tick(const ReducedFraction &tick, const std::vector<DivisionInfo> &divs_info) {
    for (const auto &div_info : divs_info) {
        if (tick < div_info.on_time || tick > div_info.on_time + div_info.len) {
            continue;
        }
        for (const auto &div_len_info : div_info.div_lengths) {
            const auto ratio = (tick - div_info.on_time) / div_len_info.len;
            if (ratio.numerator() % ratio.denominator() == 0) {
                return div_len_info.level;
            }
        }
    }
    return 0;
}

std::vector<int> metric_levels_of_bar(const ReducedFraction &bar_fraction, const std::vector<DivisionInfo> &divs_info,
                                      const ReducedFraction &min_duration) {
    std::vector<int> levels;
    for (ReducedFraction tick(0, 1); tick < bar_fraction; tick += min_duration) {
        levels.push_back(level_of_tick(tick, divs_info));
    }
    return levels;
}

// return level with pos == Fraction(-1, 1) if undefined - see MaxLevel class

Meter::MaxLevel max_level_between(const ReducedFraction &start_tick_in_bar, const ReducedFraction &end_tick_in_bar,
                                  const DivisionInfo &div_info) {
    MaxLevel level;
    const auto start_tick_in_div = start_tick_in_bar - div_info.on_time;
    const auto end_tick_in_div = end_tick_in_bar - div_info.on_time;
    if (start_tick_in_div >= end_tick_in_div || start_tick_in_div < ReducedFraction(0, 1) || end_tick_in_div > div_info.len) {
        return level;
    }

    for (const auto &div_length_info : div_info.div_lengths) {
        const auto &div_len = div_length_info.len;
        const auto ratio = end_tick_in_div / div_len;
        auto max_end_raster = div_len * (ratio.numerator() / ratio.denominator());
        if (max_end_raster == end_tick_in_div) {
            max_end_raster -= div_len;
        }
        if (start_tick_in_div < max_end_raster) {
            // max level is found
            const auto sratio = start_tick_in_div / div_len;
            const auto max_start_raster = div_len * (sratio.numerator() / sratio.denominator());
            const auto count = (max_end_raster - max_start_raster) / div_len;
            level.pos = max_start_raster + div_len + div_info.on_time;
            level.level_count = count.numerator() / count.denominator();
            level.level = div_length_info.level;
            break;
        }
    }
    return level;
}

// vector<DivisionInfo>:
//    first elements - tuplet division info, if there are any tuplets
//    last element - always the whole bar division info

// here we use levelCount = 1 always for simplicity
// because TUPLET_BOUNDARY_LEVEL is 'max enough'

Meter::MaxLevel find_max_level_between(const ReducedFraction &start_tick_in_bar, const ReducedFraction &end_tick_in_bar,
                                       const std::vector<DivisionInfo> &divs_info) {
    MaxLevel level;

    for (const auto &div_info : divs_info) {
        if (div_info.is_tuplet) {
            if (start_tick_in_bar < div_info.on_time + div_info.len && end_tick_in_bar > div_info.on_time + div_info.len) {
                level.level = ::mu::iex::midi::Meter::tuplet_boundary_level;
                level.level_count = 1;
                level.pos = div_info.on_time + div_info.len;
                break;
            }
            if (start_tick_in_bar < div_info.on_time && end_tick_in_bar > div_info.on_time &&
                end_tick_in_bar <= div_info.on_time + div_info.len) {
                level.level = ::mu::iex::midi::Meter::tuplet_boundary_level;
                level.level_count = 1;
                level.pos = div_info.on_time;
                break;
            }
            if (start_tick_in_bar >= div_info.on_time && end_tick_in_bar <= div_info.on_time + div_info.len) {
                level = max_level_between(start_tick_in_bar, end_tick_in_bar, div_info);
                break;
            }
        } else {
            level = max_level_between(start_tick_in_bar, end_tick_in_bar, div_info);
            break;
        }
    }
    return level;
}

int tuplet_number_for_duration(const ReducedFraction &start_tick, const ReducedFraction &end_tick,
                               const std::vector<MidiTuplet::TupletData> &tuplets_in_bar) {
    for (const auto &tuplet_data : tuplets_in_bar) {
        if (start_tick >= tuplet_data.on_time && end_tick <= tuplet_data.on_time + tuplet_data.len) {
            return tuplet_data.tuplet_number;
        }
    }
    return -1; // this duration is not inside any tuplet
}

bool is_power_of_two(unsigned int x) { return x && !(x & (x - 1)); }

bool is_simple_note_duration(const ReducedFraction &duration) {
    const auto division = ReducedFraction::from_ticks(engraving::Constants::division);
    auto div = (duration > division) ? duration / division : division / duration;
    if (div > ReducedFraction(0, 1)) {
        div.reduce();
        int min_val = std::min(div.numerator(), div.denominator());
        int max_val = std::max(div.numerator(), div.denominator());
        return min_val == 1 && is_power_of_two((unsigned int) max_val);
    }
    return false;
}

bool is_quarter_duration(const ReducedFraction &ticks) { return ticks.numerator() == 1 && ticks.denominator() == 4; }

// If last 2/3 of beat in compound meter is rest,
// it should be split into 2 rests

bool is23_end_of_beat_in_compound_meter(const ReducedFraction &start_tick_in_bar, const ReducedFraction &end_tick_in_bar,
                                        const ReducedFraction &bar_fraction) {
    if (end_tick_in_bar <= start_tick_in_bar) {
        return false;
    }
    if (!is_compound(bar_fraction)) {
        return false;
    }

    const auto beat_len = beat_length(bar_fraction);
    const auto div_len = beat_len / 3;
    const auto ratio1 = start_tick_in_bar / beat_len;
    const auto ratio2 = end_tick_in_bar / beat_len;
    if ((start_tick_in_bar - beat_len * (ratio1.numerator() / ratio1.denominator()) == div_len) &&
        (ratio2.numerator() % ratio2.denominator() == 0)) {
        return true;
    }
    return false;
}

bool is2of3_rest_in_triple_meter(const ReducedFraction &start_tick_in_bar, const ReducedFraction &end_tick_in_bar,
                                 const ReducedFraction &bar_fraction) {
    if (end_tick_in_bar - start_tick_in_bar <= ReducedFraction(0, 1)) {
        return false;
    }
    if (is_triple(bar_fraction) && (start_tick_in_bar == ReducedFraction(0, 1) || end_tick_in_bar == bar_fraction) &&
        end_tick_in_bar - start_tick_in_bar == (bar_fraction * 2) / 3) {
        return true;
    }
    return false;
}

struct Node {
    Node(int edge_level, int mid_level) : edge_level(edge_level), mid_level(mid_level) {}

    int edge_level;
    int mid_level;
};

// all durations inside tuplets are smaller/larger than their regular versions
// this difference is represented by tuplet ratio: 3/2 for triplets, etc.

// if node duration is completely inside some tuplet
// then assign to the node tuplet-to-regular-duration conversion coefficient

ReducedFraction find_tuplet_ratio(const ReducedFraction &start_pos, const ReducedFraction &end_pos,
                                  const std::vector<MidiTuplet::TupletData> &tuplets_in_bar) {
    ReducedFraction tuplet_ratio = {2, 2};
    int tuplet_number = tuplet_number_for_duration(start_pos, end_pos, tuplets_in_bar);
    if (tuplet_number != -1) {
        tuplet_ratio = MidiTuplet::tuplet_limits(tuplet_number).ratio;
    }

    return tuplet_ratio;
}

std::vector<std::pair<ReducedFraction, engraving::TDuration>> collect_durations(const std::map<ReducedFraction, Node> &nodes,
                                                                                const std::vector<MidiTuplet::TupletData> &tuplets_in_bar,
                                                                                bool use_dots, bool print_rest_remains) {
    std::vector<std::pair<ReducedFraction, engraving::TDuration>> result_durations;

    for (auto it1 = nodes.begin(); it1 != nodes.end(); ++it1) {
        auto it2 = it1;
        ++it2;
        if (it2 == nodes.end()) {
            break;
        }
        const auto tuplet_ratio = find_tuplet_ratio(it1->first, it2->first, tuplets_in_bar);
        const auto duration = tuplet_ratio * (it2->first - it1->first);
        auto list = mu::engraving::to_duration_list(duration.fraction(), use_dots, 1, print_rest_remains);
        for (const auto &dur : list) {
            result_durations.push_back({tuplet_ratio, dur});
        }
    }

    return result_durations;
}

bool bad_level_condition(int start_level_diff, int end_level_diff, int tol) { return start_level_diff > tol || end_level_diff > tol; }

int note_count(const ReducedFraction &duration, bool use_dots) {
    return int(mu::engraving::to_duration_list(duration.fraction(), use_dots, 1, false).size());
}

bool is_less_note_count(const ReducedFraction &t1, const ReducedFraction &t2, const ReducedFraction &t3, bool use_dots) {
    return note_count(t3 - t1, use_dots) < note_count(t2 - t1, use_dots) + note_count(t3 - t2, use_dots);
}

void exclude_nodes(std::map<ReducedFraction, Node> &nodes, int tol, bool use_dots) {
    if (tol == 0) { // no nodes can be excluded
        return;
    }

    auto p1 = nodes.begin();
    if (p1 == nodes.end()) {
        return;
    }
    auto p2 = p1;
    ++p2;
    if (p2 == nodes.end()) {
        return;
    }
    auto p3 = p2;
    ++p3;
    if (p3 == nodes.end()) {
        return;
    }

    while (p3 != nodes.end()) {
        if (!bad_level_condition(p2->second.mid_level - p1->second.edge_level, p2->second.mid_level - p3->second.edge_level, tol) &&
            is_less_note_count(p1->first, p2->first, p3->first, use_dots)) {
            p2 = nodes.erase(p2);
            ++p3;
            continue;
        }
        ++p1;
        ++p2;
        ++p3;
    }
}

// set tuplet boundary level to regular, non-tuplet bar division level
// because there is no more need in tuplet boundary level after split
// and such big level may confuse the estimation algorithm

int adjust_edge_level_if_tuplet(const Meter::MaxLevel &split_point, const std::vector<DivisionInfo> &div_info) {
    int tuplet_level = split_point.level;
    if (split_point.level == ::mu::iex::midi::Meter::tuplet_boundary_level) {
        std::vector<DivisionInfo> non_tuplet_divs({div_info.back()});
        tuplet_level = level_of_tick(split_point.pos, non_tuplet_divs);
    }

    return tuplet_level;
}

// duration start/end should be quantisized, quantum >= 1/128 note
// pair here represents the tuplet ratio of duration and the duration itself
// for regular (non-tuplet) durations fraction.numerator == fraction.denominator

// tol - max allowed difference between start/end level of duration and split point level
//    1 for notes, 0 for rests

std::vector<std::pair<ReducedFraction, engraving::TDuration>>
to_duration_list(const ReducedFraction &start_tick_in_bar, const ReducedFraction &end_tick_in_bar, const ReducedFraction &bar_fraction,
                 const std::vector<MidiTuplet::TupletData> &tuplets_in_bar, DurationType duration_type, bool use_dots,
                 bool print_rest_remains) {
    if (start_tick_in_bar < ReducedFraction(0, 1) || end_tick_in_bar <= start_tick_in_bar || end_tick_in_bar > bar_fraction) {
        return std::vector<std::pair<ReducedFraction, engraving::TDuration>>();
    }

    const auto div_info = division_info(bar_fraction, tuplets_in_bar); // metric structure of bar
    const auto min_duration = MChord::min_allowed_duration() * 2;      // >= minAllowedDuration() after subdivision

    std::map<ReducedFraction, Node> nodes; // <onTime, Node>
    {
        int level = level_of_tick(start_tick_in_bar, div_info);
        nodes.insert({start_tick_in_bar, Node(level, level)});
        level = level_of_tick(end_tick_in_bar, div_info);
        nodes.insert({end_tick_in_bar, Node(level, level)});
    }

    std::deque<std::pair<ReducedFraction, ReducedFraction>> gaps_to_process;
    gaps_to_process.push_back({start_tick_in_bar, end_tick_in_bar});

    while (!gaps_to_process.empty()) {
        const auto gap = gaps_to_process.front();
        gaps_to_process.pop_front();
        // don't split gap if its duration is less than minDuration
        if (gap.second - gap.first < min_duration) {
            continue;
        }
        const auto split_point = find_max_level_between(gap.first, gap.second, div_info);
        // sum levels if there are several positions (beats) with max level value
        // for example, 8th + half duration + 8th in 3/4, and half is over two beats
        if (split_point.pos == ReducedFraction(-1, 1)) { // undefined
            continue;
        }
        const int effective_level = split_point.level + split_point.level_count - 1;
        const Node &start_node = nodes.find(gap.first)->second;
        const Node &end_node = nodes.find(gap.second)->second;

        if (bad_level_condition(effective_level - start_node.edge_level, effective_level - end_node.edge_level, 0) ||
            is2of3_rest_in_triple_meter(gap.first, gap.second, bar_fraction) ||
            (duration_type == DurationType::REST && is23_end_of_beat_in_compound_meter(gap.first, gap.second, bar_fraction))) {
            const int edge_level = adjust_edge_level_if_tuplet(split_point, div_info);
            // split gap in splitPoint position
            nodes.insert({split_point.pos, Node(edge_level, effective_level)});
            gaps_to_process.push_back({gap.first, split_point.pos});
            gaps_to_process.push_back({split_point.pos, gap.second});
        }
    }

    const int tol = (duration_type == DurationType::NOTE) ? 1 : 0;
    exclude_nodes(nodes, tol, use_dots);

    return collect_durations(nodes, tuplets_in_bar, use_dots, print_rest_remains);
}
}} // namespace mu::iex::midi::Meter
