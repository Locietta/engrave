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
#include "staffwrite.h"

#include "dom/score.h"
#include "dom/staff.h"
#include "dom/measure.h"

#include "twrite.h"
#include "measurewrite.h"

using namespace mu::engraving;
using namespace mu::engraving::write;

static void write_measure(XmlWriter &xml, WriteContext &ctx, MeasureBase *m, staff_idx_t staff_idx, bool write_system_elements,
                          bool force_time_sig) {
    //
    // special case multi measure rest
    //
    if (m->is_measure() || staff_idx == 0) {
        if (Measure::classof(m)) {
            MeasureWrite::write_measure(to_measure(m), xml, ctx, staff_idx, write_system_elements, force_time_sig);
        } else {
            TWrite::write_item(m, xml, ctx);
        }
    }

    if (m->score()->style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS) && m->is_measure() && to_measure(m)->mm_rest()) {
        MeasureWrite::write_measure(to_measure(m)->mm_rest(), xml, ctx, staff_idx, write_system_elements, force_time_sig);
    }

    ctx.set_cur_tick(m->end_tick());
}

void StaffWrite::write_staff(const Staff *staff, XmlWriter &xml, write::WriteContext &ctx, MeasureBase *measure_start,
                             MeasureBase *measure_end, staff_idx_t staff_start, staff_idx_t staff_idx) {
    xml.start_element(staff, {{"id", static_cast<int>(staff_idx + 1 - staff_start)}});

    ctx.set_cur_tick(measure_start->tick());
    ctx.set_tick_diff(ctx.cur_tick());
    ctx.set_cur_track(staff_idx * ::mu::engraving::voices);
    bool write_system_elements = (staff_idx == staff_start);
    bool first_measure_written = false;
    bool force_time_sig = false;
    for (MeasureBase *m = measure_start; m != measure_end; m = m->next()) {
        // force timesig if first measure and range is set
        if (ctx.should_write_range() && m->is_measure()) {
            if (!first_measure_written) {
                force_time_sig = true;
                first_measure_written = true;
            } else {
                force_time_sig = false;
            }
        }
        write_measure(xml, ctx, m, staff_idx, write_system_elements, force_time_sig);
    }

    xml.end_element();
}
