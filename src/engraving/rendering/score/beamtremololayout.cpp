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

#include "beamtremololayout.h"

#include <array>

#include "../dom/beam.h"
#include "../dom/chordrest.h"
#include "../dom/chord.h"
#include "../dom/measure.h"
#include "../dom/note.h"
#include "../dom/score.h"
#include "../dom/staff.h"
#include "../dom/stem.h"
#include "../dom/stemslash.h"
#include "../dom/system.h"
#include "../dom/tremolotwochord.h"

#include "tlayout.h"
#include "chordlayout.h"
#include "stemlayout.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;
using namespace mu::engraving::rendering::score;

constexpr std::array max_slopes = {0, 1, 2, 3, 4, 5, 6, 7};

void BeamTremoloLayout::setup_l_data(const BeamBase *item, BeamBase::LayoutData *ldata, const LayoutContext &ctx) {
    bool is_grace = false;
    IF_ASSERT_FAILED(item && (item->is_beam() || item->is_type(ElementType::TREMOLO_TWOCHORD))) {
        // right now only beams and trems are supported
        return;
    }
    else if (item->is_beam()) {
        ldata->beam_type = BeamType::BEAM;
        ldata->beam = item_cast<const Beam *>(item);
        ldata->up = ldata->beam->up();
        ldata->trem = nullptr; // there can be many different trems in a beam, they will all be checked
        is_grace = ldata->beam->elements().front()->is_grace();
    }
    else {
        ldata->trem = item_cast<const TremoloTwoChord *>(item);
        ldata->beam_type = BeamType::TREMOLO;
        // check to see if there is a beam happening during this trem
        // if so, it needs to be taken into account in trem placement
        if (ldata->trem->chord1()->beam() && ldata->trem->chord1()->beam() == ldata->trem->chord2()->beam()) {
            ldata->beam = ldata->trem->chord1()->beam();
        } else {
            ldata->beam = nullptr;
        }
        ldata->up = compute_tremolo_up(ldata);
        //! HACK Temporary
        const_cast<TremoloTwoChord *>(ldata->trem)->set_up(ldata->up);
        is_grace = ldata->trem->chord1()->is_grace();
    }

    ldata->spatium = item->spatium();
    ldata->tick = item->tick();
    ldata->beam_spacing = ctx.conf().style_b(Sid::USE_WIDE_BEAMS) ? 4 : 3;
    ldata->beam_dist =
        (ldata->beam_spacing / 4.0) * ldata->spatium * ldata->mag() * (is_grace ? ctx.conf().style_d(Sid::GRACE_NOTE_MAG) : 1.);
    ldata->beam_width = (ctx.conf().style_s(Sid::BEAM_WIDTH).val() * ldata->spatium) * ldata->mag();
    const StaffType *staff_type = item->staff_type();
    ldata->tab = (staff_type && staff_type->is_tab_staff()) ? staff_type : nullptr;
    ldata->is_beside_tab_staff = ldata->tab && !ldata->tab->stemless() && !ldata->tab->stem_through();
}

int BeamTremoloLayout::min_stem_length(const ChordRest *cr, const BeamBase::LayoutData *ldata) {
    // min stem lengths in quarter spaces according to how many beams there are (starting with 1)
    static constexpr int beams_count = 9;
    static constexpr int min_stem_lengths[beams_count] = {11, 13, 15, 18, 21, 24, 27, 30, 33};
    int beams = stroke_count(ldata, cr);
    if (beams > beams_count) {
        LOGE() << "Beam count " << beams << " out of range (" << beams_count - 1 << ")";
        return min_stem_lengths[beams_count - 1];
    }

    return min_stem_lengths[std::max(beams - 1, 0)];
}

void BeamTremoloLayout::offset_beam_to_remove_collisions(const BeamBase *item, const BeamBase::LayoutData *ldata,
                                                         const std::vector<ChordRest *> &chord_rests, int &dictator, int &pointer,
                                                         const double start_x, const double end_x, bool is_flat, bool is_start_dictator) {
    if (end_x == start_x) {
        return;
    }

    IF_ASSERT_FAILED(!is_flat || (is_flat == (dictator == pointer))) {
        // This will produce false positives if the pointer & dictator are not flat when they should be
        LOGE() << "Beam is flat, dictator and pointer should be equal";
        pointer = dictator;
    }

    // tolerance eliminates all possibilities of floating point rounding errors
    const double tolerance = ldata->beam_width * 0.25 * (ldata->up ? -1 : 1);
    const bool is_small = ldata->is_grace || ldata->mag() < 1.;

    double start_y = (is_start_dictator ? dictator : pointer) * ldata->spatium / 4 + tolerance;
    double end_y = (is_start_dictator ? pointer : dictator) * ldata->spatium / 4 + tolerance;

    const ChordRest *first_chord_rest = chord_rests.front();
    const ChordRest *last_chord_rest = chord_rests.back();

    size_t cur_chord_rest = 0;
    for (ChordRest *chord_rest : chord_rests) {
        if (!chord_rest->is_chord() || chord_rest == ldata->elements.back() || chord_rest == ldata->elements.front()) {
            cur_chord_rest++;
            continue;
        }

        int same_line_exception = 0;

        if (chord_rest->is_chord() && ((cur_chord_rest == 1 && first_chord_rest->is_chord()) ||
                                       (cur_chord_rest == chord_rests.size() - 2 && last_chord_rest->is_chord()))) {
            const Chord *inner_chord = to_chord(chord_rest);
            const Chord *outer_chord =
                cur_chord_rest == 1 && first_chord_rest->is_chord() ? to_chord(first_chord_rest) : to_chord(last_chord_rest);
            const int inner_line = ldata->up ? inner_chord->up_note()->line() : inner_chord->down_note()->line();
            const int outer_line = ldata->up ? outer_chord->up_note()->line() : outer_chord->down_note()->line();

            if (inner_line == outer_line) {
                same_line_exception = 1;
            } else if (chord_rests.size() == 3) {
                const ChordRest *other_outer_cr = outer_chord == first_chord_rest ? last_chord_rest : first_chord_rest;
                const Chord *other_outer_chord = other_outer_cr->is_chord() ? to_chord(other_outer_cr) : nullptr;
                if (!other_outer_chord) {
                    continue;
                }

                const int other_outer_line = ldata->up ? other_outer_chord->up_note()->line() : other_outer_chord->down_note()->line();
                if (inner_line == other_outer_line) {
                    same_line_exception = 1;
                }
            }
        }

        PointF anchor = chord_beam_anchor(ldata, chord_rest, ChordBeamAnchorType::MIDDLE) - item->page_pos();

        const int min_len = min_stem_length(chord_rest, ldata) - same_line_exception;

        // avoid division by zero for zero-length beams (can exist as a pre-layout state used
        // for horizontal spacing computations)
        if (!muse::real_is_equal(end_x, start_x)) {
            const double proportion_along_x = (anchor.x() - start_x) / (end_x - start_x);
            while (true) {
                const int slope = std::abs(dictator - pointer);
                double reduction = 0.0;
                if (!is_flat) {
                    if (slope <= 3) {
                        reduction = 0.25 * ldata->spatium;
                    } else if (slope <= 6) {
                        reduction = 0.5 * ldata->spatium;
                    } else { // slope > 6
                        reduction = 0.75 * ldata->spatium;
                    }
                }
                // Ensure the beam clears the anchor's height and inner note's stem minimum stem length is enforced
                const double desired_y = proportion_along_x * (end_y - start_y) + start_y; // start note stem len + extra for slope
                const bool beam_clears_anchor = (ldata->up && muse::real_is_equal_or_less(desired_y, anchor.y() + reduction)) ||
                                                (!ldata->up && muse::real_is_equal_or_more(desired_y, anchor.y() - reduction));

                const Note *note = ldata->up ? to_chord(chord_rest)->down_note() : to_chord(chord_rest)->up_note();
                const double note_anchor =
                    (ldata->up ? note->stem_up_se().y() : note->stem_down_nw().y()) + note->page_pos().y() - item->page_pos().y();
                // Resultant length in quarter spaces
                const int desired_len = std::abs(round((desired_y - note_anchor) / ldata->spatium * 4)) + 1;

                if (beam_clears_anchor && desired_len >= round(min_len * chord_rest->mag())) {
                    break;
                }

                if (is_flat || (is_small && dictator == pointer)) {
                    dictator += ldata->up ? -1 : 1;
                    pointer += ldata->up ? -1 : 1;
                } else if (std::abs(dictator - pointer) == 1) {
                    dictator += ldata->up ? -1 : 1;
                } else {
                    pointer += ldata->up ? -1 : 1;
                }

                start_y = (is_start_dictator ? dictator : pointer) * ldata->spatium / 4 + tolerance;
                end_y = (is_start_dictator ? pointer : dictator) * ldata->spatium / 4 + tolerance;
            }
        }
        cur_chord_rest++;
    }
}

void BeamTremoloLayout::offset_beam_with_anchor_shortening(const BeamBase::LayoutData *ldata, const std::vector<ChordRest *> &chord_rests,
                                                           int &dictator, int &pointer, int staff_lines, bool is_start_dictator,
                                                           int stem_length_dictator, const int target_line) {
    const Chord *start_chord = nullptr;
    const Chord *end_chord = nullptr;
    for (const ChordRest *cr : chord_rests) {
        if (cr->is_chord()) {
            end_chord = to_chord(cr);
            if (!start_chord) {
                start_chord = to_chord(cr);
            }
        }
    }
    if (!start_chord) {
        // beam full of only rests, don't adjust this
        return;
    }
    // min stem lengths according to how many beams there are (starting with 1)
    static constexpr int beams_count = 9;
    static constexpr int min_stem_lengths[beams_count] = {11, 13, 15, 18, 21, 24, 27, 30, 33};
    int dictator_beams = stroke_count(ldata, is_start_dictator ? start_chord : end_chord);
    int pointer_beams = stroke_count(ldata, is_start_dictator ? end_chord : start_chord);
    if (dictator_beams > beams_count) {
        LOGE() << "Beam count " << dictator_beams << " out of range (" << beams_count - 1 << ")";
        dictator_beams = beams_count - 1;
    }
    if (pointer_beams > beams_count) {
        LOGE() << "Beam count " << pointer_beams << " out of range (" << beams_count - 1 << ")";
        pointer_beams = beams_count - 1;
    }

    int max_dictator_reduce = stem_length_dictator - min_stem_lengths[std::max(dictator_beams - 1, 0)];
    max_dictator_reduce = std::min(std::abs(dictator - target_line), max_dictator_reduce);

    bool is_flat = dictator == pointer;
    const BeamBase::NotePosition start_pos = BeamBase::NotePosition(start_chord->line(), start_chord->v_staff_idx());
    const BeamBase::NotePosition end_pos = BeamBase::NotePosition(end_chord->line(), end_chord->v_staff_idx());
    bool is_ascending = start_pos > end_pos;
    int toward_beam = ldata->up ? -1 : 1;
    int new_dictator = dictator;
    int new_pointer = pointer;
    int reduce = 0;
    auto four_beam_exception = [](int beams, int y_pos) {
        y_pos += 400; // because there is some weirdness with modular division around zero, add
                      // a large multiple of 4 so that we can guarantee that yPos%4 will be correct
        return beams >= 4 && (y_pos % 4 == 2);
    };
    while (!four_beam_exception(dictator_beams, new_dictator) &&
           !is_valid_beam_position(ldata->up, new_dictator, is_start_dictator, is_ascending, is_flat, staff_lines, true)) {
        if (++reduce > max_dictator_reduce) {
            // we can't shorten this stem at all. bring it back to default and start extending
            new_dictator = dictator;
            new_pointer = pointer;
            while (!is_valid_beam_position(ldata->up, new_dictator, is_start_dictator, is_ascending, is_flat, staff_lines, true)) {
                new_dictator += toward_beam;
                new_pointer += toward_beam;
            }
            break;
        }
        new_dictator += -toward_beam;
        new_pointer += -toward_beam;
    }

    // newDictator is guaranteed either valid, or ==dictator
    // first, constrain pointer to valid position
    new_pointer = ldata->up ? std::min(new_pointer, target_line) : std::max(new_pointer, target_line);
    // walk it back beamwards until we get a position that satisfies both pointer and dictator
    while (!four_beam_exception(dictator_beams, new_dictator) && !four_beam_exception(pointer_beams, new_pointer) &&
           (!is_valid_beam_position(ldata->up, new_dictator, is_start_dictator, is_ascending, is_flat, staff_lines, true) ||
            !is_valid_beam_position(ldata->up, new_pointer, !is_start_dictator, is_ascending, is_flat, staff_lines, true))) {
        if (is_flat) {
            new_dictator += toward_beam;
            new_pointer += toward_beam;
        } else if (std::abs(new_dictator - new_pointer) == 1) {
            new_dictator += toward_beam;
        } else {
            new_pointer += toward_beam;
        }
    }
    dictator = new_dictator;
    pointer = new_pointer;

    if (is_flat) {
        pointer = dictator;
    }
}

void BeamTremoloLayout::extend_stem(const BeamBase::LayoutData *ldata, Chord *chord, double addition) {
    LayoutContext ctx(chord->score());

    PointF anchor = chord_beam_anchor(ldata, chord, ChordBeamAnchorType::MIDDLE);
    double desired_y;
    if (ldata->end_anchor.x() > ldata->start_anchor.x()) {
        double proportion_along_x = (anchor.x() - ldata->start_anchor.x()) / (ldata->end_anchor.x() - ldata->start_anchor.x());
        desired_y = proportion_along_x * (ldata->end_anchor.y() - ldata->start_anchor.y()) + ldata->start_anchor.y();
    } else {
        desired_y = std::max(ldata->end_anchor.y(), ldata->start_anchor.y());
    }

    if (chord->up()) {
        chord->set_beam_extension(anchor.y() - desired_y + addition);
    } else {
        chord->set_beam_extension(desired_y - anchor.y() + addition);
    }
    TLayout::layout_stem(chord->stem(), chord->stem()->mutldata(), ctx.conf());

    if (chord->stem_slash()) {
        TLayout::layout_stem_slash(chord->stem_slash(), chord->stem_slash()->mutldata(), ctx.conf());
    }

    // Update stem shape
    Stem *stem = chord->stem();
    for (ShapeElement &shape_el : chord->segment()->staff_shape(chord->v_staff_idx()).elements()) {
        if (shape_el.item() == stem) {
            shape_el = ShapeElement(stem->ldata()->bbox().translated(stem->pos() + chord->pos()), stem);
        }
    }
}

bool BeamTremoloLayout::is_beam_inside_staff(int y_pos, int staff_lines, bool allow_floater) {
    int above_staff = allow_floater ? -2 : -3;
    int below_staff = (staff_lines - 1) * 4 + (allow_floater ? 2 : 3);
    return y_pos > above_staff && y_pos < below_staff;
}

void BeamTremoloLayout::set_small_inner_beam_pos(const BeamBase::LayoutData *ldata, int &dictator, int &pointer, const int staff_lines,
                                                 const bool is_flat, const bool is_small, const LayoutContext &ctx) {
    // We only want this adjustment if the beam is small and flat
    if (!is_small || !is_flat) {
        return;
    }
    bool note_outside_staff = false;
    int beam_count = 0;
    for (const ChordRest *cr : ldata->elements) {
        if (!cr->is_chord()) {
            continue;
        }
        const Chord *c = to_chord(cr);
        beam_count = std::max(beam_count, stroke_count(ldata, cr));
        note_outside_staff |= (c->down_line() < -2 && !ldata->up) || (c->up_line() >= 11 && ldata->up);
    }

    // AND stems have been extended to the second stave line when the notes are far enough outside of the stave
    if (!note_outside_staff) {
        return;
    }

    // Allow fractions of quarter spaces in inner beam position, as mag can change
    const double mag = ldata->is_grace ? ctx.conf().style().value(Sid::GRACE_NOTE_MAG).to_double() : ldata->mag();
    const double inner_beam = dictator + (beam_count - 1) * (ldata->up ? ldata->beam_spacing : -ldata->beam_spacing) * mag;
    const int above_staff = 1;
    const int below_staff = (staff_lines - 1) * 4 - 1;
    int offset = 0;

    // Make sure the innermost beam is at least 0.5sp within the stave
    if (ldata->up) {
        offset = std::min(offset, int(std::floor(below_staff - inner_beam)));
    } else {
        offset = std::max(offset, int(std::ceil(above_staff - inner_beam)));
    }

    // the beam is flat, so dictator and pointer should match
    dictator += offset;
    pointer = dictator;
}

int BeamTremoloLayout::get_outer_beam_pos_offset(const BeamBase::LayoutData *ldata, int inner_beam, int beam_count, int staff_lines) {
    int spacing = (ldata->up ? -ldata->beam_spacing : ldata->beam_spacing);
    int offset = (beam_count - 1) * spacing;
    bool is_inner = false;
    while (offset != 0 && !is_beam_inside_staff(inner_beam + offset, staff_lines, is_inner)) {
        offset -= spacing;
        is_inner = true;
    }
    return offset;
}

bool BeamTremoloLayout::is_valid_beam_position(const bool is_up, int y_pos, bool is_start, bool is_ascending, bool is_flat, int staff_lines,
                                               bool is_outer) {
    // outside the staff
    bool slants_away = (is_up && is_ascending == is_start) || (!is_up && is_ascending != is_start);
    if (!is_beam_inside_staff(y_pos, staff_lines, is_outer && (slants_away || is_flat))) {
        return true;
    }

    // removes modulo weirdness with negative numbers (i.e., right above staff)
    y_pos += 8;
    // is floater
    if (y_pos % 4 == 2) {
        return false;
    }
    if (is_flat) {
        return true;
    }
    // is on line
    if (y_pos % 4 == 0) {
        return true;
    }
    // is sitting
    if (y_pos % 4 == 3) {
        // return true only if we're starting here and descending, or ascending and ending here
        return is_ascending != is_start;
    }
    // is hanging
    // return true only if we're starting here and ascending, or decending and ending here
    return is_ascending == is_start;
}

bool BeamTremoloLayout::is64th_beam_position_exception(const int beam_spacing, int &y_pos, int staff_lines) {
    if (beam_spacing == 4) {
        return false;
    }
    return y_pos == 2 || y_pos == staff_lines * 4 - 2 || y_pos == staff_lines * 4 - 6 || y_pos == -2;
}

int BeamTremoloLayout::find_valid_beam_offset(const BeamBase::LayoutData *ldata, int outer, int beam_count, int staff_lines, bool is_start,
                                              bool is_ascending, bool is_flat) {
    bool is_beam_valid = false;
    int offset = 0;
    int inner_beam = outer + (beam_count - 1) * (ldata->up ? ldata->beam_spacing : -ldata->beam_spacing);
    while (!is_beam_valid) {
        while (!is_valid_beam_position(ldata->up, inner_beam + offset, is_start, is_ascending, is_flat, staff_lines, beam_count < 2)) {
            offset += ldata->up ? -1 : 1;
        }
        int outer_most_beam = inner_beam + offset + get_outer_beam_pos_offset(ldata, inner_beam + offset, beam_count, staff_lines);
        if (is_valid_beam_position(ldata->up, outer_most_beam, is_start, is_ascending, is_flat, staff_lines, true) ||
            (beam_count == 4 && is64th_beam_position_exception(ldata->beam_spacing, outer_most_beam, staff_lines))) {
            is_beam_valid = true;
        } else {
            offset += ldata->up ? -1 : 1;
        }
    }
    return offset;
}

void BeamTremoloLayout::set_valid_beam_positions(const BeamBase::LayoutData *ldata, int &dictator, int &pointer, int beam_count_d,
                                                 int beam_count_p, int staff_lines, bool is_start_dictator, bool is_flat,
                                                 bool is_ascending) {
    bool are_beams_valid = false;
    bool has3_beams_inside_staff = beam_count_d >= 3 || beam_count_p >= 3;
    while (!are_beams_valid && has3_beams_inside_staff && ldata->beam_spacing != 4) {
        int dictator_inner = dictator + (beam_count_d - 1) * (ldata->up ? ldata->beam_spacing : -ldata->beam_spacing);
        // use dictatorInner for both to simulate flat beams
        int outer_dictator_offset = get_outer_beam_pos_offset(ldata, dictator_inner, beam_count_d, staff_lines);
        if (std::abs(outer_dictator_offset) <= ldata->beam_spacing) {
            has3_beams_inside_staff = false;
            break;
        }
        int offset_d = find_valid_beam_offset(ldata, dictator, beam_count_d, staff_lines, is_start_dictator, false, true);
        int offset_p = find_valid_beam_offset(ldata, pointer, beam_count_p, staff_lines, is_start_dictator, false, true);
        int offset = (offset_d == 0 ? offset_p : offset_d);
        if (pointer == dictator) {
            dictator += offset;
        }
        pointer = dictator;
        if (offset == 0) {
            are_beams_valid = true;
        }
    }
    if (is_flat) {
        // flat beams need more checks (non-dictator/pointer notes with floater inner beams)
        are_beams_valid = false;
    }
    while (!are_beams_valid) {
        int dictator_offset = find_valid_beam_offset(ldata, dictator, beam_count_d, staff_lines, is_start_dictator, is_ascending, is_flat);
        dictator += dictator_offset;
        pointer += dictator_offset;
        if (is_flat) {
            pointer = dictator;
            int curr_offset = 0;
            for (ChordRest *cr : ldata->elements) {
                if (!cr->is_chord() && (cr != ldata->elements.front() && cr != ldata->elements.back())) {
                    continue;
                }
                // we can use dictator beam position because all of the notes have the same beam position
                int beam_count = stroke_count(ldata, cr);
                curr_offset = find_valid_beam_offset(ldata, dictator, beam_count, staff_lines, is_start_dictator, is_ascending, is_flat);
                if (curr_offset) {
                    break;
                }
            }

            if (curr_offset == 0) {
                are_beams_valid = true;
            } else {
                dictator += curr_offset;
                pointer += curr_offset;
            }
        } else {
            pointer += find_valid_beam_offset(ldata, pointer, beam_count_p, staff_lines, !is_start_dictator, is_ascending, is_flat);
            if ((ldata->up && pointer <= dictator) || (!ldata->up && pointer >= dictator)) {
                dictator = pointer + (ldata->up ? -1 : 1);
            } else {
                are_beams_valid = true;
            }
        }
    }
}

void BeamTremoloLayout::add_middle_line_slant(const BeamBase::LayoutData *ldata, int &dictator, int &pointer, int beam_count,
                                              int target_line, int interval, int desired_slant) {
    bool is_small = ldata->mag() < 1. || ldata->is_grace;
    if (interval == 0 || (!is_small && beam_count > 2 && ldata->beam_spacing != 4) || no_slope(ldata->beam)) {
        return;
    }
    bool is_on_target_line = pointer == target_line && (std::abs(pointer - dictator) < 2);
    if (is_on_target_line) {
        if (std::abs(desired_slant) == 1 || interval == 1 || (beam_count == 2 && ldata->beam_spacing != 4 && !is_small)) {
            dictator = target_line + (ldata->up ? -1 : 1);
        } else {
            dictator = target_line + (ldata->up ? -2 : 2);
        }
    }
}

void BeamTremoloLayout::add8th_space_slant(BeamBase::LayoutData *ldata, PointF &dictator_anchor, int dictator, int pointer, int beam_count,
                                           int interval, int target_line, bool is_flat) {
    if (beam_count != 3 || no_slope(ldata->beam) || ldata->beam_spacing != 3) {
        return;
    }
    if ((is_flat && dictator != target_line) || (dictator != pointer) || interval == 0) {
        return;
    }
    if ((ldata->up && (dictator + 4) % 4 == 3) || (!ldata->up && (dictator + 4) % 4 == 1)) {
        return;
    }
    dictator_anchor.set_y(dictator_anchor.y() + (ldata->up ? -0.125 * ldata->spatium : 0.125 * ldata->spatium));
    ldata->beam_dist += 0.0625 * ldata->spatium;
}

bool BeamTremoloLayout::compute_tremolo_up(const BeamBase::LayoutData *ldata) {
    if (!ldata->beam || !ldata->beam->cross()) {
        return ldata->trem->up();
    }
    Chord *c1 = ldata->trem->chord1();
    Chord *c2 = ldata->trem->chord2();
    int staff_move = 0;
    for (ChordRest *cr : ldata->beam->elements()) {
        if (cr->staff_move() != 0) {
            staff_move = cr->staff_move();
            break;
        }
    }
    // for inset trems in cross-staff beams, there are two potentialities:
    // 1) both trem notes are on the same staff
    if (c1->staff_move() == c2->staff_move()) {
        // (in which case, up is opposite to the move)
        if (staff_move < 0) {
            return c1->staff_move() != staff_move;
        } else {
            return c1->staff_move() == staff_move;
        }
    } else {
        // or 2) the trem notes are between staves. what a pain.
        return ldata->trem->up();
    }
}

int BeamTremoloLayout::stroke_count(const BeamBase::LayoutData *ldata, const ChordRest *cr) {
    if (cr->is_rest()) {
        return cr->beams();
    }
    int strokes = 0;
    const Chord *c = to_chord(cr);
    if (ldata->beam_type == BeamType::TREMOLO) {
        strokes = ldata->trem->lines();
    } else if (ldata->beam_type == BeamType::BEAM && c->tremolo_two_chord()) {
        strokes = c->tremolo_two_chord()->lines();
    }
    strokes += ldata->beam ? cr->beams() : 0;
    return strokes;
}

bool BeamTremoloLayout::calculate_anchors(const BeamBase *item, BeamBase::LayoutData *ldata, const LayoutContext &ctx,
                                          const std::vector<ChordRest *> &chord_rests,
                                          const std::vector<BeamBase::NotePosition> &note_positions) {
    ldata->start_anchor = PointF();
    ldata->end_anchor = PointF();
    if (ldata->beam_type == BeamType::TREMOLO && ldata->beam) {
        // this is a trem inside a beam, and we are currently calculating the anchors of the tremolo.
        // we can do this as long as the beam has been layed out first: it saves trem anchor positions
        ChordRest *cr1 = ldata->trem->chord1();
        ChordRest *cr2 = ldata->trem->chord2();
        double anchor_x1 = chord_beam_anchor_x(ldata, cr1, ChordBeamAnchorType::MIDDLE);
        double anchor_x2 = chord_beam_anchor_x(ldata, cr2, ChordBeamAnchorType::MIDDLE);
        for (const TremAnchor &t : ldata->beam->trem_anchors()) {
            if (t.chord1 == cr1) {
                ldata->start_anchor = PointF(anchor_x1, t.y1);
                ldata->end_anchor = PointF(anchor_x2, t.y2);
                ldata->slope = (t.y2 - t.y1) / (anchor_x2 - anchor_x1);
                return true;
            }
        }
        // if we finish the loop before we find the anchor for this tremolo, it could mean that the
        // beam hasn't been laid out yet
        return false;
    }
    Chord *start_chord = nullptr;
    Chord *end_chord = nullptr;
    ChordRest *start_cr = nullptr;
    ChordRest *end_cr = nullptr;
    ldata->elements = chord_rests;
    if (chord_rests.empty()) {
        return false;
    }

    bool has_chord_above_beam = false;
    bool has_chord_below_beam = false;

    for (auto chord_rest : chord_rests) {
        if (!start_cr) {
            start_cr = chord_rest;
        }
        end_cr = chord_rest;
        if (chord_rest->is_chord()) {
            if (!start_chord) {
                start_chord = to_chord(chord_rest);
                end_chord = start_chord;
            } else {
                end_chord = to_chord(chord_rest);
            }
            ChordLayout::layout_stem(to_chord(chord_rest), ctx);
        }
        if (chord_rest->is_below_cross_beam(item)) {
            has_chord_below_beam = true;
        } else {
            has_chord_above_beam = true;
        }
    }
    if (!start_chord) {
        // we were passed a vector of only rests. we don't support beams across only rests
        // this beam will be deleted in LayoutBeams
        return false;
    }
    ldata->is_grace = start_chord->is_grace();
    ldata->note_positions = note_positions;

    // Set cross staff beam position, above, below or between staves
    ldata->cross_staff_beam_pos = BeamBase::CrossStaffBeamPosition::INVALID;
    if (item->min_cr_move() != item->max_cr_move()) {
        if (has_chord_above_beam && !has_chord_below_beam) {
            ldata->cross_staff_beam_pos = BeamBase::CrossStaffBeamPosition::BELOW;
        } else if (has_chord_below_beam && !has_chord_above_beam) {
            ldata->cross_staff_beam_pos = BeamBase::CrossStaffBeamPosition::ABOVE;
        } else if (has_chord_above_beam && has_chord_below_beam) {
            ldata->cross_staff_beam_pos = BeamBase::CrossStaffBeamPosition::BETWEEN;
        }
    }

    if (calculate_anchors_cross(item, ldata, ctx.conf())) {
        return true;
    }

    const int closest_move =
        ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::ABOVE ? item->min_cr_move() : item->max_cr_move();
    const staff_idx_t closest_staff_to_beam = ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::ABOVE ||
                                                      ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::BELOW ?
                                                  item->staff_idx() + closest_move :
                                                  start_chord->v_staff_idx();

    std::vector<Chord *> chords_closest_to_beam;

    for (ChordRest *cr : ldata->elements) {
        if (cr->is_chord() && cr->v_staff_idx() == closest_staff_to_beam) {
            chords_closest_to_beam.push_back(to_chord(cr));
        }
    }

    ldata->start_anchor = chord_beam_anchor(ldata, start_chord, ChordBeamAnchorType::START);
    ldata->end_anchor = chord_beam_anchor(ldata, end_chord, ChordBeamAnchorType::END);

    double start_length = start_chord->default_stem_length();
    double end_length = end_chord->default_stem_length();
    double start_anchor_base = ldata->start_anchor.y() + (ldata->up ? start_length : -start_length);
    double end_anchor_base = ldata->end_anchor.y() + (ldata->up ? end_length : -end_length);
    int start_note = ldata->up ? start_chord->up_note()->line() : start_chord->down_note()->line();
    int end_note = ldata->up ? end_chord->up_note()->line() : end_chord->down_note()->line();
    if (ldata->tab) {
        start_note = ldata->up ? start_chord->up_string() : start_chord->down_string();
        end_note = ldata->up ? end_chord->up_string() : end_chord->down_string();
    }
    staff_idx_t start_staff = start_chord->v_staff_idx();
    staff_idx_t end_staff = end_chord->v_staff_idx();
    BeamBase::NotePosition start_pos = BeamBase::NotePosition(start_note, start_staff);
    BeamBase::NotePosition end_pos = BeamBase::NotePosition(end_note, end_staff);

    const int interval = std::abs(start_note - end_note);
    const bool is_start_dictator = ldata->up ? start_pos < end_pos : start_pos > end_pos;
    const double quarter_space = ldata->spatium / 4;
    PointF start_anchor = ldata->start_anchor - item->page_pos();
    PointF end_anchor = ldata->end_anchor - item->page_pos();
    int dictator = round((is_start_dictator ? start_anchor.y() : end_anchor.y()) / quarter_space);
    int pointer = round((is_start_dictator ? end_anchor.y() : start_anchor.y()) / quarter_space);

    const int staff_lines = ctx.dom().staff(closest_staff_to_beam)->lines(ldata->tick);
    // This is the middle of the stave if the notes are normal size or the second line from the bottom if notes are small
    const int target_line =
        get_target_staff_line(ldata, ctx, start_chord, end_chord, staff_lines, closest_staff_to_beam, item->staff_idx());

    int slant = compute_desired_slant(item, ldata, start_pos, end_pos, chords_closest_to_beam, target_line, dictator, pointer);
    bool is_flat = slant == 0;
    SlopeConstraint special_slant = is_flat ? get_slope_constraint(ldata, start_pos, end_pos) : SlopeConstraint::NO_CONSTRAINT;
    bool force_flat = special_slant == SlopeConstraint::FLAT;
    bool small_slant = special_slant == SlopeConstraint::SMALL_SLOPE;
    if (is_flat) {
        dictator = ldata->up ? std::min(pointer, dictator) : std::max(pointer, dictator);
        pointer = dictator;
    } else {
        if ((dictator > pointer) != (is_start_dictator ? start_pos > end_pos : end_pos > start_pos)) {
            dictator = pointer - slant;
        } else {
            pointer = dictator + slant;
        }
    }
    bool is_ascending = start_pos > end_pos;
    int beam_count_d = stroke_count(ldata, is_start_dictator ? start_chord : end_chord);
    int beam_count_p = stroke_count(ldata, is_start_dictator ? end_chord : start_chord);

    int stem_length_start = std::abs(round((start_anchor_base - ldata->start_anchor.y()) / ldata->spatium * 4));
    int stem_length_end = std::abs(round((end_anchor_base - ldata->end_anchor.y()) / ldata->spatium * 4));
    int stem_length_dictator = is_start_dictator ? stem_length_start : stem_length_end;
    bool is_small = ldata->mag() < 1. || ldata->is_grace;
    if (end_anchor.x() > start_anchor.x()) {
        /* When beam layout is called before horizontal spacing (see LayoutMeasure::getNextMeasure() to
         * know why) the x positions aren't yet determined and may be all zero, which would cause the
         * following function to get stuck in a loop. The if() condition avoids that case. */

        // Make sure grace & small note inner beams are within the stave
        set_small_inner_beam_pos(ldata, dictator, pointer, staff_lines, is_flat, is_small, ctx);

        if (!is_small) {
            // Adjust anchor stems
            offset_beam_with_anchor_shortening(ldata, chord_rests, dictator, pointer, staff_lines, is_start_dictator, stem_length_dictator,
                                               target_line);
        }
        // Adjust inner stems
        offset_beam_to_remove_collisions(item, ldata, chord_rests, dictator, pointer, start_anchor.x(), end_anchor.x(), is_flat,
                                         is_start_dictator);
    }

    int beam_count = std::max(beam_count_d, beam_count_p);
    if (!ldata->tab) {
        if (!ldata->is_grace) {
            set_valid_beam_positions(ldata, dictator, pointer, beam_count_d, beam_count_p, staff_lines, is_start_dictator, is_flat,
                                     is_ascending);
        }
        if (!force_flat) {
            add_middle_line_slant(ldata, dictator, pointer, beam_count, target_line, interval, small_slant ? 1 : slant);
        }
    }

    ldata->start_anchor.set_y(quarter_space * (is_start_dictator ? dictator : pointer) + item->page_pos().y());
    ldata->end_anchor.set_y(quarter_space * (is_start_dictator ? pointer : dictator) + item->page_pos().y());

    bool add8th = true;
    if (item->is_type(ElementType::BEAM) && item_cast<const Beam *>(item)->user_modified()) {
        add8th = false;
    }
    if (!ldata->tab && add8th) {
        add8th_space_slant(ldata, is_start_dictator ? ldata->start_anchor : ldata->end_anchor, dictator, pointer, beam_count, interval,
                           target_line, is_flat);
    }
    ldata->start_anchor.set_x(chord_beam_anchor_x(ldata, start_cr, ChordBeamAnchorType::START));
    ldata->end_anchor.set_x(chord_beam_anchor_x(ldata, end_cr, ChordBeamAnchorType::END));
    return true;
}

bool BeamTremoloLayout::calculate_anchors_cross(const BeamBase *item, BeamBase::LayoutData *ldata, const LayoutConfiguration &conf) {
    if (ldata->cross_staff_beam_pos != BeamBase::CrossStaffBeamPosition::BETWEEN) {
        // The beam is either not cross or above/below and must be layed out like a non-cross beam
        return false;
    }

    double spatium = conf.spatium();
    ChordRest *start_cr = ldata->elements.front();
    ChordRest *end_cr = ldata->elements.back();

    const double quarter_space = spatium / 4;
    // imagine a line of beamed notes all in a row on the same staff. the first and last of those
    // are the 'outside' notes, and the slant of the beam is going to be affected by the 'middle' notes
    // between them.
    // we have to keep track of this for both staves.
    Chord *top_first = nullptr;
    Chord *top_last = nullptr;
    Chord *bottom_first = nullptr;
    Chord *bottom_last = nullptr;
    int max_middle_top_line = std::numeric_limits<int>::min();    // lowest note in the top staff
    int min_middle_bottom_line = std::numeric_limits<int>::max(); // highest note in the bottom staff
    int prev_top_line = max_middle_top_line;                      // previous note's line position (top)
    int prev_bottom_line = min_middle_bottom_line;                // previous note's line position (bottom)
    // if the immediate neighbor of one of the two 'outside' notes on either the top or bottom
    // are the same as that outside note, we need to record it so that we can add a 1/4 space slant.
    bool second_top_is_same = false;
    bool second_bottom_is_same = false;
    bool penultimate_top_is_same = false;
    bool penultimate_bottom_is_same = false;
    double max_y = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::min();
    // Get switches between above & below the beam
    // The direction is neutral if there is more than one switch
    bool prev_below_beam = ldata->elements.front()->is_below_cross_beam(item);
    bool has_beam_side_switch = false;
    int beam_side_switch_direction = 0;

    // Get switches between staves on the same side of a beam
    // The direction is neutral is there is more than one switch
    staff_idx_t prev_staff = ldata->elements.front()->v_staff_idx();
    bool has_staff_switch = false;
    int staff_switch_direction = 0;

    // Find the notes on the top and bottom of staves
    //
    bool check_next_top = false;
    bool check_next_bottom = false;
    for (ChordRest *cr : ldata->elements) {
        if (!cr->is_chord()) {
            continue;
        }
        Chord *c = to_chord(cr);

        if (c->is_below_cross_beam(item) == prev_below_beam) {
            c->v_staff_idx();
            if (prev_staff != c->v_staff_idx()) {
                int diff = static_cast<int>(prev_staff - c->v_staff_idx());
                staff_switch_direction = has_staff_switch ? 0 : (diff < 0 ? -1 : 1);
                has_staff_switch = true;
            }
        }
        prev_staff = c->v_staff_idx();

        if (c->is_below_cross_beam(item)) {
            // this chord is on the bottom staff
            if (penultimate_bottom_is_same) {
                // the chord we took as the penultimate bottom note wasn't.
                // so treat it properly as a middle note
                min_middle_bottom_line = std::min(min_middle_bottom_line, prev_bottom_line);
                penultimate_bottom_is_same = false;
            }
            check_next_top = false; // we are no longer looking for the second note in the top
                                    // staff being the same as the first--this note is on the bottom.
            if (!bottom_first) {
                bottom_first = c;
                check_next_bottom = true; // this was the first bottom note, so check for second next time
            } else {
                penultimate_bottom_is_same = prev_bottom_line == c->line();
                if (!penultimate_bottom_is_same) {
                    min_middle_bottom_line = std::min(min_middle_bottom_line, prev_bottom_line);
                }
                if (check_next_bottom) {
                    // this is the second bottom note, so we should see if this one is same line as first
                    second_bottom_is_same = c->line() == bottom_first->line();
                    check_next_bottom = false;
                } else {
                    prev_bottom_line = c->line();
                }
                bottom_last = c;
            }
            if (!prev_below_beam) {
                beam_side_switch_direction = has_beam_side_switch ? 0 : -1;
                has_beam_side_switch = true;
            }
            prev_below_beam = true;
            max_y = std::min(max_y, chord_beam_anchor_y(ldata, to_chord(c)));
        } else {
            // this chord is on the top staff
            if (penultimate_top_is_same) {
                // the chord we took as the penultimate top note wasn't.
                // so treat it properly as a middle note
                max_middle_top_line = std::max(max_middle_top_line, prev_top_line);
                penultimate_top_is_same = false;
            }
            check_next_bottom = false; // no longer looking for a bottom second note since this is on top
            if (!top_first) {
                top_first = c;
                check_next_top = true;
            } else {
                penultimate_top_is_same = prev_top_line == c->line();
                if (!penultimate_top_is_same) {
                    max_middle_top_line = std::max(max_middle_top_line, prev_top_line);
                }
                if (check_next_top) {
                    second_top_is_same = c->line() == top_first->line();
                    check_next_top = false;
                } else {
                    prev_top_line = c->line();
                }
                top_last = c;
            }
            if (prev_below_beam) {
                beam_side_switch_direction = has_beam_side_switch ? 0 : 1;
                has_beam_side_switch = true;
            }
            prev_below_beam = false;
            min_y = std::max(min_y, chord_beam_anchor_y(ldata, to_chord(c)));
        }
    }

    double y_mid_point = (max_y + min_y) / 2;
    if (item->is_beam()) {
        // Cross-staff beams may have segments both above and below the midline.
        // Compute the y-difference between the top and bottom segment and aim for centering in between.
        const Beam *beam = to_beam(item);
        const std::vector<BeamSegment *> &beam_segments = beam->beam_segments();
        const Chord *limiting_chord_above = beam->ldata()->limiting_chord_above;
        const Chord *limiting_chord_below = beam->ldata()->limiting_chord_below;
        if (!beam_segments.empty() && limiting_chord_above && limiting_chord_below) {
            double y_center_segment = 0.5 * (beam_segments.front()->line.y1() + beam_segments.front()->line.y2());

            const BeamSegment *top_beam_segment_for_chord_above = beam->top_level_segment_for_element(limiting_chord_above);
            double y_top_seg_above = 0.5 * (top_beam_segment_for_chord_above->line.y1() + top_beam_segment_for_chord_above->line.y2());
            double dist_above = y_top_seg_above - y_center_segment;

            const BeamSegment *top_beam_segment_for_chord_below = beam->top_level_segment_for_element(limiting_chord_below);
            double y_top_seg_below = 0.5 * (top_beam_segment_for_chord_below->line.y1() + top_beam_segment_for_chord_below->line.y2());
            double dist_below = y_top_seg_below - y_center_segment;

            double average_dist = 0.5 * (dist_above + dist_below);
            y_mid_point -= 0.5 * average_dist;
        }
    }

    ldata->start_anchor.set_y(y_mid_point);
    ldata->end_anchor.set_y(y_mid_point);
    ldata->start_anchor.set_x(chord_beam_anchor_x(ldata, start_cr, ChordBeamAnchorType::START));
    ldata->end_anchor.set_x(chord_beam_anchor_x(ldata, end_cr, ChordBeamAnchorType::END));

    ldata->slope = 0;

    // Calculates & sets slope
    if (!no_slope(ldata->beam)) {
        int top_first_line = top_first ? top_first->down_note()->line() : 0;
        int top_last_line = top_last ? top_last->down_note()->line() : 0;
        int bottom_first_line = bottom_first ? bottom_first->up_note()->line() : 0;
        int bottom_last_line = bottom_last ? bottom_last->up_note()->line() : 0;
        bool constrain_top_to_quarter = false;
        bool constrain_bottom_to_quarter = false;
        if ((top_first_line > top_last_line && second_top_is_same) || (top_first_line < top_last_line && penultimate_top_is_same)) {
            constrain_top_to_quarter = true;
        }
        if ((bottom_first_line < bottom_last_line && second_bottom_is_same) ||
            (bottom_first_line > bottom_last_line && penultimate_bottom_is_same)) {
            constrain_bottom_to_quarter = true;
        }
        if (!top_last && !bottom_last && top_first && bottom_first) {
            // if there are only two notes, one on each staff, special case
            // take max slope into account
            double y_first, y_last;
            // we can't rely on comparing topFirst and bottomFirst ->tick() because beamed
            // graces have the same tick
            if (ldata->elements[0] == top_first) {
                y_first = StemLayout::stem_pos(top_first).y();
                y_last = StemLayout::stem_pos(bottom_first).y();
            } else {
                y_first = StemLayout::stem_pos(bottom_first).y();
                y_last = StemLayout::stem_pos(top_first).y();
            }
            int desired_slant = round((y_first - y_last) / spatium);
            int slant = std::min(std::abs(desired_slant), get_max_slope(ldata));
            slant *= (desired_slant < 0) ? -quarter_space : quarter_space;
            ldata->start_anchor.ry() += (slant / 2);
            ldata->end_anchor.ry() -= (slant / 2);
        } else {
            // Get the direction of groups of notes on top and bottom staves and the direction of switches between sides of the beam and switches between staves
            // Directions are up, down or neutral
            bool force_horiz = false;
            bool single_note_top = false;
            bool single_note_bottom = false;

            int top_slant = top_first_line - top_last_line;
            if (constrain_top_to_quarter && top_slant != 0) {
                top_slant = top_first_line < top_last_line ? -1 : 1;
            }
            if (!top_last) {
                // If there's only one note, set direction to neutral
                top_slant = 0;
                single_note_top = true;
            }
            int bottom_slant = bottom_first_line - bottom_last_line;
            if (constrain_bottom_to_quarter && bottom_slant != 0) {
                bottom_slant = bottom_first_line < bottom_last_line ? -1 : 1;
            }
            if (!bottom_last) {
                // If there's only one note, set direction to neutral
                bottom_slant = 0;
                single_note_bottom = true;
            }
            if ((max_middle_top_line >= std::max(top_first_line, top_last_line) ||
                 (min_middle_bottom_line <= std::min(bottom_first_line, bottom_last_line)))) {
                force_horiz = true;
            }

            const int top_slant_dir = top_slant <= 0 ? (top_slant == 0 ? 0 : -1) : 1;
            const int bottom_slant_dir = bottom_slant <= 0 ? (bottom_slant == 0 ? 0 : -1) : 1;
            // Check all directions which aren't neutral (0) match
            // If there is any contradiction between top staff direction, bottom direction, beam side switch direction and staff switch direction, force horizontal
            // If all are neutral, force horizontal
            int overall_direction = 0;
            for (int direction : {top_slant_dir, bottom_slant_dir, beam_side_switch_direction, staff_switch_direction}) {
                if (direction != 0) {
                    overall_direction = direction;
                    break;
                }
            }

            const bool overall_slant_flat = overall_direction == 0;
            const bool top_slant_overall_diff = (top_slant_dir != 0 && top_slant_dir != overall_direction);
            const bool bottom_slant_overall_diff = (bottom_slant_dir != 0 && bottom_slant_dir != overall_direction);
            const bool beam_side_switch_overall_diff = (beam_side_switch_direction != 0 && beam_side_switch_direction != overall_direction);
            const bool staff_switch_overall_diff = (staff_switch_direction != 0 && staff_switch_direction != overall_direction);
            const bool flatten_neutral_beam_side_switch =
                beam_side_switch_direction == 0 && (top_slant_dir != bottom_slant_dir) && !(single_note_top || single_note_bottom);

            force_horiz =
                force_horiz || overall_slant_flat || flatten_neutral_beam_side_switch ||
                (top_slant_overall_diff || bottom_slant_overall_diff || beam_side_switch_overall_diff || staff_switch_overall_diff);

            if (!force_horiz) {
                int slant = 0;
                if (top_slant == 0) {
                    slant = bottom_slant;
                } else if (bottom_slant == 0) {
                    slant = top_slant;
                } else {
                    slant = (std::abs(top_slant) < std::abs(bottom_slant)) ? top_slant : bottom_slant;
                }

                const int abs_slant = std::min(std::abs(slant), get_max_slope(ldata));
                const double slope = abs_slant * ((slant < 0) ? -quarter_space : quarter_space);
                ldata->start_anchor.ry() += (slope / 2);
                ldata->end_anchor.ry() -= (slope / 2);
            }
        }
        ldata->slope = (ldata->end_anchor.y() - ldata->start_anchor.y()) / (ldata->end_anchor.x() - ldata->start_anchor.x());
    }
    return true;
}

bool BeamTremoloLayout::no_slope(const Beam *beam) { return beam && beam->no_slope(); }

int BeamTremoloLayout::get_target_staff_line(const BeamBase::LayoutData *ldata, const LayoutContext &ctx, const ChordRest *start_chord,
                                             const ChordRest *end_chord, const int staff_lines, const staff_idx_t beam_closest_staff_idx,
                                             const staff_idx_t actual_beam_staff_idx) {
    bool is_full_size = muse::real_is_equal(ldata->mag(), 1.0) && !ldata->is_grace;
    bool use_wide_beams = ctx.conf().style_b(Sid::USE_WIDE_BEAMS);
    int start_beams = stroke_count(ldata, start_chord);
    int end_beams = stroke_count(ldata, end_chord);
    int start_target_line =
        StemLayout::min_staff_overlap(ldata->up, staff_lines, start_beams, false, ldata->beam_spacing / 4.0, use_wide_beams, is_full_size);
    int end_target_line = StemLayout::min_staff_overlap(ldata->up, staff_lines, end_beams, false, ldata->beam_spacing / 4.0, use_wide_beams,
                                                        !ldata->is_grace);

    int diff = 0;
    // Get diff between beam closest staff and actual staff, unless its full cross?
    if (ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::ABOVE ||
        ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::BELOW) {
        // Make sure we calculate the distance to the target line on the stave closest to the beam
        const System *cur_sys = start_chord->measure()->system();
        const int dir = ldata->up ? -1 : 1;
        if (cur_sys && beam_closest_staff_idx != actual_beam_staff_idx) {
            diff = std::round(std::abs(cur_sys->staff_ypage(beam_closest_staff_idx) - cur_sys->staff_ypage(actual_beam_staff_idx)) /
                              ldata->spatium * 4 * dir);
        }
    }
    // offset target line by 1 or -1 since the anchor is at the middle of the beam,
    // not at the tip of the stem
    if (ldata->up) {
        return std::min(start_target_line + diff, end_target_line + diff) + 1;
    }
    return std::max(start_target_line + diff, end_target_line + diff) - 1;
}

int BeamTremoloLayout::compute_desired_slant(const BeamBase *item, const BeamBase::LayoutData *ldata,
                                             const BeamBase::NotePosition &start_pos, const BeamBase::NotePosition &end_pos,
                                             std::vector<Chord *> closest_chords_to_beam, int target_line, int dictator, int pointer) {
    if (item->is_type(ElementType::BEAM) && item_cast<const Beam *>(item)->no_slope()) {
        return 0;
    }
    int dictator_extension = target_line - dictator; // we need to make sure that beams extended to the middle line
    int pointer_extension = target_line - pointer;   // are properly treated as flat.
    if (ldata->up) {
        dictator_extension = std::min(dictator_extension, 0);
        pointer_extension = std::min(pointer_extension, 0);
    } else {
        dictator_extension = std::max(dictator_extension, 0);
        pointer_extension = std::max(pointer_extension, 0);
    }
    if (dictator + dictator_extension == target_line && pointer + pointer_extension == target_line) {
        return 0;
    }
    if (start_pos == end_pos) {
        return 0;
    }
    SlopeConstraint slope_constrained = get_slope_constraint(ldata, start_pos, end_pos);
    if (slope_constrained == SlopeConstraint::FLAT) {
        return 0;
    } else if (slope_constrained == SlopeConstraint::SMALL_SLOPE) {
        return dictator > pointer ? -1 : 1;
    }

    // calculate max slope based on distance between first and last chords
    int max_slope = get_max_slope(ldata);

    if (ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::ABOVE ||
        ldata->cross_staff_beam_pos == BeamBase::CrossStaffBeamPosition::BELOW) {
        // Calculate slant between first and last notes
        int beam_dir = start_pos == end_pos ? 0 : (start_pos > end_pos ? 1 : -1);

        // Calculate slant between notes on stave closest to the beam
        size_t closest_chords_size = closest_chords_to_beam.size();
        BeamBase::NotePosition closest_start_pos(closest_chords_size > 0 ? closest_chords_to_beam.front()->line() : 0,
                                                 closest_chords_size > 0 ? closest_chords_to_beam.front()->v_staff_idx() : 0);
        BeamBase::NotePosition closest_end_pos(closest_chords_size > 0 ? closest_chords_to_beam.back()->line() : 0,
                                               closest_chords_size > 0 ? closest_chords_to_beam.back()->v_staff_idx() : 0);
        int stave_closest_to_beam_dir = closest_start_pos == closest_end_pos ? 0 : (closest_start_pos > closest_end_pos ? 1 : -1);

        // Contradiction, beam must be flat
        if (beam_dir != stave_closest_to_beam_dir && beam_dir != 0 && stave_closest_to_beam_dir != 0) {
            return 0;
        }

        // When the notes on the stave closest to the beam have a neutral direction, set a slant of 1/-1 in the direction of the staff change between first and last chords
        if (stave_closest_to_beam_dir == 0 && beam_dir != 0) {
            if (start_pos.staff != end_pos.staff) {
                return std::abs(beam_dir) * (ldata->up ? 1 : -1);
            }
        }
    }

    // calculate max slope based on note interval
    int interval = std::min(std::abs(end_pos.line - start_pos.line), (int) ::max_slopes.size() - 1);
    return std::min(max_slope, ::max_slopes[interval]) * (ldata->up ? 1 : -1);
}

SlopeConstraint BeamTremoloLayout::get_slope_constraint(const BeamBase::LayoutData *ldata, const BeamBase::NotePosition &start_pos,
                                                        const BeamBase::NotePosition &end_pos) {
    if (ldata->note_positions.empty()) {
        return SlopeConstraint::NO_CONSTRAINT;
    }

    // 0 to constrain to flat, 1 to constrain to 0.25, <0 for no constraint
    if (start_pos == end_pos) {
        return SlopeConstraint::FLAT;
    } else if (ldata->beam_type == BeamType::TREMOLO) {
        // tremolos don't need the small slope constraint since they only have two notes
        return SlopeConstraint::NO_CONSTRAINT;
    }

    // if a note is more extreme than the endpoints, slope is 0
    // p.s. _notes is a sorted vector
    const std::vector<ChordRest *> &elements = ldata->elements;
    const std::vector<BeamBase::NotePosition> &note_positions = ldata->note_positions;

    if (elements.size() > 2) {
        if (ldata->up) {
            BeamBase::NotePosition higher_end = std::min(start_pos, end_pos);
            if (higher_end > note_positions[0]) {
                return SlopeConstraint::FLAT; // a note is higher in the staff than the highest end
            }
            if (higher_end == note_positions[0] && higher_end >= note_positions[1]) {
                if (higher_end > note_positions[1]) {
                    return SlopeConstraint::FLAT; // a note is higher in the staff than the highest end
                }
                size_t chord_count = elements.size();
                if (chord_count >= 3 && note_positions.size() >= 3) {
                    bool middle_note_higher_than_higher_end = higher_end >= note_positions[2];
                    if (middle_note_higher_than_higher_end) {
                        return SlopeConstraint::
                            FLAT; // two notes are the same as the highest end (notes [0] [1] and [2] higher than or same as higherEnd)
                    }
                    Chord *second_note = elements[1]->is_chord() ? to_chord(elements[1]) : nullptr;
                    Chord *second_to_last_note = elements[chord_count - 2]->is_chord() ? to_chord(elements[chord_count - 2]) : nullptr;
                    bool second_note_same_height_as_higher_end =
                        start_pos < end_pos && second_note &&
                        BeamBase::NotePosition(second_note->up_line(), second_note->v_staff_idx()) == higher_end;
                    bool second_to_last_note_same_height_as_higher_end =
                        end_pos.line < start_pos.line && second_to_last_note &&
                        BeamBase::NotePosition(second_to_last_note->up_line(), second_to_last_note->v_staff_idx()) == higher_end;
                    if (!(second_note_same_height_as_higher_end || second_to_last_note_same_height_as_higher_end)) {
                        return SlopeConstraint::FLAT; // only one note same as higher end, but it is not a neighbor
                    } else {
                        // there is a single note next to the highest one with equivalent height
                        // and they are neighbors. this is our exception, so
                        // the slope may be a max of 0.25.
                        return SlopeConstraint::SMALL_SLOPE;
                    }
                } else {
                    return SlopeConstraint::FLAT; // only two notes in entire beam, in this case startNote == endNote
                }
            }
        } else {
            BeamBase::NotePosition lower_end = std::max(start_pos, end_pos);
            if (lower_end < note_positions[note_positions.size() - 1]) {
                return SlopeConstraint::FLAT;
            }
            if (lower_end == note_positions[note_positions.size() - 1] && lower_end <= note_positions[note_positions.size() - 2]) {
                if (lower_end < note_positions[note_positions.size() - 2]) {
                    return SlopeConstraint::FLAT;
                }
                size_t chord_count = elements.size();
                if (chord_count >= 3 && note_positions.size() >= 3) {
                    bool middle_note_lower_than_lower_end = lower_end <= note_positions[note_positions.size() - 3];
                    if (middle_note_lower_than_lower_end) {
                        return SlopeConstraint::FLAT;
                    }
                    const Chord *second_note = elements[1]->is_chord() ? to_chord(elements[1]) : nullptr;
                    const Chord *second_to_last_note =
                        elements[chord_count - 2]->is_chord() ? to_chord(elements[chord_count - 2]) : nullptr;
                    const BeamBase::NotePosition second_note_pos =
                        second_note ? BeamBase::NotePosition(second_note->down_line(), second_note->v_staff_idx()) :
                                      BeamBase::NotePosition(0, muse::nidx);
                    const BeamBase::NotePosition second_to_last_note_pos =
                        second_to_last_note ? BeamBase::NotePosition(second_to_last_note->down_line(), second_to_last_note->v_staff_idx()) :
                                              BeamBase::NotePosition(0, muse::nidx);

                    bool second_note_same_height_as_lower_end = start_pos.line > end_pos.line && second_note_pos == lower_end;
                    bool second_to_last_note_same_height_as_lower_end =
                        end_pos.line > start_pos.line && second_to_last_note_pos == lower_end;
                    if (!(second_note_same_height_as_lower_end || second_to_last_note_same_height_as_lower_end)) {
                        return SlopeConstraint::FLAT;
                    } else {
                        return SlopeConstraint::SMALL_SLOPE;
                    }
                } else {
                    return SlopeConstraint::FLAT;
                }
            }
        }
    }
    return SlopeConstraint::NO_CONSTRAINT;
}

int BeamTremoloLayout::get_max_slope(const BeamBase::LayoutData *ldata) {
    // for 2-indexed interval i (seconds, thirds, etc.)
    // maxSlopes[i] = max slope of beam for notes with interval i

    // calculate max slope based on distance between first and last chords
    double end_x = chord_beam_anchor_x(ldata, ldata->elements[ldata->elements.size() - 1], ChordBeamAnchorType::START);
    double start_x = chord_beam_anchor_x(ldata, ldata->elements[0], ChordBeamAnchorType::END);
    double beam_width = end_x - start_x;
    beam_width /= ldata->spatium;
    int max_slope = ::max_slopes.back();
    if (beam_width < 3.0) {
        max_slope = ::max_slopes[1];
    } else if (beam_width < 5.0) {
        max_slope = ::max_slopes[2];
    } else if (beam_width < 7.5) {
        max_slope = ::max_slopes[3];
    } else if (beam_width < 10.0) {
        max_slope = ::max_slopes[4];
    } else if (beam_width < 15.0) {
        max_slope = ::max_slopes[5];
    } else if (beam_width < 20.0) {
        max_slope = ::max_slopes[6];
    } else {
        max_slope = ::max_slopes[7];
    }

    return max_slope;
}

int BeamTremoloLayout::get_beam_count(const BeamBase::LayoutData *ldata, const std::vector<const ChordRest *> &chord_rests) {
    int max_beams = 0;
    for (const ChordRest *chord_rest : chord_rests) {
        if (chord_rest->is_chord() && stroke_count(ldata, chord_rest) > max_beams) {
            max_beams = stroke_count(ldata, chord_rest);
        }
    }
    return max_beams;
}

double BeamTremoloLayout::chord_beam_anchor_x(const BeamBase::LayoutData *ldata, const ChordRest *cr, ChordBeamAnchorType anchor_type) {
    double page_pos_x = ldata->trem ? ldata->trem->page_pos().x() : ldata->beam ? ldata->beam->page_pos().x() : 0.0;
    double stem_pos_x = StemLayout::stem_pos_x(cr) + cr->page_pos().x() - page_pos_x;

    if (!cr->is_chord() || !to_chord(cr)->stem()) {
        if (!ldata->up) {
            // rests always return the right side of the glyph as their stemPosX
            // so we need to adjust back to the left side if stems are down
            stem_pos_x -= StemLayout::stem_pos_x(cr);
        }
        return stem_pos_x;
    }
    const Chord *chord = to_chord(cr);
    const Stem *stem = chord->stem();

    double stem_width = stem->line_width_mag();

    switch (anchor_type) {
        case ChordBeamAnchorType::START:
            if (ldata->tab) {
                return stem_pos_x - 0.5 * stem_width;
            }

            if (chord->up()) {
                return stem_pos_x - stem_width;
            }

            break;
        case ChordBeamAnchorType::MIDDLE:
            if (ldata->tab) {
                return stem_pos_x;
            }

            return chord->up() ? stem_pos_x - 0.5 * stem_width : stem_pos_x + 0.5 * stem_width;

        case ChordBeamAnchorType::END:
            if (ldata->tab) {
                return stem_pos_x + 0.5 * stem_width;
            }

            if (!chord->up()) {
                return stem_pos_x + stem_width;
            }

            break;
    }

    return stem_pos_x;
}

double BeamTremoloLayout::chord_beam_anchor_y(const BeamBase::LayoutData *ldata, const ChordRest *cr) {
    if (!cr->is_chord()) {
        return cr->page_pos().y();
    }

    const Chord *chord = to_chord(cr);
    Note *note = cr->up() ? chord->down_note() : chord->up_note();
    PointF position = note->page_pos();

    int up_value = chord->up() ? -1 : 1;
    double beam_offset = ldata->beam_width / 2 * up_value;

    if (ldata->is_beside_tab_staff) {
        double stem_length = StemLayout::tab_stem_length(chord, ldata->tab) * (ldata->up ? -1 : 1);
        double y = StemLayout::tab_rest_stem_pos_y(chord, ldata->tab) + stem_length;
        y *= ldata->spatium;
        y -= beam_offset;
        return y + chord->page_pos().y();
    }

    return position.y() - note->offset().y() + (chord->default_stem_length() * up_value) - beam_offset;
}

PointF BeamTremoloLayout::chord_beam_anchor(const BeamBase::LayoutData *ldata, const ChordRest *cr, ChordBeamAnchorType anchor_type) {
    return PointF(chord_beam_anchor_x(ldata, cr, anchor_type), chord_beam_anchor_y(ldata, cr));
}
