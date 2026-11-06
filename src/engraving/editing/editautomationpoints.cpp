/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "editautomationpoints.h"

#include "engraving/automation/internal/scoreautomationcontroller.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/repeatlist.h"
#include "engraving/dom/staff.h"
#include "log.h"

using namespace mu::engraving;

static std::optional<ChangedRange> compute_changed_range(const Score *score, const AutomationCurveKey &key,
                                                         const std::map<utick_t, std::optional<AutomationPoint>> &point_states) {
    IF_ASSERT_FAILED(score && !point_states.empty()) { return std::nullopt; }

    size_t staff_idx = muse::nidx;
    if (const std::optional<muse::ID> staff_id = key.staff_id()) {
        for (size_t i = 0; i < score->nstaves(); ++i) {
            const Staff *staff = score->staff(i);
            if (staff && staff->id() == *staff_id) {
                staff_idx = i;
                break;
            }
        }

        IF_ASSERT_FAILED(staff_idx != muse::nidx) { return std::nullopt; }
    }
    // else: Global/Instrument-scoped key - not tied to any staff, leave staffIdx as muse::nidx

    const RepeatList &repeat_list = score->expanded_repeat_list();

    //! NOTE: utick2tick can be non-monotonic across repeat passes (e.g. D.S. al Coda),
    //! so the base-tick extremes aren't necessarily at the utick extremes
    Fraction tick_from = Fraction::from_ticks(repeat_list.utick2tick(point_states.begin()->first));
    Fraction tick_to = tick_from;
    for (const auto &state : point_states) {
        const Fraction tick = Fraction::from_ticks(repeat_list.utick2tick(state.first));
        if (tick < tick_from) {
            tick_from = tick;
        }
        if (tick > tick_to) {
            tick_to = tick;
        }
    }

    return ChangedRange{tick_from, tick_to, staff_idx, staff_idx};
}

static void widen_changed_range(const Score *score, const AutomationCurveKey &key, const AutomationPointEdits &edits,
                                ChangedRange &changed_range) {
    if (key.type == AutomationType::TEMPO) {
        const Measure *last_measure = score->last_measure();
        if (last_measure) {
            changed_range.tick_to = last_measure->end_tick();
        }
        return;
    }

    const RepeatList &repeat_list = score->expanded_repeat_list();
    for (const AutomationPointEdit &edit : edits) {
        const Fraction tick = Fraction::from_ticks(repeat_list.utick2tick(edit.tick));
        changed_range.tick_from = std::min(changed_range.tick_from, tick);
        changed_range.tick_to = std::max(changed_range.tick_to, tick);
    }
}

EditAutomationPoints::EditAutomationPoints(Score *score, ScoreAutomationController *controller, const AutomationCurveKey &key,
                                           const AutomationPointEdits &edits)
    : score_(score), controller_(controller), key_(key) {
    assert(score && controller && !edits.empty());

    // Collapse the edit list into the final value to write at each touched tick,
    // replaying the same erase-then-write order AutomationData::editPoints itself would apply
    for (const AutomationPointEdit &edit : edits) {
        if (const auto *set_point = std::get_if<AutomationPointEdit::SetPoint>(&edit.change)) {
            point_states_[edit.tick] = set_point->point;
        } else if (const auto *move_point = std::get_if<AutomationPointEdit::MovePoint>(&edit.change)) {
            if (move_point->from != edit.tick) {
                point_states_[move_point->from] = std::nullopt;
            }
            point_states_[edit.tick] = move_point->point;
        } else {
            point_states_[edit.tick] = std::nullopt;
        }
    }
}

std::optional<ChangedRange> EditAutomationPoints::changed_range() const { return changed_range_; }

void EditAutomationPoints::flip() {
    TRACEFUNC;

    IF_ASSERT_FAILED(score_ && controller_ && controller_->automation_data()) { return; }

    const AutomationCurve &curve = controller_->automation_data()->curve(key_);
    changed_range_ = compute_changed_range(score_, key_, point_states_);

    std::map<utick_t, std::optional<AutomationPoint>> previous_states;
    AutomationPointEdits automation_edits;
    automation_edits.reserve(point_states_.size());

    for (const auto &[tick, point] : point_states_) {
        const auto it = curve.find(tick);
        if (it != curve.cend()) {
            previous_states.emplace(tick, it->second);
        } else {
            previous_states.emplace(tick, std::nullopt);
        }

        if (point) {
            automation_edits.push_back({tick, AutomationPointEdit::SetPoint{*point}});
        } else {
            automation_edits.push_back({tick, AutomationPointEdit::ErasePoint{}});
        }
    }

    if (changed_range_) {
        controller_->edit_points(key_, automation_edits);
        widen_changed_range(score_, key_, automation_edits, *changed_range_);
    }

    point_states_ = std::move(previous_states);
}
