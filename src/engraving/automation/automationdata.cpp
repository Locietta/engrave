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

#include "automationdata.h"

#include "log.h"

using namespace mu::engraving;

static const AutomationCurve empty_curve;

static void diff_points(const AutomationCurveKey &key, const AutomationCurve &old_curve, const AutomationCurve &new_curve,
                        AutomationChanges &changes) {
    auto old_it = old_curve.begin();
    auto new_it = new_curve.begin();

    bool changed = false;
    utick_t first_tick = 0;
    utick_t last_tick = 0;

    // An ArrivalFromPrevious point can be bit-identical yet resolve differently if its predecessor's
    // outValue changed; only meaningful when that predecessor is the same point in both curves
    std::optional<real_t> prev_old_out;
    std::optional<real_t> prev_new_out;
    bool predecessor_aligned = true;

    while (old_it != old_curve.end() && new_it != new_curve.end()) {
        utick_t tick;
        bool differs;

        if (old_it->first < new_it->first) {
            tick = old_it->first; // point removed
            differs = true;
            prev_old_out = old_it->second.value.out_value;
            predecessor_aligned = false;
            ++old_it;
        } else if (new_it->first < old_it->first) {
            tick = new_it->first; // point added
            differs = true;
            prev_new_out = new_it->second.value.out_value;
            predecessor_aligned = false;
            ++new_it;
        } else {
            tick = old_it->first;
            differs = old_it->second != new_it->second // point changed
                      || (predecessor_aligned &&
                          resolve_in_value(old_it->second.value, prev_old_out) != resolve_in_value(new_it->second.value, prev_new_out));
            prev_old_out = old_it->second.value.out_value;
            prev_new_out = new_it->second.value.out_value;
            predecessor_aligned = true;
            ++old_it;
            ++new_it;
        }

        if (differs) {
            if (!changed) {
                first_tick = tick;
            }
            last_tick = tick;
            changed = true;
        }
    }

    if (old_it != old_curve.end() || new_it != new_curve.end()) {
        const bool old_has_tail = old_it != old_curve.end();
        const utick_t tail_first_tick = old_has_tail ? old_it->first : new_it->first;
        const utick_t tail_last_tick = old_has_tail ? old_curve.rbegin()->first : new_curve.rbegin()->first;

        if (!changed) {
            first_tick = tail_first_tick;
        }
        last_tick = tail_last_tick;
        changed = true;
    }

    if (changed) {
        changes.extend(key, first_tick, last_tick);
    }
}

const AutomationCurveMap &AutomationData::curves() const { return curve_map_; }

const AutomationCurve &AutomationData::curve(const AutomationCurveKey &key) const {
    auto curve_it = curve_map_.find(key);
    if (curve_it == curve_map_.end()) {
        return ::empty_curve;
    }

    return curve_it->second;
}

bool AutomationData::is_empty() const { return curve_map_.empty(); }

void AutomationData::set_curves(const AutomationCurveMap &curves) {
    TRACEFUNC;

    if (curve_map_.empty() && !curves.empty()) {
        curve_map_ = curves;
        return;
    }

    size_t overlap_count = 0;
    AutomationChanges changes;

    for (const auto &[key, new_curve] : curves) {
        const auto old_it = curve_map_.find(key);
        const bool existed = old_it != curve_map_.end();
        if (existed) {
            ++overlap_count;
        }

        const AutomationCurve &old_curve = existed ? old_it->second : ::empty_curve;
        diff_points(key, old_curve, new_curve, changes);
    }

    for (const auto &[key, old_curve] : curve_map_) {
        if (!curves.contains(key)) {
            diff_points(key, old_curve, ::empty_curve, changes);
        }
    }

    // Every key was touched by this change
    const size_t all_keys_count = curves.size() + curve_map_.size() - overlap_count;
    if (changes.affected_keys.size() == all_keys_count) {
        changes.is_full_reset = true;
    }

    curve_map_ = curves;
}

void AutomationData::replace_curves(const AutomationCurveMap &curves) {
    TRACEFUNC;

    if (curve_map_.empty() && !curves.empty()) {
        curve_map_ = curves;
        return;
    }

    const size_t old_size = curve_map_.size();
    size_t overlap_count = 0;
    AutomationChanges changes;

    for (const auto &[key, new_curve] : curves) {
        const auto old_it = curve_map_.find(key);
        const bool existed = old_it != curve_map_.end();
        if (existed) {
            ++overlap_count;
        }

        const AutomationCurve &old_curve = existed ? old_it->second : ::empty_curve;
        diff_points(key, old_curve, new_curve, changes);

        if (new_curve.empty()) {
            if (existed) {
                curve_map_.erase(old_it);
            }
        } else if (existed) {
            old_it->second = new_curve;
        } else {
            curve_map_.emplace(key, new_curve);
        }
    }

    // Every key was touched by this change
    const size_t all_keys_count = old_size + curves.size() - overlap_count;
    if (all_keys_count > 0 && changes.affected_keys.size() == all_keys_count) {
        changes.is_full_reset = true;
    }
}

void AutomationData::edit_points(const AutomationCurveKey &key, const AutomationPointEdits &edits) {
    TRACEFUNC;

    if (edits.empty()) {
        return;
    }

    const auto curve_it = curve_map_.try_emplace(key).first;
    AutomationCurve &curve = curve_it->second;
    AutomationChanges changes;
    bool changed = false;

    for (const auto &edit : edits) {
        if (std::holds_alternative<AutomationPointEdit::ErasePoint>(edit.change)) {
            const auto it = curve.find(edit.tick);
            if (it != curve.end()) {
                curve.erase(it);
                changes.extend(edit.tick, edit.tick);
                changed = true;
            }
            continue;
        }

        utick_t range_from = edit.tick;
        utick_t range_to = edit.tick;
        bool moved = false;

        const auto *move = std::get_if<AutomationPointEdit::MovePoint>(&edit.change);
        if (move && move->from != edit.tick) {
            curve.erase(move->from);
            range_from = std::min(range_from, move->from);
            range_to = std::max(range_to, move->from);
            moved = true;
        }

        const AutomationPoint &point = move ? move->point : std::get<AutomationPointEdit::SetPoint>(edit.change).point;

        auto point_it = curve.lower_bound(edit.tick);
        const bool exists = point_it != curve.end() && point_it->first == edit.tick;

        if (exists && point_it->second == point) {
            if (moved) {
                changes.extend(range_from, range_to);
                changed = true;
            }
            continue;
        }

        if (exists) {
            point_it->second = point;
        } else {
            curve.emplace_hint(point_it, edit.tick, point);
        }

        changes.extend(range_from, range_to);
        changed = true;
    }

    if (curve.empty()) {
        curve_map_.erase(curve_it);
    }

    if (changed) {
        changes.affected_keys.insert(key);
    }
}

std::string AutomationData::dump() const {
    auto type_name = [](AutomationType type) -> const char * {
        switch (type) {
            case AutomationType::DYNAMICS: return "Dynamics";
            case AutomationType::TEMPO: return "Tempo";
            case AutomationType::VOLUME: return "Volume";
            case AutomationType::PAN: return "Pan";
            case AutomationType::UNKNOWN: break;
        }
        return "Unknown";
    };

    auto scope_str = [](const AutomationCurveKey &key) -> std::string {
        if (const std::optional<InstrumentTrackId> track_id = key.track_id()) {
            return "instrument = " + track_id->part_id.to_std_string() + "/" + track_id->instrument_id.to_std_string();
        }

        if (const std::optional<muse::ID> staff_id = key.staff_id()) {
            std::string scope = "staff = " + staff_id->to_std_string();
            if (const std::optional<size_t> voice_idx = key.voice_idx()) {
                scope += ", voice = " + std::to_string(*voice_idx);
            }
            return scope;
        }

        return "global";
    };

    auto in_value_str = [](const AutomationPoint::InValue &in_value) -> std::string {
        if (std::holds_alternative<AutomationPoint::ArrivalFromPrevious>(in_value)) {
            return "in = <from previous>";
        }

        const AutomationPoint::ExplicitArrival &arrival = std::get<AutomationPoint::ExplicitArrival>(in_value);
        std::string result = "in = " + std::to_string(arrival.value.raw());
        if (!arrival.ease.is_none()) {
            result += " (ease t = " + std::to_string(arrival.ease.t.raw()) + ", value = " + std::to_string(arrival.ease.value.raw()) + ")";
        }
        return result;
    };

    std::string result = "AutomationData: " + std::to_string(curve_map_.size()) + " curves";

    for (const auto &[key, curve] : curve_map_) {
        result += "\n[" + std::string(type_name(key.type)) + " " + scope_str(key) + "] " + std::to_string(curve.size()) + " points";

        for (const auto &[tick, point] : curve) {
            result += "\n    tick = " + std::to_string(tick) + ", " + in_value_str(point.value.in_value) +
                      ", out = " + std::to_string(point.value.out_value.raw());
            if (point.generated) {
                result += ", [generated]";
            }
            if (point.item_id.has_value()) {
                result += ", itemId = " + point.item_id->to_std_string();
            }
        }
    }

    return result;
}
