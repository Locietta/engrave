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
#include "modifydom.h"

#include "dom/chord.h"
#include "dom/factory.h"
#include "dom/hook.h"
#include "dom/keysig.h"
#include "dom/measure.h"
#include "dom/note.h"
#include "dom/ornament.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/spannermap.h"
#include "dom/staff.h"
#include "dom/stem.h"
#include "dom/stemslash.h"
#include "dom/timesig.h"
#include "dom/trill.h"
#include "dom/utils.h"
#include "editing/addremoveelement.h"
#include "rendering/score/chordlayout.h"

using namespace mu::engraving::rendering::score;

void ModifyDom::create_stems(const Measure *measure, LayoutContext &ctx) {
    const DomAccessor &dom = ctx.dom();
    for (staff_idx_t staff_idx = 0; staff_idx < dom.nstaves(); ++staff_idx) {
        const Staff *staff = dom.staff(staff_idx);
        if (!staff->show()) {
            continue;
        }

        track_idx_t start_track = staff_idx * ::mu::engraving::voices;
        track_idx_t end_track = start_track + ::mu::engraving::voices;

        for (const Segment &segment : measure->segments()) {
            if (!segment.is_just_type(SegmentType::CHORD_REST)) {
                continue;
            }

            for (track_idx_t t = start_track; t < end_track; ++t) {
                ChordRest *cr = segment.cr(t);
                if (!cr) {
                    continue;
                }

                auto create_stems = [](Chord *chord) {
                    if (!chord->should_have_stem()) {
                        delete chord->stem();
                        delete chord->hook();
                        delete chord->stem_slash();
                        chord->mutldata()->stem = nullptr;
                        chord->mutldata()->hook = nullptr;
                        chord->mutldata()->stem_slash = nullptr;
                        return;
                    }

                    if (!chord->stem()) {
                        auto *stem = Factory::create_stem(chord);
                        stem->set_track(chord->track());
                        stem->set_generated(true);
                        chord->mutldata()->stem = stem;
                        apply_authored_overrides(*stem);
                    }
                };

                if (cr->is_chord()) {
                    Chord *chord = to_chord(cr);

                    for (Chord *c : chord->grace_notes()) {
                        create_stems(c);
                    }

                    create_stems(chord); // create stems needed to calculate spacing
                    // stem direction can change later during beam processing
                }
            }
        }
    }
}

void ModifyDom::sort_measure_segments(Measure *measure, LayoutContext &ctx) {
    measure = measure->covering_mm_rest_or_this();

    // Move segments between measure which need to move

    // Move key and time signature segments to the correct measure.
    // Depending on configuration, this could mean moving segments at the end of this measure to the start of the following measure
    // or moving segments from the start of the following measure to the end of this measure

    // Loop through clef, key, time sigs in this bar.  If any are found and shouldn't be in this bar, move to the next

    auto change_applies_to_repeat_and_continuation = [&](const Segment &seg) -> bool {
        // Check if the change applies to the beginning of the repeat section as well as the continuation
        const std::vector<Measure *> measures = find_following_repeat_measures(measure);
        for (const Measure *repeat_measure : measures) {
            if (repeat_measure == measure->next_measure_mm()) {
                continue;
            }
            const Fraction start_tick = repeat_measure->tick();
            for (track_idx_t track = 0; track < ctx.dom().nstaves(); track += ::mu::engraving::voices) {
                const Staff *staff = ctx.dom().staff(track2staff(track));

                if (seg.is_key_sig_type()) {
                    const Key continuation_key = staff->rendered_key(seg.tick());
                    const Key repeat_key = staff->rendered_key(start_tick);
                    const Key key_before_continuation = staff->rendered_key(seg.tick() - Fraction::eps());
                    return key_before_continuation != continuation_key && continuation_key == repeat_key;
                }

                if (seg.is_time_sig_type()) {
                    const TimeSig *continuation_time_sig = staff->rendered_time_sig(seg.tick());
                    const TimeSig *repeat_time_sig = staff->rendered_time_sig(start_tick);
                    const TimeSig *time_sig_before_continuation = staff->rendered_time_sig(seg.tick() - Fraction::eps());
                    return continuation_time_sig && repeat_time_sig && time_sig_before_continuation &&
                           time_sig_before_continuation->sig() != continuation_time_sig->sig() &&
                           continuation_time_sig->sig() == repeat_time_sig->sig();
                }

                if (seg.is_clef_type()) {
                    const ClefType continuation_clef = staff->rendered_clef(seg.tick());
                    const ClefType repeat_clef = staff->rendered_clef(start_tick);
                    const ClefType clef_before_continuation = staff->rendered_clef(seg.tick() - Fraction::eps());
                    if (clef_before_continuation != continuation_clef && continuation_clef == repeat_clef) {
                        return true;
                    }
                }
            }
        }
        return false;
    };

    auto clef_seg_barline_position = [&](const Segment &seg) -> ClefToBarlinePosition {
        for (EngravingItem *el : seg.effective_elements()) {
            if (!el || !el->is_clef()) {
                continue;
            }
            const Clef *clef = to_clef(el);
            if (clef->clef_to_barline_position() != ClefToBarlinePosition::AUTO) {
                return clef->clef_to_barline_position();
            }
        }
        return ClefToBarlinePosition::AUTO;
    };

    MeasureBase *next_mb = measure ? measure->next_mm() : nullptr;

    if (!next_mb || !next_mb->is_measure()) {
        return;
    }

    Measure *next_measure = to_measure(next_mb);

    const bool sigs_should_be_in_this_measure = ((measure->repeat_end() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_REPEATS)) ||
                                                 (measure->repeat_jump() && ctx.conf().style_b(Sid::CHANGES_BEFORE_BARLINE_OTHER_JUMPS)));
    std::vector<Segment *> segs_to_remove;

    std::vector<Segment *> segs_to_move_to_next_measure;
    for (Segment *rendered = measure->rendered_first(); rendered; rendered = rendered->rendered_next()) {
        Segment &seg = *rendered;
        if (seg.tick() != measure->end_tick() || seg.is_chord_rest_type()) {
            continue;
        }

        // Move clefs at the end of this measure into the next measure
        if (seg.is_clef_type()) {
            ClefToBarlinePosition pos = clef_seg_barline_position(seg);
            // Clef position explicitly set by user
            if (pos == ClefToBarlinePosition::BEFORE) {
                continue;
            } else if (pos == ClefToBarlinePosition::AFTER) {
                segs_to_move_to_next_measure.push_back(&seg);
                continue;
            }

            // AUTO on normal barline
            if (!measure->repeat_end()) {
                continue;
            }

            // Otherwise, pos is AUTO
            // Place changes before barline & clefs before repeats style settings
            if ((!sigs_should_be_in_this_measure && !change_applies_to_repeat_and_continuation(seg)) ||
                (measure->repeat_end() && !ctx.conf().style_b(Sid::PLACE_CLEFS_BEFORE_REPEATS))) {
                segs_to_move_to_next_measure.push_back(&seg);
                continue;
            }
        }

        // Move key sigs and time sigs at the end of this measure into the next measure
        if ((seg.is_key_sig_type() || seg.is_time_sig_type()) && !seg.header() && !seg.trailer() &&
            (!sigs_should_be_in_this_measure || !change_applies_to_repeat_and_continuation(seg))) {
            if (next_measure && next_measure->find_rendered_segment_r(seg.segment_type(), Fraction(0, 1))) {
                segs_to_remove.push_back(&seg);
                continue;
            }
            segs_to_move_to_next_measure.push_back(&seg);
        }
    }

    // No next measure, so these segments are useless
    if (!next_measure) {
        for (Segment *seg : segs_to_move_to_next_measure) {
            seg->mutldata()->suppressed = true;
            seg->set_enabled(false);
        }
        return;
    }

    std::vector<Segment *> segs_to_move_to_this_measure;
    for (Segment *rendered = next_measure->rendered_first(); rendered; rendered = rendered->rendered_next()) {
        Segment &seg = *rendered;
        if (seg.tick() != next_measure->tick() || seg.is_chord_rest_type()) {
            continue;
        }

        // Move clefs at the beginning of the next measure into this measure
        if (seg.is_clef_type()) {
            ClefToBarlinePosition pos = clef_seg_barline_position(seg);
            // Clef position explicitly set by user
            if (pos == ClefToBarlinePosition::BEFORE) {
                segs_to_move_to_this_measure.push_back(&seg);
                continue;
            } else if (pos == ClefToBarlinePosition::AFTER) {
                continue;
            }

            if (!measure->repeat_end()) {
                segs_to_move_to_this_measure.push_back(&seg);
                continue;
            }

            // Otherwise, pos is AUTO
            // Place changes before barline & clefs before repeats style settings
            if ((sigs_should_be_in_this_measure && change_applies_to_repeat_and_continuation(seg)) || !measure->repeat_end() ||
                ctx.conf().style_b(Sid::PLACE_CLEFS_BEFORE_REPEATS)) {
                segs_to_move_to_this_measure.push_back(&seg);
            }
        }

        // Move key sigs and time sigs at the start of the next measure to the end of this measure
        if ((seg.is_time_sig_type() || seg.is_key_sig_type()) && !seg.header() && !seg.trailer() && sigs_should_be_in_this_measure &&
            change_applies_to_repeat_and_continuation(seg)) {
            if (measure->find_rendered_segment_r(seg.segment_type(), measure->ticks())) {
                segs_to_remove.push_back(&seg);
                continue;
            }
            segs_to_move_to_this_measure.push_back(&seg);
        }
    }

    const auto suppress = [](Segment *segment) {
        segment->mutldata()->suppressed = true;
        segment->set_enabled(false);
    };
    const auto relocate = [&](Segment *segment, Measure *destination, const Fraction &tick) {
        auto *target = destination->get_rendered_segment_r(segment->segment_type(), tick);
        for (auto *item : segment->effective_elements()) {
            if (!item) continue;
            // MMR copies map back to the authored item, never to another clone.
            const EngravingItem *source = item;
            if (item->is_layout_copy()) {
                auto *underlying = segment->rtick().is_zero() ? segment->measure()->mm_rest_first() : segment->measure()->mm_rest_last();
                if (underlying) {
                    auto *original = underlying->find_segment_r(segment->segment_type(),
                                                                segment->rtick().is_zero() ? Fraction(0, 1) : underlying->ticks());
                    if (original && original->element(item->track())) source = original->element(item->track());
                }
            }
            target->add_display_copy(*source);
        }
        suppress(segment);
    };
    for (auto *segment : segs_to_remove) suppress(segment);
    for (auto *segment : segs_to_move_to_next_measure) relocate(segment, next_measure, Fraction(0, 1));
    for (auto *segment : segs_to_move_to_this_measure) relocate(segment, measure, measure->ticks());
    measure->rebuild_rendered_segments();
    next_measure->rebuild_rendered_segments();
}
