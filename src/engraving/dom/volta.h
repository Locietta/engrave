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

#ifndef MU_ENGRAVING_VOLTA_H
#define MU_ENGRAVING_VOLTA_H

#include "textlinebase.h"

namespace mu::engraving {
struct Score;

struct Volta;
struct Measure;

//---------------------------------------------------------
//   @@ VoltaSegment
//---------------------------------------------------------

struct VoltaSegment final : TextLineBaseSegment {
private:
    DECLARE_CLASSOF(ElementType::VOLTA_SEGMENT)

public:
    VoltaSegment(Volta *);

    VoltaSegment *clone() const override { return new VoltaSegment(*this); }

    Volta *volta() const { return (Volta *) spanner(); }

    EngravingObject *property_delegate(Pid) const override;
};

//---------------------------------------------------------
//   @@ Volta
//   @P voltaType  enum (Volta.CLOSE, Volta.OPEN)
//---------------------------------------------------------

struct Volta final : TextLineBase {
private:
    DECLARE_CLASSOF(ElementType::VOLTA)

public:
    enum class Type : char { OPEN, CLOSED };

    Volta(DummyParentOr<EngravingItem> parent);

    Volta *clone() const override { return new Volta(*this); }

    Anchor anchor() const override { return Anchor::MEASURE; }

    LineSegment *create_line_segment() override;

    bool allow_time_anchor() const override { return false; }

    void set_channel() const;

    std::vector<int> endings() const { return endings_; }

    std::vector<int> &endings() { return endings_; }

    void set_endings(const std::vector<int> &l);
    void set_text(const String &s);
    String text() const;

    bool has_ending(int repeat) const;
    int first_ending() const;
    int last_ending() const;
    void set_volta_type(Volta::Type); // deprecated
    Type volta_type() const;          // deprecated

    bool is_first_volta() const { return endings_.size() == 1 && has_ending(1); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    PointF line_pos(Grip grip, System **system) const override;

protected:
    Sid default_pos_sid() const override;

private:
    std::vector<int> endings_;
};
} // namespace mu::engraving

#endif
