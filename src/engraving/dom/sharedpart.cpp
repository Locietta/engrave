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

#include "rendering/score/layoutdatastore.h"
#include "rendering/score/scorerenderer.h"
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

void SharedPart::add_origin_part(Part *p, size_t position) {
    DO_ASSERT(p->type() == ElementType::PART && !muse::contains(origin_parts_, p));

    const std::vector<Part *> &parts = score()->parts();
    if (position != muse::nidx) {
        origin_parts_.insert(origin_parts_.begin() + std::min(position, origin_parts_.size()), p);
    } else if (muse::contains(parts, p)) {
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
    static const SharedTrackMap empty;
    const auto *state = rendered_state_at_tick(std::max(tick, Fraction(0, 1)));
    return state ? state->shared_track_map : empty;
}

const RenderedStaveSharingStates &SharedPart::rendered_states() const {
    static const RenderedStaveSharingStates empty;
    const auto *store = score()->layout_data_store();
    if (!store) return empty;
    const auto &by_part = store->score_output().stave_sharing_states;
    const auto found = by_part.find(this);
    return found == by_part.end() ? empty : found->second;
}

const RenderedStaveSharingState *SharedPart::rendered_state_at_tick(const Fraction &tick) const {
    const auto &states = rendered_states();
    const auto next = states.upper_bound(tick);
    return next == states.begin() ? nullptr : &std::prev(next)->second;
}

void SharedPart::set_rendered_state(const Fraction &tick, RenderedStaveSharingState state) {
    auto &states = score()->renderer()->layout_data_store(*score()).score_output().stave_sharing_states[this];
    states.insert_or_assign(tick, std::move(state));
}

void SharedPart::remove_rendered_states(const Fraction &start_tick, const Fraction &end_tick) {
    auto &states = score()->renderer()->layout_data_store(*score()).score_output().stave_sharing_states[this];
    states.erase(states.lower_bound(start_tick), states.lower_bound(end_tick));
}

const AuthoredStaveSharingState *SharedPart::exact_authored_state(const Fraction &tick) const {
    const auto it = std::ranges::lower_bound(authored_states_, tick, std::less<>{}, &AuthoredStaveSharingStates::value_type::first);
    return it != authored_states_.end() && it->first == tick ? &it->second : nullptr;
}

void SharedPart::set_authored_state(const Fraction &tick, AuthoredStaveSharingState state) {
    const auto it = std::ranges::lower_bound(authored_states_, tick, std::less<>{}, &AuthoredStaveSharingStates::value_type::first);
    if (it != authored_states_.end() && it->first == tick) {
        it->second = std::move(state);
    } else {
        authored_states_.emplace(it, tick, std::move(state));
    }
}

void SharedPart::remove_authored_state(const Fraction &tick) {
    const auto it = std::ranges::lower_bound(authored_states_, tick, std::less<>{}, &AuthoredStaveSharingStates::value_type::first);
    if (it != authored_states_.end() && it->first == tick) authored_states_.erase(it);
}

void SharedPart::insert_time_shared(const Fraction &tick, const Fraction &len) {
    const auto first = std::ranges::lower_bound(authored_states_, tick, std::less<>{}, &AuthoredStaveSharingStates::value_type::first);
    auto shift_from = first;
    if (len.negative()) {
        const auto last =
            std::ranges::lower_bound(authored_states_, tick - len, std::less<>{}, &AuthoredStaveSharingStates::value_type::first);
        shift_from = authored_states_.erase(first, last);
    }
    for (auto it = shift_from; it != authored_states_.end(); ++it) it->first += len;
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

bool SharedPart::show() const { return show_ && enabled(); }
} // namespace mu::engraving
