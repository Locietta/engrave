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
#include "arpeggiolayout.h"

#include "dom/arpeggio.h"
#include "dom/chord.h"
#include "dom/ledgerline.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"

#include "tlayout.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

//   layoutArpeggio2
//    called after layout of page

void ArpeggioLayout::layout_arpeggio2(Arpeggio *item, LayoutContext &ctx) {
    if (!item) {
        return;
    }

    TLayout::layout_item(item, ctx);
}

//
// INSET:
// Arpeggios have inset white space. For instance, the bracket
// "[" shape has whitespace inside of the "C". Symbols like
// accidentals can fit inside this whitespace. These inset
// functions are used to get the size of the inner dimensions
// for this area on all arpeggios.
//

double ArpeggioLayout::inset_top(const Arpeggio *item, const Chord *c) {
    double top = c->up_note()->y() - c->up_note()->height() / 2;

    // use wiggle width, not height, since it's rotated 90 degrees
    if (item->arpeggio_type() == ArpeggioType::UP) {
        top += item->sym_bbox(SymId::WIGGLE_ARPEGGIATO_UP_ARROW).width();
    } else if (item->arpeggio_type() == ArpeggioType::UP_STRAIGHT) {
        top += item->sym_bbox(SymId::ARROWHEAD_BLACK_UP).width();
    }

    return top;
}

double ArpeggioLayout::inset_bottom(const Arpeggio *item, const Chord *c) {
    double bottom = c->down_note()->y() + c->down_note()->height() / 2;

    // use wiggle width, not height, since it's rotated 90 degrees
    if (item->arpeggio_type() == ArpeggioType::DOWN) {
        bottom -= item->sym_bbox(SymId::WIGGLE_ARPEGGIATO_UP_ARROW).width();
    } else if (item->arpeggio_type() == ArpeggioType::DOWN_STRAIGHT) {
        bottom -= item->sym_bbox(SymId::ARROWHEAD_BLACK_DOWN).width();
    }

    return bottom;
}

double ArpeggioLayout::inset_width(const Arpeggio *item) {
    double line_width = item->style().style_absolute(item->is_chord_bracket() ? Sid::CHORD_BRACKET_LINE_WIDTH : Sid::ARPEGGIO_LINE_WIDTH);
    switch (item->arpeggio_type()) {
        case ArpeggioType::NORMAL: {
            return 0.0;
        }

        case ArpeggioType::UP:
        case ArpeggioType::DOWN: {
            // use wiggle height, not width, since it's rotated 90 degrees
            return (item->width() - item->sym_bbox(SymId::WIGGLE_ARPEGGIATO_UP).height()) / 2;
        }

        case ArpeggioType::UP_STRAIGHT:
        case ArpeggioType::DOWN_STRAIGHT: {
            return (item->width() - line_width) / 2;
        }

        case ArpeggioType::BRACKET: {
            return item->width() - line_width / 2;
        }
    }
    return 0.0;
}

void ArpeggioLayout::clear_accidentals(Arpeggio *item, LayoutContext &ctx) {
    //   This function finds collisions with accidentals outside of an arpeggio's span
    //   Assumptions are made to deal with cross staff height, as we don't know this yet.
    //   Once a chord is found, its shape is stripped down to only accidentals which collide
    //   with the arpeggio.  The arpeggio's offset is calculated from the remaining shape

    if (!item || !item->chord()) {
        return;
    }

    const Segment *seg = item->chord()->segment();
    const EngravingItem *end_el = seg->element(item->end_track());
    const Chord *end_chord = end_el && end_el->is_chord() ? to_chord(end_el) : item->chord();

    const Part *part = item->part();
    const TrackRange track_range = part->track_range();
    const PaddingTable &padding_table = item->score()->padding_tables().horizontal_padding_table();

    double arpeggio_accidental_distance = padding_table.at(ElementType::ARPEGGIO).at(ElementType::ACCIDENTAL) * item->mag();
    double arpeggio_ledger_distance = padding_table.at(ElementType::ARPEGGIO).at(ElementType::LEDGER_LINE) * item->mag();

    Shape arp_shape = item->shape().translate(item->page_pos());
    double arp_x = arp_shape.right();
    double largest_overlap = 0.0;

    for (track_idx_t track = track_range.start_track; track < track_range.end_track; ++track) {
        EngravingItem *e = seg->element(track);
        if (!e || !e->is_chord()) {
            continue;
        }

        Chord *chord = to_chord(e);
        // Only check chords the arpeggio doesn't span on staves the arpeggios span
        // We already calculated the gap between chords it does span in ChordLayout::layoutPitched
        bool above_start =
            std::make_pair(chord->v_staff_idx(), chord->down_line()) < std::make_pair(item->v_staff_idx(), item->chord()->up_line());
        bool below_end =
            std::make_pair(chord->v_staff_idx(), chord->up_line()) > std::make_pair(end_chord->v_staff_idx(), end_chord->down_line());

        if ((chord->span_arpeggio() == item && !(above_start || below_end)) || chord->v_staff_idx() < item->v_staff_idx() ||
            chord->v_staff_idx() > end_chord->v_staff_idx()) {
            continue;
        }

        // We don't know the distance between staves, so our arpeggio's height is currently unknown if it spans staves
        // To make sure we spot collisions with non-arpeggio accidentals, extend arpeggio upwards/downwards a large amount
        // depending on if they are the end/start of the arpeggio respectively.
        // This is to find collisions between chords outside the arpeggios track span, but within its "visual span"
        Shape cur_arp_shape = arp_shape;
        staff_idx_t staff_idx = chord->v_staff_idx();
        if (staff_idx == item->chord()->v_staff_idx() && item->cross_staff()) {
            // Arpeggio doesn't end on this staff, so extend downwards
            cur_arp_shape.add_b_box(RectF(cur_arp_shape.bbox().bottom_left().x(), cur_arp_shape.bbox().bottom_left().y(),
                                          cur_arp_shape.bbox().width(),
                                          ::mu::engraving::rendering::score::ArpeggioLayout::arbitrary_arpeggio_length));
        } else if (staff_idx == end_chord->v_staff_idx() && item->cross_staff()) {
            // Arpeggio doesn't start on this staff so move to last note and extend upwards
            cur_arp_shape.translate(PointF(0.0, end_chord->down_note()->page_pos().y() - item->page_pos().y()));
            cur_arp_shape.add_b_box(
                RectF(cur_arp_shape.bbox().top_left().x(),
                      cur_arp_shape.bbox().top_left().y() - ::mu::engraving::rendering::score::ArpeggioLayout::arbitrary_arpeggio_length,
                      cur_arp_shape.bbox().width(), ::mu::engraving::rendering::score::ArpeggioLayout::arbitrary_arpeggio_length));
        }

        Shape chord_shape = chord->shape();
        chord_shape.translate(chord->page_pos());
        // Remove any element which isn't an accidental the arpeggio intersects
        chord_shape.remove_if([cur_arp_shape, arpeggio_accidental_distance, arpeggio_ledger_distance](ShapeElement &shape_element) {
            if (!shape_element.item() || !(shape_element.item()->is_accidental() || shape_element.item()->is_ledger_line())) {
                return true;
            }
            if (shape_element.item()->is_accidental()) {
                // Pad accidentals with Sid::arpeggioAccidentalDistance either side
                shape_element.set_top_left(
                    PointF(shape_element.top_left().x() - arpeggio_accidental_distance, shape_element.top_left().y()));
                shape_element.set_width(shape_element.width() + 2 * arpeggio_accidental_distance);
            } else if (shape_element.item()->is_ledger_line()) {
                shape_element.set_top_left(PointF(shape_element.top_left().x() - arpeggio_ledger_distance, shape_element.top_left().y()));
                shape_element.set_width(shape_element.width() + 2 * arpeggio_ledger_distance);
            }
            return !cur_arp_shape.intersects(shape_element);
        });

        if (chord_shape.empty()) {
            // No collisions with accidentals
            continue;
        }

        double chord_x = chord_shape.left();
        double diff = chord_x - arp_x;

        if (!muse::real_is_null(diff)) {
            double inset = inset_distance(item, ctx, item->mag(), chord);
            largest_overlap = std::min(largest_overlap, diff + inset);
        }
    }
    if (!muse::real_is_null(largest_overlap)) {
        item->mutldata()->move_x(largest_overlap);
    }
}

double ArpeggioLayout::inset_distance(const Arpeggio *item, const LayoutContext &ctx, double input_mag, const Chord *chord) {
    if (!item || !chord) {
        return 0.0;
    }

    std::vector<Accidental *> accidentals_value;

    // generate list of accidentals if none provided
    for (Note *note : chord->notes()) {
        Accidental *accidental = note->accidental();
        if (accidental && accidental->visible()) {
            accidentals_value.push_back(accidental);
        }
    }

    if (accidentals_value.empty()) {
        return 0.0;
    }

    return inset_distance(item, ctx, input_mag, chord, accidentals_value);
}

double ArpeggioLayout::inset_distance(const Arpeggio *item, const LayoutContext &ctx, double input_mag, const Chord *chord,
                                      const std::vector<Accidental *> &accidentals) {
    if (!item || !chord) {
        return 0.0;
    }

    if (accidentals.empty()) {
        return 0.0;
    }

    // Only be concerned about the top/bottom of an arpeggio line if this is the start/end chord

    const Segment *seg = item->chord()->segment();
    Chord *end_chord = item->chord();
    const PaddingTable &padding_table = item->score()->padding_tables().horizontal_padding_table();
    if (EngravingItem *e = seg->element(item->end_track())) {
        end_chord = e->is_chord() ? to_chord(e) : end_chord;
    }
    bool arp_start_stave = chord->v_staff_idx() == item->v_staff_idx();
    bool arp_end_stave = chord->v_staff_idx() == end_chord->v_staff_idx();

    double arpeggio_top = arp_start_stave ? inset_top(item, item->chord()) * input_mag :
                                            -::mu::engraving::rendering::score::ArpeggioLayout::arbitrary_arpeggio_length;
    double arpeggio_bottom = arp_end_stave ? inset_bottom(item, end_chord) * input_mag :
                                             ::mu::engraving::rendering::score::ArpeggioLayout::arbitrary_arpeggio_length;

    ArpeggioType type = item->arpeggio_type();
    bool has_top_arrow = type == ArpeggioType::UP || type == ArpeggioType::UP_STRAIGHT || type == ArpeggioType::BRACKET;
    bool has_bottom_arrow = type == ArpeggioType::DOWN || type == ArpeggioType::DOWN_STRAIGHT || type == ArpeggioType::BRACKET;

    const Accidental *furthest_accidental = nullptr;
    for (const Accidental *accidental : accidentals) {
        if (furthest_accidental) {
            bool current_is_further_x = accidental->x() < furthest_accidental->x();
            bool current_is_same_x = accidental->x() == furthest_accidental->x();
            auto accidental_bbox = item->sym_bbox(accidental->sym_id());
            double current_top = accidental->note()->pos().y() + accidental_bbox.top() * input_mag;
            double current_bottom = accidental->note()->pos().y() + accidental_bbox.bottom() * input_mag;
            bool collides_with_top = current_top <= arpeggio_top && has_top_arrow;
            bool collides_with_bottom = current_bottom >= arpeggio_bottom && has_bottom_arrow;

            if (current_is_further_x || (current_is_same_x && (collides_with_top || collides_with_bottom))) {
                furthest_accidental = accidental;
            }
        } else {
            furthest_accidental = accidental;
        }
    }

    IF_ASSERT_FAILED(furthest_accidental) { return 0.0; }

    double maximum_inset = (padding_table.at(ElementType::ARPEGGIO).at(ElementType::ACCIDENTAL) -
                            ctx.conf().style_absolute(Sid::ARPEGGIO_ACCIDENTAL_DISTANCE_MIN)) *
                           input_mag;

    RectF bbox = item->sym_bbox(furthest_accidental->sym_id());
    double center = furthest_accidental->note()->pos().y() * input_mag;
    double top = center + bbox.top() * input_mag;
    double bottom = center + bbox.bottom() * input_mag;
    bool collides_with_top = has_top_arrow && top <= arpeggio_top;
    bool collides_with_bottom = has_bottom_arrow && bottom >= arpeggio_bottom;

    if (collides_with_top || collides_with_bottom) {
        return maximum_inset;
    }

    return inset_width(item);
}

double ArpeggioLayout::compute_height(const Arpeggio *item) {
    Chord *chord = item->chord();
    double y = chord->up_note()->page_pos().y() - chord->up_note()->head_height() * .5;

    Note *down_note = chord->down_note();
    EngravingItem *e = chord->segment()->element(item->track() + item->span() - 1);
    if (e && e->is_chord()) {
        down_note = to_chord(e)->down_note();
    }
    double h = down_note->page_pos().y() + down_note->head_height() * .5 - y;
    return h;
}

double ArpeggioLayout::calc_top(const Arpeggio *item, const LayoutConfiguration &conf) {
    double top = -item->user_len1();
    switch (item->arpeggio_type()) {
        case ArpeggioType::BRACKET: {
            double line_width = conf.style_absolute(item->is_chord_bracket() ? Sid::CHORD_BRACKET_LINE_WIDTH : Sid::ARPEGGIO_LINE_WIDTH);
            return top - line_width / 2.0;
        }
        case ArpeggioType::NORMAL:
        case ArpeggioType::UP:
        case ArpeggioType::DOWN: {
            // if the top is in the staff on a space, move it up
            // if the bottom note is on a line, the distance is 0.25 spaces
            // if the bottom note is on a space, the distance is 0.5 spaces
            int top_note_line = item->chord()->up_note()->line();
            int lines = item->staff()->lines(item->tick());
            int bottom_line = (lines - 1) * 2;
            if (top_note_line <= 0 || top_note_line % 2 == 0 || top_note_line >= bottom_line) {
                return top;
            }
            int down_note_line = item->chord()->down_note()->line();
            if (down_note_line % 2 == 1 && down_note_line < bottom_line) {
                return top - 0.4 * item->spatium();
            }
            return top - 0.25 * item->spatium();
        }
        default: {
            return top - item->spatium() / 4;
        }
    }
}

double ArpeggioLayout::calc_bottom(const Arpeggio *item, double arpeggio_height, const LayoutConfiguration &conf) {
    double top = -item->user_len1();
    double bottom = arpeggio_height + item->user_len2();

    switch (item->arpeggio_type()) {
        case ArpeggioType::BRACKET: {
            double line_width = conf.style_absolute(item->is_chord_bracket() ? Sid::CHORD_BRACKET_LINE_WIDTH : Sid::ARPEGGIO_LINE_WIDTH);
            return bottom - top + line_width;
        }
        case ArpeggioType::NORMAL:
        case ArpeggioType::UP:
        case ArpeggioType::DOWN: {
            return bottom;
        }
        default: {
            return bottom - top + item->spatium() / 2;
        }
    }
}
