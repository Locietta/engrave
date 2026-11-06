/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include "editstaffbrackets.h"

#include "../dom/bracket.h"
#include "../dom/bracketitem.h"
#include "../dom/factory.h"
#include "../dom/score.h"
#include "../dom/staff.h"

using namespace mu::engraving;

void EditStaffBrackets::undo_add_bracket(Score *score, Staff *staff, size_t level, BracketType type, size_t span) {
    staff_idx_t start_staff_idx = staff->idx();
    staff_idx_t tot_staves = score->nstaves();

    // Make sure this brackets won't overlap with others sharing same column.
    // If overlaps are found, move the other brackets outwards (i.e. increase column).
    for (staff_idx_t staff_idx = start_staff_idx; staff_idx < start_staff_idx + span && staff_idx < tot_staves; ++staff_idx) {
        const std::vector<BracketItem *> &score_brackets = score->brackets(staff_idx);

        bool collision = false;
        for (BracketItem *b : score_brackets) {
            if (b->bracket_type() != BracketType::NO_BRACKET && b->bracket_type() != BracketType::GROUP && b->column() == level) {
                collision = true;
                break;
            }
        }

        if (!collision) {
            continue;
        }

        for (int i = static_cast<int>(score_brackets.size()) - 1; i >= static_cast<int>(level); --i) {
            if (i >= static_cast<int>(score_brackets.size())) {
                // This might theoretically happen when a lot of brackets get cleaned up
                // after changing the column of the first bracket we see
                continue;
            }

            BracketItem *bi = score_brackets[i];
            if (bi->column() > level) {
                continue;
            }

            if (bi->bracket_type() == BracketType::NO_BRACKET || bi->bracket_type() == BracketType::GROUP) {
                continue;
            }

            bi->undo_change_property(Pid::BRACKET_COLUMN, bi->column() + 1);
        }
    }

    score->undo(new AddBracket(staff, level, type, span));
}

void EditStaffBrackets::undo_remove_bracket(Score *score, Bracket *b) {
    score->undo(new RemoveBracket(b->staff(), b->column(), b->bracket_type(), b->span()));
}

void EditStaffBrackets::fill_brackets(Score *score, staff_idx_t staff_idx, size_t idx) {
    // make sure index idx is valid
    std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
    for (size_t i = staff_brackets.size(); i <= idx; ++i) {
        BracketItem *bi = Factory::create_bracket_item(score->dummy());
        bi->set_start_staff_idx(staff_idx);
        bi->set_column(i);
        bi->set_score(score);
        staff_brackets.push_back(bi);
    }
}

void EditStaffBrackets::clean_brackets(Score *score, staff_idx_t staff_idx) {
    // remove NO_BRACKET entries from the end of list
    std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
    while (!staff_brackets.empty() && (staff_brackets.back()->bracket_type() == BracketType::NO_BRACKET)) {
        BracketItem *bi = muse::take_last(staff_brackets);
        delete bi;
    }
}

void EditStaffBrackets::adjust_brackets_del(Score *score, size_t sidx, size_t eidx) {
    IF_ASSERT_FAILED(sidx < eidx && eidx <= score->staves().size()) { return; }

    for (size_t staff_idx = 0; staff_idx < eidx; ++staff_idx) {
        const std::vector<BracketItem *> staff_brackets = score->brackets(staff_idx);
        for (BracketItem *bi : staff_brackets) {
            size_t span = bi->bracket_span();
            if ((span == 0) || ((staff_idx + span) <= sidx)) {
                continue;
            }
            const bool starts_outside_deleted_range = (staff_idx < sidx);
            const bool ends_outside_deleted_range = ((staff_idx + span) >= eidx);
            if (starts_outside_deleted_range && ends_outside_deleted_range) {
                // Shorten the bracket by the number of staves deleted
                bi->undo_change_property(Pid::BRACKET_SPAN, int(span - (eidx - sidx)));
            } else if (starts_outside_deleted_range) {
                // Shorten the bracket by the number of staves deleted that were spanned by it
                bi->undo_change_property(Pid::BRACKET_SPAN, int(sidx - staff_idx));
            } else if (ends_outside_deleted_range) {
                const size_t column = bi->column();
                const BracketType bracket_type = bi->bracket_type();
                score->undo(new RemoveBracket(score->staves().at(staff_idx), bi->column(), bi->bracket_type(), span));
                int new_span = int(span - (eidx - staff_idx));
                if (eidx < score->staves().size() && new_span > 0) {
                    // Move the bracket past the end of the deleted range,
                    // and shorten it by the number of staves deleted that were spanned by it.

                    EditStaffBrackets::undo_add_bracket(score, score->staves().at(eidx), column, bracket_type, new_span);
                }
            }
        }
    }
}

void EditStaffBrackets::adjust_brackets_ins(Score *score, size_t sidx, size_t eidx) {
    for (size_t staff_idx = 0; staff_idx < score->staves().size(); ++staff_idx) {
        const std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
        for (BracketItem *bi : staff_brackets) {
            size_t span = bi->bracket_span();
            if ((span == 0) || ((staff_idx + span) < sidx) || (staff_idx > eidx)) {
                continue;
            }
            if ((sidx >= staff_idx) && (eidx <= (staff_idx + span))) {
                bi->undo_change_property(Pid::BRACKET_SPAN, int(span + (eidx - sidx)));
            }
        }
    }
}

void EditStaffBrackets::set_bracket_type(Score *score, staff_idx_t staff_idx, size_t idx, BracketType val) {
    std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);

    fill_brackets(score, staff_idx, idx);
    staff_brackets[idx]->set_bracket_type(val);
    clean_brackets(score, staff_idx);
}

void EditStaffBrackets::change_bracket_column(Score *score, staff_idx_t staff_idx, size_t old_column, size_t new_column) {
    if (old_column == new_column) {
        return;
    }
    std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);

    size_t idx = std::max(old_column, new_column);
    fill_brackets(score, staff_idx, idx);
    int step = new_column > old_column ? 1 : -1;
    for (size_t i = old_column; i != new_column; i += step) {
        size_t old_idx = i;
        size_t new_idx = i + step;
        staff_brackets[old_idx]->set_column(new_idx);
        staff_brackets[new_idx]->set_column(old_idx);
        muse::swap_items_at(staff_brackets, old_idx, new_idx);
    }
    clean_brackets(score, staff_idx);
}

void EditStaffBrackets::set_bracket_span(Score *score, staff_idx_t staff_idx, size_t idx, size_t val) {
    const std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
    fill_brackets(score, staff_idx, idx);
    staff_brackets[idx]->set_bracket_span(val);
}

void EditStaffBrackets::set_bracket_visible(Score *score, staff_idx_t staff_idx, size_t idx, bool v) {
    const std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
    fill_brackets(score, staff_idx, idx);
    staff_brackets[idx]->set_visible(v);
}

void EditStaffBrackets::add_bracket(Score *score, staff_idx_t staff_idx, BracketItem *b) {
    std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
    b->set_start_staff_idx(staff_idx);
    if (!staff_brackets.empty() && staff_brackets[0]->bracket_type() == BracketType::NO_BRACKET) {
        delete staff_brackets[0];
        staff_brackets[0] = b;
    } else {
        //
        // create new bracket level
        //
        for (staff_idx_t sidx = 0; sidx < score->nstaves(); ++sidx) {
            if (sidx == staff_idx) {
                staff_brackets.push_back(b);
                b->set_score(score);
            } else {
                BracketItem *bi = Factory::create_bracket_item(score->dummy());
                bi->set_start_staff_idx(sidx);
                bi->set_score(score);
                score->brackets(sidx).push_back(bi);
            }
        }
    }
}

void EditStaffBrackets::insert_bracket(Score *score, staff_idx_t staff_idx, BracketItem *b) {
    std::vector<BracketItem *> &staff_brackets = score->brackets(staff_idx);
    b->set_start_staff_idx(staff_idx);
    size_t column = b->column();
    if (column < staff_brackets.size()) {
        if (staff_brackets[column]) {
            delete staff_brackets[column];
        }
        staff_brackets[column] = b;
    } else if (column == staff_brackets.size()) {
        staff_brackets.push_back(b);
    } else {
        fill_brackets(score, staff_idx, column - 1);
        staff_brackets.push_back(b);
    }
}

//---------------------------------------------------------
//   AddBracket
//---------------------------------------------------------

void AddBracket::redo() {
    EditStaffBrackets::set_bracket_type(staff->score(), staff->idx(), level, bracket_type);
    EditStaffBrackets::set_bracket_span(staff->score(), staff->idx(), level, span);
    staff->trigger_layout();
}

void AddBracket::undo() {
    EditStaffBrackets::set_bracket_type(staff->score(), staff->idx(), level, BracketType::NO_BRACKET);
    staff->trigger_layout();
}

//---------------------------------------------------------
//   RemoveBracket
//---------------------------------------------------------

void RemoveBracket::redo() {
    EditStaffBrackets::set_bracket_type(staff->score(), staff->idx(), level, BracketType::NO_BRACKET);
    staff->trigger_layout();
}

void RemoveBracket::undo() {
    EditStaffBrackets::set_bracket_type(staff->score(), staff->idx(), level, bracket_type);
    EditStaffBrackets::set_bracket_span(staff->score(), staff->idx(), level, span);
    staff->trigger_layout();
}
