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

/**
 MusicXML font handling support.
 */

#include "musicxmlfonthandler.h"

#include "engraving/rw/xmlwriter.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::musicxml {
//---------------------------------------------------------
//   MScoreTextToMXML
//---------------------------------------------------------

MScoreTextToMusicXml::MScoreTextToMusicXml(const String &tag, const String &attr, const CharFormat &def_fmt, const String &mtf)
    : attribs(attr), tagname(tag), old_format(def_fmt), musical_text_font(mtf) {
    // set MusicXML defaults
    old_format.set_bold(false);
    old_format.set_italic(false);
    old_format.set_underline(false);
}

//---------------------------------------------------------
//   toPlainText
//    convert to plain text
//    naive implementation: simply remove all chars from '<' to '>'
//    typically used to remove formatting info from fields read
//    from MuseScore 1.3 file where they are stored as html, such as
//    part name and shortName
//---------------------------------------------------------

String MScoreTextToMusicXml::to_plain_text(const String &text) {
    String res;
    bool in_elem = false;
    for (size_t i = 0; i < text.size(); ++i) {
        Char ch = text.at(i);
        if (ch == u'<') {
            in_elem = true;
        } else if (ch == u'>') {
            in_elem = false;
        } else {
            if (!in_elem) {
                res += ch;
            }
        }
    }
    //LOGD("MScoreTextToMXML::toPlainText('%s') res '%s'", muPrintable(text), muPrintable(res));
    return res;
}

//---------------------------------------------------------
//   toPlainTextPlusSymbols
//    convert to plain text plus <sym>[name]</sym> encoded symbols
//---------------------------------------------------------

String MScoreTextToMusicXml::to_plain_text_plus_symbols(const std::list<TextFragment> &list) {
    String res;
    for (const TextFragment &f : list) {
        res += f.text;
    }
    return res;
}

//---------------------------------------------------------
//   plainTextPlusSymbolsSize
//---------------------------------------------------------

static int plain_text_plus_symbols_fragment_size(const TextFragment &f) { return f.columns(); }

//---------------------------------------------------------
//   plainTextPlusSymbolsSize
//---------------------------------------------------------

static int plain_text_plus_symbols_list_size(const std::list<TextFragment> &list) {
    int res = 0;
    for (const TextFragment &f : list) {
        res += plain_text_plus_symbols_fragment_size(f);
    }
    return res;
}

//---------------------------------------------------------
//   split
//---------------------------------------------------------

/**
 Split \a in into \a left, \a mid and \a right. Mid starts at \a pos and is \a len characters long.
 Pos and len refer to the representation returned by toPlainTextPlusSymbols().
 TODO Make sure surrogate pairs are handled correctly
 Return true if OK, false on error.
 */

bool MScoreTextToMusicXml::split(const std::list<TextFragment> &in, const int pos, const int len, std::list<TextFragment> &left,
                                 std::list<TextFragment> &mid, std::list<TextFragment> &right) {
    //LOGD("MScoreTextToMXML::split in size %d pos %d len %d", plainTextPlusSymbolsListSize(in), pos, len);
    //LOGD("-> in");
    //dumpText(in);

    if (pos < 0 || len < 0) {
        return false;
    }

    // ensure output is empty at start
    left.clear();
    mid.clear();
    right.clear();

    // set pos to begin of first fragment
    std::list<TextFragment>::const_iterator fragment_nr = in.begin();
    TextFragment fragment;
    if (fragment_nr != in.end()) {
        fragment = *fragment_nr;
    }
    std::list<TextFragment> *current_dest = &left;
    int current_max_size = pos;

    // while text left
    while (fragment_nr != in.end()) {
        int dest_size = plain_text_plus_symbols_list_size(*current_dest);
        int frag_size = plain_text_plus_symbols_fragment_size(fragment);
        // if no room left in current destination (check applies only to left and mid)
        if ((current_dest != &right && dest_size >= current_max_size) || current_dest == &right) {
            // move to next destination
            if (current_dest == &left) {
                current_dest = &mid;
                current_max_size = len;
            } else if (current_dest == &mid) {
                current_dest = &right;
            }
        }
        // if current fragment fits in current destination (check applies only to left and mid)
        if ((current_dest != &right && dest_size + frag_size <= current_max_size) || current_dest == &right) {
            // add it
            current_dest->push_back(fragment);
            // move to next fragment
            fragment_nr++;
            if (fragment_nr != in.end()) {
                fragment = *fragment_nr;
            }
        } else {
            // split current fragment
            TextFragment right_part = fragment.split(current_max_size - plain_text_plus_symbols_list_size(*current_dest));
            // add first part to current destination
            current_dest->push_back(fragment);
            fragment = right_part;
        }
    }

    /*
    LOGD("-> left");
    dumpText(left);
    LOGD("-> mid");
    dumpText(mid);
    LOGD("-> right");
    dumpText(right);
     */

    return true;
}

//---------------------------------------------------------
//   writeTextFragments
//---------------------------------------------------------

void MScoreTextToMusicXml::write_text_fragments(const std::list<TextFragment> &fr, XmlWriter &xml) {
    //dumpText(fr);
    bool first_time = true; // write additional attributes only the first time characters are written
    for (const TextFragment &f : fr) {
        new_format = f.format;
        String format_attr = update_format();
        xml.tag_raw(tagname + (first_time ? attribs : String()) + format_attr, f.text);
        first_time = false;
    }
}

//---------------------------------------------------------
//   attribute
//    add one attribute if necessary
//---------------------------------------------------------

static String attribute(bool needed, bool value, String true_string, String false_string) {
    String res;
    if (needed) {
        res = value ? true_string : false_string;
    }
    if (!res.empty()) {
        res = u" " + res;
    }
    return res;
}

//---------------------------------------------------------
//   updateFormat
//    update the text format by generating attributes
//    corresponding to the difference between old- and newFormat
//    copy newFormat to oldFormat
//---------------------------------------------------------

String MScoreTextToMusicXml::update_format() {
    if (new_format.font_family() == "ScoreText") {
        new_format.set_font_family(musical_text_font);
    }
    String res;
    res += attribute(new_format.bold() != old_format.bold(), new_format.bold(), u"font-weight=\"bold\"", u"font-weight=\"normal\"");
    res += attribute(new_format.italic() != old_format.italic(), new_format.italic(), u"font-style=\"italic\"", u"font-style=\"normal\"");
    res += attribute(new_format.underline() != old_format.underline(), new_format.underline(), u"underline=\"1\"", u"underline=\"0\"");
    res += attribute(new_format.strike() != old_format.strike(), new_format.strike(), u"line-through=\"1\"", u"line-though=\"0\"");
    res += attribute(new_format.font_family() != old_format.font_family(), true,
                     String(u"font-family=\"%1\"").arg(new_format.font_family()), u"");
    bool need_size = new_format.font_size() < 0.99 * old_format.font_size() || new_format.font_size() > 1.01 * old_format.font_size();
    res += attribute(need_size, true, String(u"font-size=\"%1\"").arg(new_format.font_size()), u"");
    //LOGD("updateFormat() res '%s'", muPrintable(res));
    old_format = new_format;
    return res;
}
} // namespace mu::iex::musicxml
