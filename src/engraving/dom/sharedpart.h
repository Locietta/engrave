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

#include "part.h"

namespace mu::engraving {
// Map from origin track to shared track
using SharedTrackMap = std::map<track_idx_t, track_idx_t>;
std::string dump(const SharedTrackMap &map);

struct SharedPart final : Part {
private:
    OBJECT_ALLOCATOR(engraving, SharedPart)
    DECLARE_CLASSOF(ElementType::SHARED_PART)

public:
    SharedPart(Score *score);

    void add_origin_part(Part *p);
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
    void set_track_map_at_tick(const SharedTrackMap &map, const Fraction &tick);
    void remove_maps_between_ticks(const Fraction &start_tick, const Fraction &end_tick);

    bool is_same_instruments_at_tick(const Fraction &tick);

    bool is_same_instruments() const { return is_same_instruments_; }

private:
    Part *mapped_origin_part(track_idx_t shared_track, const Fraction &tick) const;

    void compute_is_same_instruments();
    bool is_same_instruments_ = true;

    bool enabled_ = true;
    std::vector<Part *> origin_parts_;
    std::map<Fraction, SharedTrackMap> track_maps_by_tick_{{Fraction(0, 1), SharedTrackMap()}};
};
} // namespace mu::engraving
