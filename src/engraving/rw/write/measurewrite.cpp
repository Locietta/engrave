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
#include "measurewrite.h"

#include "dom/measure.h"
#include "dom/measurenumber.h"
#include "dom/mmrestrange.h"
#include "dom/spacer.h"
#include "dom/textlinebase.h"
#include "twrite.h"
#include "types/typesconv.h"

using namespace mu::engraving;
using namespace mu::engraving::write;

void MeasureWrite::write_measure(const Measure *measure, XmlWriter &xml, WriteContext &ctx, staff_idx_t staff, bool write_system_elements,
                                 bool force_time_sig) {
    if (MScore::debug_mode) {
        const int mno = measure->measure_number() + 1;
        xml.comment(String(u"Measure %1").arg(mno));
    }
    if (measure->ticks() != measure->timesig()) {
        // this is an irregular measure
        xml.start_element(measure, {{"len", measure->ticks().to_string()}});
    } else {
        xml.start_element(measure);
    }

    ctx.set_cur_tick(measure->tick());
    ctx.set_cur_track(staff * ::mu::engraving::voices);

    if (measure->mm_rest_count() > 0) {
        xml.tag("multiMeasureRest", measure->mm_rest_count());
    }
    if (write_system_elements) {
        TWrite::write_item_eid(measure, xml);

        if (measure->is_mm_rest()) {
            Measure *last_measure = measure->mm_rest_last();
            EID eid_of_last_measure = last_measure->eid();
            if (!eid_of_last_measure.is_valid()) {
                eid_of_last_measure = last_measure->assign_new_eid();
            }
            xml.tag("mmRestLast", eid_of_last_measure.to_std_string());
        }

        if (measure->repeat_start()) {
            xml.tag("startRepeat");
        }
        if (measure->repeat_end()) {
            xml.tag("endRepeat", measure->repeat_count());
        }
        TWrite::write_property(measure, xml, Pid::EXCLUDE_FROM_NUMBERING);
        TWrite::write_property(measure, xml, Pid::BREAK_MMR);
        TWrite::write_property(measure, xml, Pid::USER_STRETCH);
        TWrite::write_property(measure, xml, Pid::MEASURE_NUMBER_OFFSET);
        TWrite::write_property(measure, xml, Pid::MEASURE_NUMBER_MODE);
    }
    MStaff *mstaff = measure->mstaves()[staff];
    if (mstaff->measure_number() && !mstaff->measure_number()->generated()) {
        TWrite::write(mstaff->measure_number(), xml, ctx);
    }

    if (mstaff->mm_range_text() && !mstaff->mm_range_text()->generated()) {
        TWrite::write(mstaff->mm_range_text(), xml, ctx);
    }

    if (mstaff->vspacer_up()) {
        xml.tag("vspacerUp", mstaff->vspacer_up()->gap().val());
    }
    if (mstaff->vspacer_down()) {
        if (mstaff->vspacer_down()->spacer_type() == SpacerType::FIXED) {
            xml.tag("vspacerFixed", mstaff->vspacer_down()->gap().val());
        } else {
            xml.tag("vspacerDown", mstaff->vspacer_down()->gap().val());
        }
    }
    if (!mstaff->visible()) {
        xml.tag("visible", mstaff->visible());
    }
    if (mstaff->stemless()) {
        xml.tag("stemless", mstaff->stemless());
    }
    if (mstaff->hide_if_empty() != AutoOnOff::AUTO) {
        xml.tag("hideIfEmpty", TConv::to_xml(mstaff->hide_if_empty()));
    }
    if (mstaff->measure_repeat_count()) {
        xml.tag("measureRepeatCount", mstaff->measure_repeat_count());
    }

    track_idx_t strack = staff * ::mu::engraving::voices;
    track_idx_t etrack = strack + ::mu::engraving::voices;
    for (const EngravingItem *e : measure->el()) {
        if (e->generated()) {
            continue;
        }

        bool write_system = write_system_elements;
        if (e->system_flag()) {
            ElementType et = e->type();
            if ((et == ElementType::REHEARSAL_MARK) || (et == ElementType::SYSTEM_TEXT) || (et == ElementType::TRIPLET_FEEL) ||
                (et == ElementType::PLAYTECH_ANNOTATION) || (et == ElementType::JUMP) || (et == ElementType::MARKER) ||
                (et == ElementType::TEMPO_TEXT) || is_system_text_line(e)) {
                write_system = (e->staff_idx() == staff); // always show these on appropriate staves
            }
        }

        if (e->staff_idx() != staff) {
            if (!e->system_flag() || (e->system_flag() && !write_system)) {
                continue;
            }
        }

        TWrite::write_item(e, xml, ctx);
    }
    assert(measure->first());
    assert(measure->last());
    if (measure->first() && measure->last()) {
        TWrite::write_segments(xml, ctx, strack, etrack, segments_to_write(measure), write_system_elements, force_time_sig);
    }

    xml.end_element();
}

std::vector<Segment *> MeasureWrite::segments_to_write(const Measure *measure) {
    /* Layout moves the time signature at the start of an MM rest from its first underlying measure onto the MM rest.
     * During writing, the MM rest is written after that measure, and the reader needs the time signature before the measure's
     * contents to get its length right, so we write the signature in the underlying measure instead of the MM. */

    auto start_time_sig_segment = [](const Measure *mm_rest) { return mm_rest->find_segment_r(SegmentType::TIME_SIG, Fraction(0, 1)); };

    const Segment *seg_to_skip = nullptr;
    Segment *seg_to_insert = nullptr;
    if (measure->is_mm_rest()) {
        seg_to_skip = start_time_sig_segment(measure);
    } else if (measure->mm_rest()) {
        seg_to_insert = start_time_sig_segment(measure->mm_rest());
    }

    std::vector<Segment *> segments;
    for (Segment *s = measure->first(); s; s = s->next()) {
        // insert it where the measure would place it if it had this segment itself
        if (seg_to_insert && (s->rtick().is_not_zero() || seg_to_insert->goes_before(s))) {
            segments.push_back(seg_to_insert);
            seg_to_insert = nullptr;
        }
        if (s != seg_to_skip) {
            segments.push_back(s);
        }
    }
    if (seg_to_insert) {
        segments.push_back(seg_to_insert);
    }

    return segments;
}
