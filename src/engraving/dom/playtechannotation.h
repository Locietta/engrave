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

#ifndef MU_ENGRAVING_PLAYINGTECHNIQUEANNOTATION_H
#define MU_ENGRAVING_PLAYINGTECHNIQUEANNOTATION_H

#include "stafftextbase.h"
#include "../types/types.h"

namespace mu::engraving {
class PlayTechAnnotation final : public StaffTextBase {
    OBJECT_ALLOCATOR(engraving, PlayTechAnnotation)
    DECLARE_CLASSOF(ElementType::PLAYTECH_ANNOTATION)

public:
    PlayTechAnnotation(DummyParentOr<Segment> parent, PlayingTechniqueType technique_type = PlayingTechniqueType::NATURAL,
                       TextStyleType tid = TextStyleType::STAFF);

    PlayingTechniqueType technique_type() const;
    void set_technique_type(const PlayingTechniqueType technique_type);

    bool play_play_tech_annotation() const { return play_play_tech_annotation_; }

    void set_play_play_tech_annotation(bool play) { play_play_tech_annotation_ = play; }

    PlayTechAnnotation *clone() const override;
    TranslatableString subtype_user_name() const override;

    bool is_handbells_symbol() const;

    PropertyValue get_property(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &val) override;
    PropertyValue property_default(Pid id) const override;

private:
    PlayingTechniqueType technique_type_ = PlayingTechniqueType::UNDEFINED;
    bool play_play_tech_annotation_ = true;
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_PLAYINGTECHNIQUEANNOTATION_H
