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
#pragma once

#include <optional>

#include "part.h"

namespace mu::engraving {
// Map from origin track to shared track
using SharedTrackMap = std::map<track_idx_t, track_idx_t>;
std::string dump(const SharedTrackMap &map);

struct AuthoredStaveSharingState {
    SharedTrackMap user_track_map;
    bool is_reset = false;
    bool operator==(const AuthoredStaveSharingState &) const = default;
};

using AuthoredStaveSharingStates = std::vector<std::pair<Fraction, AuthoredStaveSharingState>>;

struct RenderedStaveSharingState {
    SharedTrackMap shared_track_map;
    bool is_user_map_valid = false;
    bool operator==(const RenderedStaveSharingState &) const = default;
};

using RenderedStaveSharingStates = std::map<Fraction, RenderedStaveSharingState>;

struct SharedPart final : Part {
private:
    DECLARE_CLASSOF(ElementType::SHARED_PART)

public:
    SharedPart(Score *score);

    void add_origin_part(Part *p, size_t position = muse::nidx);
    void remove_origin_part(Part *p);

    const std::vector<Part *> &origin_parts() const { return origin_parts_; }

    const Part *origin_part(track_idx_t shared_track, const Fraction &tick) const override;
    using Part::origin_part;
    Part *origin_part_for_staff(staff_idx_t shared_staff_idx, const Fraction &tick) const;

    String part_name() const override;

    PropertyValue get_property(Pid pid) const override;
    PropertyValue property_default(Pid pid) const override;
    bool set_property(Pid pid, const PropertyValue &v) override;

    bool enabled() const;
    bool show() const override;

    const SharedTrackMap &track_map_at_tick(const Fraction &tick) const;
    const RenderedStaveSharingState *rendered_state_at_tick(const Fraction &tick) const;
    const RenderedStaveSharingStates &rendered_states() const;
    void set_rendered_state(const Fraction &tick, RenderedStaveSharingState state);
    void remove_rendered_states(const Fraction &start_tick, const Fraction &end_tick);

    const AuthoredStaveSharingStates &authored_states() const { return authored_states_; }

    const AuthoredStaveSharingState *exact_authored_state(const Fraction &tick) const;
    void set_authored_state(const Fraction &tick, AuthoredStaveSharingState state);
    void remove_authored_state(const Fraction &tick);
    void insert_time_shared(const Fraction &tick, const Fraction &len);

    bool is_same_instruments_at_tick(const Fraction &tick);

    bool is_same_instruments() const { return is_same_instruments_; }

private:
    Part *mapped_origin_part(track_idx_t shared_track, const Fraction &tick) const;

    void compute_is_same_instruments();
    bool is_same_instruments_ = true;

    AuthoredStaveSharingStates authored_states_;
    bool enabled_ = true;
    std::vector<Part *> origin_parts_;
};
} // namespace mu::engraving
