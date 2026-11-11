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

#ifndef MU_ENGRAVING_PICKSCRAPE_H
#define MU_ENGRAVING_PICKSCRAPE_H

#include "chordtextlinebase.h"

namespace mu::engraving {
struct PickScrape;

//---------------------------------------------------------
//   @@ PickScrapeSegment
//---------------------------------------------------------

struct PickScrapeSegment final : TextLineBaseSegment {
private:
    DECLARE_CLASSOF(ElementType::PICK_SCRAPE_SEGMENT)

public:
    PickScrapeSegment(PickScrape *sp);

    PickScrapeSegment *clone() const override { return new PickScrapeSegment(*this); }

    PickScrape *pick_scrape() const { return (PickScrape *) spanner(); }

    friend struct PickScrape;
};

//---------------------------------------------------------
//   @@ PickScrape
//---------------------------------------------------------

struct PickScrape final : ChordTextLineBase {
private:
    DECLARE_CLASSOF(ElementType::PICK_SCRAPE)

public:
    PickScrape(DummyParentOr<EngravingItem> parent);

    PickScrape *clone() const override { return new PickScrape(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;

    PropertyValue property_default(Pid property_id) const override;
    Sid get_property_style(Pid) const override;

protected:
    Sid default_pos_sid() const override;
};
} // namespace mu::engraving
#endif
