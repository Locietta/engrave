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

#include "shadownote.h"

#include "draw/types/pen.h"

#include "accidental.h"
#include "articulation.h"
#include "hook.h"
#include "measure.h"
#include "rest.h"
#include "score.h"
#include "stafftype.h"

using namespace mu;
using namespace muse::draw;

namespace mu::engraving {
//---------------------------------------------------------
//   ShadowNote
//---------------------------------------------------------

ShadowNote::ShadowNote(Score *s) : EngravingItem(ElementType::SHADOW_NOTE, s), notehead_symbol_(SymId::NO_SYM) {
    line_index_ = 1000;
    duration_ = TDuration(DurationType::V_INVALID);
    is_rest_ = false;
}

bool ShadowNote::is_valid() const { return notehead_symbol_ != SymId::NO_SYM; }

void ShadowNote::set_state(SymId note_symbol, TDuration duration, bool rest, bool beyond_score, AccidentalType accidental_type,
                           const std::set<SymId> &articulation_ids) {
    notehead_symbol_ = note_symbol;
    duration_ = duration;
    is_rest_ = rest;
    beyond_score_ = beyond_score;
    accidental_type_ = accidental_type;
    articulation_ids_ = articulation_ids;
}

bool ShadowNote::has_stem() const { return !is_rest_ && duration_.has_stem(); }

bool ShadowNote::has_flag() const { return has_stem() && duration_.hooks() > 0; }

SymId ShadowNote::flag_sym() const {
    if (!has_stem()) {
        return SymId::NO_SYM;
    }

    bool straight = style().style_b(Sid::USE_STRAIGHT_NOTE_FLAGS);
    int hooks = compute_up() ? duration_.hooks() : -duration_.hooks();
    return Hook::sym_id_for_hook_index(hooks, straight);
}

AccidentalType ShadowNote::accidental_type() const { return accidental_type_; }

const std::set<SymId> &ShadowNote::articulation_ids() const { return articulation_ids_; }

bool ShadowNote::ledger_lines_visible() const { return !is_rest_ && line_index_ < 100 && line_index_ > -100; }

//---------------------------------------------------------
//   computeUp
//---------------------------------------------------------

bool ShadowNote::compute_up() const {
    const StaffType *staff_type = this->staff_type();
    bool tab_staff = staff_type && staff_type->is_tab_staff();

    if (tab_staff && (staff_type->stemless() || !staff_type->stem_through())) {
        // if TAB staff with no stems or stem beside staff
        return true;
    }

    if (score()->tick2measure(tick_)->has_voices(staff_idx()) || voice() != 0) {
        return !(voice() & 1);
    } else {
        return line_index_ > (staff_type ? staff_type->middle_line() : 4);
    }
}

// draw articulations, laying them out in a fashion similar to
// ChordLayout::layoutArticulations
void ShadowNote::draw_articulations(Painter *painter) const {
    // Approximate bounding box of note, including stem, relative to notehead
    double notehead_width = sym_width(notehead_symbol_);
    double notehead_height = sym_height(notehead_symbol_);
    double ms = spatium();
    double x1 = notehead_width * .5 - (ms * mag());
    double x2 = x1 + 2 * ms * mag();
    bool up = !compute_up();
    double y01 = -notehead_height * .5;
    double y02 = -y01;
    if (has_stem()) {
        // Stem set at 3.5 spatium in SingleDraw::draw(const ShadowNote* item ...
        if (up) {
            y02 = ms * 3.5;
        } else {
            y01 = -ms * 3.5;
        }
    }
    RectF bound_rect = RectF(PointF(x1, y01), PointF(x2, y02));

    // Place articulations kept close to the note first, near the notehead
    // Articulations close to the note are defined in Articulation::layoutCloseToNote
    bool was_staccato = false; // keep track for special accent-staccato kerning
    SymId prev_articulation = mu::engraving::SymId::NO_SYM;
    for (const SymId &artic : articulation_ids_) {
        String artic_name = Articulation::sym_id2_articulation_name(artic);
        if (artic_name == u"staccato") {
            was_staccato = true;
            draw_close_articulation(painter, artic, bound_rect, up, prev_articulation);
            prev_articulation = artic;
        } else {
            if (artic_name == u"tenuto") {
                draw_close_articulation(painter, artic, bound_rect, up, prev_articulation);
                prev_articulation = artic;
            }
            was_staccato = false;
        }
    }

    // Adjust bounding box to include the whole of the staff segment,
    // then place articulations not closeToNote
    ms *= .5;
    double y1 = -line_index_ * ms;
    double y2 = (this->staff_type()->bottom_line() - line_index_) * ms;
    if (bound_rect.y() > y1) {
        bound_rect.set_top(y1);
        if (up) {
            was_staccato = false;
        }
    }
    if (bound_rect.bottom() < y2) {
        bound_rect.set_bottom(y2);
        if (!up) {
            was_staccato = false;
        }
    }

    for (const SymId &artic : articulation_ids_) {
        String artic_name = Articulation::sym_id2_articulation_name(artic);
        if (artic_name == u"staccato" || artic_name == u"tenuto") {
            continue;
        } else {
            if (artic_name.contains(u"marcato")) {
                // Marcato always above staff
                draw_far_articulation(painter, artic, bound_rect, true, false);
            } else if (artic_name == u"accent") {
                // Accents get special kerning if adjacent to staccato
                draw_far_articulation(painter, artic, bound_rect, up, was_staccato);
            } else {
                // Other articulations (e.g., laissez-vibrer)
                draw_far_articulation(painter, artic, bound_rect, up, false);
            }
            was_staccato = false;
        }
    }
}

void ShadowNote::draw_close_articulation(muse::draw::Painter *painter, const SymId &artic, RectF &bound_rect, bool up,
                                         const SymId &prev_articulation) const {
    PointF coord;
    double spacing = spatium();
    SymId artic_sym = artic;
    double artic_height = sym_height(artic_sym);

    // Center the articulation on the note-head
    coord.rx() = bound_rect.x() + (bound_rect.width() - sym_width(artic)) / 2;

    // Place articulation above or below boundRect and adjust boundRect accordingly
    double min_dist = 0.4 * spacing;
    if (up) {
        // If operating inside the staff, center articulation between lines
        if (line_index_ > 1) {
            coord.ry() = (artic_height - spacing * (3 - (line_index_ & 1))) / 2;
            if (prev_articulation != mu::engraving::SymId::NO_SYM) {
                if (line_index_ > 2) {
                    coord.ry() -= spacing;
                } else {
                    coord.ry() -= min_dist + sym_height(prev_articulation);
                }
            }
        } else {
            double top_y = bound_rect.y();
            coord.ry() = top_y - min_dist;
        }
        bound_rect.set_top(coord.ry() - artic_height);
    } else {
        if (line_index_ < this->staff_type()->bottom_line() - 1) {
            coord.ry() = (artic_height + spacing * (3 - (line_index_ & 1))) / 2;
            if (prev_articulation != mu::engraving::SymId::NO_SYM) {
                if (line_index_ < this->staff_type()->bottom_line() - 2) {
                    coord.ry() += spacing;
                } else {
                    coord.ry() += min_dist + sym_height(prev_articulation);
                }
            }
        } else {
            double bottom_y = bound_rect.bottom();
            coord.ry() = bottom_y + min_dist + artic_height;
        }
        bound_rect.set_bottom(coord.ry());
    }

    draw_symbol(artic_sym, painter, coord);
}

void ShadowNote::draw_far_articulation(muse::draw::Painter *painter, const SymId &artic, RectF &bound_rect, bool up,
                                       bool accent_staccato_kern) const {
    PointF coord;
    double spacing = spatium();
    SymId artic_sym = artic;
    double artic_height = sym_height(artic_sym);

    // If a LaissezVibrer, place beside the note and widen the bounding box
    if (artic_sym == SymId::ARTIC_LAISSEZ_VIBRER_ABOVE || artic_sym == SymId::ARTIC_LAISSEZ_VIBRER_BELOW) {
        artic_sym = up ? SymId::ARTIC_LAISSEZ_VIBRER_ABOVE : SymId::ARTIC_LAISSEZ_VIBRER_BELOW;
        const int up_dir = up ? -1 : 1;
        const double note_height = sym_height(notehead_symbol_);

        double center = 0.5 * sym_width(notehead_symbol_);
        double visual_inset = 0.1 * spacing;

        coord.rx() = center + visual_inset - sym_bbox(artic_sym).left();
        coord.ry() = (note_height / 2 + 0.2 * spacing) * up_dir;
        bound_rect.set_width(bound_rect.width() + sym_width(artic_sym));
    } else {
        // If on the stem side of a note (e.g., marcato on a low note), move artic over for stem,
        // else, center it on the notehead;
        coord.rx() = bound_rect.x() + (bound_rect.width() - sym_width(artic)) / 2;
        if (up == compute_up() && has_stem()) {
            coord.rx() += 0.25 * sym_width(notehead_symbol_);
        }

        // Place articulation above or below boundRect and adjust boundRect accordingly
        if (up) {
            double top_y = bound_rect.y();
            coord.ry() = top_y - (accent_staccato_kern ? 0.2 : 0.4) * spacing;
            bound_rect.set_top(coord.ry() - artic_height);
        } else {
            double bottom_y = bound_rect.bottom();
            coord.ry() = bottom_y + (accent_staccato_kern ? 0.2 : 0.4) * spacing + artic_height;
            bound_rect.set_bottom(coord.ry());
        }
    }

    draw_symbol(artic_sym, painter, coord);
}
} // namespace mu::engraving
