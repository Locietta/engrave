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

#include "stemlayout.h"

#include "dom/chord.h"
#include "dom/hook.h"
#include "dom/note.h"
#include "dom/rest.h"
#include "dom/score.h"
#include "dom/staff.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"

using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

//-----------------------------------------------------------------------------
//   defaultStemLength
///   Get the default stem length for this chord
///   all internal calculation is done in quarter spaces
///   using integers to eliminate all possibilities for rounding errors
//-----------------------------------------------------------------------------
double StemLayout::calc_default_stem_length(Chord *item, const LayoutContext &ctx) {
    // returns default length even if the chord doesn't have a stem
    const MStyle &style = ctx.conf().style();
    const Chord::LayoutData *ldata = item->ldata();
    const Staff *staff = item->staff();

    double spatium = item->spatium();
    double line_distance = (staff ? staff->line_distance(item->tick()) : 1.0);

    const StaffType *staff_type = staff ? staff->staff_type_for_element(item) : nullptr;
    const StaffType *tab = (staff_type && staff_type->is_tab_staff()) ? staff_type : nullptr;

    bool is_beside_tab_staff = tab && !tab->stemless() && !tab->stem_through();
    if (is_beside_tab_staff) {
        return tab_stem_length(item, tab) * spatium;
    }

    int default_stem_length = style.style_d(Sid::STEM_LENGTH) * 4;
    default_stem_length += stem_length_beam_addition(item, ctx);
    if (tab) {
        default_stem_length *= 1.5;
    }
    // extraHeight represents the extra vertical distance between notehead and stem start
    // eg. slashed noteheads etc
    double extra_height =
        (ldata->up ? item->up_note()->stem_up_se().y() : item->down_note()->stem_down_nw().y()) / item->intrinsic_mag() / spatium;
    int shortest_stem = style.style_b(Sid::USE_WIDE_BEAMS) ? 12 : (style.style_d(Sid::SHORTEST_STEM) + std::abs(extra_height)) * 4;
    int quarter_spaces_per_line = std::floor(line_distance * 2);
    int chord_height = (item->down_line() - item->up_line()) * quarter_spaces_per_line; // convert to quarter spaces
    int stem_length = default_stem_length;

    int min_stem_length_quarter_spaces = calc_min_stem_length(item, ctx);

    int staff_line_count = staff ? staff->lines(item->tick()) : 5;
    int short_stem_start = style.style_i(Sid::SHORT_STEM_START_LOCATION) * quarter_spaces_per_line + 1;
    bool use_wide_beams = style.style_b(Sid::USE_WIDE_BEAMS);
    int beam_count = calc_beam_count(item);
    int middle_line = min_staff_overlap(ldata->up, staff_line_count, beam_count, !!item->hook(), use_wide_beams ? 4 : 3, use_wide_beams,
                                        !(item->is_grace() || item->is_small()));
    int ideal_stem_length = default_stem_length;

    if (ldata->up) {
        int stem_end_position = item->up_line() * quarter_spaces_per_line - default_stem_length;
        double stem_end_position_mag = (double) item->up_line() * quarter_spaces_per_line - (default_stem_length * item->intrinsic_mag());

        if (stem_end_position_mag <= -short_stem_start) {
            int reduction = max_reduction(item, ctx, std::abs((int) floor(stem_end_position_mag) + short_stem_start));
            ideal_stem_length = std::max(ideal_stem_length - reduction, shortest_stem);
        } else if (stem_end_position > middle_line) {
            // this case will be taken care of below; even if we were to adjust here we'd have
            // to adjust again later if the line spacing != 1.0 or if _relativeMag != 1.0
        } else {
            ideal_stem_length -= stem_optical_adjustment(item, stem_end_position);
            ideal_stem_length = std::max(ideal_stem_length, shortest_stem);
        }
        stem_length = std::max(ideal_stem_length, min_stem_length_quarter_spaces);
    } else {
        int stem_end_position = item->down_line() * quarter_spaces_per_line + default_stem_length;
        double stem_end_position_mag = (double) item->down_line() * quarter_spaces_per_line + (default_stem_length * item->intrinsic_mag());

        int down_short_stem_start = (staff_line_count - 1) * (2 * quarter_spaces_per_line) + short_stem_start;
        if (stem_end_position_mag >= down_short_stem_start) {
            int reduction = max_reduction(item, ctx, std::abs((int) ceil(stem_end_position_mag) - down_short_stem_start));
            ideal_stem_length = std::max(ideal_stem_length - reduction, shortest_stem);
        } else if (stem_end_position < middle_line) {
            // this case will be taken care of below; even if we were to adjust here we'd have
            // to adjust again later if the line spacing != 1.0 or if _relativeMag != 1.0
        } else {
            ideal_stem_length -= stem_optical_adjustment(item, stem_end_position);
            ideal_stem_length = std::max(ideal_stem_length, shortest_stem);
        }

        stem_length = std::max(ideal_stem_length, min_stem_length_quarter_spaces);
    }
    if (beam_count == 4 && !item->hook()) {
        stem_length = calc4_beams_exception(item, stem_length);
    }

    double final_stem_length = (chord_height / 4.0 * spatium) + ((stem_length / 4.0 * spatium) * item->intrinsic_mag());
    double extra_length = 0.;
    Note *start_note = ldata->up ? item->down_note() : item->up_note();
    if (!start_note->fixed()) {
        // when the chord's magnitude is < 1, the stem length with mag can find itself below the middle line.
        // in those cases, we have to add the extra amount to it to bring it to a minimum.
        double up_value = ldata->up ? -1. : 1.;
        double stem_start = start_note->ldata()->pos().y();
        double stem_end_mag = stem_start + (final_stem_length * up_value);
        double top_line = 0.0;
        line_distance *= spatium;
        double bottom_line = line_distance * (staff_line_count - 1.0);
        double target = 0.0;
        double mid_line = middle_line / 4.0 * line_distance;
        if (muse::real_is_equal_or_more(line_distance / spatium, 1.0)) {
            // need to extend to middle line, or to opposite line if staff is < 2sp tall
            if (bottom_line < 2 * spatium) {
                target = ldata->up ? top_line : bottom_line;
            } else {
                double two_sp_in = ldata->up ? bottom_line - (2 * spatium) : top_line + (2 * spatium);
                target = muse::real_is_equal(line_distance / spatium, 1.0) ? mid_line : two_sp_in;
            }
        } else {
            // need to extend to second line in staff, or to opposite line if staff has < 3 lines
            if (staff_line_count < 3) {
                target = ldata->up ? top_line : bottom_line;
            } else {
                target = ldata->up ? bottom_line - (2 * line_distance) : top_line + (2 * line_distance);
            }
        }
        extra_length = 0.0;
        if (ldata->up && stem_end_mag > target) {
            extra_length = stem_end_mag - target;
        } else if (!ldata->up && stem_end_mag < target) {
            extra_length = target - stem_end_mag;
        }
    }
    return final_stem_length + extra_length;
}

int StemLayout::min_staff_overlap(bool up, int staff_lines, int beam_count, bool has_hook, double beam_spacing, bool use_wide_beams,
                                  bool is_full_size) {
    int beam_overlap = 8;
    if (is_full_size) {
        if (beam_count == 3 && !has_hook) {
            beam_overlap = 12;
        } else if (beam_count >= 4 && !has_hook) {
            beam_overlap = (beam_count - 4) * beam_spacing + (use_wide_beams ? 16 : 14);
        }
    }

    int staff_line_offset = is_full_size ? 1 : 4;
    int staff_overlap = std::min(beam_overlap, (staff_lines - staff_line_offset) * 4);
    if (!up) {
        return staff_overlap;
    }
    return (staff_lines - 1) * 4 - staff_overlap;
}

double StemLayout::stem_pos_x(const ChordRest *item) { return item->is_chord() ? stem_pos_x(to_chord(item)) : stem_pos_x(to_rest(item)); }

double StemLayout::stem_pos_x(const Chord *item) {
    const StaffType *staff_type = item->staff_type();
    if (!staff_type || !staff_type->is_tab_staff()) {
        bool up = item->ldata()->up;
        const Note *ref_note = up ? item->up_note() : item->down_note();
        PointF stem_attach = up ? ref_note->stem_up_se() : ref_note->stem_down_nw();
        if (stem_attach.is_null()) {
            return up ? item->note_head_width() : 0.0;
        }
        double note_width_offset = up ? (ref_note->head_body_width() - item->note_head_width()) : 0.0;
        return stem_attach.x() - note_width_offset;
    }

    const Note *ref_note = item->ldata()->up ? item->up_note() : item->down_note();
    double x_pos = ref_note->pos().x() + 0.5 * ref_note->width();
    if (item->is_grace_bend_end()) {
        GraceNotesGroup &grace_before = item->grace_notes_before();
        Chord *grace = grace_before.empty() ? nullptr : grace_before.front();
        if (grace) {
            x_pos += grace->pos().x();
        }
    }
    return x_pos;
}

double StemLayout::stem_pos_x(const Rest *item) {
    if (item->ldata()->up) {
        return item->ldata()->bbox().right();
    }

    return item->ldata()->bbox().left();
}

PointF StemLayout::stem_pos(const ChordRest *item) { return item->is_chord() ? stem_pos(to_chord(item)) : stem_pos(to_rest(item)); }

PointF StemLayout::stem_pos(const Chord *item) {
    const Staff *staff = item->staff();
    const StaffType *staff_type = staff ? staff->staff_type_for_element(item) : nullptr;
    if (staff_type && staff_type->is_tab_staff()) {
        return item->page_pos() + rendering::score::StemLayout::tab_stem_pos(item, staff_type) * item->spatium();
    }

    if (item->ldata()->up) {
        const Note *down_note = item->down_note();
        double nhw = item->notes().size() == 1 ? down_note->bbox_right_pos() : item->note_head_width();
        return item->page_pos() + PointF(nhw, down_note->pos().y());
    }

    return item->page_pos() + PointF(0.0, item->up_note()->pos().y());
}

PointF StemLayout::stem_pos(const Rest *item) { return item->page_pos(); }

//---------------------------------------------------------
//   tabRestStemPosY / chordStemPos / chordStemPosBeam / chordStemLength
//
//    computes the stem data for the given chord, according to TAB settings
//    NOTE: unit: spatium, position: relative to chord (DIFFERENT from Chord own functions)
//
//   tabRestStemPosY
//          returns the vertical position of stem start point
//---------------------------------------------------------

double StemLayout::tab_rest_stem_pos_y(const ChordRest *item, const StaffType *st) {
    if (st->stem_through()) { // does not make sense for "stems through staves" setting; just return top line vert. position
        return 0.0;
    }

    // if stems beside staff, position are fixed, but take into account delta for half notes
    double delta // displacement for half note stems (if used)
        =        // if half notes have not a short stem OR not a half note => 0
        (st->minim_style() != TablatureMinimStyle::SHORTER || item->duration_type().type() != DurationType::V_HALF) ?
            0.0 : // if stem is up, displace of half stem length down (positive)
            // if stem is down, displace of half stem length up (negative)
            (item->up() ? -::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_up :
                          ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_dn) *
                0.5;
    // if fret marks above lines and chordRest is up, move half a line distance up
    if (!st->on_lines() && item->up()) {
        delta -= st->line_distance().val() * 0.5;
    }
    double y = (item->up() ? ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemposy_up :
                             (st->lines() - 1) * st->line_distance().val() +
                                 ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemposy_dn) +
               delta;
    return y;
}

//---------------------------------------------------------
//   tabStemLength
//          return length of stem
//---------------------------------------------------------

double StemLayout::tab_stem_length(const Chord *item, const StaffType *st) {
    double stem_len;
    // if stems are through staff, length should be computed by relevant chord algorithm;
    // here, just return default length (= 1 'octave' = 3.5 line spaces)
    if (st->stem_through()) {
        return ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_thru * st->line_distance().val();
    }
    // if stems beside staff, length is fixed, but take into account shorter half note stems
    else {
        bool shrt = (st->minim_style() == TablatureMinimStyle::SHORTER) && (item->duration_type().type() == DurationType::V_HALF);
        stem_len = (st->stems_down() ? ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_dn :
                                       ::mu::engraving::rendering::score::StemLayout::stafftype_tab_defaultstemlen_up) *
                   (shrt ? ::mu::engraving::rendering::score::StemLayout::stafftype_tab_shortstemratio : 1.0) *
                   (item->style().style_b(Sid::USE_WIDE_BEAMS) ? 1.25 : 1.0);
    }
    // scale length by scale of parent chord, but relative to scale of context staff
    return stem_len * item->mag() / item->staff()->staff_mag(item->tick());
}

//---------------------------------------------------------
//   tabStemPos
//    return position of note at other side of beam
//---------------------------------------------------------

PointF StemLayout::tab_stem_pos(const Chord *item, const StaffType *st) {
    double y = 0.0;
    if (st->stem_through()) {
        // if stems are through staff, stem goes from farthest note string
        y = (item->up() ? item->down_string() : item->up_string()) * st->line_distance().val();
    } else {
        // if stems are beside staff, stem start point has a fixed vertical position,
        // according to TAB parameters and stem up/down
        y = tab_rest_stem_pos_y(item, st);
    }
    return PointF(0.5 * item->score()->note_head_width() * item->mag() / item->spatium(), y);
}

int StemLayout::stem_length_beam_addition(const Chord *item, const LayoutContext &ctx) {
    if (item->hook()) {
        return 0;
    }
    const MStyle &style = ctx.conf().style();
    int beam_count = calc_beam_count(item);
    switch (beam_count) {
        case 0:
        case 1:
        case 2: return 0;
        case 3: return 2;
        default: return (beam_count - 3) * (style.style_b(Sid::USE_WIDE_BEAMS) ? 4 : 3);
    }
}

int StemLayout::max_reduction(const Chord *item, const LayoutContext &ctx, int extension_outside_staff) {
    const MStyle &style = ctx.conf().style();
    if (!style.style_b(Sid::SHORTEN_STEM)) {
        return 0;
    }
    // [extensionOutsideStaff][beamCount]
    static const int max_reductions[4][5] = {
        //1sp 1.5sp 2sp 2.5sp >=3sp -- extensionOutsideStaff
        {1, 2, 3, 4, 4}, // 0 beams
        {0, 1, 2, 3, 3}, // 1 beam
        {0, 1, 1, 1, 1}, // 2 beams
        {0, 0, 0, 1, 1}, // 3 beams
    };
    int beam_count = 0;
    if (!item->hook()) {
        beam_count = calc_beam_count(item);
    }
    bool has_trad_hook = item->hook() && !style.style_b(Sid::USE_STRAIGHT_NOTE_FLAGS);
    if (item->hook() && !has_trad_hook) {
        beam_count = std::min(beam_count, 2); // the straight glyphs extend outwards after 2 beams
    }
    if (beam_count >= 4) {
        return 0;
    }
    int extension_half_spaces = floor(extension_outside_staff / 2.0);
    extension_half_spaces = std::min(extension_half_spaces, 4);
    int reduction = max_reductions[beam_count][extension_half_spaces];
    if (item->intrinsic_mag() < 1) {
        // there is an exception for grace-sized stems with hooks.
        // reducing by the full amount puts the hooks too low. Limit reduction to 0.5sp
        if (has_trad_hook) {
            reduction = std::min(reduction, 1);
        }
    } else {
        // there are a few exceptions for normal-sized (non-grace) beams
        if (beam_count == 1 && extension_half_spaces < 2) {
            // 1) if the extension is less than 1sp above or below the staff, they've been adjusted
            //    already to play nicely with staff lines. Reduce by 1sp.
            reduction = 2;
        } else if (beam_count == 3 && extension_half_spaces == 3) {
            // 2) if there are three beams and it extends 1.5sp above or below the staff, we need to
            //    *extend* the stem rather than reduce it.
            reduction = 0;
        }
        if (has_trad_hook) {
            reduction = std::min(reduction, 1);
        } else if (item->hook() && item->beams() > 2) {
            reduction += 1;
        }
    }
    return reduction;
}

int StemLayout::stem_optical_adjustment(const Chord *item, int stem_end_position) {
    if (item->hook() && !item->beam()) {
        return 0;
    }
    int beam_count = calc_beam_count(item);
    if (beam_count == 0 || beam_count > 2) {
        return 0;
    }
    bool is_on_even_line = fmod(stem_end_position + 4, 4) == 2;
    if (is_on_even_line) {
        return 1;
    }
    return 0;
}

int StemLayout::calc_min_stem_length(Chord *item, const LayoutContext &ctx) {
    int min_stem_length = 0; // in quarter spaces
    double spatium = item->spatium();
    const MStyle &style = ctx.conf().style();
    const Chord::LayoutData *ldata = item->ldata();
    const Staff *staff = item->staff();

    if (item->tremolo_single_chord()) {
        // buzz roll's height is actually half of the visual height,
        // so we need to multiply it by 2 to get the actual height
        int buzz_roll_multiplier = item->tremolo_single_chord()->is_buzz_roll() ? 2 : 1;
        min_stem_length += ceil(item->tremolo_single_chord()->min_height() / item->intrinsic_mag() * 4.0 * buzz_roll_multiplier);
        int out_side_padding = style.style_absolute(Sid::TREMOLO_OUT_SIDE_PADDING) / spatium * 4.0;
        int note_side_padding = style.style_absolute(Sid::TREMOLO_NOTE_SIDE_PADDING) / spatium * 4.0;

        int outside_staff_offset = 0;
        if (!staff->is_tab_staff(item->tick())) {
            Note *line_note = ldata->up ? item->up_note() : item->down_note();
            if (line_note->line() == ::mu::engraving::invalid_line) {
                line_note->update_line();
            }

            int line = line_note->line();
            line *= 2; // convert to quarter spaces

            if (!ldata->up && line < -2) {
                outside_staff_offset = -line;
            } else if (ldata->up && line > staff->lines(item->tick()) * 4) {
                outside_staff_offset = line - (staff->lines(item->tick()) * 4) + 4;
            }
        }
        min_stem_length += (out_side_padding + std::max(note_side_padding, outside_staff_offset));

        if (item->hook()) {
            bool straight_flags = style.style_b(Sid::USE_STRAIGHT_NOTE_FLAGS);
            double smufl_anchor = item->hook()->smufl_anchor().y() * (ldata->up ? 1 : -1);
            int hook_offset =
                floor((item->hook()->height() / item->intrinsic_mag() + smufl_anchor) / spatium * 4) - (straight_flags ? 0 : 2);
            // some fonts have hooks that extend very far down (making the height of the hook very large)
            // so we constrain to a reasonable maximum for hook length
            hook_offset = std::min(hook_offset, 11);
            // TODO: when the SMuFL metadata includes a cutout for flags, replace this with that metadata
            // https://github.com/w3c/smufl/issues/203
            int cutout = ldata->up ? 5 : 7;
            if (straight_flags) {
                // don't need cutout for straight flags (they are similar to beams)
                cutout = 0;
            } else if (item->beams() >= 2) {
                // beams greater than two extend outwards and thus don't factor into the cutout
                cutout -= 2;
            }

            min_stem_length += hook_offset - cutout;

            // hooks with trems inside them no longer ceil (snap) to nearest 0.5sp.
            // if we want to add that back in, here is the place to do it:
            // minStemLength = ceil(minStemLength / 2.0) * 2;
        }
    }
    if (item->beam() || item->tremolo_two_chord()) {
        int beam_count = calc_beam_count(item);
        static const int min_inner_stem_lengths[4] = {10, 9, 8, 7};
        int inner_stem_length = min_inner_stem_lengths[std::min(beam_count, 3)];
        int beams_height = beam_count * (style.style_b(Sid::USE_WIDE_BEAMS) ? 4 : 3) - 1;
        int new_min_stem_length = std::max(min_stem_length, inner_stem_length);
        new_min_stem_length += beams_height;
        // for 4+ beams, there are a few situations where we need to lengthen the stem by 1
        int note_line = item->line();
        int staff_lines = staff->lines(item->tick());
        bool note_in_staff = (ldata->up && note_line > 0) || (!ldata->up && note_line < (staff_lines - 1) * 2);
        if (beam_count >= 4 && note_in_staff) {
            new_min_stem_length++;
        }
        min_stem_length = std::max(min_stem_length, new_min_stem_length);
    }
    return min_stem_length;
}

int StemLayout::calc4_beams_exception(const Chord *item, int stem_length) {
    const Chord::LayoutData *ldata = item->ldata();
    int difference = 0;
    int staff_lines = (item->staff()->lines(item->tick()) - 1) * 2;
    if (ldata->up && item->up_note()->line() > staff_lines) {
        difference = item->up_note()->line() - staff_lines;
    } else if (!ldata->up && item->down_note()->line() < 0) {
        difference = std::abs(item->down_note()->line());
    }
    switch (difference) {
        case 2: return std::max(stem_length, 21);
        case 3:
        case 4: return std::max(stem_length, 23);
        default: return stem_length;
    }
}

int StemLayout::calc_beam_count(const Chord *item) {
    const TremoloTwoChord *trem = item->tremolo_two_chord();
    return (trem ? trem->lines() : 0) + (item->beam() ? item->beams() : 0);
}
