/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2023 MuseScore Limited and others
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
#include <algorithm>
#include <cfloat>

#include "accidentalslayout.h"
#include "layoutcontext.h"
#include "../paddingtable.h"
#include "shape.h"
#include "tlayout.h"

#include "dom/accidental.h"
#include "dom/chord.h"
#include "dom/ledgerline.h"
#include "dom/note.h"
#include "dom/score.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

AccidentalsLayout::AccidentalsLayoutContext::AccidentalsLayoutContext(std::vector<Accidental *> acc, std::vector<Chord *> ch)
    : all_accidentals(std::move(acc)), chords(std::move(ch)) {
    sort_top_down(all_accidentals);
    stacked_accidentals.reserve(all_accidentals.size());
    stacked_accidentals_shape.elements().reserve(4 * all_accidentals.size());

    init_constants();
}

void AccidentalsLayout::AccidentalsLayoutContext::init_constants() {
    spatium_ = all_accidentals.front()->spatium();

    // Hardcoded engraving parameters that don't have a style (some of these may get a style in future)
    vertical_accidental_to_accidental_clearance_ = 0.15 * spatium_;
    vertical_sharp_to_sharp_clearance_ = 0.05 * spatium_;
    vertical_accidental_to_chord_clearance_ = 0.10 * spatium_;
    additional_padding_for_verticals_ = 0.1 * spatium_;
    x_pos_split_threshold_ = 0.5 * spatium_;
    x_vertical_alignment_threshold_ = 0.75 * spatium_;
    sharp_and_natural_ledger_line_padding_ = 0.1 * spatium_;
    reduced_flat_to_note_padding_ = 0.15 * spatium_;
    flat_kerning_of_fourth_ = -0.15 * spatium_;
    natural_kerning_of_fourth_ = -0.05 * spatium_;

    const MStyle &style = all_accidentals.front()->style();
    accidental_accidental_distance_ = style.style_absolute(Sid::ACCIDENTAL_DISTANCE);
    order_follow_note_displacement_ = style.style_b(Sid::ACCIDENTAL_ORDER_FOLLOWS_NOTE_DISPLACEMENT);
    align_octaves_across_sub_chords_ = style.style_b(Sid::ALIGN_ACCIDENTAL_OCTAVES_ACROSS_SUB_CHORDS);
    keep_seconds_together_ = style.style_b(Sid::KEEP_ACCIDENTAL_SECONDS_TOGETHER);
    align_offset_octaves_ = style.style_b(Sid::ALIGN_OFFSET_OCTAVE_ACCIDENTALS);
}

void AccidentalsLayout::layout_accidentals(const std::vector<Chord *> &chords, LayoutContext &ctx) {
    std::vector<Accidental *> all_accidentals;
    std::vector<Accidental *> redundant_accidentals;
    std::vector<Accidental *> invisible_accidentals;

    collect_accidentals(chords, all_accidentals, redundant_accidentals, invisible_accidentals);

    for (Accidental *acc : invisible_accidentals) {
        acc->compute_mag();
        TLayout::layout_accidental(acc, acc->mutldata(), ctx.conf());
        double x = -acc->ldata()->bbox().width();
        x -= acc->note() ? acc->note()->pos().x() : 0.0;
        acc->set_pos(x, 0.0);
    }

    if (all_accidentals.empty()) {
        return;
    }

    for (Accidental *acc : redundant_accidentals) {
        acc->set_pos(0.0, 0.0);
        acc->setbbox(RectF());
    }

    for (Accidental *acc : all_accidentals) {
        acc->set_pos(0.0, 0.0);
        acc->compute_mag();
        TLayout::layout_accidental(acc, acc->mutldata(), ctx.conf());
    }

    AccidentalsLayoutContext accidentals_layout_context(std::move(all_accidentals), chords);
    do_accidental_placement(accidentals_layout_context);
}

void AccidentalsLayout::collect_accidentals(const std::vector<Chord *> chords, std::vector<Accidental *> &all_accidentals,
                                            std::vector<Accidental *> &redundant_accidentals,
                                            std::vector<Accidental *> &invisible_accidentals) {
    for (const Chord *chord : chords) {
        for (const Note *note : chord->notes()) {
            Accidental *acc = note->accidental();
            if (!acc) {
                continue;
            }
            if (accidental_is_redundant(acc, all_accidentals)) {
                redundant_accidentals.push_back(acc);
            } else if (!acc->add_to_skyline()) {
                invisible_accidentals.push_back(acc);
            } else {
                all_accidentals.push_back(acc);
            }
        }
    }
}

bool AccidentalsLayout::accidental_is_redundant(const Accidental *acc, const std::vector<Accidental *> &all_accidentals) {
    for (const Accidental *other_acc : all_accidentals) {
        if (other_acc->accidental_type() == acc->accidental_type() && other_acc->line() == acc->line()) {
            return true;
        }
    }

    return false;
}

void AccidentalsLayout::do_accidental_placement(AccidentalsLayoutContext &ctx) {
    create_chords_shape(ctx);

    find_octaves_and_seconds(ctx);
    split_into_sub_chords(ctx);

    for (std::vector<Accidental *> &accidental_sub_chord : ctx.accidental_sub_chords) {
        layout_sub_chord(accidental_sub_chord, ctx);
    }

    vertically_align_accidentals(ctx);
}

void AccidentalsLayout::find_octaves_and_seconds(const AccidentalsLayoutContext &ctx) {
    for (Accidental *acc1 : ctx.all_accidentals) {
        std::vector<Accidental *> &octaves = acc1->mutldata()->octaves.mut_value();
        octaves.clear();
        std::vector<Accidental *> &seconds = acc1->mutldata()->seconds.mut_value();
        seconds.clear();
        for (Accidental *acc2 : ctx.all_accidentals) {
            if (acc2 == acc1) {
                continue;
            }
            if (is_octave(acc1, acc2)) {
                octaves.push_back(acc2);
            } else if (abs(acc1->line() - acc2->line()) == 1) {
                seconds.push_back(acc2);
            }
        }
    }
}

void AccidentalsLayout::split_into_sub_chords(AccidentalsLayoutContext &ctx) {
    ctx.accidental_sub_chords.reserve(2);

    std::vector<Accidental *> first_group;
    first_group.reserve(ctx.all_accidentals.size());
    first_group.push_back(ctx.all_accidentals.front());
    ctx.accidental_sub_chords.push_back(first_group);

    static constexpr int line_diff_of_seventh = 6;

    for (size_t i = 1; i < ctx.all_accidentals.size(); ++i) {
        Accidental *prev_acc = ctx.all_accidentals[i - 1];
        Accidental *cur_acc = ctx.all_accidentals[i];
        bool start_new_group = cur_acc->line() - prev_acc->line() >= line_diff_of_seventh;
        if (start_new_group) {
            ctx.accidental_sub_chords.push_back(std::vector<Accidental *>{cur_acc});
        } else {
            ctx.accidental_sub_chords.back().push_back(cur_acc);
        }
    }

    merge_adjacent_sub_groups_if_too_small(ctx);

    ctx.sub_chords_count_before_octave_merge = ctx.accidental_sub_chords.size();
    if (ctx.align_octaves_across_sub_chords()) {
        merge_sub_groups_with_octaves_across(ctx);
    }
}

void AccidentalsLayout::merge_adjacent_sub_groups_if_too_small(AccidentalsLayoutContext &ctx) {
    const double approx_offset_note_threshold = 0.5 * ctx.spatium();
    bool grouping_changed = false;
    do {
        grouping_changed = false;
        for (size_t i = 0; i < ctx.accidental_sub_chords.size() - 1; ++i) {
            std::vector<Accidental *> &this_group = ctx.accidental_sub_chords[i];
            std::vector<Accidental *> &next_group = ctx.accidental_sub_chords[i + 1];
            bool group_is_small = this_group.size() + next_group.size() <= ctx.small_group_limit();
            bool next_starts_with_offset_note = next_group.front()->note()->x() > approx_offset_note_threshold;
            if (group_is_small && !next_starts_with_offset_note) {
                grouping_changed = true;
                this_group.insert(this_group.end(), next_group.begin(), next_group.end());
                ctx.accidental_sub_chords.erase(ctx.accidental_sub_chords.begin() + i + 1);
            }
        }
    } while (grouping_changed);
}

void AccidentalsLayout::merge_sub_groups_with_octaves_across(AccidentalsLayoutContext &ctx) {
    auto is_octave_across_groups = [](const std::vector<Accidental *> &group1, const std::vector<Accidental *> &group2) {
        for (const Accidental *acc1 : group1) {
            for (Accidental *octave_acc : acc1->ldata()->octaves.value()) {
                if (muse::contains(group2, octave_acc)) {
                    return true;
                }
            }
        }
        return false;
    };

    bool found_octave = false;
    do {
        found_octave = false;
        for (size_t i = 0; i < ctx.accidental_sub_chords.size() - 1; ++i) {
            std::vector<Accidental *> &this_group = ctx.accidental_sub_chords[i];
            for (size_t j = i + 1; j < ctx.accidental_sub_chords.size(); ++j) {
                std::vector<Accidental *> &next_group = ctx.accidental_sub_chords[j];
                found_octave = is_octave_across_groups(this_group, next_group);
                if (found_octave) {
                    this_group.insert(this_group.end(), next_group.begin(), next_group.end());
                    ctx.accidental_sub_chords.erase(ctx.accidental_sub_chords.begin() + j);
                    break;
                }
            }
            if (found_octave) {
                break;
            }
        }
    } while (found_octave);
}

void AccidentalsLayout::create_chords_shape(AccidentalsLayoutContext &ctx) {
    ctx.chords_shape.clear();

    for (const Chord *chord : ctx.chords) {
        PointF chord_pos = keep_accidentals_close_to_chord(chord) ? PointF() : chord->pos();
        for (const LedgerLine *ledger : chord->ledger_lines()) {
            ctx.chords_shape.add(ledger->shape().translate(ledger->pos() + chord_pos));
        }
        for (const Note *note : chord->notes()) {
            SymId note_sym = note->ldata()->cached_notehead_sym;
            if (note_sym == SymId::NO_SYM) {
                note_sym = note->note_head();
            }
            ctx.chords_shape.add(note->sym_shape_with_cutouts(note_sym).translate(note->pos() + chord_pos));
        }
        const Stem *stem = chord->stem();
        if (stem && stem->add_to_skyline()) {
            ctx.chords_shape.add(stem->shape().translate(stem->pos() + chord_pos));
        }
    }
}

void AccidentalsLayout::layout_sub_chord(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    compute_ordering(accidentals, ctx);

    for (Accidental *acc : accidentals) {
        stack_accidental(acc, ctx);
    }
}

void AccidentalsLayout::stack_accidental(Accidental *acc, AccidentalsLayoutContext &ctx) {
    Shape acc_shape = acc->shape();
    acc_shape.translate_y(acc->note()->y());

    double x = min_accidental_to_chord_distance(acc, acc_shape, ctx);
    acc_shape.translate_x(-x);

    double min_dist_to_accid_group = min_accidental_to_accidental_group_distance(acc, acc_shape, ctx.stacked_accidentals_shape, ctx);
    acc_shape.translate_x(-min_dist_to_accid_group);
    x += min_dist_to_accid_group;

    set_xpos_relative_to_segment(acc, -x);

    check_zero_column(acc, ctx);
    ctx.stacked_accidentals_shape.add(acc_shape);
    ctx.stacked_accidentals.push_back(acc);
}

void AccidentalsLayout::check_zero_column(Accidental *acc, const AccidentalsLayoutContext &ctx) {
    int column = acc->ldata()->column;
    if (column != 0) {
        return;
    }

    RectF this_b_box = acc->ldata()->bbox().translated(x_pos_relative_to_segment(acc), 0.0);
    for (Accidental *stacked_acc : ctx.stacked_accidentals) {
        RectF placed_b_box = stacked_acc->ldata()->bbox().translated(x_pos_relative_to_segment(stacked_acc), 0.0);
        if (this_b_box.right() < placed_b_box.left()) {
            column = std::max(column, stacked_acc->ldata()->column.value() + 1);
        }
    }
    acc->mutldata()->column = column;
}

void AccidentalsLayout::compute_ordering(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    AccidentalGroups priority_groups = split_into_priority_groups(accidentals, ctx);

    accidentals.clear();
    for (std::vector<Accidental *> &group : priority_groups) {
        determine_stacking_order(group, ctx);
        accidentals.insert(accidentals.end(), group.begin(), group.end());
    }

    apply_ordering_offsets(accidentals);
}

AccidentalGroups AccidentalsLayout::split_into_priority_groups(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    AccidentalGroups groups;

    if (ctx.order_follow_note_displacement()) {
        AccidentalGroups note_split_groups = split_according_to_note_displacement(accidentals, ctx);
        for (auto note_split_group : note_split_groups) {
            AccidentalGroups accid_split_groups = split_according_to_accid_displacement(note_split_group, ctx);
            groups.insert(groups.end(), accid_split_groups.begin(), accid_split_groups.end());
        }
    } else {
        groups = split_according_to_accid_displacement(accidentals, ctx);
    }

    size_t cur_groups_count = groups.size();
    if (ctx.align_octaves_across_sub_chords()) {
        merge_sub_groups_with_octaves_across(ctx);
        if (groups.size() != cur_groups_count) {
            for (auto &sub_group : groups) {
                sort_top_down(sub_group);
            }
        }
    }

    return groups;
}

AccidentalGroups AccidentalsLayout::split_according_to_accid_displacement(std::vector<Accidental *> &accidentals,
                                                                          const AccidentalsLayoutContext &ctx) {
    AccidentalGroups sub_groups = group_accidentals_by_x_pos(accidentals, ctx);

    muse::remove_if(sub_groups, [](auto &sub_group) { return sub_group.size() == 0; });

    if (sub_groups.size() > 1 && sub_groups[0].size() > 1) {
        move_octaves_to_second_group(sub_groups);
    }

    for (std::vector<Accidental *> &sub_group : sub_groups) {
        sort_top_down(sub_group);
    }

    return sub_groups;
}

AccidentalGroups AccidentalsLayout::group_accidentals_by_x_pos(std::vector<Accidental *> &accidentals,
                                                               const AccidentalsLayoutContext &ctx) {
    std::map<double, std::vector<Accidental *>> groups_of_equal_x;

    for (Accidental *acc : accidentals) {
        Shape acc_rough_shape = Shape(acc->sym_bbox(acc->sym_id()), acc);
        acc_rough_shape.translate_y(acc->note()->y());
        double x = -min_accidental_to_chord_distance(acc, acc_rough_shape, ctx) + acc->sym_width(acc->sym_id());
        groups_of_equal_x[std::round(x)].push_back(acc);
    }

    double split_point = DBL_MAX;
    double prev_x = (*groups_of_equal_x.begin()).first;
    for (auto &pair : groups_of_equal_x) {
        double cur_x = pair.first;
        if (cur_x - prev_x > ctx.x_pos_split_threshold()) {
            split_point = cur_x;
            break;
        }
        prev_x = cur_x;
    }

    AccidentalGroups sub_groups(2);
    for (auto &pair : groups_of_equal_x) {
        if (pair.first >= split_point) {
            for (Accidental *acc : pair.second) {
                sub_groups[0].push_back(acc);
                acc->mutldata()->vertical_subgroup = 0;
            }
        } else {
            for (Accidental *acc : pair.second) {
                sub_groups[1].push_back(acc);
                acc->mutldata()->vertical_subgroup = 1;
            }
        }
    }

    return sub_groups;
}

void AccidentalsLayout::move_octaves_to_second_group(AccidentalGroups &sub_groups) {
    IF_ASSERT_FAILED(sub_groups.size() == 2) { return; }

    std::vector<Accidental *> &first_group = sub_groups[0];
    std::vector<Accidental *> &second_group = sub_groups[1];
    auto iter = first_group.begin();
    while (iter != first_group.end()) {
        Accidental *acc = *iter;
        bool remove_one = false;
        for (Accidental *octave_acc : acc->ldata()->octaves.value()) {
            if (muse::contains(second_group, octave_acc)) {
                remove_one = true;
                second_group.push_back(acc);
                acc->mutldata()->vertical_subgroup = 1;
                break;
            }
        }
        if (remove_one) {
            iter = first_group.erase(iter);
        } else {
            ++iter;
        }
    }
}

AccidentalGroups AccidentalsLayout::split_according_to_note_displacement(std::vector<Accidental *> &accidentals,
                                                                         const AccidentalsLayoutContext &ctx) {
    AccidentalGroups sub_groups = group_accidentals_by_note_x_pos(accidentals);

    if (ctx.keep_seconds_together()) {
        move_seconds_in_same_group(sub_groups);
    }

    for (size_t i = 0; i < sub_groups.size(); ++i) {
        for (Accidental *acc : sub_groups[i]) {
            acc->mutldata()->vertical_subgroup = static_cast<int>(i);
        }
    }

    return sub_groups;
}

AccidentalGroups AccidentalsLayout::group_accidentals_by_note_x_pos(const std::vector<Accidental *> &accidentals) {
    AccidentalGroups sub_groups;

    std::map<double, std::vector<Accidental *>> groups_of_equal_note_x;
    for (Accidental *accidental : accidentals) {
        Note *note = accidental->note();
        double x = std::round(note->pos().x() + note->chord()->pos().x());
        groups_of_equal_note_x[x].push_back(accidental);
    }

    for (auto iter = groups_of_equal_note_x.rbegin(); iter != groups_of_equal_note_x.rend(); ++iter) {
        sub_groups.push_back((*iter).second);
    }

    return sub_groups;
}

void AccidentalsLayout::move_seconds_in_same_group(AccidentalGroups &sub_groups) {
    bool second_moved = false;
    do {
        second_moved = false;
        for (size_t i = 0; i < sub_groups.size() - 1; ++i) {
            std::vector<Accidental *> &this_group = sub_groups[i];
            std::vector<Accidental *> &next_group = sub_groups[i + 1];
            for (size_t j = 0; j < this_group.size(); ++j) {
                Accidental *acc1 = this_group[j];
                for (Accidental *second_acc : acc1->ldata()->seconds.value()) {
                    if (muse::remove(next_group, second_acc)) {
                        second_moved = true;
                        auto position = this_group.begin() + j;
                        if (second_acc->line() < acc1->line()) {
                            this_group.insert(position, second_acc);
                        } else {
                            this_group.insert(position + 1, second_acc);
                        }
                        break;
                    }
                }
                if (second_moved) {
                    break;
                }
            }
            if (next_group.size() == 0) {
                sub_groups.erase(sub_groups.begin() + i + 1);
            }
            if (second_moved) {
                break;
            }
        }
    } while (second_moved);
}

void AccidentalsLayout::determine_stacking_order(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    size_t group_size = accidentals.size();

    if (group_size <= ctx.small_group_limit()) {
        compute_standard_ordering(accidentals, ctx);
    } else if (group_size > ctx.large_group_limit() ||
               (ctx.align_octaves_across_sub_chords() && ctx.sub_chords_count_before_octave_merge > 1)) {
        compute_compact_ordering(accidentals, ctx);
    } else {
        compute_ordering_with_least_columns(accidentals, ctx);
    }
}

void AccidentalsLayout::compute_ordering_with_least_columns(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    std::vector<Accidental *> standard_ordered = accidentals;
    compute_standard_ordering(standard_ordered, ctx);

    std::vector<Accidental *> compact_ordered = accidentals;
    compute_compact_ordering(compact_ordered, ctx);

    auto total_columns = [&](std::vector<Accidental *> &acc_group) {
        std::map<Accidental *, int> accidental_column;
        accidental_column[acc_group.front()] = 0;
        int max_column = 0;
        for (size_t i = 1; i < acc_group.size(); ++i) {
            Accidental *acc1 = acc_group[i];
            accidental_column[acc1] = max_column;
            for (int j = static_cast<int>(i - 1); j >= 0; --j) {
                Accidental *acc2 = acc_group[j];
                if (accidental_column[acc2] < max_column) {
                    continue;
                }
                if (!can_fit_in_same_column(acc1, acc2, ctx)) {
                    max_column += 1;
                    accidental_column[acc1] = max_column;
                }
            }
        }
        return max_column;
    };

    accidentals = total_columns(compact_ordered) < total_columns(standard_ordered) ? compact_ordered : standard_ordered;
}

void AccidentalsLayout::compute_standard_ordering(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    std::vector<Accidental *> accidentals_placed;
    accidentals_placed.reserve(accidentals.size());

    AccidentalsVectorView accidentals_to_place{accidentals};

    bool pick_from_top = true;
    while (!accidentals_to_place.empty()) {
        Accidental *acc = pick_from_top ? accidentals_to_place.pop_front() : accidentals_to_place.pop_back();
        accidentals_placed.push_back(acc);

        if (ctx.keep_seconds_together()) {
            find_and_insert_second(acc, accidentals_placed, accidentals_to_place, ctx);
        }

        const bool found_octave = find_and_insert_octave(acc, accidentals_placed, accidentals_to_place, ctx);
        if (found_octave) {
            pick_from_top = true;
        } else {
            pick_from_top = !pick_from_top;
        }
    }

    accidentals = accidentals_placed;
}

void AccidentalsLayout::compute_compact_ordering(std::vector<Accidental *> &accidentals, AccidentalsLayoutContext &ctx) {
    const size_t tot_accid_number = accidentals.size();
    Accidental *const bottom_acc = accidentals.back();

    std::vector<Accidental *> accidentals_placed;
    accidentals_placed.reserve(tot_accid_number);

    AccidentalsVectorView accidentals_to_place{accidentals};

    bool pick_from_top = true;
    while (!accidentals_to_place.empty()) {
        Accidental *acc = pick_from_top ? accidentals_to_place.pop_front() : accidentals_to_place.pop_back();
        accidentals_placed.push_back(acc);

        bool found_one_that_fits = false;
        bool hit_bottom = false;

        if (find_and_insert_octave(acc, accidentals_placed, accidentals_to_place, ctx, !pick_from_top, pick_from_top)) {
            found_one_that_fits = true;
            acc = accidentals_placed.back();
            if (acc == bottom_acc) {
                hit_bottom = true;
            }
        }

        // fit others into same column
        const auto pred = [&](Accidental *acc2) {
            if ((pick_from_top && acc2->line() < acc->line()) || (!pick_from_top && acc2->line() > acc->line())) {
                return false;
            }
            if (can_fit_in_same_column(acc, acc2, ctx)) {
                if (acc2->ldata()->octaves.value().size() > 0) {
                    return false;
                }
                found_one_that_fits = true;
                accidentals_placed.push_back(acc2);
                acc = acc2;
                if (acc2 == bottom_acc) {
                    hit_bottom = true;
                }
                return true;
            }
            return false;
        };

        if (pick_from_top) {
            accidentals_to_place.remove_if(pred);
        } else {
            accidentals_to_place.reverse_remove_if(pred);
        }

        if (ctx.keep_seconds_together()) {
            for (Accidental *already_placed_acc : accidentals_placed) {
                find_and_insert_second(already_placed_acc, accidentals_placed, accidentals_to_place, ctx);
            }
        }

        if (hit_bottom || (found_one_that_fits && tot_accid_number <= ctx.large_group_limit())) {
            pick_from_top = true;
        } else {
            pick_from_top = !pick_from_top;
        }
    }

    accidentals = accidentals_placed;
}

void AccidentalsLayout::find_and_insert_second(Accidental *acc, std::vector<Accidental *> &accidentals_placed,
                                               AccidentalsVectorView &accidentals_to_place, AccidentalsLayoutContext &ctx) {
    const std::vector<Accidental *> &seconds = acc->ldata()->seconds.value();
    for (Accidental *second_acc : seconds) {
        const bool removed = accidentals_to_place.remove(second_acc);
        if (removed) {
            if (ctx.order_follow_note_displacement() && second_acc->note()->x() > acc->note()->x()) {
                auto position = std::find(accidentals_placed.begin(), accidentals_placed.end(), acc);
                accidentals_placed.insert(position, second_acc);
            } else {
                accidentals_placed.push_back(second_acc);
            }
        }
    }
}

bool AccidentalsLayout::find_and_insert_octave(Accidental *acc, std::vector<Accidental *> &accidentals_placed,
                                               AccidentalsVectorView &accidentals_to_place, AccidentalsLayoutContext &ctx,
                                               bool accept_above, bool accept_below) {
    bool found_octave = false;
    int this_line = acc->line();
    const std::vector<Accidental *> &octaves = acc->ldata()->octaves.value();
    for (Accidental *octave_acc : octaves) {
        const bool removed = accidentals_to_place.remove_if([&](Accidental *a) {
            if (a == octave_acc) {
                int octave_line = octave_acc->line();
                bool accept_octave = (octave_line < this_line && accept_above) || (octave_line > this_line && accept_below);
                if (accept_octave) {
                    found_octave = true;
                    return true;
                }
            }
            return false;
        });

        if (removed) {
            accidentals_placed.push_back(octave_acc);

            if (ctx.keep_seconds_together()) {
                find_and_insert_second(octave_acc, accidentals_placed, accidentals_to_place, ctx);
            }
        }
    }

    return found_octave;
}

void AccidentalsLayout::apply_ordering_offsets(std::vector<Accidental *> &accidentals) {
    // Set default stacking number
    int stacking_number = 0;
    for (Accidental *acc : accidentals) {
        acc->mutldata()->stacking_number = stacking_number;
        ++stacking_number;
    }

    // Limit stacking offsets within boundaries
    for (Accidental *acc : accidentals) {
        int actual_stacking_order = acc->stacking_order();
        if (actual_stacking_order < 0) {
            acc->set_stacking_order_offset(acc->stacking_order_offset() - actual_stacking_order);
        } else if (actual_stacking_order >= stacking_number) {
            int diff = actual_stacking_order - stacking_number;
            acc->set_stacking_order_offset(acc->stacking_order_offset() - diff - 1);
        }
    }

    // Sort vector
    std::sort(accidentals.begin(), accidentals.end(), [](const Accidental *acc1, const Accidental *acc2) {
        int stacking_order1 = acc1->stacking_order();
        int stacking_order2 = acc2->stacking_order();
        return stacking_order1 == stacking_order2 ? acc1->stacking_order_offset() < acc2->stacking_order_offset() :
                                                    stacking_order1 < stacking_order2;
    });
}

double AccidentalsLayout::min_accidental_to_chord_distance(Accidental *acc, const Shape &acc_shape, const AccidentalsLayoutContext &ctx) {
    double dist = -DBL_MAX;
    for (const ShapeElement &accidental_element : acc_shape.elements()) {
        double acc_top = accidental_element.top();
        double acc_bottom = accidental_element.bottom();
        for (const ShapeElement &chord_element : ctx.chords_shape.elements()) {
            double chord_element_top = chord_element.top();
            double chord_element_bottom = chord_element.bottom();
            if (mu::engraving::intersects(acc_top, acc_bottom, chord_element_top, chord_element_bottom,
                                          ctx.vertical_accidental_to_chord_clearance())) {
                double padding = compute_padding(acc, chord_element.item(), ctx);
                padding += additional_padding_for_verticals(acc, chord_element.item(), ctx);
                dist = std::max(dist, accidental_element.right() - chord_element.left() + padding);
            }
            dist = std::max(dist, kerning_limitations_into_chord(acc, acc_shape, chord_element, ctx));
        }
    }

    return dist;
}

double AccidentalsLayout::kerning_limitations_into_chord(Accidental *acc, const Shape &acc_shape, const ShapeElement &chord_element,
                                                         const AccidentalsLayoutContext &ctx) {
    const EngravingItem *chord_item = chord_element.item();
    if (!chord_item) {
        return 0.0;
    }

    AccidentalType acc_type = acc->accidental_type();
    if (chord_item->is_ledger_line() && (acc_type == AccidentalType::NATURAL || acc_type == AccidentalType::SHARP)) {
        if (chord_item->y() > acc_shape.top() && chord_item->y() < acc_shape.bottom()) {
            return acc_shape.right() - chord_element.left() + ctx.sharp_and_natural_ledger_line_padding();
        }
    }

    return 0.0;
}

double AccidentalsLayout::compute_padding(Accidental *acc, const EngravingItem *chord_element, const AccidentalsLayoutContext &ctx) {
    AccidentalType acc_type = acc->accidental_type();
    bool is_flat = acc_type == AccidentalType::FLAT || acc_type == AccidentalType::FLAT2;

    bool kern_flat_into_ledger = is_flat && chord_element->is_ledger_line() && chord_element->y() > acc->note()->y();
    if (kern_flat_into_ledger) {
        return 0.0;
    }

    const double y_approx_threshold_for_sharp_ledger_kerning = 1.1 * ctx.spatium();
    bool kern_sharp_into_ledger = acc_type == AccidentalType::SHARP && chord_element->is_ledger_line() &&
                                  chord_element->y() > acc->note()->y() + y_approx_threshold_for_sharp_ledger_kerning;
    if (kern_sharp_into_ledger) {
        return 0.0;
    }

    bool kern_flat_into_note = is_flat && chord_element->is_note() && chord_element->y() > acc->note()->y();
    if (kern_flat_into_note) {
        return ctx.reduced_flat_to_note_padding();
    }

    const PaddingTable &padding_table = acc->score()->padding_tables().horizontal_padding_table();

    return padding_table.at(ElementType::ACCIDENTAL).at(chord_element->type()) * 0.5 * (acc->mag() + chord_element->mag());
}

double AccidentalsLayout::min_accidental_to_accidental_group_distance(Accidental *acc, Shape acc_shape, const Shape &accidentals_shape,
                                                                      const AccidentalsLayoutContext &ctx) {
    double dist = 0.0;
    int column = acc->ldata()->column;

    bool collision_found;
    int iter = 0;
    static constexpr int safety_net = 50;
    do {
        ++iter;
        double cur_dist = 0.0;
        collision_found = false;
        for (const ShapeElement &group_element : accidentals_shape.elements()) {
            const Accidental *acc2 = to_accidental(group_element.item());

            if (is_exception_of_fourth(acc, acc2)) {
                double dist_fourth = kerning_of_fourth(acc_shape, group_element, ctx);
                if (!muse::real_is_null(dist_fourth) && dist_fourth > cur_dist) {
                    collision_found = true;
                    cur_dist = dist_fourth;
                }
                column = std::max(column, acc2->ldata()->column + 1);
                continue;
            }

            double vert_padding = vertical_padding(acc, acc2, ctx);
            double hor_padding = horizontal_padding(acc, acc2, ctx);
            // If the tolerance for detecting intersections is the same as the padding,
            // we may randomly detect wrong intersections just because of rounding errors
            double hor_padding_with_tolerance = 0.999 * hor_padding;
            if (acc_shape.intersects(
                    group_element.adjusted(-hor_padding_with_tolerance, -vert_padding, hor_padding_with_tolerance, vert_padding))) {
                collision_found = true;
            } else {
                continue;
            }

            if (!is_exception_of_naturals_sixth(acc, acc2)) {
                column = std::max(column, acc2->ldata()->column + 1);
            }

            double group_top = group_element.top();
            double group_bottom = group_element.bottom();
            for (ShapeElement &acc_element : acc_shape.elements()) {
                if (mu::engraving::intersects(acc_element.top(), acc_element.bottom(), group_top, group_bottom, vert_padding)) {
                    double min_dist = acc_element.right() - group_element.left() + hor_padding;
                    min_dist += additional_padding_for_verticals(acc, acc2, ctx);
                    cur_dist = std::max(cur_dist, min_dist);
                }
            }
        }

        acc_shape.translate_x(-cur_dist);
        dist += cur_dist;
    } while (collision_found && iter < safety_net);

    IF_ASSERT_FAILED(iter < safety_net) { LOGE() << "Accidental layout error."; }

    acc->mutldata()->column = column;

    return dist;
}

bool AccidentalsLayout::is_exception_of_fourth(const Accidental *acc1, const Accidental *acc2) {
    if (!acc1 || !acc2) {
        return false;
    }

    if ((acc1->bracket() == AccidentalBracket::BRACKET || acc2->bracket() == AccidentalBracket::BRACKET) && acc1->mag() == 1 &&
        acc2->mag() == 1) {
        return false;
    }

    AccidentalType acc_type1 = acc1->accidental_type();
    AccidentalType acc_type2 = acc2->accidental_type();

    return (acc_type1 == AccidentalType::FLAT || acc_type1 == AccidentalType::FLAT2) &&
           (acc_type2 == AccidentalType::FLAT || acc_type2 == AccidentalType::FLAT2 || acc_type2 == AccidentalType::NATURAL) &&
           acc1->line() - acc2->line() == 3;
}

bool AccidentalsLayout::is_exception_of_naturals_sixth(const Accidental *acc1, const Accidental *acc2) {
    return acc1->accidental_type() == AccidentalType::NATURAL && acc2->accidental_type() == AccidentalType::NATURAL &&
           abs(acc2->line() - acc1->line()) == 5;
}

bool AccidentalsLayout::can_fit_in_same_column(const Accidental *acc1, const Accidental *acc2, AccidentalsLayoutContext &ctx) {
    if (is_exception_of_naturals_sixth(acc1, acc2)) {
        return true;
    }

    RectF acc1_box = acc1->ldata()->bbox().translated(PointF(0.0, acc1->note()->y()));
    RectF acc2_box = acc2->ldata()->bbox().translated(PointF(0.0, acc2->note()->y()));

    return !mu::engraving::intersects(acc1_box.top(), acc1_box.bottom(), acc2_box.top(), acc2_box.bottom(),
                                      vertical_padding(acc1, acc2, ctx));
}

bool AccidentalsLayout::is_octave(const Accidental *acc1, const Accidental *acc2) {
    return acc1->accidental_type() == acc2->accidental_type() && std::abs(acc2->line() - acc1->line()) % 7 == 0;
}

double AccidentalsLayout::kerning_of_fourth(const Shape &acc_shape, const ShapeElement &acc_group_shape_element,
                                            const AccidentalsLayoutContext &ctx) {
    AccidentalType acc_type2 = to_accidental(acc_group_shape_element.item())->accidental_type();

    return acc_shape.right() - acc_group_shape_element.left() +
           (acc_type2 == AccidentalType::NATURAL ? ctx.natural_kerning_of_fourth() : ctx.flat_kerning_of_fourth());
}

double AccidentalsLayout::additional_padding_for_verticals(const Accidental *acc, const EngravingItem *item,
                                                           const AccidentalsLayoutContext &ctx) {
    if (acc->accidental_type() != AccidentalType::NATURAL || acc->bracket() != AccidentalBracket::NONE ||
        (!item->is_accidental() && !item->is_stem())) {
        return 0.0;
    }

    if (item->is_accidental()) {
        const Accidental *acc2 = to_accidental(item);
        AccidentalType acc_type2 = acc2->accidental_type();
        if (acc_type2 != AccidentalType::NATURAL && acc_type2 != AccidentalType::FLAT && acc_type2 != AccidentalType::FLAT2) {
            return 0.0;
        }
        int line_diff = acc2->line() - acc->line();
        if (line_diff < -2 || line_diff > 5) {
            return 0.0;
        }
    }

    return ctx.additional_padding_for_verticals();
}

void AccidentalsLayout::vertically_align_accidentals(AccidentalsLayoutContext &ctx) {
    std::set<Accidental *> accidentals_already_grouped;
    std::map<Accidental *, std::vector<Accidental *>> vertical_sets;

    if (ctx.align_offset_octaves()) {
        collect_vertical_sets_of_offset_octaves(accidentals_already_grouped, vertical_sets, ctx);
    }

    collect_vertical_sets(accidentals_already_grouped, vertical_sets, ctx);

    if (vertical_sets.empty()) {
        return;
    }

    // Turn map into vector
    AccidentalGroups vert_sets;
    vert_sets.reserve(vertical_sets.size());
    for (auto &pair : vertical_sets) {
        vert_sets.push_back(pair.second);
    }

    // Sort vertical sets by column
    std::sort(vert_sets.begin(), vert_sets.end(),
              [&](auto &set1, auto &set2) { return set1.front()->ldata()->column < set2.front()->ldata()->column; });

    align_vertical_sets(vert_sets, ctx);
}

void AccidentalsLayout::collect_vertical_sets_of_offset_octaves(std::set<Accidental *> &accidentals_already_grouped,
                                                                std::map<Accidental *, std::vector<Accidental *>> &vertical_sets,
                                                                AccidentalsLayoutContext &ctx) {
    for (std::vector<Accidental *> &accidental_group : ctx.accidental_sub_chords) {
        for (size_t i = 0; i < accidental_group.size(); ++i) {
            Accidental *acc1 = accidental_group[i];
            acc1->ldata()->column.value();
            if (muse::contains(accidentals_already_grouped, acc1)) {
                continue;
            }
            double x1 = x_pos_relative_to_segment(acc1);
            for (size_t j = i + 1; j < accidental_group.size(); ++j) {
                Accidental *acc2 = accidental_group[j];
                if (!is_octave(acc1, acc2) || muse::contains(accidentals_already_grouped, acc2)) {
                    continue;
                }
                double x2 = x_pos_relative_to_segment(acc2);
                bool align_octave = abs(x1 - x2) < 2.0 * ctx.spatium();
                acc2->ldata()->column.value();
                if (align_octave) {
                    std::vector<Accidental *> &vertical_set = vertical_sets[acc1];
                    if (vertical_set.empty()) {
                        vertical_set.push_back(acc1);
                    }
                    vertical_set.push_back(acc2);
                    accidentals_already_grouped.insert(acc1);
                    accidentals_already_grouped.insert(acc2);
                    int max_column = 0;
                    for (Accidental *acc : vertical_set) {
                        max_column = std::max(max_column, acc->ldata()->column.value());
                    }
                    for (Accidental *acc : vertical_set) {
                        acc->mutldata()->column = max_column;
                    }
                }
            }
        }
    }
}

void AccidentalsLayout::collect_vertical_sets(std::set<Accidental *> &accidentals_already_grouped,
                                              std::map<Accidental *, std::vector<Accidental *>> &vertical_sets,
                                              AccidentalsLayoutContext &ctx) {
    for (size_t i = 0; i < ctx.all_accidentals.size(); ++i) {
        Accidental *acc1 = ctx.all_accidentals[i];
        if (muse::contains(accidentals_already_grouped, acc1)) {
            continue;
        }
        int column1 = acc1->ldata()->column;
        int vertical_sub1 = acc1->ldata()->vertical_subgroup;
        double x1 = x_pos_relative_to_segment(acc1);
        for (size_t j = i + 1; j < ctx.all_accidentals.size(); ++j) {
            Accidental *acc2 = ctx.all_accidentals[j];
            if (acc2->accidental_type() != acc1->accidental_type() || muse::contains(accidentals_already_grouped, acc2) ||
                !muse::real_is_equal(acc1->mag(), acc2->mag())) {
                continue;
            }
            int column2 = acc2->ldata()->column;
            int vertical_sub2 = acc2->ldata()->vertical_subgroup;
            double x2 = x_pos_relative_to_segment(acc2);
            bool align_accidentals =
                column1 == column2 && vertical_sub1 == vertical_sub2 && abs(x1 - x2) < ctx.x_vertical_alignment_threshold();
            if (align_accidentals) {
                std::vector<Accidental *> &vertical_set = vertical_sets[acc1];
                if (vertical_set.empty()) {
                    vertical_set.push_back(acc1);
                }
                vertical_set.push_back(acc2);
                accidentals_already_grouped.insert(acc1);
                accidentals_already_grouped.insert(acc2);
            }
        }
    }
}

void AccidentalsLayout::align_vertical_sets(AccidentalGroups &vert_sets, AccidentalsLayoutContext &ctx) {
    AccidentalGroups accidental_columns;
    for (std::vector<Accidental *> &group : ctx.accidental_sub_chords) {
        for (Accidental *acc : group) {
            size_t column = acc->ldata()->column;
            while (accidental_columns.size() <= column) {
                accidental_columns.push_back(std::vector<Accidental *>());
            }
            accidental_columns[column].push_back(acc);
        }
    }

    for (std::vector<Accidental *> &vert_set : vert_sets) {
        // Align the set
        double x = DBL_MAX;
        for (Accidental *acc : vert_set) {
            double right_edge = acc->ldata()->bbox().right() + x_pos_relative_to_segment(acc);
            x = std::min(x, right_edge);
        }
        for (Accidental *acc : vert_set) {
            double bbox_right = acc->ldata()->bbox().right();
            set_xpos_relative_to_segment(acc, x - bbox_right);
        }

        // Re-check the outer ones for collisions
        size_t cur_column = vert_set.front()->ldata()->column.value();
        Shape accidental_group_shape;
        for (size_t column = 0; column < accidental_columns.size(); ++column) {
            if (column < cur_column) {
                continue;
            }
            for (Accidental *acc : accidental_columns[column]) {
                double cur_x_pos = x_pos_relative_to_segment(acc);
                Shape acc_shape = acc->shape().translate(PointF(cur_x_pos, acc->note()->y()));
                if (!muse::contains(vert_set, acc)) {
                    double min_dist_to_accid_group =
                        min_accidental_to_accidental_group_distance(acc, acc_shape, accidental_group_shape, ctx);
                    acc_shape.translate_x(-min_dist_to_accid_group);
                    set_xpos_relative_to_segment(acc, cur_x_pos - min_dist_to_accid_group);
                }
                accidental_group_shape.add(acc_shape);
            }
        }
    }
}

void AccidentalsLayout::set_xpos_relative_to_segment(Accidental *accidental, double x) {
    Note *note = accidental->note();
    Chord *chord = note ? note->chord() : nullptr;
    if (note) {
        x -= note->pos().x();
    }
    if (chord && !keep_accidentals_close_to_chord(chord)) {
        x -= chord->pos().x();
    }
    accidental->mutldata()->set_pos_x(x);
}

double AccidentalsLayout::x_pos_relative_to_segment(const Accidental *accidental) {
    Note *note = accidental->note();
    Chord *chord = note ? note->chord() : nullptr;
    double x = accidental->ldata()->pos().x();
    if (note) {
        x += note->pos().x();
    }
    if (chord && !keep_accidentals_close_to_chord(chord)) {
        x += chord->pos().x();
    }
    return x;
}

void AccidentalsLayout::sort_top_down(std::vector<Accidental *> &accidentals) {
    std::sort(accidentals.begin(), accidentals.end(), [](const Accidental *acc1, const Accidental *acc2) {
        int line1 = acc1->line();
        int line2 = acc2->line();
        if (line1 == line2) {
            Note *note1 = acc1->note();
            Note *note2 = acc2->note();
            return note1->x() + note1->chord()->x() > note2->x() + note2->chord()->x();
        }
        return line1 < line2;
    });
}

bool AccidentalsLayout::keep_accidentals_close_to_chord(const Chord *chord) {
    return chord->is_trill_cue_note() || chord->is_grace_after();
}

double AccidentalsLayout::vertical_padding(const Accidental *acc1, const Accidental *acc2, const AccidentalsLayoutContext &ctx) {
    double vertical_clearance = acc1->accidental_type() == AccidentalType::SHARP && acc2->accidental_type() == AccidentalType::SHARP ?
                                    ctx.vertical_sharp_to_sharp_clearance() :
                                    ctx.vertical_accidental_to_accidental_clearance();
    return vertical_clearance *= 0.5 * (acc1->mag() + acc2->mag());
}

double AccidentalsLayout::horizontal_padding(const Accidental *acc1, const Accidental *acc2, const AccidentalsLayoutContext &ctx) {
    return ctx.accidental_accidental_distance() * (0.5 * (acc1->mag() + acc2->mag()));
}
