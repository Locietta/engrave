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
#include "sharedpart.h"

#include <sstream>

#include "score.h"
#include "staff.h"

namespace mu::engraving {
std::string dump(const SharedTrackMap &map) {
    std::stringstream ss;
    ss << "SharedTrackMap size: " << map.size() << " {";
    for (auto it = map.begin(); it != map.end(); ++it) {
        if (it != map.begin()) {
            ss << ", ";
        }
        ss << it->first << ": " << it->second;
    }
    ss << "}";
    return ss.str();
}

SharedPart::SharedPart(Score *score) : Part(score, ElementType::SHARED_PART) {}

void SharedPart::add_origin_part(Part *p) {
    DO_ASSERT(p->type() == ElementType::PART && !muse::contains(origin_parts_, p));

    const std::vector<Part *> &parts = score()->parts();
    if (muse::contains(parts, p)) {
        std::unordered_map<Part *, size_t> order;
        for (size_t i = 0; i < parts.size(); ++i) {
            order[parts[i]] = i;
        }

        auto it = std::lower_bound(origin_parts_.begin(), origin_parts_.end(), p,
                                   [&order](Part *part1, Part *part2) { return order[part1] < order[part2]; });

        origin_parts_.insert(it, p);
    } else {
        // Can happen while reading files as p has not been added to the score yet
        origin_parts_.push_back(p);
    }

    p->set_shared_part(this);

    compute_is_same_instruments();
}

void SharedPart::remove_origin_part(Part *p) {
    DO_ASSERT(muse::remove(origin_parts_, p));

    p->set_shared_part(nullptr);

    compute_is_same_instruments();
}

Part *SharedPart::mapped_origin_part(track_idx_t shared_track, const Fraction &tick) const {
    const SharedTrackMap &track_map = track_map_at_tick(tick.negative() ? Fraction(0, 1) : tick);

    track_idx_t origin_track = muse::key(track_map, shared_track, muse::nidx);

    const Staff *origin_staff = origin_track != muse::nidx ? score()->staff(track2staff(origin_track)) : nullptr;
    return origin_staff ? origin_staff->part() : nullptr;
}

const Part *SharedPart::origin_part(track_idx_t shared_track, const Fraction &tick) const {
    if (shared_track != muse::nidx) {
        if (const Part *p = mapped_origin_part(shared_track, tick)) {
            return p;
        }
    }

    return origin_parts_.empty() ? this : origin_parts_.front();
}

Part *SharedPart::origin_part_for_staff(staff_idx_t shared_staff_idx, const Fraction &tick) const {
    return mapped_origin_part(staff2track(shared_staff_idx, 0), tick);
}

const SharedTrackMap &SharedPart::track_map_at_tick(const Fraction &tick) const {
    static constexpr Fraction tick_zero = Fraction(0, 1);

    IF_ASSERT_FAILED(track_maps_by_tick_.size() > 0) {
        const_cast<SharedPart *>(this)->track_maps_by_tick_[tick_zero] = SharedTrackMap();
        return track_maps_by_tick_.at(tick_zero);
    }

    DO_ASSERT(muse::contains(track_maps_by_tick_, tick_zero));

    IF_ASSERT_FAILED(tick.positive()) { return track_maps_by_tick_.at(tick_zero); }

    auto upper_bound_value = track_maps_by_tick_.upper_bound(tick);
    DO_ASSERT(upper_bound_value != track_maps_by_tick_.begin());

    --upper_bound_value;

    return upper_bound_value->second;
}

void SharedPart::set_track_map_at_tick(const SharedTrackMap &map, const Fraction &tick) { track_maps_by_tick_[tick] = map; }

void mu::engraving::SharedPart::remove_maps_between_ticks(const Fraction &start_tick, const Fraction &end_tick) {
    auto upper_bound_value = track_maps_by_tick_.lower_bound(start_tick);
    if (upper_bound_value == track_maps_by_tick_.end()) {
        return;
    }

    for (auto iter = upper_bound_value; iter != track_maps_by_tick_.end();) {
        if (iter->first >= end_tick) {
            break;
        }
        iter = track_maps_by_tick_.erase(iter);
    }
}

bool SharedPart::is_same_instruments_at_tick(const Fraction &tick) {
    if (origin_parts_.empty()) {
        return false;
    }

    const Instrument *instr = origin_parts_.front()->instrument(tick);
    for (Part *p : origin_parts_) {
        if (p->instrument(tick)->id() != instr->id()) {
            return false;
        }
    }

    return true;
}

void SharedPart::compute_is_same_instruments() { is_same_instruments_ = is_same_instruments_at_tick(Fraction(0, 1)); }

mu::engraving::String mu::engraving::SharedPart::part_name() const {
    const Instrument *i = instrument();
    String full_name = plain_part_name(i);

    const String &transp = i->transposition();
    if (!transp.empty()) {
        //: For instrument transposition, e.g. Horn in F
        full_name += u" " + muse::mtrc("engraving", "in") + u" " + transp;
    }

    int first_number = 10000;
    int last_number = 0;

    for (const Part *origin_part : origin_parts()) {
        int number = origin_part->instrument()->number();
        first_number = std::min(first_number, number);
        last_number = std::max(last_number, number);
    }

    if (first_number == last_number) {
        return full_name;
    }

    full_name += u" " + String::number(first_number) + u"-" + String::number(last_number);

    return full_name;
}

PropertyValue SharedPart::get_property(Pid pid) const {
    switch (pid) {
        case Pid::SHARED_PART_ENABLED: return enabled_;
        default: return Part::get_property(pid);
    }
}

PropertyValue SharedPart::property_default(Pid pid) const {
    switch (pid) {
        case Pid::SHARED_PART_ENABLED: return true;
        default: return Part::property_default(pid);
    }
}

bool SharedPart::set_property(Pid pid, const PropertyValue &v) {
    switch (pid) {
        case Pid::SHARED_PART_ENABLED: enabled_ = v.to_bool(); break;
        default: return Part::set_property(pid, v);
    }

    score()->set_layout_all();
    return true;
}

bool SharedPart::enabled() const { return enabled_ && style().style_b(Sid::ENABLE_STAVE_SHARING); }

bool SharedPart::show() const { return show_ && enabled_ && style().style_b(Sid::ENABLE_STAVE_SHARING); }
} // namespace mu::engraving
