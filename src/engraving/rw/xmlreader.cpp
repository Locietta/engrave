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

#include "xmlreader.h"

#include "log.h"

using namespace mu;
using namespace muse::draw;

namespace mu::engraving {
XmlReader::~XmlReader() {}

PointF XmlReader::read_point() {
    assert(token_type() == XmlStreamReader::START_ELEMENT);
#ifndef NDEBUG
    if (!has_attribute("x")) {
        LOGD("XmlReader::readPoint: x attribute missing: %s", name().ascii());
        unknown();
    }
    if (!has_attribute("y")) {
        LOGD("XmlReader::readPoint: y attribute missing: %s", name().ascii());
        unknown();
    }
#endif
    double x = double_attribute("x", 0.0);
    double y = double_attribute("y", 0.0);
    read_next();
    return PointF(x, y);
}

//---------------------------------------------------------
//   readColor
//---------------------------------------------------------

Color XmlReader::read_color() {
    assert(token_type() == XmlStreamReader::START_ELEMENT);
    Color c;
    c.set_red(int_attribute("r"));
    c.set_green(int_attribute("g"));
    c.set_blue(int_attribute("b"));
    c.set_alpha(int_attribute("a", 255));
    skip_current_element();
    return c;
}

//---------------------------------------------------------
//   readSize
//---------------------------------------------------------

SizeF XmlReader::read_size() {
    assert(token_type() == XmlStreamReader::START_ELEMENT);
    SizeF p;
    p.set_width(double_attribute("w", 0.0));
    p.set_height(double_attribute("h", 0.0));
    skip_current_element();
    return p;
}

ScaleF XmlReader::read_scale() {
    assert(token_type() == XmlStreamReader::START_ELEMENT);
    ScaleF p;
    p.set_width(double_attribute("w", 0.0));
    p.set_height(double_attribute("h", 0.0));
    skip_current_element();
    return p;
}

//---------------------------------------------------------
//   readRect
//---------------------------------------------------------

RectF XmlReader::read_rect() {
    assert(token_type() == XmlStreamReader::START_ELEMENT);
    RectF p;
    p.set_left(double_attribute("x", 0.0));
    p.set_top(double_attribute("y", 0.0));
    p.set_width(double_attribute("w", 0.0));
    p.set_height(double_attribute("h", 0.0));
    skip_current_element();
    return p;
}

//---------------------------------------------------------
//   readFraction
//    recognizes this two styles:
//    <move z="2" n="4"/>     (old style)
//    <move>2/4</move>        (new style)
//---------------------------------------------------------

Fraction XmlReader::read_fraction() {
    assert(token_type() == XmlStreamReader::START_ELEMENT);
    int z = int_attribute("z", 0);
    int n = int_attribute("n", 1);
    AsciiStringView s = read_ascii_text();
    if (!s.empty()) {
        size_t i = s.index_of('/');
        if (i == muse::nidx) {
            return Fraction::from_ticks(s.to_int());
        } else {
            String str = String::from_ascii(s.ascii());
            z = str.left(i).to_int();
            n = str.mid(i + 1).to_int();
        }
    }
    return Fraction(z, n);
}

//---------------------------------------------------------
//   unknown
//    unknown tag read
//---------------------------------------------------------

void XmlReader::unknown() {
    if (XmlStreamReader::error()) {
        LOGD("%s ", muPrintable(error_string()));
    }
    if (!doc_name_.is_empty()) {
        LOGD() << "tag in <" << doc_name_ << "> byte offset " << byte_offset() + byte_offset_adjustment_ << ": " << name();
    } else {
        LOGD() << "byte offset " << byte_offset() + byte_offset_adjustment_ << ": " << name();
    }
    skip_current_element();
}

double XmlReader::read_double(double min, double max) {
    double val = read_double();
    if (val < min) {
        val = min;
    } else if (val > max) {
        val = max;
    }
    return val;
}

//---------------------------------------------------------
//   htmlToString
//---------------------------------------------------------

void XmlReader::html_to_string(int level, String *s) {
    bool self_closing = no_children();
    *s += u'<' + String::from_ascii(name().ascii());
    for (const Attribute &a : attributes()) {
        *s += u' ' + String::from_ascii(a.name.ascii()) + u"=\"" + a.value + u'\"';
    }
    *s += self_closing ? u"/>" : u">";
    ++level;
    for (;;) {
        XmlStreamReader::TokenType t = read_next();
        switch (t) {
            case XmlStreamReader::START_ELEMENT: html_to_string(level, s); break;
            case XmlStreamReader::END_ELEMENT:
                if (!self_closing) {
                    *s += u"</" + String::from_ascii(name().ascii()) + u'>';
                }
                --level;
                return;
            case XmlStreamReader::CHARACTERS:
                if (!s->empty() || !is_whitespace()) {
                    *s += text().to_xml_escaped();
                } else {
                    LOGD() << "ignoring whitespace";
                }
                break;
            case XmlStreamReader::COMMENT: break;

            default: LOGD() << "htmlToString: read token: " << token_string(); return;
        }
    }
}

//-------------------------------------------------------------------
//   readXml
//    read verbatim until end tag of current level is reached
//-------------------------------------------------------------------

String XmlReader::read_xml() {
    String s;
    int level = 1;
    for (XmlStreamReader::TokenType t = read_next(); t != XmlStreamReader::END_ELEMENT; t = read_next()) {
        switch (t) {
            case XmlStreamReader::START_ELEMENT: html_to_string(level, &s); break;
            case XmlStreamReader::END_ELEMENT: break;
            case XmlStreamReader::CHARACTERS:
                if (!is_whitespace()) {
                    s += text().to_xml_escaped();
                }
                break;
            case XmlStreamReader::COMMENT: break;

            default: LOGD() << "read token: " << token_string(); return s;
        }
    }
    return s;
}
} // namespace mu::engraving
