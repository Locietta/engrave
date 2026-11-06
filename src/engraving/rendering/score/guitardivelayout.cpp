/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
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
#include "guitardivelayout.h"

#include "dom/guitarbend.h"
#include "dom/note.h"
#include "dom/staff.h"
#include "dom/stafftype.h"
#include "dom/system.h"
#include "dom/whammybar.h"
#include "horizontalspacing.h"
#include "textlayout.h"
#include "tlayout.h"

using namespace mu::engraving;
using namespace muse::draw;
using namespace mu::engraving::rendering::score;

void GuitarDiveLayout::update_dive_sequences(const std::vector<GuitarBend *> &bends, const LayoutContext &ctx) {
    std::vector<GuitarBend *> dives;
    for (GuitarBend *bend : bends) {
        if ((bend->bend_type() == GuitarBendType::DIVE || bend->bend_type() == GuitarBendType::PRE_DIVE) &&
            bend->staff_type()->is_tab_staff()) {
            dives.push_back(bend);
        }
    }

    for (GuitarBend *dive : dives) {
        dive->compute_bend_amount();
        dive->mutldata()->reset_dive_levels();
    }

    for (auto riter = dives.rbegin(); riter != dives.rend(); ++riter) {
        GuitarBend *dive = *riter;
        if (!dive->ldata()->dive_levels().empty()) {
            continue;
        }

        // On-staff vs above-staff layout
        DirectionV tab_pos = dive->dive_tab_pos();

        std::set<int> dive_levels;
        std::unordered_set<GuitarBend *> dive_sequence;

        dive_levels.insert(dive->tot_bend_amount_including_preceding_bends());
        dive_sequence.insert(dive);

        // Consider dives part of the same sequence if they are directly connected
        // or if they are spanned by the same whammy bar line

        if (WhammyBar *whammy_bar = dive->find_overlapping_whammy_bar(dive->tick(), dive->tick2())) {
            for (auto riter2 = riter + 1; riter2 != dives.rend(); ++riter2) {
                GuitarBend *earlier_dive = *riter2;
                if (earlier_dive->tick() >= whammy_bar->tick()) {
                    dive_levels.insert(earlier_dive->tot_bend_amount_including_preceding_bends());
                    dive_sequence.insert(earlier_dive);
                    if (earlier_dive->dive_tab_pos() != DirectionV::AUTO) {
                        tab_pos = earlier_dive->dive_tab_pos();
                    }
                }
            }
        }

        GuitarBend *prev_dive = dive->find_preceding_bend();
        while (prev_dive) {
            dive_levels.insert(prev_dive->tot_bend_amount_including_preceding_bends());
            dive_sequence.insert(prev_dive);
            if (prev_dive->dive_tab_pos() != DirectionV::AUTO) {
                tab_pos = prev_dive->dive_tab_pos();
            }
            prev_dive = prev_dive->find_preceding_bend();
        }

        bool layout_above_staff =
            tab_pos == DirectionV::UP || (tab_pos == DirectionV::AUTO && ctx.conf().style_b(Sid::GUITAR_DIVES_ABOVE_STAFF));

        for (GuitarBend *d : dive_sequence) {
            d->mutldata()->set_dive_levels(dive_levels);
            d->mutldata()->set_above_staff(layout_above_staff);
        }
    }
}

void GuitarDiveLayout::layout_dive_tab_staff(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();
    if (bend->bend_type() == GuitarBendType::DIP) {
        layout_dip(item, ctx);
        return;
    }
    if (bend->bend_type() == GuitarBendType::SCOOP) {
        layout_scoop(item);
        return;
    }

    GuitarBendSegment::LayoutData *ldata = item->mutldata();

    Note *start_note = bend->start_note();
    Note *end_note = bend->end_note();
    if (!start_note || !end_note) {
        return;
    }

    Chord *start_chord = start_note->chord();
    Chord *end_chord = end_note->chord();
    if (start_note->string() != start_chord->up_string() || end_note->string() != end_chord->up_string()) {
        return; // On TAB only one dive line can exist and only from the top note
    }

    if (bend->bend_type() == GuitarBendType::PRE_DIVE) {
        bool align_to_grace = ctx.conf().style_b(Sid::ALIGN_PRE_BEND_AND_PRE_DIVE_TO_GRACE_NOTE);
        for (Note *note : start_chord->notes()) {
            if (!note->override_bend_visibility_rules()) {
                note->set_visible(align_to_grace);
            }
        }
        for (Note *note : end_chord->notes()) {
            if (!note->override_bend_visibility_rules()) {
                note->set_visible(!align_to_grace);
            }
        }
    } else if (bend->is_full_release_dive() || item->is_end_type()) {
        for (Note *note : end_chord->notes()) {
            if (!note->override_bend_visibility_rules()) {
                note->set_visible(true);
            }
            note->set_ghost(true);
            note->mutldata()->reset();
        }
        TLayout::layout_chord(end_note->chord(), ctx);
    } else if (!bend->is_full_release_dive() || !ctx.conf().style_b(Sid::SHOW_FRET_ON_FULL_BEND_RELEASE)) {
        for (Note *note : end_chord->notes()) {
            if (!note->override_bend_visibility_rules()) {
                note->set_visible(false);
            }
        }
    }

    bool above_staff = bend->ldata()->above_staff();
    if (above_staff) {
        item->set_pos(compute_start_pos_above_staff(item, ctx));
        item->set_pos2(compute_end_pos_above_staff(item, ctx) - item->ldata()->pos());
    } else {
        item->set_pos(compute_start_pos_on_staff(item, ctx));
        item->set_pos2(compute_end_pos_on_staff(item, ctx) - item->ldata()->pos());
    }

    GuitarBendText *bend_text = item->bend_text();
    if (item->is_single_end_type()) {
        bend_text->set_xml_text(bend->ldata()->bend_digit());
        TextLayout::layout_base_text_base(bend_text, ctx);
        double vertical_text_pad = 0.35 * item->spatium();
        PointF bend_text_pos = item->pos2();
        if (above_staff) {
            if (bend->bend_type() == GuitarBendType::PRE_DIVE && bend->bend_amount_in_quarter_tones() < 0) {
                bend_text_pos = PointF(); // If predive is downwards the text must be above the start
            }
            bend_text_pos += PointF(-0.5 * bend_text->width(), -bend_text->height() - vertical_text_pad);
        } else {
            bend_text_pos += PointF(-0.5 * bend_text->width(),
                                    item->pos().y() + item->pos2().y() > 0 ? vertical_text_pad : -bend_text->height() - vertical_text_pad);
        }
        bend_text->set_pos(bend_text_pos);
    } else {
        bend_text->mutldata()->reset();
    }

    PainterPath path;
    if (bend->is_slack()) {
        const double slack_line_len = 2 * item->spatium();
        PointF vertex_point = item->pos2() - PointF(0.5 * slack_line_len, 0.0);
        path.line_to(vertex_point);
        item->set_pos2(item->pos2() + PointF(0.5 * slack_line_len, 0.0));
    }
    path.line_to(item->pos2());
    ldata->set_path(path);

    const double line_width = item->line_width();
    RectF r = RectF(PointF(), item->pos2()).normalized();
    if (muse::real_is_null(r.height())) {
        r.set_height(line_width);
    }
    if (muse::real_is_null(r.width())) {
        r.set_width(line_width);
    }
    item->mutldata()->set_bbox(r);
}

PointF GuitarDiveLayout::compute_start_pos_on_staff(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();

    if (bend->bend_type() == GuitarBendType::PRE_DIVE) {
        Note *note = ctx.conf().style_b(Sid::ALIGN_PRE_BEND_AND_PRE_DIVE_TO_GRACE_NOTE) ? bend->start_note() : bend->end_note();
        if (bend->bend_amount_in_quarter_tones() > 0) {
            note = note->chord()->up_note();
        } else {
            note = note->chord()->down_note();
        }
        PointF note_pos = note->system_pos();
        RectF note_bbox = get_note_and_parentheses_shape(note);
        double vertical_padding = 0.25 * item->spatium();
        note_pos += PointF(0.5 * note_bbox.width(), bend->bend_amount_in_quarter_tones() > 0 ? note_bbox.top() - vertical_padding :
                                                                                               note_bbox.bottom() + vertical_padding);
        return note_pos;
    }

    LineSegment *prev_segment = find_prev_hold_or_bend_segment(item, /*excludeFullReleaseDive*/ true);
    if (prev_segment) {
        return prev_segment->pos() + prev_segment->pos2();
    }

    PointF start_pos;
    Note *start_note = bend->start_note();
    RectF start_note_bbox = get_note_and_parentheses_shape(start_note);
    if (item->is_single_begin_type()) {
        start_pos = start_note->system_pos();
        double horizontal_indent = 0.25 * item->spatium();
        start_pos += PointF(start_note_bbox.right() + horizontal_indent, -0.5 * start_note_bbox.height() + 0.5 * item->line_width());
    } else {
        start_pos.set_x(item->system()->first_note_rest_segment_x(true));
        start_pos.set_y(bend->front_segment()->ldata()->pos().y() + bend->front_segment()->ipos2().y());
    }

    return start_pos;
}

PointF GuitarDiveLayout::compute_end_pos_on_staff(GuitarBendSegment *item, LayoutContext &) {
    GuitarBend *bend = item->guitar_bend();
    double spatium = item->spatium();

    double x = 0.0;
    if (item->is_single_end_type()) {
        Note *end_note = bend->end_note();
        PointF end_note_pos = end_note->system_pos();
        RectF end_note_bbox = get_note_and_parentheses_shape(end_note);

        if (bend->is_full_release_dive()) {
            x = end_note_pos.x() + end_note_bbox.left() - 0.25 * spatium;
            double y = end_note_pos.y() + end_note_bbox.top() + 0.5 * item->line_width();
            return PointF(x, y);
        }

        x = bend->bend_type() == GuitarBendType::PRE_DIVE ? item->pos().x() : end_note_pos.x() + 0.5 * end_note_bbox.width();
    } else {
        x = item->system()->ending_x_for_open_ended_lines();

        if (bend->is_full_release_dive()) {
            /* Full release dives end at the same y as they started, so we can use the startNote's position
             * here in the case that the segment is BEGIN type (bc its endNote hasn't been laid out yet) */
            Note *start_note = bend->start_note();
            PointF start_note_pos = start_note->system_pos();
            RectF start_note_bbox = get_note_and_parentheses_shape(start_note);
            double y = start_note_pos.y() + start_note_bbox.top() + 0.5 * item->line_width();
            return PointF(x, y);
        }
    }

    if (bend->bend_type() == GuitarBendType::PRE_DIVE) {
        LineSegment *prev_segment = find_prev_hold_or_bend_segment(item);
        if (prev_segment && prev_segment->is_guitar_bend_hold_segment()) {
            double y = prev_segment->pos().y() + prev_segment->pos2().y();
            return PointF(x, y);
        }
    }

    int dive_level = bend->tot_bend_amount_including_preceding_bends();
    bool above_staff = dive_level > 0;

    const StaffType *staff_type = item->staff_type();
    IF_ASSERT_FAILED(staff_type) { return PointF(x, 0.0); }
    int staff_lines = staff_type->lines();
    double line_dist = item->absolute_from_spatium(staff_type->line_distance());

    if (!item->is_single_end_type()) {
        double y = above_staff ? -line_dist : staff_lines * line_dist;
        return PointF(x, y);
    }

    const std::set<int> &dive_levels = bend->ldata()->dive_levels();
    std::deque<int> dive_levels_in_this_direction;
    for (int level : dive_levels) {
        if (above_staff && level > 0) {
            dive_levels_in_this_direction.push_back(level);
        } else if (!above_staff && level < 0) {
            dive_levels_in_this_direction.push_front(level);
        }
    }

    size_t level_number = muse::index_of(dive_levels_in_this_direction, dive_level);
    IF_ASSERT_FAILED(level_number != muse::nidx) { return PointF(x, 0.0); }

    double zero_dive_level = above_staff ? -1.5 * line_dist : line_dist * (staff_lines + 0.5);
    double increment = spatium;

    double y = zero_dive_level + (above_staff ? -1 : 1) * (increment * level_number);

    return PointF(x, y);
}

PointF GuitarDiveLayout::compute_start_pos_above_staff(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();

    LineSegment *prev_segment = find_prev_hold_or_bend_segment(item);
    if (prev_segment) {
        return prev_segment->pos() + prev_segment->pos2();
    }

    PointF start_pos;

    if (item->is_single_begin_type()) {
        Note *note = bend->bend_type() == GuitarBendType::PRE_BEND && !ctx.conf().style_b(Sid::ALIGN_PRE_BEND_AND_PRE_DIVE_TO_GRACE_NOTE) ?
                         bend->end_note() :
                         bend->start_note();
        RectF note_bbox = get_note_and_parentheses_shape(note);
        start_pos.set_x(note->system_pos().x() + 0.5 * note_bbox.width());

        const StaffType *staff_type = item->staff_type();
        IF_ASSERT_FAILED(staff_type) { start_pos.set_y(0.0); }
        else {
            double spatium = item->spatium();
            double line_dist = item->absolute_from_spatium(staff_type->line_distance());
            const double base_line = -1.5 * line_dist;
            const double increment = spatium;
            std::set<int> dive_levels = bend->mutldata()->dive_levels();
            dive_levels.insert(0); // Because for dives above the staff we need the zero level too
            size_t zero_level = muse::index_of(dive_levels, 0);
            double tot_increment = increment * zero_level;
            start_pos.set_y(base_line - tot_increment);
        }
    } else {
        start_pos.set_x(item->system()->first_note_rest_segment_x(true));
        start_pos.set_y(bend->front_segment()->ldata()->pos().y());
    }

    return start_pos;
}

PointF GuitarDiveLayout::compute_end_pos_above_staff(GuitarBendSegment *item, LayoutContext &) {
    GuitarBend *bend = item->guitar_bend();
    double spatium = item->spatium();

    double x = 0.0;
    if (item->is_single_end_type()) {
        Note *end_note = bend->end_note();
        PointF end_note_pos = end_note->system_pos();
        RectF end_note_bbox = get_note_and_parentheses_shape(end_note);
        x = bend->bend_type() == GuitarBendType::PRE_DIVE ? item->pos().x() : end_note_pos.x() + 0.5 * end_note_bbox.width();
    } else {
        x = item->system()->ending_x_for_open_ended_lines();
    }

    if (bend->bend_type() == GuitarBendType::PRE_DIVE) {
        LineSegment *prev_segment = find_prev_hold_or_bend_segment(item);
        if (prev_segment && prev_segment->is_guitar_bend_hold_segment()) {
            double y = prev_segment->pos().y() + prev_segment->pos2().y();
            return PointF(x, y);
        }
    }

    int ending_level = bend->tot_bend_amount_including_preceding_bends();
    int starting_level = ending_level - bend->bend_amount_in_quarter_tones();

    std::set<int> dive_levels = bend->ldata()->dive_levels();
    dive_levels.insert(0); // Because for dives above the staff we need the zero level too

    size_t ending_idx = muse::index_of(dive_levels, ending_level);
    size_t starting_idx = muse::index_of(dive_levels, starting_level);
    IF_ASSERT_FAILED(ending_idx != muse::nidx && starting_idx != muse::nidx && ending_idx != starting_idx) { return PointF(x, 0.0); }

    int steps = static_cast<int>(ending_idx) - static_cast<int>(starting_idx);
    double increment = spatium;

    double y = item->ldata()->pos().y() - steps * increment;

    return PointF(x, y);
}

LineSegment *GuitarDiveLayout::find_prev_hold_or_bend_segment(GuitarBendSegment *item, bool exclude_full_release_dive) {
    GuitarBend *bend = item->guitar_bend();
    GuitarBend *prev_bend = item->guitar_bend()->find_preceding_bend();
    if (!prev_bend) {
        return nullptr;
    }

    GuitarBendHold *hold = prev_bend->hold_line();
    if (hold && !hold->segments_empty() && prev_bend->end_note() != bend->start_note() &&
        hold->back_segment()->system() == item->system()) {
        return hold->back_segment();
    }

    if (!prev_bend->segments_empty() && !(exclude_full_release_dive && prev_bend->is_full_release_dive()) &&
        prev_bend->back_segment()->system() == item->system()) {
        return prev_bend->back_segment();
    }

    return nullptr;
}

void GuitarDiveLayout::layout_dip(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();
    Note *start_note = bend->start_note();
    IF_ASSERT_FAILED(start_note) { return; }

    PointF note_pos = start_note->system_pos();
    double center_x = note_pos.x() + 0.5 * start_note->width();
    double spatium = item->spatium();
    double base_y = std::min(-spatium, note_pos.y() - 0.5 * start_note->height() - spatium);

    const double v_height = 2 * spatium;
    const double v_width = 2 * spatium;

    bool upwards = bend->bend_amount_in_quarter_tones() > 0;
    PointF start = PointF(center_x - 0.5 * v_width, upwards ? base_y : base_y - v_height);
    PointF vertex = PointF(center_x, upwards ? base_y - v_height : base_y) - start;
    PointF end = PointF(center_x + 0.5 * v_width, upwards ? base_y : base_y - v_height) - start;

    item->set_pos(start);
    item->set_pos2(end);

    PainterPath path;
    path.line_to(vertex);
    path.line_to(end);
    item->mutldata()->set_path(path);

    RectF bbox = RectF(PointF(), vertex).normalized().united(RectF(vertex, end).normalized());
    item->mutldata()->set_bbox(bbox);

    GuitarBendText *bend_text = item->bend_text();
    bend_text->set_xml_text(bend->ldata()->bend_digit());
    TextLayout::layout_base_text_base(bend_text, ctx);
    double vertical_text_pad = 0.35 * item->spatium();
    bend_text->set_pos(PointF(vertex.x() - 0.5 * bend_text->width(), bbox.top() - vertical_text_pad - bend_text->height()));
}

void GuitarDiveLayout::layout_scoop(GuitarBendSegment *item) {
    GuitarBend *bend = item->guitar_bend();
    Note *start_note = bend->start_note();
    IF_ASSERT_FAILED(start_note) { return; }

    const SymId scoop_sym = SymId::GUITAR_VIBRATO_BAR_SCOOP;
    RectF bbox = item->sym_bbox(scoop_sym);
    item->setbbox(bbox);

    PointF pos = start_note->system_pos();
    const double spatium = item->spatium();
    const double left_pad = 0.25 * spatium;
    const double down_pad = 0.35 * spatium;

    double y_move = bbox.height();
    if (item->staff_type() && item->staff_type()->is_tab_staff()) {
        y_move -= 0.5 * start_note->height();
    } else {
        y_move += start_note->line() % 2 ? 0.0 : down_pad;
    }

    Shape chord_shape = start_note->chord()->shape();
    chord_shape.remove_types({ElementType::GUITAR_BEND, ElementType::GUITAR_BEND_SEGMENT, ElementType::BEAM});

    double x_move =
        -HorizontalSpacing::min_horizontal_distance(Shape(bbox.translated(0.0, start_note->y())), chord_shape, spatium) - left_pad;

    start_note->chord()->segment()->staff_shape(start_note->staff_idx()).add(bbox.translated(x_move, y_move + start_note->y()));

    item->set_pos(pos + PointF(x_move, y_move));
    item->set_pos2(PointF());
}

RectF GuitarDiveLayout::get_note_and_parentheses_shape(const Note *note) {
    const NoteParenthesisInfo *paren_info = note->parenthesis_info();
    RectF note_bbox = note->ldata()->bbox();

    if (!paren_info) {
        return note_bbox;
    }

    const Parenthesis *left_paren = paren_info->left_paren();
    const Parenthesis *right_paren = paren_info->right_paren();

    note_bbox.unite(left_paren->ldata()->bbox().translated(left_paren->pos() - note->pos()));
    note_bbox.unite(right_paren->ldata()->bbox().translated(right_paren->pos() - note->pos()));

    return note_bbox;
}
