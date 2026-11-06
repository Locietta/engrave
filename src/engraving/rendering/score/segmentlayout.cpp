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
#include "segmentlayout.h"

#include "chordlayout.h"
#include "dom/drumset.h"
#include "dom/parenthesis.h"
#include "dom/part.h"
#include "dom/staff.h"
#include "horizontalspacing.h"
#include "log.h"
#include "tlayout.h"

using namespace mu::engraving::rendering::score;

void SegmentLayout::layout_measure_independent_elements(const Segment &segment, track_idx_t track, const LayoutContext &ctx) {
    LAYOUT_CALL() << segment.type_name() << "(" << segment.eid() << ")";

    if (segment.is_just_type(SegmentType::KEY_SIG)) {
        KeySig *ks = to_key_sig(segment.element(track));
        if (ks) {
            TLayout::layout_key_sig(ks, ks->mutldata(), ctx.conf()); // LD_INDEPENDENT
        }
    } else if (segment.is_just_type(SegmentType::CLEF)) {
        Clef *cl = item_cast<Clef *>(segment.element(track));
        if (cl) {
            cl->set_small(true);
            TLayout::layout_clef(cl, cl->mutldata(), ctx.conf()); // LD_INDEPENDENT
        }
    } else if (segment.is_type(SegmentType::HEADER_CLEF)) {
        Clef *cl = item_cast<Clef *>(segment.element(track));
        if (cl) {
            TLayout::layout_clef(cl, cl->mutldata(), ctx.conf()); // LD_INDEPENDENT
        }
    } else if (segment.is_type(SegmentType::TIME_SIG)) {
        TimeSig *ts = item_cast<TimeSig *>(segment.element(track));
        if (ts) {
            TLayout::layout_time_sig(ts, ts->mutldata(), ctx); // LD_INDEPENDENT
        }
    } else if (segment.is_type(SegmentType::AMBITUS)) {
        Ambitus *am = item_cast<Ambitus *>(segment.element(track));
        if (am) {
            TLayout::layout_ambitus(am, am->mutldata(), ctx); // LD_INDEPENDENT
        }
    } else if (segment.is_type(SegmentType::BAR_LINE)) {
        BarLine *bl = to_bar_line(segment.element(track));
        if (bl) {
            // check conditions (see TLayout::layoutBarLine)
            {
                for (const EngravingItem *e : *bl->el()) {
                    if (e->is_type(ElementType::ARTICULATION)) {
                        LD_CONDITION(item_cast<const Articulation *>(e)->ldata()->sym_id.has_value());
                    }
                }
            }
            TLayout::layout_bar_line(bl, bl->mutldata(), ctx);
        }
    }
}

void SegmentLayout::set_chord_mag(const Staff *staff, const Segment &segment, track_idx_t start_track, track_idx_t end_track,
                                  const LayoutConfiguration &conf) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(staff) << " " << segment.type_name() << "(" << segment.eid() << ")";

    IF_ASSERT_FAILED(segment.is_just_type(SegmentType::CHORD_REST)) { return; }

    const double staff_mag = staff->staff_mag(&segment);
    const double small_note_mag = conf.style_d(Sid::SMALL_NOTE_MAG);
    const double grace_note_mag = conf.style_d(Sid::GRACE_NOTE_MAG);

    for (track_idx_t t = start_track; t < end_track; ++t) {
        ChordRest *cr = segment.cr(t);
        if (!cr) {
            continue;
        }

        double m = staff_mag;
        if (cr->is_small()) {
            m *= small_note_mag;
        }

        if (cr->is_chord()) {
            double grace_mag = m * grace_note_mag;
            Chord *chord = to_chord(cr);
            for (Chord *c : chord->grace_notes()) {
                c->mutldata()->set_mag(grace_mag);
            }
        }
        cr->mutldata()->set_mag(m);
    }
}

void SegmentLayout::check_staff_move_validity(const Segment &segment, track_idx_t start_track, track_idx_t end_track) {
    LAYOUT_CALL() << segment.type_name() << "(" << segment.eid() << ")";

    IF_ASSERT_FAILED(segment.is_just_type(SegmentType::CHORD_REST)) { return; }

    for (track_idx_t t = start_track; t < end_track; ++t) {
        ChordRest *cr = to_chord_rest(segment.element(t));
        if (cr) {
            // Check if requested cross-staff is possible
            if (cr->staff_move() || cr->stored_staff_move()) {
                cr->check_staff_move_validity();
            }
        }
    }
}

void SegmentLayout::layout_chord_drumset(const Staff *staff, const Segment &segment, track_idx_t start_track, track_idx_t end_track,
                                         const LayoutConfiguration &conf) {
    LAYOUT_CALL() << LAYOUT_ITEM_INFO(staff) << " " << segment.type_name() << "(" << segment.eid() << ")";

    IF_ASSERT_FAILED(segment.is_just_type(SegmentType::CHORD_REST)) { return; }

    const Instrument *ins = staff->origin_part(segment.tick())->instrument(segment.tick());
    if (!ins->use_drumset()) {
        return;
    }

    const Drumset *drumset = ins->drumset();
    IF_ASSERT_FAILED(drumset) { return; }

    const StaffType *st = staff->staff_type_for_element(&segment);

    auto layout_drumset = [](Chord *c, const Drumset *drumset, const StaffType *st, double spatium) {
        for (Note *note : c->notes()) {
            int pitch = note->pitch();
            if (!drumset->is_valid(pitch)) {
                // LOGD("unmapped drum note %d", pitch);
            } else if (!note->fixed()) {
                note->undo_change_property(Pid::HEAD_GROUP, drumset->note_head(pitch));
                int line = drumset->line(pitch);
                note->set_line(line);

                int off = st->step_offset();
                double ld = st->line_distance().val();
                note->mutldata()->set_pos_y((line + off * 2.0) * spatium * .5 * ld);
            }
        }
    };

    double spatium = conf.spatium();
    for (track_idx_t t = start_track; t < end_track; ++t) {
        Chord *chord = item_cast<Chord *>(segment.element(t), CastMode::MAYBE_BAD); // maybe Rest
        if (!chord) {
            continue;
        }

        for (Chord *c : chord->grace_notes()) {
            layout_drumset(c, drumset, st, spatium);
        }

        layout_drumset(chord, drumset, st, spatium);
    }
}

void SegmentLayout::compute_chords_up(const Segment &segment, track_idx_t start_track, track_idx_t end_track, const LayoutContext &ctx) {
    IF_ASSERT_FAILED(segment.is_just_type(SegmentType::CHORD_REST)) { return; }

    for (track_idx_t t = start_track; t < end_track; ++t) {
        ChordRest *cr = segment.cr(t);
        if (!cr) {
            continue;
        }

        if (cr->is_chord()) {
            Chord *chord = to_chord(cr);

            for (Chord *c : chord->grace_notes()) {
                ChordLayout::compute_up(c, ctx);
            }

            ChordLayout::compute_up(chord, ctx);
        }
    }
}

void SegmentLayout::layout_chords_stem(const Segment &segment, track_idx_t start_track, track_idx_t end_track, const LayoutContext &ctx) {
    IF_ASSERT_FAILED(segment.is_just_type(SegmentType::CHORD_REST)) { return; }

    for (track_idx_t t = start_track; t < end_track; ++t) {
        ChordRest *cr = segment.cr(t);
        if (!cr) {
            continue;
        }

        if (cr->is_chord()) {
            Chord *chord = to_chord(cr);

            for (Chord *c : chord->grace_notes()) {
                ChordLayout::layout_stem(c, ctx);
            }

            ChordLayout::layout_stem(chord, ctx);
            // stem direction can change later during beam processing
        }
    }
}
