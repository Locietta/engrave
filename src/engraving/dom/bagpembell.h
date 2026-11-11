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

#ifndef MU_ENGRAVING_BAGPEMBELL_H
#define MU_ENGRAVING_BAGPEMBELL_H

#include <vector>

#include "engravingitem.h"

namespace mu::engraving {
using BagpipeNoteList = std::vector<int>;

//---------------------------------------------------------
//   BagpipeEmbellishmentInfo
//    name and notes for a BagpipeEmbellishment
//---------------------------------------------------------

struct BagpipeEmbellishmentInfo {
    const char *name = 0;
    AsciiStringView notes;
};

//---------------------------------------------------------
//   BagpipeEmbellishmentInfo
//    name, staff line and pitch for a bagpipe note
//---------------------------------------------------------

struct BagpipeNoteInfo {
    AsciiStringView name;
    int line = 0;
    int pitch = 0;
};

//---------------------------------------------------------
//   BagpipeEmbellishment
//    dummy element, used for drag&drop
//---------------------------------------------------------

struct BagpipeEmbellishment final : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::BAGPIPE_EMBELLISHMENT)

public:
    BagpipeEmbellishment(DummyParentOr<EngravingItem> parent)
        : EngravingItem(ElementType::BAGPIPE_EMBELLISHMENT, parent), embel_type_(EmbellishmentType(0)) {}

    BagpipeEmbellishment *clone() const override { return new BagpipeEmbellishment(*this); }

    EmbellishmentType embel_type() const { return embel_type_; }

    void set_embel_type(EmbellishmentType val) { embel_type_ = val; }

    double mag() const override;

    static const BagpipeNoteInfo bagpipe_noteinfo_list[];
    BagpipeNoteList resolve_note_list() const;

    struct LayoutData : public EngravingItem::LayoutData {
        struct NoteData {
            PointF head_xy;
            LineF stem_line;
            PointF flag_xy;
            LineF ledger_line;
        };

        struct BeamData {
            double width = 0.0; // line width for beam
            double y = 0.0;
            double x1 = 0.0;
            double x2 = 0.0;
        };

        bool is_draw_beam = false;
        bool is_draw_flag = false;
        double spatium = 1.0;          // spatium
        SymId headsym = SymId::NO_SYM; // grace note head symbol
        SymId flagsym = SymId::NO_SYM; // grace note flag symbol
        double stem_line_w = 0.0;      // line width for stem
        std::map<size_t /*note index*/, NoteData> notes_data;
        BeamData beam_data;
    };
    DECLARE_LAYOUTDATA_METHODS(BagpipeEmbellishment)

private:

    EmbellishmentType embel_type_;
};
} // namespace mu::engraving

#endif
