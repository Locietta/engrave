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

#ifndef MU_ENGRAVING_GRADUALTEMPOCHANGE_H
#define MU_ENGRAVING_GRADUALTEMPOCHANGE_H

#include <optional>

#include "../types/types.h"
#include "textlinebase.h"

namespace mu::engraving {
struct GradualTempoChangeSegment;

struct GradualTempoChange : TextLineBase {
private:
    DECLARE_CLASSOF(ElementType::GRADUAL_TEMPO_CHANGE)

public:
    GradualTempoChange(DummyParentOr<EngravingItem> parent);

    GradualTempoChange *clone() const override;

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;

    GradualTempoChangeType tempo_change_type() const;
    ChangeMethod easing_method() const;
    void set_tempo_change_type(const GradualTempoChangeType type);

    double tempo_change_factor() const;

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid id, const PropertyValue &val) override;
    PropertyValue property_default(Pid property_id) const override;
    Sid get_property_style(Pid id) const override;

    TranslatableString subtype_user_name() const override;

    bool snap_to_item_after() const { return snap_to_item_after_; }

    void set_snap_to_item_after(bool v) { snap_to_item_after_ = v; }

    bool adjust_for_rehearsal_mark(bool start) const;
    PointF line_pos(Grip grip, System **system) const override;

protected:
    Sid default_pos_sid() const override;

private:
    GradualTempoChangeType tempo_change_type_ = GradualTempoChangeType::UNDEFINED;
    ChangeMethod tempo_easing_method_ = ChangeMethod::NORMAL;
    std::optional<float> tempo_change_factor_;

    bool snap_to_item_after_ = true;

    bool align_right_of_rehearsal_mark_ = true;

    friend struct GradualTempoChangeSegment;
};

struct GradualTempoChangeSegment : TextLineBaseSegment {
private:
    DECLARE_CLASSOF(ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT)

public:
    GradualTempoChangeSegment(GradualTempoChange *annotation);

    GradualTempoChangeSegment *clone() const override;

    GradualTempoChange *tempo_change() const;

    GradualTempoChangeSegment *find_element_to_snap_before() const;
    TempoText *find_element_to_snap_after() const;

    friend struct GradualTempoChange;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_GRADUALTEMPOCHANGE_H
