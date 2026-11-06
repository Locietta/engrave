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
#include "linkedobjects.h"

#include "masterscore.h"
#include "measure.h"
#include "score.h"
#include "staff.h"

#include "log.h"

using namespace mu::engraving;

bool LinkedObjects::contains(const EngravingObject *o) const { return std::find(this->begin(), this->end(), o) != this->end(); }

//---------------------------------------------------------
//   mainElement
//    Returns "main" linked element which is expected to
//    be written to the file prior to others.
//---------------------------------------------------------

EngravingObject *LinkedObjects::main_element() {
    if (empty()) {
        return nullptr;
    }
    MasterScore *ms = front()->score()->master_score();
    const bool elements = front()->is_engraving_item();
    const bool staves = front()->is_staff();
    return *std::min_element(begin(), end(), [ms, elements, staves](EngravingObject *s1, EngravingObject *s2) {
        if (s1->score() == ms && s2->score() != ms) {
            return true;
        }
        if (s1->score() != s2->score()) {
            return false;
        }
        if (staves) {
            return to_staff(s1)->idx() < to_staff(s2)->idx();
        }
        if (elements) {
            // Now we compare either two elements from master score
            // or two elements from excerpt.
            EngravingItem *e1 = to_engraving_item(s1);
            EngravingItem *e2 = to_engraving_item(s2);
            const track_idx_t tr1 = e1->track();
            const track_idx_t tr2 = e2->track();
            if (tr1 == tr2) {
                const Fraction tick1 = e1->tick();
                const Fraction tick2 = e2->tick();
                if (tick1 == tick2) {
                    Measure *m1 = e1->find_measure();
                    Measure *m2 = e2->find_measure();
                    if (!m1 || !m2) {
                        return false;
                    }

                    // MM rests are written to MSCX in the following order:
                    // 1) first measure of MM rest (m->hasMMRest() == true);
                    // 2) MM rest itself (m->isMMRest() == true);
                    // 3) other measures of MM rest (m->hasMMRest() == false).
                    //
                    // As mainElement() must find the first element that
                    // is going to be written to a file, MM rest writing
                    // order should also be considered.

                    if (m1->is_mm_rest() == m2->is_mm_rest()) {
                        // no difference if both are MM rests or both are usual measures
                        return false;
                    }

                    // MM rests may be generated but not written (e.g. if
                    // saving a file right after disabling MM rests)
                    bool mm_rests_written = e1->style().style_b(Sid::CREATE_MULTI_MEASURE_RESTS);
                    if (m1->tick() == m2->tick()) {
                        // MM rests may be still enabled but the mmRest may have been removed by editing
                        // NOTE: the fact that we have linked clones between the original measure and the MMRest
                        // and we have to decide which one is the "main" one is really bad tech debt. We should not
                        // use linked clones for that. Needs fix in future. [M.S.]
                        bool one_is_mm_rest_covering_other = m1->mm_rest() == m2 || m2->mm_rest() == m1;
                        mm_rests_written &= one_is_mm_rest_covering_other;
                    }

                    if (m1->is_mm_rest()) {
                        // m1 is earlier if m2 is *not* the first MM rest measure
                        return mm_rests_written && !m2->has_mm_rest();
                    }
                    if (m2->is_mm_rest()) {
                        // m1 is earlier if it *is* the first MM rest measure
                        return !mm_rests_written || m1->has_mm_rest();
                    }
                    return false;
                }
                return tick1 < tick2;
            }
            return tr1 < tr2;
        }
        return false;
    });
}
