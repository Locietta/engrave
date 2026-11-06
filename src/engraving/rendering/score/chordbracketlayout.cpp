/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "chordbracketlayout.h"

#include <algorithm>
#include <vector>

#include "chordlayout.h"
#include "dom/chord.h"
#include "dom/chordbracket.h"
#include "dom/engravingitem.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/segment.h"
#include "horizontalspacing.h"
#include "tlayout.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

//---------------------------------------------------------
//   chordBracketsInSegment
//---------------------------------------------------------

std::vector<ChordBracket *> ChordBracketLayout::chord_brackets_in_segment(const Segment *segment) {
    std::vector<ChordBracket *> result;

    if (!segment) {
        return result;
    }

    for (EngravingItem *item : segment->elist()) {
        if (!item || !item->is_chord()) {
            continue;
        }

        for (EngravingItem *child : to_chord(item)->el()) {
            if (child && child->is_chord_bracket()) {
                result.push_back(to_chord_bracket(child));
            }
        }
    }

    return result;
}

//---------------------------------------------------------
//   bracketVisualSpan
//---------------------------------------------------------

ChordBracketLayout::BracketVisualSpan ChordBracketLayout::bracket_visual_span(const ChordBracket *bracket) {
    const Chord *start_chord = bracket->chord();
    EngravingItem *end_item = start_chord->segment()->element(bracket->end_track());
    const Chord *end_chord = end_item && end_item->is_chord() ? to_chord(end_item) : start_chord;

    const auto top_y = [](const Chord *chord) {
        const Note *note = chord->up_note();
        return chord->pos().y() + note->pos().y() + note->ldata()->bbox().top();
    };

    const auto bottom_y = [](const Chord *chord) {
        const Note *note = chord->down_note();
        return chord->pos().y() + note->pos().y() + note->ldata()->bbox().bottom();
    };

    const staff_idx_t start_staff = start_chord->v_staff_idx();
    const staff_idx_t end_staff = end_chord->v_staff_idx();

    const double spatium = bracket->spatium();
    const double line_width = bracket->style().style_absolute(Sid::CHORD_BRACKET_LINE_WIDTH);

    // Match TLayout::layoutChordBracket() padding.
    const double top_padding = bracket->user_len1() + 0.25 * spatium + 0.5 * line_width;
    const double bottom_padding = bracket->user_len2() + 0.5 * spatium + 0.5 * line_width;

    BracketVisualSpan result;
    result.top_staff = std::min(start_staff, end_staff);
    result.bottom_staff = std::max(start_staff, end_staff);

    if (start_staff == end_staff) {
        result.top_y = std::min(top_y(start_chord), top_y(end_chord)) - top_padding;
        result.bottom_y = std::max(bottom_y(start_chord), bottom_y(end_chord)) + bottom_padding;

        // Enforce the minimum length using endpoint coordinates.
        const double min_len = 2 * spatium;
        const double diff = min_len - std::abs(result.bottom_y - result.top_y);

        if (diff > 0.0) {
            if (bracket->hook_pos() == DirectionV::DOWN) {
                result.top_y -= diff;
            } else if (bracket->hook_pos() == DirectionV::AUTO) {
                result.top_y -= 0.5 * diff;
                result.bottom_y += 0.5 * diff;
            } else {
                result.bottom_y += diff;
            }
        }
    } else {
        const Chord *upper_anchor = start_staff < end_staff ? start_chord : end_chord;
        const Chord *lower_anchor = start_staff < end_staff ? end_chord : start_chord;

        result.top_y = top_y(upper_anchor) - top_padding;
        result.bottom_y = bottom_y(lower_anchor) + bottom_padding;
    }

    result.top_y += bracket->offset().y();
    result.bottom_y += bracket->offset().y();

    return result;
}

//---------------------------------------------------------
//   bracketCollisionShape
//---------------------------------------------------------

Shape ChordBracketLayout::bracket_collision_shape(const ChordBracket *bracket, const BracketVisualSpan &span, staff_idx_t staff_idx,
                                                  double x) {
    if (staff_idx < span.top_staff || staff_idx > span.bottom_staff) {
        return Shape();
    }

    double top = span.top_y;
    double bottom = span.bottom_y;

    // Final staff distances are not known during horizontal spacing.
    // Approximate the bracket per staff: extend it toward the other endpoint on the endpoint staves
    // and across the full height of intermediate staves.
    static constexpr double cross_staff_collision_extent = 100000.0;

    if (span.top_staff != span.bottom_staff) {
        top = staff_idx == span.top_staff ? span.top_y : -cross_staff_collision_extent;
        bottom = staff_idx == span.bottom_staff ? span.bottom_y : cross_staff_collision_extent;
    }

    if (bottom < top) {
        std::swap(top, bottom);
    }

    return Shape(RectF(x, top, bracket->width(), bottom - top), bracket);
}

//---------------------------------------------------------
//   layoutHorizontal
//---------------------------------------------------------
void ChordBracketLayout::layout_horizontal(ChordBracket *bracket) {
    Chord *owner = bracket->chord();
    Segment *segment = owner->segment();
    const TrackRange range = bracket->part()->track_range();
    const BracketVisualSpan span = bracket_visual_span(bracket);

    double required_distance = 0.0;
    const double base_x = owner->pos().x() + bracket->offset().x();

    for (track_idx_t track = range.start_track; track < range.end_track; ++track) {
        EngravingItem *item = segment->element(track);
        if (!item || !item->is_chord()) {
            continue;
        }

        Chord *chord = to_chord(item);
        Shape chord_shape = chord->shape();
        chord_shape.remove_types({ElementType::CHORD_BRACKET});
        chord_shape.translate(chord->pos());
        if (chord_shape.empty()) {
            continue;
        }

        Shape bracket_shape = bracket_collision_shape(bracket, span, chord->v_staff_idx(), base_x);
        if (bracket_shape.empty()) {
            continue;
        }

        const double distance = bracket->right_side() ?
                                    HorizontalSpacing::min_horizontal_distance(chord_shape, bracket_shape, bracket->spatium()) :
                                    HorizontalSpacing::min_horizontal_distance(bracket_shape, chord_shape, bracket->spatium());
        required_distance = std::max(required_distance, distance);
    }

    bracket->mutldata()->set_pos_x(bracket->right_side() ? required_distance : -required_distance);
}

//---------------------------------------------------------
//   layoutSegment
//---------------------------------------------------------

void ChordBracketLayout::layout_segment(Segment *segment, LayoutContext &ctx) {
    if (!segment) {
        return;
    }

    for (ChordBracket *bracket : chord_brackets_in_segment(segment)) {
        Chord *owner = bracket->chord();
        if (!owner || owner->segment() != segment || !bracket->part() || owner->on_tab_staff()) {
            continue;
        }

        // Initialize the bracket's width, vertical geometry, and Shape.
        // Replace the chord-local horizontal position with the segment-level result.
        TLayout::layout_item(bracket, ctx);
        layout_horizontal(bracket);

        // Rebuild the owner Shape after moving the bracket.
        ChordLayout::fill_shape(owner, owner->mutldata());
    }
}

//---------------------------------------------------------
//   updateHorizontalSpacing
//---------------------------------------------------------

void ChordBracketLayout::update_horizontal_spacing(const Segment *first_segment, const Segment *second_segment, staff_idx_t staff_idx,
                                                   double squeeze_factor, double &min_distance) {
    if (!first_segment || !second_segment) {
        return;
    }

    auto update_for_brackets = [&](const Segment *segment, const Shape &other_shape, bool right_side) {
        for (ChordBracket *bracket : chord_brackets_in_segment(segment)) {
            const Chord *owner = bracket->chord();
            if (!owner || owner->on_tab_staff() || !bracket->visible() || !bracket->add_to_skyline() ||
                bracket->right_side() != right_side) {
                continue;
            }

            const BracketVisualSpan span = bracket_visual_span(bracket);

            if (span.top_staff == span.bottom_staff) {
                continue;
            }

            const double x = owner->pos().x() + bracket->pos().x();
            const Shape bracket_shape = bracket_collision_shape(bracket, span, staff_idx, x);

            if (bracket_shape.empty()) {
                continue;
            }

            const double distance =
                right_side ? HorizontalSpacing::min_horizontal_distance(bracket_shape, other_shape, bracket->spatium(), squeeze_factor) :
                             HorizontalSpacing::min_horizontal_distance(other_shape, bracket_shape, bracket->spatium(), squeeze_factor);

            min_distance = std::max(min_distance, distance);
        }
    };

    // Check the brackets facing the gap between the two Segments:
    // left-side brackets on the second Segment and right-side brackets on the first.
    update_for_brackets(second_segment, first_segment->staff_shape(staff_idx), /*rightSide=*/false);
    update_for_brackets(first_segment, second_segment->staff_shape(staff_idx), /*rightSide=*/true);
}

//---------------------------------------------------------
//   updateVerticalGeometry
//---------------------------------------------------------

void ChordBracketLayout::update_vertical_geometry(ChordBracket *bracket, LayoutContext &ctx) {
    if (!bracket || !bracket->chord() || !bracket->chord()->segment()) {
        return;
    }

    // Re-layout the final height without changing the horizontal position.
    const double x = bracket->ldata()->pos().x();
    TLayout::layout_item(bracket, ctx);
    bracket->mutldata()->set_pos_x(x);

    // Refresh cached shapes after restoring the segment-level position.
    Chord *chord = bracket->chord();
    Segment *segment = chord->segment();
    ChordLayout::fill_shape(chord, chord->mutldata());

    const BracketVisualSpan span = bracket_visual_span(bracket);
    for (staff_idx_t staff_idx = span.top_staff; staff_idx <= span.bottom_staff; ++staff_idx) {
        segment->create_shape(staff_idx);
    }
}
