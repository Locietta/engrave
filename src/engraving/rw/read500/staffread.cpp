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
#include "staffread.h"

#include "dom/factory.h"
#include "dom/measure.h"
#include "dom/score.h"

#include "measureread.h"
#include "tread.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read500;

void StaffRead::read_staff(Score *score, XmlReader &e, ReadContext &ctx) {
    int staff = e.int_attribute("id", 1) - 1;
    int measure_idx = 0;
    ctx.set_current_measure_index(0);
    ctx.set_tick(Fraction(0, 1));
    ctx.set_track(staff * ::mu::engraving::voices);

    if (staff == 0) {
        while (e.read_next_start_element()) {
            const AsciiStringView tag(e.name());

            if (tag == "Measure") {
                Measure *measure = Factory::create_measure(ctx.score());
                measure->set_tick(ctx.tick());
                ctx.set_current_measure_index(measure_idx++);
                //
                // inherit timesig from previous measure
                //
                Measure *m = ctx.last_measure(); // measure->prevMeasure();
                Fraction f(ctx.time_sig_for_next_measure() != Fraction(0, 1) ? ctx.time_sig_for_next_measure() :
                           m                                                 ? m->timesig() :
                                                                               Fraction(4, 4));
                measure->set_ticks(f);
                measure->set_timesig(f);
                ctx.set_time_sig_for_next_measure(Fraction(0, 1));

                MeasureRead::read_measure(measure, e, ctx, staff);
                measure->check_measure(staff);
                if (!measure->is_mm_rest()) {
                    score->measures()->append(measure);
                    score->check_spanner(ctx.tick(), ctx.tick() + measure->ticks(), /*removeOrphans*/ false);
                    ctx.set_last_measure(measure);
                    ctx.set_tick(measure->tick() + measure->ticks());
                } else {
                    // this is a multi measure rest
                    // always preceded by the first measure it replaces
                    Measure *m1 = ctx.last_measure();

                    if (m1) {
                        m1->set_mm_rest(measure);
                        measure->set_tick(m1->tick());
                        measure->set_prev(m1->prev());
                    }
                }
            } else if (tag == "HBox" || tag == "VBox" || tag == "TBox" || tag == "FBox") {
                MeasureBase *mb = to_measure_base(Factory::create_item_by_name(tag, ctx.dummy()));
                mb->set_tick(ctx.tick());
                score->measures()->append(mb);

                // This default value needs initialising after being added to the score, as it depends on whether this is the title frame
                mb->set_size_is_spatium_dependent(mb->property_default(Pid::SIZE_SPATIUM_DEPENDENT).to_bool());

                TRead::read_item(mb, e, ctx);
            } else if (tag == "tick") {
                ctx.set_tick(Fraction::from_ticks(ctx.file_division(e.read_int())));
            } else {
                e.unknown();
            }
        }
    } else {
        Measure *measure = score->first_measure();
        while (e.read_next_start_element()) {
            const AsciiStringView tag(e.name());

            if (tag == "Measure") {
                if (measure == 0) {
                    LOGD("Score::readStaff(): missing measure!");
                    measure = Factory::create_measure(ctx.score());
                    measure->set_tick(ctx.tick());
                    score->measures()->append(measure);
                }
                ctx.set_tick(measure->tick());
                ctx.set_current_measure_index(measure_idx++);
                MeasureRead::read_measure(measure, e, ctx, staff);
                measure->check_measure(staff);
                if (measure->is_mm_rest()) {
                    measure = ctx.last_measure()->next_measure();
                } else {
                    ctx.set_last_measure(measure);
                    if (measure->mm_rest()) {
                        measure = measure->mm_rest();
                    } else {
                        measure = measure->next_measure();
                    }
                }
            } else if (tag == "tick") {
                ctx.set_tick(Fraction::from_ticks(ctx.file_division(e.read_int())));
            } else {
                e.unknown();
            }
        }
    }
}
