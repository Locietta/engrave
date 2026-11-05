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

#include "textlinebase.h"
#include "../types/types.h"

namespace mu::engraving {
class GradualTempoChangeSegment;

class GradualTempoChange : public TextLineBase {
    OBJECT_ALLOCATOR(engraving, GradualTempoChange)
    DECLARE_CLASSOF(ElementType::GRADUAL_TEMPO_CHANGE)

public:
    GradualTempoChange(DummyParentOr<EngravingItem> parent);

    GradualTempoChange *clone() const override;

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *createLineSegment() override;

    GradualTempoChangeType tempoChangeType() const;
    ChangeMethod easingMethod() const;
    void setTempoChangeType(const GradualTempoChangeType type);

    double tempoChangeFactor() const;

    PropertyValue getProperty(Pid id) const override;
    bool setProperty(Pid id, const PropertyValue &val) override;
    PropertyValue propertyDefault(Pid propertyId) const override;
    Sid getPropertyStyle(Pid id) const override;

    TranslatableString subtypeUserName() const override;

    bool snapToItemAfter() const { return snapToItemAfter_; }

    void setSnapToItemAfter(bool v) { snapToItemAfter_ = v; }

    bool adjustForRehearsalMark(bool start) const;
    PointF linePos(Grip grip, System **system) const override;

protected:
    Sid defaultPosSid() const override;

private:
    GradualTempoChangeType tempoChangeType_ = GradualTempoChangeType::Undefined;
    ChangeMethod tempoEasingMethod_ = ChangeMethod::NORMAL;
    std::optional<float> tempoChangeFactor_;

    bool snapToItemAfter_ = true;

    bool alignRightOfRehearsalMark_ = true;

    friend class GradualTempoChangeSegment;
};

class GradualTempoChangeSegment : public TextLineBaseSegment {
    OBJECT_ALLOCATOR(engraving, GradualTempoChangeSegment)
    DECLARE_CLASSOF(ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT)

public:
    GradualTempoChangeSegment(GradualTempoChange *annotation);

    GradualTempoChangeSegment *clone() const override;

    GradualTempoChange *tempoChange() const;

    GradualTempoChangeSegment *findElementToSnapBefore() const;
    TempoText *findElementToSnapAfter() const;

    friend class GradualTempoChange;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_GRADUALTEMPOCHANGE_H
