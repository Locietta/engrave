/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "flip.h"

#include <functional>
#include <set>

#include "../dom/articulation.h"
#include "../dom/beam.h"
#include "../dom/chord.h"
#include "../dom/chordbracket.h"
#include "../dom/guitarbend.h"
#include "../dom/hairpin.h"
#include "../dom/hook.h"
#include "../dom/note.h"
#include "../dom/ornament.h"
#include "../dom/score.h"
#include "../dom/select.h"
#include "../dom/slurtie.h"
#include "../dom/spanner.h"
#include "../dom/staff.h"
#include "../dom/stem.h"
#include "../dom/tremolotwochord.h"
#include "../dom/trill.h"
#include "../dom/tuplet.h"
#include "log.h"

using namespace mu::engraving;

//---------------------------------------------------------
//   flip
//---------------------------------------------------------

void Flip::flip(Transaction &, Score *score) {
    const std::vector<EngravingItem *> &el = score->selection().elements();
    if (el.empty()) {
        MScore::set_error(MsError::NO_FLIPPABLE_SELECTED);
        return;
    }

    std::set<const EngravingItem *> already_flipped_elements;
    auto flip_once = [&already_flipped_elements](const EngravingItem *element, std::function<void()> flip_function) -> void {
        if (already_flipped_elements.insert(element).second) {
            flip_function();
        }
    };

    for (EngravingItem *e : el) {
        if (e->has_voice_assignment_properties()) {
            flip_once(e, [e]() {
                PlacementV cur_placement = e->get_property(Pid::PLACEMENT).value<PlacementV>();
                e->undo_change_property(Pid::DIRECTION, cur_placement == PlacementV::ABOVE ? DirectionV::DOWN : DirectionV::UP);
            });
        } else if (e->is_note() || e->is_stem() || e->is_hook()) {
            Chord *chord = nullptr;
            if (e->is_note()) {
                chord = to_note(e)->chord();
            } else if (e->is_stem()) {
                chord = to_stem(e)->chord();
            } else {
                chord = to_hook(e)->chord();
            }

            IF_ASSERT_FAILED(chord) { continue; }

            if (chord->beam()) {
                if (!score->selection().is_range()) {
                    e = chord->beam();
                } else {
                    continue;
                }
            } else if (chord->tremolo_two_chord()) {
                if (!score->selection().is_range()) {
                    e = chord->tremolo_two_chord();
                } else {
                    continue;
                }
            } else {
                flip_once(chord, [chord]() {
                    DirectionV dir = chord->up() ? DirectionV::DOWN : DirectionV::UP;
                    chord->undo_change_property(Pid::STEM_DIRECTION, dir);
                });
            }
        }

        if (e->is_beam()) {
            Beam *beam = to_beam(e);
            flip_once(beam, [beam]() {
                if (beam->cross()) {
                    int new_cross_staff_move = beam->cross_staff_move() + 1;
                    if (beam->accept_cross_staff_move(new_cross_staff_move)) {
                        beam->undo_change_property(Pid::BEAM_CROSS_STAFF_MOVE, new_cross_staff_move);
                    } else {
                        beam->undo_change_property(Pid::BEAM_CROSS_STAFF_MOVE, beam->min_cr_move() - beam->default_cross_staff_idx());
                    }
                } else {
                    DirectionV dir = beam->up() ? DirectionV::DOWN : DirectionV::UP;
                    beam->undo_change_property(Pid::STEM_DIRECTION, dir);
                }
            });
        } else if (e->is_type(ElementType::TREMOLO_TWOCHORD)) {
            TremoloTwoChord *tremolo = item_cast<TremoloTwoChord *>(e);
            flip_once(tremolo, [tremolo]() {
                DirectionV dir = tremolo->up() ? DirectionV::DOWN : DirectionV::UP;
                tremolo->undo_change_property(Pid::STEM_DIRECTION, dir);
            });
        } else if (e->is_slur_tie_segment()) {
            auto slur_tie_segment = to_slur_tie_segment(e)->slur_tie();
            flip_once(slur_tie_segment, [slur_tie_segment]() {
                DirectionV dir = slur_tie_segment->up() ? DirectionV::DOWN : DirectionV::UP;
                slur_tie_segment->undo_change_property(Pid::SLUR_DIRECTION, dir);
            });
        } else if (e->is_articulation_family()) {
            auto artic = to_articulation(e);
            flip_once(artic, [artic]() {
                ArticulationAnchor artic_anchor = artic->anchor();
                switch (artic_anchor) {
                    case ArticulationAnchor::TOP: artic_anchor = ArticulationAnchor::BOTTOM; break;
                    case ArticulationAnchor::BOTTOM: artic_anchor = ArticulationAnchor::TOP; break;
                    case ArticulationAnchor::AUTO: artic_anchor = artic->up() ? ArticulationAnchor::BOTTOM : ArticulationAnchor::TOP; break;
                }
                PropertyFlags pf = artic->property_flags(Pid::ARTICULATION_ANCHOR);
                if (pf == PropertyFlags::STYLED) {
                    pf = PropertyFlags::UNSTYLED;
                }
                artic->undo_change_property(Pid::ARTICULATION_ANCHOR, int(artic_anchor), pf);
            });
        } else if (e->is_tuplet()) {
            auto tuplet = to_tuplet(e);
            flip_once(tuplet, [tuplet]() {
                DirectionV dir = tuplet->is_up() ? DirectionV::DOWN : DirectionV::UP;
                tuplet->undo_change_property(Pid::DIRECTION, PropertyValue::from_value<DirectionV>(dir), PropertyFlags::UNSTYLED);
            });
        } else if (e->is_note_dot() && e->ownership_parent()->is_note()) {
            Note *note = to_note(e->ownership_parent());
            DirectionV d = note->dot_is_up() ? DirectionV::DOWN : DirectionV::UP;
            note->undo_change_property(Pid::DOT_POSITION, PropertyValue::from_value<DirectionV>(d));
        } else if (e->is_chord_bracket()) {
            ChordBracket *cb = to_chord_bracket(e);
            flip_once(cb, [cb]() {
                DirectionV d = cb->hook_pos() == DirectionV::UP ? DirectionV::DOWN : DirectionV::UP;
                cb->undo_change_property(Pid::BRACKET_HOOK_POS, PropertyValue::from_value<DirectionV>(d));
            });
        } else if (e->is_guitar_bend_segment()) {
            GuitarBend *bend = to_guitar_bend_segment(e)->guitar_bend();
            flip_once(bend, [bend] {
                DirectionV direction = bend->ldata()->up() ? DirectionV::DOWN : DirectionV::UP;
                bend->undo_change_property(Pid::DIRECTION, PropertyValue::from_value<DirectionV>(direction));
            });
        } else if (e->is_trill_segment()) {
            TrillSegment *trill_segment = to_trill_segment(e);
            Trill *trill = trill_segment->trill();
            Ornament *ornament = trill->ornament();

            flip_once(ornament, [ornament]() {
                ArticulationAnchor artic_anchor = ArticulationAnchor(ornament->get_property(Pid::ARTICULATION_ANCHOR).to_int());

                switch (artic_anchor) {
                    case ArticulationAnchor::TOP: artic_anchor = ArticulationAnchor::BOTTOM; break;
                    case ArticulationAnchor::BOTTOM: artic_anchor = ArticulationAnchor::TOP; break;
                    case ArticulationAnchor::AUTO:
                        artic_anchor = ornament->up() ? ArticulationAnchor::BOTTOM : ArticulationAnchor::TOP;
                        break;
                }
                PropertyFlags pf = ornament->property_flags(Pid::ARTICULATION_ANCHOR);
                if (pf == PropertyFlags::STYLED) {
                    pf = PropertyFlags::UNSTYLED;
                }
                ornament->undo_change_property(Pid::ARTICULATION_ANCHOR, int(artic_anchor), pf);
            });
        } else if (e->is_staff_text() || e->is_system_text() || e->is_tempo_text() || e->is_triplet_feel() || e->is_jump() ||
                   e->is_marker() || e->is_rehearsal_mark() || e->is_measure_number() || e->is_instrument_change() ||
                   e->is_play_tech_annotation() || e->is_capo() || e->is_string_tunings() || e->is_sticking() || e->is_fingering() ||
                   e->is_harmony() || e->is_fret_diagram() || e->is_harp_pedal_diagram() || e->is_ottava() || e->is_ottava_segment() ||
                   e->is_text_line() || e->is_text_line_segment() || e->is_let_ring() || e->is_let_ring_segment() || e->is_vibrato() ||
                   e->is_vibrato_segment() || e->is_palm_mute() || e->is_palm_mute_segment() || e->is_whammy_bar() ||
                   e->is_whammy_bar_segment() || e->is_rasgueado() || e->is_rasgueado_segment() || e->is_harmonic_mark() ||
                   e->is_harmonic_mark_segment() || e->is_gradual_tempo_change() || e->is_gradual_tempo_change_segment() || e->is_pedal() ||
                   e->is_pedal_segment() || e->is_lyrics() || e->is_breath() || e->is_fermata() || e->is_hammer_on_pull_off_text()) {
            e->undo_change_property(Pid::AUTOPLACE, true);
            // TODO: undoChangeProperty() should probably do this directly
            // see https://musescore.org/en/node/281432
            EngravingItem *ee = to_engraving_item(e->property_delegate(Pid::PLACEMENT));
            if (!ee) {
                ee = e;
            }

            flip_once(ee, [ee]() {
                // getProperty() delegates call from spannerSegment to Spanner
                PlacementV p = ee->get_property(Pid::PLACEMENT).value<PlacementV>();
                p = (p == PlacementV::ABOVE) ? PlacementV::BELOW : PlacementV::ABOVE;
                PropertyFlags pf = ee->property_flags(Pid::PLACEMENT);
                if (pf == PropertyFlags::STYLED) {
                    pf = PropertyFlags::UNSTYLED;
                }
                double old_default_y = ee->property_default(Pid::OFFSET).value<PointF>().y();
                ee->undo_change_property(Pid::PLACEMENT, p, pf);
                // flip and rebase user offset to new default now that placement has changed
                double new_default_y = ee->property_default(Pid::OFFSET).value<PointF>().y();
                if (ee->is_spanner()) {
                    Spanner *spanner = to_spanner(ee);
                    for (SpannerSegment *ss : spanner->spanner_segments()) {
                        if (!ss->offset().is_null()) {
                            PointF off = ss->get_property(Pid::OFFSET).value<PointF>();
                            double old_y = off.y() - old_default_y;
                            off.ry() = new_default_y - old_y;
                            ss->undo_change_property(Pid::OFFSET, off);
                        }
                    }
                } else if (!ee->offset().is_null()) {
                    PointF off = ee->get_property(Pid::OFFSET).value<PointF>();
                    double old_y = off.y() - old_default_y;
                    off.ry() = new_default_y - old_y;
                    ee->undo_change_property(Pid::OFFSET, off);
                }
            });
        }
    }
}

//---------------------------------------------------------
//   flipHorizontally
//---------------------------------------------------------

void Flip::flip_horizontally(Transaction &, Score *score) {
    const std::vector<EngravingItem *> &el = score->selection().elements();
    if (el.empty()) {
        MScore::set_error(MsError::NO_FLIPPABLE_SELECTED);
        return;
    }

    std::set<const EngravingItem *> already_flipped_elements;
    auto flip_once = [&already_flipped_elements](const EngravingItem *element, std::function<void()> flip_function) -> void {
        if (already_flipped_elements.insert(element).second) {
            flip_function();
        }
    };

    for (EngravingItem *e : el) {
        if (e->is_hairpin_segment() || e->is_hairpin()) {
            Hairpin *h = e->is_hairpin() ? to_hairpin(e) : to_hairpin_segment(e)->hairpin();
            flip_once(h, [h] {
                if (h->hairpin_type() == HairpinType::CRESC_HAIRPIN) {
                    h->undo_change_property(Pid::HAIRPIN_TYPE, int(HairpinType::DIM_HAIRPIN));
                } else if (h->hairpin_type() == HairpinType::DIM_HAIRPIN) {
                    h->undo_change_property(Pid::HAIRPIN_TYPE, int(HairpinType::CRESC_HAIRPIN));
                } else if (h->hairpin_type() == HairpinType::CRESC_LINE) {
                    h->undo_change_property(Pid::HAIRPIN_TYPE, int(HairpinType::DIM_LINE));
                } else if (h->hairpin_type() == HairpinType::DIM_LINE) {
                    h->undo_change_property(Pid::HAIRPIN_TYPE, int(HairpinType::CRESC_LINE));
                }
            });
        } else if (e->is_chord_bracket()) {
            ChordBracket *cb = to_chord_bracket(e);
            flip_once(cb, [cb] { cb->undo_change_property(Pid::BRACKET_RIGHT_SIDE, !cb->right_side()); });
        }
    }
}

//---------------------------------------------------------
//   mirrorNoteHead
//---------------------------------------------------------

void Flip::mirror_note_head(Transaction &, Score *score) {
    const std::vector<EngravingItem *> &el = score->selection().elements();
    for (EngravingItem *e : el) {
        if (e->is_note()) {
            Note *note = to_note(e);
            if (note->staff() && note->staff()->is_tab_staff(note->chord()->tick())) {
                e->undo_change_property(Pid::DEAD, !note->dead_note());
            } else {
                DirectionH d = note->user_mirror();
                if (d == DirectionH::AUTO) {
                    d = note->chord()->up() ? DirectionH::RIGHT : DirectionH::LEFT;
                } else {
                    d = d == DirectionH::LEFT ? DirectionH::RIGHT : DirectionH::LEFT;
                }
                note->undo_change_property(Pid::MIRROR_HEAD, d);
            }
        }
    }
}
