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

#include "edithairpin.h"

#include "../dom/anchors.h"
#include "../dom/chord.h"
#include "../dom/chordrest.h"
#include "../dom/dynamic.h"
#include "../dom/factory.h"
#include "../dom/hairpin.h"
#include "../dom/measure.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"

using namespace mu::engraving;

std::vector<Hairpin *> EditHairpin::add_hairpins(Transaction &tx, Score *score, HairpinType type) {
    std::vector<Hairpin *> hairpins;

    // add hairpin on each staff if possible
    const Selection &selection = score->selection();
    if (selection.is_range() && selection.staff_start() != selection.staff_end() - 1) {
        for (staff_idx_t staff_idx = selection.staff_start(); staff_idx < selection.staff_end(); ++staff_idx) {
            ChordRest *cr1 = selection.first_chord_rest(staff_idx * ::mu::engraving::voices);
            ChordRest *cr2 = selection.last_chord_rest(staff_idx * ::mu::engraving::voices);
            Hairpin *h = cr1 ? add_hairpin(tx, score, type, cr1, cr2) :
                               add_hairpin(tx, score, type, selection.tick_start(), selection.tick_end(), staff2track(staff_idx));
            if (h) {
                hairpins.push_back(h);
            }
        }
    } else {
        // for single staff range selection, or single selection,
        // find start & end elements elements
        ChordRest *cr1 = nullptr;
        ChordRest *cr2 = nullptr;
        score->get_selected_start_end_chord_rests(cr1, cr2);
        Hairpin *h = add_hairpin(tx, score, type, cr1, cr2);
        if (h) {
            hairpins.push_back(h);
        }
    }

    for (Hairpin *hairpin : hairpins) {
        hairpin->set_initial_track_and_voice_assignment(hairpin->track(), /*curVoiceOnlyOverride*/ false);
    }

    return hairpins;
}

Hairpin *EditHairpin::add_hairpin(Transaction &tx, Score *score, HairpinType type, ChordRest *cr1, ChordRest *cr2) {
    if (!cr1) {
        return nullptr;
    }

    Hairpin *hairpin = Factory::create_hairpin(score->dummy());
    hairpin->set_hairpin_type(type);
    if (type == HairpinType::CRESC_LINE) {
        hairpin->set_begin_text(u"cresc.");
        hairpin->set_continue_text(u"(cresc.)");
    } else if (type == HairpinType::DIM_LINE) {
        hairpin->set_begin_text(u"dim.");
        hairpin->set_continue_text(u"(dim.)");
    }

    add_hairpin(tx, score, hairpin, cr1, cr2);

    return hairpin;
}

Hairpin *EditHairpin::add_hairpin(Transaction &, Score *score, HairpinType type, Fraction s_tick, Fraction e_tick, track_idx_t track) {
    Hairpin *hairpin = Factory::create_hairpin(score->dummy());
    hairpin->set_hairpin_type(type);
    if (type == HairpinType::CRESC_LINE) {
        hairpin->set_begin_text(u"cresc.");
        hairpin->set_continue_text(u"(cresc.)");
    } else if (type == HairpinType::DIM_LINE) {
        hairpin->set_begin_text(u"dim.");
        hairpin->set_continue_text(u"(dim.)");
    }

    hairpin->set_track(track);
    hairpin->set_tick(s_tick);
    hairpin->set_tick2(e_tick);

    score->undo_add_element(hairpin);

    return hairpin;
}

void EditHairpin::add_hairpin(Transaction &, Score *score, Hairpin *hairpin, ChordRest *cr1, ChordRest *cr2) {
    track_idx_t track = cr1->track();
    Fraction start_tick = cr1->tick();

    Fraction end_tick = start_tick;
    if (score->selection().is_range()) {
        end_tick = score->selection().tick_end();
    } else {
        if (cr2 && cr2 != cr1) {
            end_tick = cr2->end_tick();
        } else {
            end_tick = cr1->is_chord() ? to_chord(cr1)->end_tick_including_tied() : cr1->end_tick();
        }
        const Segment *start_segment = cr2 ? cr2->segment() : cr1->segment();
        for (const Segment *segment = start_segment; segment && segment->tick() < end_tick;
             segment = segment->next1(SegmentType::DURATION)) {
            if (segment == start_segment) {
                continue;
            }
            if (segment->find_annotation(ElementType::DYNAMIC, track, track)) {
                end_tick = segment->tick();
                break;
            }
        }
    }

    hairpin->set_track(track);
    hairpin->set_track2(track);
    hairpin->set_tick(start_tick);
    hairpin->set_tick2(end_tick);

    score->undo_add_element(hairpin);
}

void EditHairpin::add_hairpin_to_dynamic(Transaction &, Score *score, Hairpin *hairpin, Dynamic *dynamic) {
    track_idx_t track = dynamic->track();
    hairpin->set_track(track);
    hairpin->set_track2(track);

    hairpin->set_tick(dynamic->tick());

    Segment *dynamic_segment = dynamic->segment();

    Chord *start_chord = nullptr;
    for (Segment *segment = dynamic_segment; segment; segment = segment->prev(SegmentType::CHORD_REST)) {
        EngravingItem *element = segment->element(track);
        if (element && element->is_chord()) {
            start_chord = to_chord(element);
            break;
        }
    }

    Fraction end_tick = start_chord ? start_chord->end_tick_including_tied() : dynamic->segment()->measure()->end_tick();

    for (Segment *segment = dynamic_segment; segment && segment->tick() < end_tick; segment = segment->next1(SegmentType::DURATION)) {
        if (segment == dynamic_segment) {
            continue;
        }
        if (segment->find_annotation(ElementType::DYNAMIC, track, track)) {
            end_tick = segment->tick();
            break;
        }
    }

    hairpin->set_tick2(end_tick);

    hairpin->set_voice_assignment(dynamic->voice_assignment());

    score->undo_add_element(hairpin);
}

Hairpin *EditHairpin::add_hairpin_to_dynamic_on_grip_drag(Transaction &, Score *score, Dynamic *dynamic, bool is_left_grip,
                                                          const muse::PointF &pos) {
    const track_idx_t track = dynamic->track();
    staff_idx_t staff_index = dynamic->staff_idx();
    Segment *seg = nullptr;
    constexpr double spacing_factor = 0.5;

    // Ensure time tick segments are created
    EditTimeTickAnchors::update_anchors(dynamic);

    // Find segment of type ChordRest or TimeTick near cursor postion
    score->drag_position(pos, &staff_index, &seg, spacing_factor, /*allowTimeAnchor*/ true);

    const bool has_valid_tick = seg && (is_left_grip ? seg->tick() < dynamic->tick() : seg->tick() > dynamic->tick());
    if (!has_valid_tick) {
        return nullptr;
    }

    Hairpin *hairpin = Factory::create_hairpin(score->dummy());
    hairpin->set_hairpin_type(is_left_grip ? HairpinType::DIM_HAIRPIN : HairpinType::CRESC_HAIRPIN);

    hairpin->set_track(track);
    hairpin->set_track2(track);

    if (is_left_grip) {
        hairpin->set_tick(seg->tick());
        hairpin->set_tick2(dynamic->tick());
    } else {
        hairpin->set_tick(dynamic->tick());
        hairpin->set_tick2(seg->tick());
    }

    hairpin->set_voice_assignment(dynamic->voice_assignment());

    score->undo_add_element(hairpin);

    return hairpin;
}
