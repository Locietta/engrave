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

#pragma once

#include "engraving/dom/textbase.h"
#include "engraving/rw/xmlwriter.h"

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   MScoreTextToMXML
//---------------------------------------------------------

class MScoreTextToMusicXml {
public:
    MScoreTextToMusicXml(const muse::String &tag, const muse::String &attr, const engraving::CharFormat &def_fmt, const muse::String &mtf);
    static muse::String to_plain_text(const muse::String &text);
    static muse::String to_plain_text_plus_symbols(const std::list<engraving::TextFragment> &list);
    static bool split(const std::list<engraving::TextFragment> &in, const int pos, const int len, std::list<engraving::TextFragment> &left,
                      std::list<engraving::TextFragment> &mid, std::list<engraving::TextFragment> &right);
    void write_text_fragments(const std::list<engraving::TextFragment> &fr, engraving::XmlWriter &xml);

private:
    muse::String update_format();
    muse::String attribs;
    muse::String tagname;
    engraving::CharFormat old_format;
    engraving::CharFormat new_format;
    muse::String musical_text_font;
};
} // namespace mu::iex::musicxml
