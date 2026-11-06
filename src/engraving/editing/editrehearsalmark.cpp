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

#include "editrehearsalmark.h"

#include "../dom/measure.h"
#include "../dom/rehearsalmark.h"
#include "../dom/score.h"
#include "../dom/segment.h"
#include "../dom/select.h"

using namespace mu;
using namespace mu::engraving;

//---------------------------------------------------------
//   resequenceRehearsalMarks
///   resequences rehearsal marks within a range selection
///   or, if nothing is selected, the entire score
//---------------------------------------------------------

void EditRehearsalMark::resequence_rehearsal_marks(Transaction &, Score *score) {
    bool no_selection = !score->selection().is_range();

    if (no_selection) {
        score->cmd_select_all();
    } else if (!score->selection().is_range()) {
        return;
    }

    RehearsalMark *last = nullptr;
    for (Segment *s = score->selection().start_segment(); s && s != score->selection().end_segment(); s = s->next1()) {
        for (EngravingItem *e : s->annotations()) {
            if (!e->is_rehearsal_mark()) {
                continue;
            }
            RehearsalMark *rm = to_rehearsal_mark(e);
            if (!rm->is_top_system_object()) {
                continue;
            }
            if (last) {
                String rm_text = next_rehearsal_mark_text(last, rm);
                for (EngravingObject *le : rm->link_list()) {
                    le->undo_change_property(Pid::TEXT, rm_text);
                }
            }
            last = rm;
        }
    }

    if (no_selection) {
        score->deselect_all();
    }
}

//---------------------------------------------------------
//   createRehearsalMarkText
//---------------------------------------------------------

String EditRehearsalMark::create_rehearsal_mark_text(const Score *score, RehearsalMark *current) {
    Fraction tick = current->segment()->tick();
    RehearsalMark *before = 0;
    RehearsalMark *after = 0;
    for (Segment *s = score->first_segment(SegmentType::ALL); s; s = s->next1()) {
        for (EngravingItem *e : s->annotations()) {
            if (e && e->is_rehearsal_mark()) {
                if (s->tick() < tick) {
                    before = to_rehearsal_mark(e);
                } else if (s->tick() > tick) {
                    after = to_rehearsal_mark(e);
                    break;
                }
            }
        }
        if (after) {
            break;
        }
    }
    String s = u"A";
    String s1 = before ? before->xml_text() : u"";
    String s2 = after ? after->xml_text() : u"";
    if (s1.is_empty()) {
        return s;
    }
    s = next_rehearsal_mark_text(before, current); // try to sequence
    if (s == current->xml_text()) {
        // no sequence detected (or current happens to be correct)
        return s;
    } else if (s == s2) {
        // next in sequence already present
        if (s1.at(0).is_letter()) {
            if (s1.size() == 2) {
                s = String(s1.at(0)) + Char::from_ascii(s1.at(1).to_ascii() + 1); // BB, BC, CC
            } else {
                s = s1 + u'1'; // B, B1, C
            }
        } else {
            s = s1 + u'A'; // 2, 2A, 3
        }
    }
    return s;
}

//---------------------------------------------------------
//   nextRehearsalMarkText
//    finds next rehearsal in sequence established by previous
//     Alphabetic sequences:
//      A, B, ..., Y, Z, AA, BB, ..., YY, ZZ
//      a, b, ..., y, z, aa, bb, ..., yy, zz
//     Numeric sequences:
//      1, 2, 3, ...
//      If number of previous rehearsal mark matches measure number, assume use of measure numbers throughout
//---------------------------------------------------------

String EditRehearsalMark::next_rehearsal_mark_text(const RehearsalMark *previous, const RehearsalMark *current) {
    String previous_text = previous->xml_text();
    String fallback = current ? current->xml_text() : previous_text + u"'";

    if (previous_text.size() == 1 && previous_text.at(0).is_letter()) {
        // single letter sequence
        if (previous_text == "Z") {
            return u"AA";
        } else if (previous_text == "z") {
            return u"aa";
        } else {
            return String(Char::from_ascii(previous_text.at(0).to_ascii() + 1));
        }
    } else if (previous_text.size() == 2 && previous_text.at(0).is_letter() && previous_text.at(1).is_letter()) {
        // double letter sequence
        if (previous_text.at(0) == previous_text.at(1)) {
            // repeated letter sequence
            if (previous_text.to_upper() != "ZZ") {
                String c = Char::from_ascii(previous_text.at(0).to_ascii() + 1);
                return c + c;
            } else {
                return fallback;
            }
        } else {
            return fallback;
        }
    } else {
        // try to interpret as number
        bool ok;
        int n = previous_text.to_int(&ok);
        if (!ok) {
            return fallback;
        } else if (current && n == previous->segment()->measure()->measure_number() + 1) {
            // use measure number
            n = current->segment()->measure()->measure_number() + 1;
            return String::number(n);
        } else {
            // use number sequence
            n = previous_text.to_int() + 1;
            return String::number(n);
        }
    }
}
