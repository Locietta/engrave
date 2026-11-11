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

#ifndef MU_ENGRAVING_STAFFTEXT_H
#define MU_ENGRAVING_STAFFTEXT_H

#include "stafftextbase.h"

namespace mu::engraving {
struct StaffText final : StaffTextBase {
private:
    DECLARE_CLASSOF(ElementType::STAFF_TEXT)

public:
    StaffText(DummyParentOr<Segment> parent, TextStyleType = TextStyleType::STAFF);
    StaffText(const StaffText &);

    StaffText *clone() const override { return new StaffText(*this); }

    EngravingItem *linked_clone() override;

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    void set_track(track_idx_t idx) override;

    bool has_sound_flag() const;
    SoundFlag *sound_flag() const;
    void set_sound_flag(SoundFlag *flag);

private:
    PropertyValue property_default(Pid id) const override;

    SoundFlag *sound_flag_ = nullptr;
};
} // namespace mu::engraving
#endif // MU_ENGRAVING_STAFFTEXT_H
