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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "measureread.h"

#include "translation.h"

#include "../dom/ambitus.h"
#include "../dom/anchors.h"
#include "../dom/barline.h"
#include "../dom/beam.h"
#include "../dom/breath.h"
#include "../dom/chord.h"
#include "../dom/dynamic.h"
#include "../dom/expression.h"
#include "../dom/factory.h"
#include "../dom/fermata.h"
#include "../dom/fret.h"
#include "../dom/keysig.h"
#include "../dom/location.h"
#include "../dom/measure.h"
#include "../dom/measurenumber.h"
#include "../dom/measurerepeat.h"
#include "../dom/mmrest.h"
#include "../dom/mmrestrange.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/spacer.h"
#include "../dom/staff.h"
#include "../dom/stafflines.h"
#include "../dom/stafftext.h"
#include "../dom/system.h"
#include "../dom/systemdivider.h"
#include "../dom/timesig.h"
#include "../dom/tuplet.h"
#include "../dom/harmony.h"
#include "../dom/fret.h"
#include "../dom/tremolobar.h"
#include "../dom/tempotext.h"
#include "../dom/image.h"

#include "../types/typesconv.h"
#include "tread.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read500;

void MeasureRead::read_measure(Measure *measure, XmlReader &e, ReadContext &ctx, int staff_idx) {
    IF_ASSERT_FAILED(ctx.is_same_score(measure)) { return; }

    ctx.set_current_measure(measure);
    int next_track = staff_idx * ::mu::engraving::voices;
    ctx.set_track(next_track);

    for (int n = int(measure->mstaves().size()); n <= staff_idx; ++n) {
        Staff *staff = ctx.staff(n);
        MStaff *s = new MStaff;
        s->set_lines(Factory::create_staff_lines(measure));
        s->lines()->set_track(n * ::mu::engraving::voices);
        s->lines()->set_visible(!staff->is_lines_invisible(measure->tick()));
        measure->mstaves().push_back(s);
    }

    bool irregular = false;
    if (e.has_attribute("len")) {
        bool ok = true;
        measure->set_ticks(Fraction::from_string(e.attribute("len"), &ok));
        if (!ok || measure->ticks() < Fraction(1, 128)) {
            e.raise_error(muse::mtrc("engraving", "MSCX error at byte offset %1: invalid measure length: %2")
                              .arg(e.byte_offset())
                              .arg(e.attribute("len")));
            return;
        }
        irregular = true;
    }

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "voice") {
            ctx.set_track(next_track++);
            ctx.set_tick(measure->tick());
            read_voice(measure, e, ctx, staff_idx, irregular);
        } else if (tag == "Marker" || tag == "Jump") {
            EngravingItem *el = Factory::create_item_by_name(tag, measure);
            el->set_track(ctx.track());
            TRead::read_item(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            measure->add(el);
        } else if (tag == "stretch") {
            double val = e.read_double();
            if (val < 0.0) {
                val = 0;
            }
            measure->set_user_stretch(val);
        } else if (tag == "noOffset") {
            measure->set_measure_number_offset(e.read_int());
        } else if (tag == "measureNumberMode") {
            measure->set_measure_number_mode(MeasureNumberMode(e.read_int()));
        } else if (tag == "irregular") {
            measure->set_exclude_from_numbering(e.read_bool());
        } else if (tag == "breakMultiMeasureRest") {
            measure->set_break_multi_measure_rest(e.read_bool());
        } else if (tag == "startRepeat") {
            measure->set_repeat_start(true);
            e.read_next();
        } else if (tag == "endRepeat") {
            measure->set_repeat_count(e.read_int());
            measure->set_repeat_end(true);
        } else if (tag == "vspacer" || tag == "vspacerDown") {
            if (!measure->mstaves()[staff_idx]->vspacer_down()) {
                Spacer *spacer = Factory::create_spacer(measure);
                spacer->set_spacer_type(SpacerType::DOWN);
                spacer->set_track(staff_idx * ::mu::engraving::voices);
                measure->add(spacer);
            }
            measure->mstaves()[staff_idx]->vspacer_down()->set_gap(Spatium(e.read_double()));
        } else if (tag == "vspacerFixed") {
            if (!measure->mstaves()[staff_idx]->vspacer_down()) {
                Spacer *spacer = Factory::create_spacer(measure);
                spacer->set_spacer_type(SpacerType::FIXED);
                spacer->set_track(staff_idx * ::mu::engraving::voices);
                measure->add(spacer);
            }
            measure->mstaves()[staff_idx]->vspacer_down()->set_gap(Spatium(e.read_double()));
        } else if (tag == "vspacerUp") {
            if (!measure->mstaves()[staff_idx]->vspacer_up()) {
                Spacer *spacer = Factory::create_spacer(measure);
                spacer->set_spacer_type(SpacerType::UP);
                spacer->set_track(staff_idx * ::mu::engraving::voices);
                measure->add(spacer);
            }
            measure->mstaves()[staff_idx]->vspacer_up()->set_gap(Spatium(e.read_double()));
        } else if (tag == "visible") {
            measure->mstaves()[staff_idx]->set_visible(e.read_int());
        } else if (tag == "stemless") {
            measure->mstaves()[staff_idx]->set_stemless(e.read_int());
        } else if (tag == "hideIfEmpty") {
            measure->mstaves()[staff_idx]->set_hide_if_empty(TConv::from_xml(e.read_ascii_text(), AutoOnOff::AUTO));
        } else if (tag == "measureRepeatCount") {
            measure->set_measure_repeat_count(e.read_int(), staff_idx);
        } else if (tag == "SystemDivider") {
            SystemDivider *sd = new SystemDivider(ctx.dummy());
            TRead::read(sd, e, ctx);
            //! TODO Looks like a bug.
            //! The SystemDivider parent must be System
            //! there is a method: `System* system() const { return (System*)parent(); }`,
            //! but when we add it to Measure, the parent will be rewritten.
            measure->add(sd);
        } else if (tag == "multiMeasureRest") {
            measure->set_mm_rest_count(e.read_int());
            // set tick to previous measure
            measure->set_tick(ctx.last_measure()->tick());
            ctx.set_tick(ctx.last_measure()->tick());
        } else if (tag == "mmRestLast") {
            AsciiStringView s = e.read_ascii_text();
            EID eid = EID::from_std_string(s);
            DO_ASSERT(eid.is_valid());
            ctx.add_mm_rest_end_measure_eid(measure, eid);
        } else if (tag == "MeasureNumber") {
            MeasureNumber *no_text = new MeasureNumber(measure);
            TRead::read(no_text, e, ctx);
            no_text->set_track(ctx.track());
            measure->add(no_text);
        } else if (tag == "MMRestRange") {
            MMRestRange *range = new MMRestRange(measure);
            TRead::read(range, e, ctx);
            range->set_track(ctx.track());
            measure->add(range);
        } else if (TRead::read_properties(static_cast<MeasureBase *>(measure), e, ctx)) {
        } else {
            e.unknown();
        }
    }
    if (measure->is_mm_rest()) {
        Measure *lm = ctx.last_measure();
        ctx.set_tick(lm->end_tick());
    }
    ctx.set_current_measure(nullptr);

    measure->connect_tremolo();
}

void MeasureRead::read_voice(Measure *measure, XmlReader &e, ReadContext &ctx, int staff_idx, bool irregular) {
    Segment *segment = nullptr;
    std::vector<Chord *> grace_notes;
    Beam *starting_beam = nullptr;
    Tuplet *tuplet = nullptr;
    Fermata *fermata = nullptr;

    Staff *staff = ctx.staff(staff_idx);
    Fraction time_stretch(staff->time_stretch(measure->tick()));

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "location") {
            Location loc = Location::relative();
            TRead::read(&loc, e, ctx);
            ctx.set_location(loc);
            if (loc.is_time_tick()) {
                EditTimeTickAnchors::create_time_tick_anchor(measure, ctx.tick() - measure->tick(), track2staff(ctx.track()));
            }
        } else if (tag == "tick") { // obsolete?
            LOGD() << "read midi tick";
            ctx.set_tick(Fraction::from_ticks(ctx.file_division(e.read_int())));
        } else if (tag == "BarLine") {
            BarLine *bar_line = Factory::create_bar_line(ctx.dummy());
            bar_line->set_track(ctx.track());
            TRead::read(bar_line, e, ctx);
            //
            //  StartRepeatBarLine: at rtick == 0, always BarLineType::START_REPEAT
            //  BarLine:            in the middle of a measure, has no semantic
            //  EndBarLine:         at the end of a measure
            //  BeginBarLine:       first segment of a measure, systemic barline

            SegmentType st = SegmentType::INVALID;
            Fraction t = ctx.tick() - measure->tick();
            if (t.is_not_zero() && (t != measure->ticks())) {
                st = SegmentType::BAR_LINE;
            } else if (bar_line->bar_line_type() == BarLineType::START_REPEAT && t.is_zero()) {
                st = SegmentType::START_REPEAT_BAR_LINE;
            } else if (bar_line->bar_line_type() == BarLineType::START_REPEAT && t == measure->ticks()) {
                // old version, ignore
                delete bar_line;
                bar_line = 0;
            } else if (t.is_zero() && segment == 0) {
                st = SegmentType::BEGIN_BAR_LINE;
            } else {
                st = SegmentType::END_BAR_LINE;
            }
            if (bar_line) {
                segment = measure->get_segment_r(st, t);
                segment->add(bar_line);
            }
            if (fermata) {
                segment->add(fermata);
                fermata = nullptr;
            }
        } else if (tag == "Chord") {
            Chord *chord = Factory::create_chord(ctx.dummy());
            chord->set_track(ctx.track());
            TRead::read(chord, e, ctx);
            if (starting_beam) {
                starting_beam->add(chord); // also calls chord->setBeam(startingBeam)
                starting_beam = nullptr;
            }
            //                  if (tuplet && !chord->isGrace())
            //                        chord->readAddTuplet(tuplet);
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            if (chord->note_type() != NoteType::NORMAL) {
                grace_notes.push_back(chord);
            } else {
                segment->add(chord);
                for (size_t i = 0; i < grace_notes.size(); ++i) {
                    Chord *gc = grace_notes[i];
                    gc->set_grace_index(static_cast<int>(i));
                    chord->add(gc);
                }
                grace_notes.clear();
                if (tuplet) {
                    tuplet->add(chord);
                }
                ctx.inc_tick(chord->actual_ticks());
            }
            if (fermata) {
                segment->add(fermata);
                fermata = nullptr;
            }
        } else if (tag == "Rest") {
            if (measure->is_mm_rest()) {
                segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
                MMRest *mmr = Factory::create_mm_rest(segment);
                mmr->set_track(ctx.track());
                TRead::read(mmr, e, ctx);
                segment->add(mmr);
                ctx.inc_tick(mmr->actual_ticks());
            } else {
                segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
                Rest *rest = Factory::create_rest(segment);
                rest->set_duration_type(DurationType::V_MEASURE);
                rest->set_ticks(measure->timesig() / time_stretch);
                rest->set_track(ctx.track());
                TRead::read(rest, e, ctx);
                if (starting_beam) {
                    starting_beam->add(rest); // also calls rest->setBeam(startingBeam)
                    starting_beam = nullptr;
                }

                segment->add(rest);
                if (fermata) {
                    segment->add(fermata);
                    fermata = nullptr;
                }

                if (!rest->ticks().is_valid()) { // hack
                    rest->set_ticks(measure->timesig() / time_stretch);
                }

                if (tuplet) {
                    tuplet->add(rest);
                }
                ctx.inc_tick(rest->actual_ticks());
            }
        } else if (tag == "Breath") {
            segment = measure->get_segment(SegmentType::BREATH, ctx.tick());
            Breath *breath = Factory::create_breath(segment);
            breath->set_track(ctx.track());
            breath->set_placement(breath->track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE);
            TRead::read(breath, e, ctx);
            segment->add(breath);
        } else if (tag == "MeasureRepeat" || tag == "RepeatMeasure") {
            //             4.x                       3.x
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            MeasureRepeat *mr = Factory::create_measure_repeat(segment);
            mr->set_track(ctx.track());
            TRead::read(mr, e, ctx);
            if (!mr->num_measures()) {
                mr->set_num_measures(1); // 3.x doesn't have any other possibilities
            }
            if (!measure->measure_repeat_count(staff_idx)) {
                measure->set_measure_repeat_count(1, staff_idx);
            }
            segment->add(mr);
            ctx.inc_tick(measure->ticks());
        } else if (tag == "Clef") {
            Clef *clef = Factory::create_clef(ctx.dummy());
            clef->set_track(ctx.track());
            bool is_header = false;
            TRead::read(clef, e, ctx, &is_header);
            clef->set_generated(false);

            segment = measure->get_segment(is_header ? SegmentType::HEADER_CLEF : SegmentType::CLEF, ctx.tick());
            segment->add(clef);
        } else if (tag == "TimeSig") {
            TimeSig *ts = Factory::create_time_sig(ctx.dummy());
            ts->set_track(ctx.track());
            TRead::read(ts, e, ctx);

            Fraction curr_tick = ctx.tick();
            bool courtesy_sig = ts->is_courtesy();
            segment = measure->get_segment(courtesy_sig ? SegmentType::TIME_SIG_ANNOUNCE : SegmentType::TIME_SIG, curr_tick);
            segment->add(ts);

            if (!courtesy_sig) {
                if (curr_tick == measure->end_tick()) {
                    segment->set_end_of_measure_change(true);
                    measure->set_end_of_measure_change(true);
                }

                if (curr_tick == measure->tick()) {
                    time_stretch = ts->stretch().reduced();
                    measure->set_timesig(ts->sig() / time_stretch);

                    if (!irregular) {
                        measure->set_ticks(measure->timesig());
                    }
                }

                if (curr_tick > measure->tick()) {
                    ctx.set_time_sig_for_next_measure(ts->sig() / ts->stretch().reduced());
                }
            }
        } else if (tag == "KeySig") {
            KeySig *ks = Factory::create_key_sig(ctx.dummy());
            ks->set_track(ctx.track());
            TRead::read(ks, e, ctx);

            Fraction cur_tick = ctx.tick();
            bool courtesy_sig = ks->is_courtesy();
            segment = measure->get_segment(courtesy_sig ? SegmentType::KEY_SIG_ANNOUNCE : SegmentType::KEY_SIG, cur_tick);
            segment->add(ks);

            if (!courtesy_sig) {
                if (cur_tick == measure->end_tick()) {
                    segment->set_end_of_measure_change(true);
                    measure->set_end_of_measure_change(true);
                }

                staff->set_key(cur_tick, ks->key_sig_event());
            }
        } else if (tag == "Text") {
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            StaffText *t = Factory::create_staff_text(segment);
            t->set_track(ctx.track());
            TRead::read(t, e, ctx);
            if (t->empty()) {
                LOGD("==reading empty text: deleted");
                delete t;
            } else {
                segment->add(t);
            }
        }
        //----------------------------------------------------
        // Annotation
        else if (tag == "Dynamic") {
            segment = measure->get_chord_rest_or_time_tick_segment(ctx.tick());
            Dynamic *dyn = Factory::create_dynamic(segment);
            dyn->set_track(ctx.track());
            TRead::read(dyn, e, ctx);
            segment->add(dyn);
        } else if (tag == "Expression") {
            segment = measure->get_chord_rest_or_time_tick_segment(ctx.tick());
            Expression *expr = Factory::create_expression(segment);
            expr->set_track(ctx.track());
            TRead::read(expr, e, ctx);
            segment->add(expr);
        } else if (tag == "Harmony") {
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            Harmony *el = Factory::create_harmony(segment);

            el->set_track(ctx.track());
            TRead::read(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            if (el->chords().empty()) {
                // Invalid harmony
                delete el;
            } else {
                segment->add(el);
            }
        } else if (tag == "FretDiagram") {
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            FretDiagram *el = Factory::create_fret_diagram(segment);

            el->set_track(ctx.track());
            TRead::read(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        } else if (tag == "TremoloBar") {
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            TremoloBar *el = Factory::create_tremolo_bar(segment);
            el->set_track(ctx.track());
            TRead::read(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        } else if (tag == "Symbol") {
            segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            Symbol *el = Factory::create_symbol(segment);

            el->set_track(ctx.track());
            TRead::read(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        } else if (tag == "Tempo") {
            segment = measure->get_chord_rest_or_time_tick_segment(ctx.tick());
            TempoText *el = Factory::create_tempo_text(segment);

            el->set_track(ctx.track());
            TRead::read(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        } else if (tag == "StaffText") {
            segment = measure->get_chord_rest_or_time_tick_segment(ctx.tick());
            StaffText *el = Factory::create_staff_text(segment);
            el->set_track(ctx.track());
            TRead::read(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        } else if (tag == "StaveSharingLabel" || tag == "Sticking" || tag == "SystemText" || tag == "PlayTechAnnotation" || tag == "Capo" ||
                   tag == "StringTunings" || tag == "RehearsalMark" || tag == "InstrumentChange" || tag == "StaffState" ||
                   tag == "FiguredBass" || tag == "HarpPedalDiagram") {
            EngravingItem *el = Factory::create_item_by_name(tag, ctx.dummy());
            if (el->allow_time_anchor()) {
                segment = measure->get_chord_rest_or_time_tick_segment(ctx.tick());
            } else {
                segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
            }

            el->set_track(ctx.track());
            TRead::read_item(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        } else if (tag == "Fermata") {
            fermata = Factory::create_fermata(ctx.dummy());
            fermata->set_track(ctx.track());
            fermata->set_placement(fermata->track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE);
            TRead::read(fermata, e, ctx);
        } else if (tag == "Image") {
            if (MScore::no_images) {
                e.skip_current_element();
            } else {
                segment = measure->get_segment(SegmentType::CHORD_REST, ctx.tick());
                Image *el = Factory::create_image(segment);
                el->set_track(ctx.track());
                TRead::read(el, e, ctx);
                segment->add(el);
            }
        } else if (tag == "PlayCountText") {
            segment = measure->get_segment(SegmentType::END_BAR_LINE, ctx.tick());
            EngravingItem *el = Factory::create_item_by_name(tag, segment);

            el->set_track(ctx.track());
            TRead::read_item(el, e, ctx);
            if (el->system_flag() && el->is_top_system_object()) {
                el->set_track(0); // original system object always goes on top
            }
            segment->add(el);
        }
        //----------------------------------------------------
        else if (tag == "Tuplet") {
            Tuplet *old_tuplet = tuplet;
            tuplet = Factory::create_tuplet(measure);
            tuplet->set_track(ctx.track());
            tuplet->set_tick(ctx.tick());
            TRead::read(tuplet, e, ctx);
            if (old_tuplet) {
                old_tuplet->add(tuplet);
            }
        } else if (tag == "endTuplet") {
            if (!tuplet) {
                LOGD("Measure::read: encountered <endTuplet/> when no tuplet was started");
                e.skip_current_element();
                continue;
            }
            // Gap rests are serialized as location jumps. Restore a leading gap
            // inside its tuplet before checkMeasure() treats the first chord as
            // the start of the full tuplet and fills over subsequent notes.
            if (!tuplet->elements().empty()) {
                const Fraction start = tuplet->tick();
                const Fraction gap = tuplet->elements().front()->tick() - start;
                if (gap > Fraction(0, 1)) {
                    Fraction duration = gap * staff->time_stretch(start);
                    for (Tuplet *parent = tuplet; parent; parent = parent->tuplet()) {
                        duration *= parent->ratio();
                    }
                    Segment *gap_segment = measure->get_segment(SegmentType::CHORD_REST, start);
                    if (!gap_segment->element(ctx.track())) {
                        Rest *rest = Factory::create_rest(gap_segment);
                        rest->set_track(ctx.track());
                        rest->set_duration_type(TDuration(duration, true));
                        rest->set_ticks(duration);
                        rest->set_gap(true);
                        gap_segment->add(rest);
                        tuplet->add(rest);
                    }
                }
            }
            Tuplet *old_tuplet = tuplet;
            tuplet = tuplet->tuplet();
            if (old_tuplet->elements().empty()) {
                // this should not happen and is a sign of input file corruption
                LOGD("Measure:read: empty tuplet in measure index=%d, input file corrupted?", ctx.current_measure_index());
                if (tuplet) {
                    tuplet->remove(old_tuplet);
                }
                delete old_tuplet;
            }
            e.read_next();
        } else if (tag == "Beam") {
            Beam *beam = Factory::create_beam(ctx.score());
            beam->set_track(ctx.track());
            TRead::read(beam, e, ctx);
            if (starting_beam) {
                LOGD("The read beam was not used");
                delete starting_beam;
            }
            starting_beam = beam;
        } else if (tag == "Segment" && segment) {
            TRead::read(segment, e, ctx);
        } else if (tag == "Ambitus") {
            segment = measure->get_segment(SegmentType::AMBITUS, ctx.tick());
            Ambitus *range = Factory::create_ambitus(segment);
            TRead::read(range, e, ctx);
            range->set_track(track_zero_voice(ctx.track()));
            segment->add(range);
        } else {
            e.unknown();
        }
    }
    if (starting_beam) {
        LOGD("The read beam was not used");
        delete starting_beam;
    }
    if (tuplet) {
        LOGD("Measure:readVoice: measure index=%d, <endTuplet/> not found", ctx.current_measure_index());
        if (tuplet->elements().empty()) {
            if (tuplet->tuplet()) {
                tuplet->tuplet()->remove(tuplet);
            }
            delete tuplet;
        }
    }
    if (fermata) {
        SegmentType st = (ctx.tick() == measure->end_tick() ? SegmentType::END_BAR_LINE : SegmentType::CHORD_REST);
        segment = measure->get_segment(st, ctx.tick());
        segment->add(fermata);
        fermata = nullptr;
    }
}
