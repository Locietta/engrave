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

#include "tlayout.h"
#include "textlayout.h"
#include "guitarbendlayout.h"
#include "guitardivelayout.h"

#include "../../dom/chord.h"
#include "../../dom/guitarbend.h"
#include "../../dom/measure.h"
#include "../../dom/note.h"
#include "../../dom/score.h"
#include "../../dom/segment.h"
#include "../../dom/staff.h"
#include "../../dom/system.h"

using namespace mu::engraving;
using namespace muse::draw;
using namespace mu::engraving::rendering::score;

void GuitarBendLayout::update_segments_and_layout(SLine *item, LayoutContext &ctx, System *system) {
    Note *start_note = to_note(item->start_element());
    Note *end_note = to_note(item->end_element());
    if (!start_note || !end_note) {
        return;
    }

    System *system1 = start_note->find_measure()->system();
    System *system2 = end_note->find_measure()->system();

    if (!system1) { // system2 may be null if the end note's system hasn't been laid out yet
        return;
    }

    item->set_tick(start_note->tick());
    item->set_tick2(end_note->tick());

    unsigned int segments_needed = system1 == system2 ? 1 : 2;
    size_t segment_count = item->spanner_segments().size();
    if (segment_count != segments_needed) {
        item->fixup_segments(segments_needed, [item]() { return item->create_line_segment(); });
    }

    item->front_segment()->move_to_system(system1);
    if (segments_needed == 1) {
        item->front_segment()->set_spanner_segment_type(SpannerSegmentType::SINGLE);
    } else {
        item->front_segment()->set_spanner_segment_type(SpannerSegmentType::BEGIN);
        item->back_segment()->move_to_system(system2);
        item->back_segment()->set_spanner_segment_type(SpannerSegmentType::END);
    }

    for (SpannerSegment *segment : item->spanner_segments()) {
        if (!segment->system() || (system && segment->system() != system)) {
            /* this segment belongs to a system which either has already been
             * laid out or will be laid out in a later pass */
            continue;
        }
        segment->set_track(item->track());
        TLayout::layout_line_segment(to_line_segment(segment), ctx);
    }

    TLayout::fill_note_shape(start_note, start_note->mutldata());
}

void GuitarBendLayout::layout_standard_staff(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();

    if (bend->bend_type() == GuitarBendType::DIP) {
        GuitarDiveLayout::layout_dip(item, ctx);
    } else if (bend->bend_type() == GuitarBendType::SCOOP) {
        GuitarDiveLayout::layout_scoop(item);
    } else if (bend->bend_type() == GuitarBendType::SLIGHT_BEND) {
        layout_slight_bend(item, ctx);
    } else {
        layout_angular_bend(item, ctx);
    }
}

void GuitarBendLayout::layout_angular_bend(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();

    compute_up(bend);
    compute_is_inside(bend);

    Note *start_note = bend->start_note();
    Note *end_note = bend->end_note();
    if (!start_note || !end_note) {
        return;
    }

    if ((bend->bend_type() == GuitarBendType::PRE_BEND || bend->bend_type() == GuitarBendType::PRE_DIVE) &&
        !start_note->parenthesis_info()) {
        start_note->set_ghost(true);
        start_note->mutldata()->reset();
        TLayout::layout_chord(start_note->chord(), ctx);
    }

    System *start_system = start_note->chord()->segment()->system();
    if (!start_system) {
        return;
    }

    const bool up = bend->ldata()->up();
    const int up_sign = up ? -1 : 1;
    const bool is_inside = bend->ldata()->is_inside();
    const double spatium = item->spatium();
    const double vertical_padding = 0.2 * spatium;
    const double horizontal_indent = 0.2 * spatium;
    double height_diff = end_note->y() - start_note->y();

    PointF start_pos;
    PointF end_pos;

    if (item->is_single_begin_type()) {
        Chord *start_chord = start_note->chord();
        PointF start_note_pos = start_note->pos() + start_chord->pos() + start_chord->segment()->pos() + start_chord->measure()->pos();
        start_pos = start_note_pos;

        double x_off = (is_inside ? 1 : 0.5) * start_note->width() + horizontal_indent * start_note->mag();
        double y_off = up_sign * vertical_padding * start_note->mag();
        if (!is_inside) {
            y_off += up_sign * 0.5 * start_note->height();
        }

        start_pos += PointF(x_off, y_off);
    } else {
        System *end_system = end_note->chord()->segment()->system();
        if (!end_system) {
            return;
        }
        double x = end_system->first_note_rest_segment_x(true);
        double y = end_note->y() + up_sign * 0.5 * end_note->height() - 0.5 * height_diff;
        start_pos = PointF(x, y);
    }

    if (item->is_single_end_type()) {
        Chord *end_chord = end_note->chord();
        PointF end_note_pos = end_note->pos() + end_chord->pos() + end_chord->segment()->pos() + end_chord->measure()->pos();
        end_pos = end_note_pos;

        double x_off = (is_inside ? 0 : 0.5) * end_note->width() - horizontal_indent * end_note->mag();
        double y_off = up_sign * vertical_padding * end_note->mag();
        if (!is_inside) {
            y_off += up_sign * 0.5 * end_note->height();
        }

        end_pos += PointF(x_off, y_off);
    } else {
        double x = start_system->ending_x_for_open_ended_lines();
        double y = start_note->y() + up_sign * 0.5 * start_note->height() + 0.5 * height_diff;
        end_pos = PointF(x, y);
    }

    avoid_bad_staff_line_intersection(item, start_pos);
    avoid_bad_staff_line_intersection(item, end_pos);
    adjust_x(item, start_pos, end_pos, start_note, end_note);

    if (bend->is_dive() && bend->overlapping_bend_or_dive()) {
        double y_off = (up ? -1 : 1) * spatium;
        start_pos.set_y(start_pos.y() + y_off);
        end_pos.set_y(end_pos.y() + y_off);
    }

    item->set_pos(start_pos);
    item->set_pos2(end_pos - start_pos);

    const double vertex_height_min = 0.75 * spatium;
    const double vertex_height_max = 2.0 * spatium;

    PointF rel_end_point = end_pos - start_pos;
    double angle = -atan(rel_end_point.y() / rel_end_point.x());

    double base_length = rel_end_point.x();
    double min_length = spatium;
    double vertex_height = vertex_height_min + 0.1 * (base_length - min_length);
    vertex_height = std::min(vertex_height, vertex_height_max);

    PointF vertex = 0.5 * rel_end_point;
    PointF vertex_displ = PointF(up_sign * vertex_height * sin(angle), up_sign * vertex_height * cos(angle));
    vertex += vertex_displ;

    PointF vertex_abs_coord = vertex + start_pos;
    avoid_bad_staff_line_intersection(item, vertex_abs_coord);
    vertex = vertex_abs_coord - start_pos;

    item->mutldata()->set_vertex_point(vertex);

    if (item->is_single_begin_type()) {
        start_note->add_start_line_attach_point(item->pos(), bend);
    }
    if (item->is_single_end_type()) {
        end_note->add_end_line_attach_point(item->pos() + item->pos2(), bend);
    }

    PainterPath path;
    path.line_to(vertex + item->vertex_point_off());
    path.line_to(item->pos2());
    item->mutldata()->set_path(path);

    RectF r = RectF(PointF(0.0, 0.0), vertex + item->vertex_point_off());
    r.unite(RectF(vertex + item->vertex_point_off(), item->pos2()));
    item->setbbox(r);
}

void GuitarBendLayout::compute_up(GuitarBend *item) {
    // NOTE: at some point, it would be good to find a way to reuse the logic of Tie::computeUp() (M.S.)

    GuitarBend::LayoutData *layout_data = item->mutldata();

    if (item->direction() != DirectionV::AUTO) {
        layout_data->set_up(item->direction() == DirectionV::UP);
        return;
    }

    Measure *measure = item->score()->tick2measure(item->tick());
    if (!measure) {
        return;
    }
    if (measure->has_voices(item->staff_idx())) {
        layout_data->set_up(item->track() % 2);
        return;
    }

    Note *start_n = item->start_note();
    Chord *start_chord = start_n->chord();
    Note *end_n = item->end_note();
    Chord *end_chord = end_n->chord();

    if (start_chord->up() != end_chord->up()) {
        layout_data->set_up(true);
        return;
    }

    if (start_chord->notes().size() == 1 && start_chord->stem()) {
        layout_data->set_up(!start_chord->up());
        return;
    }

    if (end_chord->notes().size() == 1 && end_chord->stem()) {
        layout_data->set_up(!end_chord->up());
        return;
    }

    int bend_count = 0;
    int index_of_this = 0;
    for (Note *note : (start_chord->notes().size() > 1 ? start_chord->notes() : end_chord->notes())) {
        for (Spanner *spanner : note->spanner_for()) {
            if (spanner->is_guitar_bend()) {
                if (spanner == item) {
                    index_of_this = bend_count;
                }
                ++bend_count;
            }
        }
    }

    bool up = index_of_this >= floor(double(bend_count) / 2);
    layout_data->set_up(up);
}

void GuitarBendLayout::compute_is_inside(GuitarBend *item) {
    GuitarBend::LayoutData *layout_data = item->mutldata();
    Note *start_n = item->start_note();
    Chord *start_chord = start_n->chord();
    Note *end_n = item->end_note();
    Chord *end_chord = end_n->chord();

    if (start_chord->notes().size() <= 2 || end_chord->notes().size() <= 2) {
        layout_data->set_is_inside(false);
        return;
    }

    if (start_n == start_chord->up_note() || start_n == start_chord->down_note() || end_n == end_chord->up_note() ||
        end_n == end_chord->down_note()) {
        layout_data->set_is_inside(false);
        return;
    }

    layout_data->set_is_inside(true);
}

void GuitarBendLayout::avoid_bad_staff_line_intersection(GuitarBendSegment *item, PointF &point) {
    int up_sign = item->guitar_bend()->ldata()->up() ? -1 : 1;
    double spatium = item->spatium();
    double line_dist = spatium * item->staff()->line_distance(item->tick());
    double closest_staff_line_y = line_dist * round(point.y() / line_dist);

    bool is_inside_staff = closest_staff_line_y >= 0 && closest_staff_line_y <= 4 * spatium;
    if (is_inside_staff && item->autoplace()) {
        const double min_line_dist = 0.5 * item->style().style_absolute(Sid::STAFF_LINE_WIDTH) + 0.15 * spatium;
        double point_line_dist = point.y() - closest_staff_line_y;
        if (std::abs(point_line_dist) < min_line_dist) {
            point.set_y(closest_staff_line_y + up_sign * min_line_dist);
        }
    }
}

void GuitarBendLayout::adjust_x(GuitarBendSegment *item, PointF &start_pos, PointF &end_pos, const Note *start_note, const Note *end_note) {
    GuitarBend *bend = item->guitar_bend();
    GuitarBend::LayoutData *layout_data = bend->mutldata();
    bool up = item->guitar_bend()->ldata()->up();
    Chord *start_chord = start_note->chord();
    Chord *end_chord = end_note->chord();

    bool adjust_start =
        item->is_single_begin_type() && ((start_chord->stem() && start_chord->up() && up) || (up && start_note != start_chord->up_note()) ||
                                         (!up && start_note != start_chord->down_note()));
    bool adjust_end =
        item->is_single_end_type() && ((end_chord->stem() && !end_chord->up() && !up) || (up && end_note != end_chord->up_note()) ||
                                       (!up && end_note != end_chord->down_note()));

    if (!layout_data->is_inside() && !adjust_start && !adjust_end) {
        return;
    }

    const double vert_margin = 0.1 * item->spatium();
    const double padding = 0.2 * item->spatium();

    if (item->is_single_begin_type()) {
        Shape start_chord_shape =
            start_chord->shape().translate(start_chord->pos() + start_chord->segment()->pos() + start_chord->measure()->pos());
        double point_to_clear = start_chord_shape.right_most_edge_at_height(start_pos.y() - vert_margin, start_pos.y() + vert_margin);
        point_to_clear += padding;
        double resulting_x = std::max(start_pos.x(), point_to_clear);
        start_pos.set_x(resulting_x);
    }

    if (item->is_single_end_type()) {
        Shape end_chord_shape = end_chord->shape().translate(end_chord->pos() + end_chord->segment()->pos() + end_chord->measure()->pos());
        double point_to_clear = end_chord_shape.left_most_edge_at_height(end_pos.y() - vert_margin, end_pos.y() + vert_margin);
        point_to_clear -= padding;
        double resulting_x = std::min(end_pos.x(), point_to_clear);
        end_pos.set_x(resulting_x);
    }
}

void GuitarBendLayout::layout_slight_bend(GuitarBendSegment *item, LayoutContext &) {
    GuitarBend *bend = item->guitar_bend();
    Note *start_note = bend->start_note();
    assert(start_note);

    Chord *start_chord = start_note->chord();
    PointF start_note_pos = start_note->pos() + start_chord->pos() + start_chord->segment()->pos() + start_chord->measure()->pos();

    double spatium = item->spatium();
    PointF start_pos = start_note_pos + PointF(start_note->width() + 0.25 * spatium, -0.25 * spatium);
    PointF end_pos(1.25 * spatium, -1.0 * spatium);
    PointF curve(end_pos.x(), 0.0);

    item->set_pos(start_pos);
    item->set_pos2(end_pos);
    item->mutldata()->set_vertex_point(curve);

    item->mutldata()->set_bbox(RectF(PointF(), item->pos2()).normalized());

    PainterPath path;
    path.cubic_to(PointF(0.0, 0.0), curve + item->vertex_point_off(), item->pos2());
    item->mutldata()->set_path(path);
}

void GuitarBendLayout::check_conflict_with_other_bends(GuitarBendSegment *item) {
    GuitarBend *this_bend = item->guitar_bend();
    Note *start_note = this_bend->start_note();
    Chord *start_chord = start_note->chord();

    GuitarBend *other_bend = nullptr;
    for (Note *note : start_chord->notes()) {
        if (note->string() >= start_note->string()) {
            continue;
        }
        GuitarBend *bend_for = note->bend_for();
        if (bend_for) {
            other_bend = bend_for;
            break;
        }
    }

    if (!other_bend) {
        return;
    }

    if (other_bend->ldata()->bend_digit() != this_bend->ldata()->bend_digit()) {
        if (this_bend->bend_type() != GuitarBendType::PRE_BEND) {
            GuitarBendSegment::LayoutData *ldata = item->mutldata();
            double spatium = item->spatium();
            PointF end_point_move(spatium, spatium);
            item->set_pos2(item->pos2() + end_point_move);
            item->mutldata()->arrow().translate(end_point_move);
            ldata->set_vertex_point(ldata->vertex_point() + PointF(end_point_move.x(), 0.0));

            GuitarBendText *text = item->bend_text();
            text->mutldata()->set_pos(text->ldata()->pos() + end_point_move + PointF(0.35 * text->width(), 0.0));
        }
    }
}

bool GuitarBendLayout::start_on_end_note(GuitarBend *bend) {
    GuitarBendType bend_t = bend->bend_type();
    if (!(bend_t == GuitarBendType::PRE_BEND || bend_t == GuitarBendType::GRACE_NOTE_BEND || bend_t == GuitarBendType::PRE_DIVE)) {
        return false;
    }

    bool pre_bends_align_to_grace = bend->style().style_b(Sid::ALIGN_PRE_BEND_AND_PRE_DIVE_TO_GRACE_NOTE);
    if (!pre_bends_align_to_grace && (bend_t == GuitarBendType::PRE_BEND || bend_t == GuitarBendType::PRE_DIVE)) {
        return true;
    }

    if (bend_t == GuitarBendType::PRE_BEND || bend_t == GuitarBendType::GRACE_NOTE_BEND) {
        Chord *start_chord = bend->start_note()->chord();
        size_t bends_count = 0;
        for (Note *note : start_chord->notes()) {
            if (note->bend_for()) {
                ++bends_count;
            }
        }

        if (bends_count < bend->end_note()->chord()->notes().size()) {
            return true;
        }
    }

    return false;
}

void GuitarBendLayout::layout_tab_staff(GuitarBendSegment *item, LayoutContext &ctx) {
    if (item->guitar_bend()->is_dive()) {
        GuitarDiveLayout::layout_dive_tab_staff(item, ctx);
    } else {
        layout_bend_tab_staff(item, ctx);
    }
}

void GuitarBendLayout::layout_bend_tab_staff(GuitarBendSegment *item, LayoutContext &ctx) {
    GuitarBend *bend = item->guitar_bend();
    GuitarBendSegment::LayoutData *ldata = item->mutldata();

    Note *start_note = bend->start_note();
    Note *end_note = bend->end_note();
    if (!start_note || !end_note) {
        return;
    }

    double spatium = bend->spatium();
    const MStyle &style = item->style();
    const double vertical_pad = 0.25 * spatium;
    const double dist_above_tab =
        style.style_d(Sid::GUITAR_BEND_HEIGHT_ABOVE_TAB_STAFF) * spatium * item->staff()->line_distance(item->tick());
    const double arrow_width = style.style_absolute(Sid::GUITAR_BEND_ARROW_WIDTH);
    const double arrow_height = style.style_absolute(Sid::GUITAR_BEND_ARROW_HEIGHT);
    const double line_width = item->line_width();

    PointF start_pos = PointF(0.0, 0.0);
    PointF end_pos = PointF(0.0, 0.0);
    PointF vertex = PointF(0.0, 0.0);
    PolygonF arrow;

    GuitarBend *prev_bend = bend->find_preceding_bend();
    GuitarBendSegment *prev_bend_seg = prev_bend && !prev_bend->segments_empty() && !prev_bend->is_full_release() ?
                                           to_guitar_bend_segment(prev_bend->back_segment()) :
                                           nullptr;
    GuitarBendHoldSegment *prev_hold_line =
        prev_bend && prev_bend->hold_line() && !prev_bend->hold_line()->segments_empty() && prev_bend->end_note() != bend->start_note() ?
            to_guitar_bend_hold_segment(prev_bend->hold_line()->back_segment()) :
            nullptr;

    PointF prev_end_point = PointF(0.0, 0.0);
    if (prev_hold_line) {
        prev_end_point = prev_hold_line->pos2() + prev_hold_line->pos();
    } else if (prev_bend_seg) {
        const PolygonF &prev_arrow = prev_bend_seg->ldata()->arrow();
        if (!prev_arrow.empty()) {
            prev_end_point = prev_arrow.front() + prev_bend_seg->pos();
            prev_end_point += PointF(0.0, (prev_bend->is_release_bend() ? -0.5 : 0.5) * line_width);
        }
    }

    if (item->is_single_begin_type() && bend->bend_type() != GuitarBendType::PRE_BEND && !prev_end_point.is_null()) {
        start_pos = prev_end_point;
    } else {
        start_pos = compute_start_pos(item, start_note, dist_above_tab, vertical_pad, arrow_height);
    }

    end_pos = compute_end_pos(item, end_note, dist_above_tab, vertical_pad, arrow_height, arrow_width, start_pos, prev_end_point);
    end_pos.set_x(
        std::max(start_pos.x(), end_pos.x())); // Edge case for ultra-ultra-narrow situations: ensure bend doesn't end before its start

    vertex = bend->bend_type() == GuitarBendType::PRE_BEND && !bend->angled_pre_bend() ? 0.5 * (start_pos + end_pos) :
                                                                                         PointF(end_pos.x(), start_pos.y());

    arrow = bend->is_release_bend() ?
                arrow << PointF(0, arrow_height) << PointF(arrow_width * .5, 0.0) << PointF(-arrow_width * .5, 0.0) // arrow-down
                :
                arrow << PointF(0, -arrow_height) << PointF(arrow_width * .5, 0.0) << PointF(-arrow_width * .5, 0.0); // arrow-up

    if (bend->is_release_bend() && !end_note->ghost()) {
        end_note->set_ghost(true);
        end_note->mutldata()->reset();
        TLayout::layout_chord(end_note->chord(), ctx);
    }

    if (start_on_end_note(bend)) {
        if (!start_note->override_bend_visibility_rules()) {
            start_note->set_visible(false);
        }
        if (!end_note->override_bend_visibility_rules()) {
            end_note->set_visible(true);
        }
    } else if (bend->bend_type() != GuitarBendType::SLIGHT_BEND &&
               (!style.style_b(Sid::SHOW_FRET_ON_FULL_BEND_RELEASE) || !bend->is_full_release())) {
        if (bend->bend_type() == GuitarBendType::GRACE_NOTE_BEND || bend->bend_type() == GuitarBendType::PRE_BEND) {
            if (!start_note->override_bend_visibility_rules()) {
                start_note->set_visible(true);
            }
        }
        if (!end_note->override_bend_visibility_rules()) {
            end_note->set_visible(false);
        }
    }

    item->set_pos(start_pos);
    item->set_pos2(end_pos - start_pos);
    ldata->set_vertex_point(vertex - start_pos);

    arrow.translate(item->pos2());
    item->mutldata()->set_arrow(arrow);

    GuitarBendText *guitar_bend_text = item->bend_text();
    guitar_bend_text->set_xml_text(bend->ldata()->bend_digit());
    TextLayout::layout_base_text_base(to_text_base(guitar_bend_text), ctx);
    double vertical_text_pad = 0.2 * spatium;
    PointF centering(-0.5 * guitar_bend_text->width(),
                     (bend->is_release_bend() ? vertical_text_pad : -(guitar_bend_text->height() + vertical_text_pad)));
    guitar_bend_text->mutldata()->set_pos(arrow.front() + centering);

    GuitarBendLayout::check_conflict_with_other_bends(item);

    PainterPath path;
    if (bend->angled_pre_bend()) {
        path.line_to(ldata->vertex_point() + item->vertex_point_off());
        path.line_to(item->pos2());
    } else {
        path.cubic_to(PointF(0.0, 0.0), ldata->vertex_point() + item->vertex_point_off(), item->pos2());
    }
    ldata->set_path(path);

    RectF r;
    if (muse::real_is_null(item->pos2().x())) {
        r = RectF(-0.5 * line_width, 0.0, line_width, arrow.front().y());
    } else {
        r = RectF(PointF(), PointF(arrow[1].x(), arrow[0].y()));
    }
    item->mutldata()->set_bbox(r.normalized());
}

PointF GuitarBendLayout::compute_start_pos(GuitarBendSegment *item, Note *start_note, double dist_above_tab, double vertical_pad,
                                           double arrow_height) {
    GuitarBend *bend = item->guitar_bend();
    Chord *start_chord = start_note->chord();
    PointF start_note_pos = start_note->pos() + start_chord->pos() + start_chord->segment()->pos() + start_chord->measure()->pos();

    const double spatium = item->spatium();
    const double line_width = item->line_width();
    const double horizontal_indent = 0.25 * spatium;
    System *system = item->system();

    PointF start_pos;

    if (start_on_end_note(bend)) {
        Note *end_n = bend->end_note();
        start_pos = end_n->system_pos();
        Chord *end_chord = end_n->chord();
        bool start_from_the_side = bend->bend_type() == GuitarBendType::GRACE_NOTE_BEND ||
                                   (end_chord->notes().size() > 1 && end_n->string() > end_chord->up_string());
        if (start_from_the_side) {
            start_pos += PointF(end_n->width() + horizontal_indent, -0.5 * end_n->height() + 0.5 * line_width);
        } else {
            start_pos += PointF(0.5 * end_n->width(), -0.5 * end_n->height() - vertical_pad);
        }
        return start_pos;
    }

    if (bend->bend_type() == GuitarBendType::PRE_BEND) {
        start_pos = start_note->system_pos();
        start_pos += PointF(0.5 * start_note->width(), -0.5 * start_note->height() - vertical_pad);
        return start_pos;
    }

    if (bend->is_release_bend()) {
        if (item->is_single_begin_type()) {
            start_pos.set_x(start_note_pos.x());
            start_pos.set_y(-dist_above_tab - arrow_height + 0.5 * line_width);
        } else {
            start_pos.set_x(system->first_note_rest_segment_x(true));
            start_pos.set_y(-dist_above_tab);
        }
        return start_pos;
    }

    if (item->is_single_begin_type()) {
        start_pos = start_note_pos;
        start_pos += PointF(start_note->width() + horizontal_indent, -0.5 * start_note->height() + 0.5 * line_width);
    } else {
        start_pos.set_x(system->first_note_rest_segment_x(true));
        start_pos.set_y(0.5 * spatium);
    }

    return start_pos;
}

PointF GuitarBendLayout::compute_end_pos(GuitarBendSegment *item, Note *end_note, double dist_above_tab, double vertical_pad,
                                         double arrow_height, double arrow_width, const PointF &start_pos, const PointF &prev_end_point) {
    GuitarBend *bend = item->guitar_bend();
    Chord *end_chord = end_note->chord();
    PointF end_note_pos = end_note->pos() + end_chord->pos() + end_chord->segment()->pos() + end_chord->measure()->pos();

    const MStyle &style = item->style();
    const double partial_bend_height = style.style_absolute(Sid::GUITAR_BEND_PARTIAL_BEND_HEIGHT);
    const double spatium = item->spatium();
    System *system = item->system();

    PointF end_pos;

    if (bend->bend_type() == GuitarBendType::PRE_BEND && !bend->angled_pre_bend()) {
        end_pos = PointF(start_pos.x(), -dist_above_tab); // TODO: style
        return end_pos;
    }

    if (bend->is_release_bend()) {
        if (item->is_single_end_type()) {
            end_pos = end_note_pos;
            end_pos += PointF(0.5 * end_note->width(), 0.0);
            if (bend->is_full_release()) {
                end_pos.set_y(end_pos.y() - 0.5 * end_note->height() - arrow_height - vertical_pad);
            } else {
                end_pos.set_y(start_pos.y() + partial_bend_height);
            }
        } else {
            end_pos.set_x(system->ending_x_for_open_ended_lines() - 0.5 * arrow_width);
            end_pos.set_y(-arrow_height);
        }
        return end_pos;
    }

    if (bend->bend_type() == GuitarBendType::SLIGHT_BEND) {
        const double slight_bend_width = 1.25 * spatium;
        end_pos = start_pos + PointF(slight_bend_width, 0.0);
    } else {
        if (item->is_single_end_type()) {
            end_pos = end_note_pos;
            end_pos += PointF(0.5 * end_note->width(), 0.0);
        } else {
            end_pos.set_x(system->ending_x_for_open_ended_lines() - 0.5 * arrow_width);
        }
    }
    end_pos = PointF(end_pos.x(), prev_end_point.is_null() ? -dist_above_tab : start_pos.y() - partial_bend_height); // TODO: style
    end_pos.set_x(std::max(end_pos.x(), start_pos.x() + 0.75 * spatium));

    return end_pos;
}

void GuitarBendLayout::layout_hold_line(GuitarBendHoldSegment *item) {
    if (!item->system()) {
        return;
    }

    GuitarBend *start_bend = item->guitar_bend_hold()->guitar_bend();
    if (!start_bend || start_bend->segments_empty()) {
        return;
    }

    const double spatium = item->spatium();
    const double line_width = item->line_width();

    PointF start_pos = PointF();
    PointF end_pos = PointF();

    Note *end_note = item->guitar_bend_hold()->end_note();

    Chord *end_chord = end_note->chord();
    PointF end_note_pos = end_note->pos() + end_chord->pos() + end_chord->segment()->pos() + end_chord->measure()->pos();

    GuitarBendSegment *start_bend_segment = nullptr;
    if (!start_bend->is_full_release() && !start_bend->segments_empty()) {
        start_bend_segment = to_guitar_bend_segment(start_bend->back_segment());
    } else {
        return;
    }
    item->set_start_bend_seg(start_bend_segment);

    GuitarBendSegment *end_bend_segment = nullptr;
    if (item->is_single_end_type()) {
        GuitarBend *bend = end_note->bend_for();
        if (bend && bend != start_bend && !bend->segments_empty()) {
            end_bend_segment = to_guitar_bend_segment(bend->front_segment());
        }
    }
    item->set_end_bend_seg(end_bend_segment);

    if (item->is_single_begin_type()) {
        start_pos =
            start_bend_segment->pos() + (start_bend->is_dive() ? start_bend_segment->pos2() : start_bend_segment->ldata()->arrow().front());
        if (!start_bend->is_dive()) {
            double gap = item->dash_length() * line_width;
            start_pos += PointF(gap, (start_bend->is_release_bend() ? -0.5 : +0.5) * line_width);
        }
    } else {
        start_pos.set_x(item->system()->first_note_rest_segment_x(true));
        start_pos.set_y(item->guitar_bend_hold()->front_segment()->pos().y());
    }

    if (item->is_single_end_type()) {
        if (end_bend_segment) {
            GuitarBend *end_bend = end_bend_segment->guitar_bend();
            if (end_bend->bend_type() == GuitarBendType::PRE_DIVE || end_bend->bend_type() == GuitarBendType::PRE_BEND) {
                Note *note = start_on_end_note(end_bend) ? end_bend->end_note() : end_bend->start_note();
                end_pos.set_x(note->system_pos().x() + 0.5 * note->ldata()->bbox().width());
            } else {
                end_pos.set_x(end_note_pos.x());
            }
        } else {
            end_pos.set_x(end_note_pos.x() + end_note->chord()->segment()->width() - 1.5 * spatium);
        }
    } else {
        end_pos.set_x(item->system()->ending_x_for_open_ended_lines());
    }

    const double min_len = spatium;
    end_pos.set_x(std::max(start_pos.x() + min_len, end_pos.x()));

    end_pos.set_y(start_pos.y());

    item->set_pos(start_pos);
    item->set_pos2(end_pos - start_pos);

    if (start_bend->dip_vibrato_type() != VibratoType::NONE) {
        SymId sym_id = SymId::NO_SYM;
        switch (start_bend->dip_vibrato_type()) {
            case VibratoType::GUITAR_VIBRATO: sym_id = SymId::GUITAR_VIBRATO_STROKE; break;
            case VibratoType::GUITAR_VIBRATO_WIDE: sym_id = SymId::GUITAR_WIDE_VIBRATO_STROKE; break;
            default: sym_id = SymId::NO_SYM;
        }
        double line_length = item->pos2().x();
        double sym_advance = item->sym_advance(sym_id);
        int syms_count = std::floor(line_length / sym_advance);
        syms_count = std::max(syms_count, 1);
        item->mutldata()->set_sym_ids(SymIdList(syms_count, sym_id));
        item->mutldata()->move_y(0.5 * item->sym_height(sym_id));
        RectF bbox = item->sym_bbox(sym_id);
        for (size_t i = 1; i < item->ldata()->sym_ids().size(); ++i) {
            bbox.unite(bbox.translated(PointF(sym_advance, 0.0)));
        }
        item->mutldata()->set_bbox(bbox);
        return;
    }

    RectF r(0.0, -0.5 * line_width, item->pos2().x(), line_width);
    item->mutldata()->set_bbox(r);
}
