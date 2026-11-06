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

#include "chordlist.h"

#include "global/containers.h"
#include "global/io/buffer.h"
#include "global/io/file.h"
#include "global/io/fileinfo.h"
#include "log.h"
#include "mscore.h"
#include "pitchspelling.h"
#include "rw/compat/compatutils.h"
#include "rw/xmlreader.h"
#include "rw/xmlwriter.h"
#include "style/style.h"
#include "types/constants.h"

using namespace mu;
using namespace muse;
using namespace muse::io;

namespace mu::engraving {
//---------------------------------------------------------
//   HChord
//---------------------------------------------------------

HChord::HChord(const String &str) {
    static const char *const scale_names[2][12] = {{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"},
                                                   {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"}};
    keys_ = 0;
    StringList sl = str.split(u' ', muse::SKIP_EMPTY_PARTS);
    for (const String &s : sl) {
        for (int i = 0; i < 12; ++i) {
            if (s == scale_names[0][i] || s == scale_names[1][i]) {
                operator+=(i);
                break;
            }
        }
    }
}

//---------------------------------------------------------
//   HChord
//---------------------------------------------------------

HChord::HChord(int a, int b, int c, int d, int e, int f, int g, int h, int i, int k, int l) {
    keys_ = 0;
    if (a >= 0) {
        operator+=(a);
    }
    if (b >= 0) {
        operator+=(b);
    }
    if (c >= 0) {
        operator+=(c);
    }
    if (d >= 0) {
        operator+=(d);
    }
    if (e >= 0) {
        operator+=(e);
    }
    if (f >= 0) {
        operator+=(f);
    }
    if (g >= 0) {
        operator+=(g);
    }
    if (h >= 0) {
        operator+=(h);
    }
    if (i >= 0) {
        operator+=(i);
    }
    if (k >= 0) {
        operator+=(k);
    }
    if (l >= 0) {
        operator+=(l);
    }
}

//---------------------------------------------------------
//   rotate
//    rotate 12 Bits
//---------------------------------------------------------

void HChord::rotate(int semi_tones) {
    while (semi_tones > 0) {
        if (keys_ & 0x800) {
            keys_ = ((keys_ & ~0x800) << 1) + 1;
        } else {
            keys_ <<= 1;
        }
        --semi_tones;
    }
    while (semi_tones < 0) {
        if (keys_ & 1) {
            keys_ = (keys_ >> 1) | 0x800;
        } else {
            keys_ >>= 1;
        }
        ++semi_tones;
    }
}

//---------------------------------------------------------
//   name
//---------------------------------------------------------

String HChord::name(int tpc) const {
    static const HChord C0(0, 3, 6, 9);
    static const HChord C1(0, 3);

    String buf = tpc2name(tpc, NoteSpellingType::STANDARD, NoteCaseType::AUTO, false);
    HChord c(*this);

    int key = tpc2pitch(tpc);

    c.rotate(-key); // transpose to C

    // special cases
    if (c == C0) {
        buf += u"dim";
        return buf;
    }
    if (c == C1) {
        buf += u"no5";
        return buf;
    }

    bool seven = false;
    bool sharp9 = false;
    bool nat11 = false;
    bool sharp11 = false;
    bool nat13 = false;
    bool flat13 = false;

    // minor?
    if (c.contains(3)) {
        if (!c.contains(4)) {
            buf += u"m";
        } else {
            sharp9 = true;
        }
    }

    // 7
    if (c.contains(11)) {
        buf += u"Maj7";
        seven = true;
    } else if (c.contains(10)) {
        buf += u"7";
        seven = true;
    }

    // 4
    if (c.contains(5)) {
        if (!c.contains(4)) {
            buf += u"sus4";
        } else {
            nat11 = true;
        }
    }

    // 5
    if (c.contains(7)) {
        if (c.contains(6)) {
            sharp11 = true;
        }
        if (c.contains(8)) {
            flat13 = true;
        }
    } else {
        if (c.contains(6)) {
            buf += u"b5";
        }
        if (c.contains(8)) {
            buf += u"#5";
        }
    }

    // 6
    if (c.contains(9)) {
        if (!seven) {
            buf += u"6";
        } else {
            nat13 = true;
        }
    }

    // 9
    if (c.contains(1)) {
        buf += u"b9";
    }
    if (c.contains(2)) {
        buf += u"9";
    }
    if (sharp9) {
        buf += u"#9";
    }

    // 11
    if (nat11) {
        buf += u"11 ";
    }
    if (sharp11) {
        buf += u"#11";
    }

    // 13
    if (flat13) {
        buf += u"b13";
    }
    if (nat13) {
        if (c.contains(1) || c.contains(2) || sharp9 || nat11 || sharp11) {
            buf += u"13";
        } else {
            buf += u"add13";
        }
    }
    return buf;
}

//---------------------------------------------------------
//   voicing
//---------------------------------------------------------

String HChord::voicing() const {
    String s = u"C";
    const char *names[] = {"C", " Db", " D", " Eb", " E", " F", " Gb", " G", " Ab", " A", " Bb", " B"};

    for (int i = 1; i < 12; i++) {
        if (contains(i)) {
            s += String::from_ascii(names[i]);
        }
    }
    return s;
}

//---------------------------------------------------------
//   print
//---------------------------------------------------------

void HChord::print() const {
    const char *names[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

    for (int i = 0; i < 12; i++) {
        if (contains(i)) {
            LOGD(" %s", names[i]);
        }
    }
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void HChord::add(const std::vector<HDegree> &degree_list) {
    // LOGD("HChord::add   ");print();
    // convert degrees to semitones
    static const int degree_table[] = {// 1  2  3  4  5  6   7
                                       // C  D  E  F  G  A   B
                                       0, 2, 4, 5, 7, 9, 11};
    // factor in the degrees
    for (const HDegree &d : degree_list) {
        int i = (d.value() - 1) % 7;
        if (i < 0) {
            continue; // ?
        }
        int dv = degree_table[i] + d.alter();
        int dv1 = degree_table[i];

        if (d.value() == 7 && d.alter() == 0) {
            dv -= 1;
        }

        if (d.type() == HDegreeType::ADD) {
            *this += dv;
        } else if (d.type() == HDegreeType::ALTER) {
            if (contains(dv1)) {
                *this -= dv1;
                *this += dv;
            } else {
                //                        LOGD("HDegreeType::ALTER: chord does not contain degree %d(%d):",
                //                           d.value(), d.alter());
                //                        print();
                *this += dv; // DEBUG: default to add
            }
        } else if (d.type() == HDegreeType::SUBTRACT) {
            if (contains(dv1)) {
                *this -= dv1;
            } else {
                LOGD("SUB: chord does not contain degree %d(%d):", d.value(), d.alter());
            }
        } else {
            LOGD("degree type %d not supported", static_cast<int>(d.type()));
        }

        // LOGD("  HCHord::added  "); print();
    }
}

//---------------------------------------------------------
//   readRenderList
//---------------------------------------------------------

static void read_render_list(String val, std::vector<RenderActionPtr> &render_list, int msc_version) {
    render_list.clear();
    StringList sl = val.split(u' ', muse::SKIP_EMPTY_PARTS);
    for (const String &s : sl) {
        if (s.starts_with(u"m:")) {
            StringList ssl = s.split(u':', muse::SKIP_EMPTY_PARTS);
            if (ssl.size() == 3) {
                // m:x:y
                double movex = ssl.at(1).to_double();
                double movey = ssl.at(2).to_double();

                if (msc_version < 460) {
                    movex = compat::CompatUtils::convert_chord_ext_mod_units(movex);
                    movey = compat::CompatUtils::convert_chord_ext_mod_units(movey);
                }

                render_list.emplace_back(new RenderActionMove(movex, movey));
            }
        } else if (s.starts_with(u"ms:")) {
            StringList ssl = s.split(u':', muse::SKIP_EMPTY_PARTS);
            if (ssl.size() == 3) {
                // ms:x:y
                double movex = ssl[1].to_double();
                double movey = ssl[2].to_double();

                render_list.emplace_back(new RenderActionMoveScaled(movex, movey));
            }
        } else if (s == u":mx") {
            render_list.emplace_back(new RenderActionMoveXHeight(true));
        } else if (s == u":mxs") {
            render_list.emplace_back(new RenderActionMoveXHeightScaled(true));
        } else if (s.starts_with(u"sc:")) {
            StringList ssl = s.split(u':', muse::SKIP_EMPTY_PARTS);
            if (ssl.size() == 2) {
                double scale = ssl.at(1).to_double();
                render_list.emplace_back(new RenderActionScale(scale));
            }
        } else if (s == u":push") {
            render_list.emplace_back(new RenderActionPush());
        } else if (s == u":pop") {
            render_list.emplace_back(new RenderActionPop());
        } else if (s == u":popx") {
            render_list.emplace_back(new RenderActionPopX());
        } else if (s == u":popy") {
            render_list.emplace_back(new RenderActionPopY());
        } else if (s == u":n") {
            render_list.emplace_back(new RenderActionNote());
        } else if (s == u":a") {
            render_list.emplace_back(new RenderActionAccidental());
        } else if (s == u":pl") {
            render_list.emplace_back(new RenderActionParenLeft());
        } else if (s == u":pr") {
            render_list.emplace_back(new RenderActionParenRight());
        } else {
            render_list.emplace_back(new RenderActionSet(s));
        }
    }
}

//---------------------------------------------------------
//   writeRenderList
//---------------------------------------------------------

static void write_render_list(XmlWriter &xml, const std::vector<RenderActionPtr> &al, const AsciiStringView &name) {
    String s;

    for (const RenderActionPtr &a : al) {
        if (!s.is_empty()) {
            s += u" ";
        }
        switch (a->action_type()) {
            case RenderAction::RenderActionType::SET: {
                const RenderActionSetPtr set = std::static_pointer_cast<RenderActionSet>(a);
                s += set->text();
                break;
            }
            case RenderAction::RenderActionType::MOVE: {
                const RenderActionMovePtr move = std::static_pointer_cast<RenderActionMove>(a);

                if (!real_is_null(move->x()) || !real_is_null(move->y())) {
                    String scaled = move->scaled() ? u"s" : u"";
                    s += String(u"m%1:%2:%3").arg(scaled).arg(move->x()).arg(move->y());
                }
                break;
            }
            case RenderAction::RenderActionType::MOVEXHEIGHT: {
                const RenderActionMoveXHeightPtr movex = std::static_pointer_cast<RenderActionMoveXHeight>(a);
                String scaled = movex->scaled() ? u"s" : u"";
                s += String(u":mx%1").arg(scaled);
                break;
            }
            case RenderAction::RenderActionType::SCALE: {
                const RenderActionScalePtr scale = std::static_pointer_cast<RenderActionScale>(a);
                s += String(u"sc:%1").arg(scale->scale());
                break;
            }
            case RenderAction::RenderActionType::PUSH: s += u":push"; break;
            case RenderAction::RenderActionType::POP: {
                const RenderActionPopPtr pop = std::static_pointer_cast<RenderActionPop>(a);
                String coord = pop->pop_x() && pop->pop_y() ? u"" : (pop->pop_x() ? u"x" : u"y");
                s += String(u":pop%1").arg(coord);
                break;
            }
            case RenderAction::RenderActionType::NOTE: s += u":n"; break;
            case RenderAction::RenderActionType::ACCIDENTAL: s += u":a"; break;
            case RenderAction::RenderActionType::PAREN: {
                const RenderActionParenPtr paren = std::static_pointer_cast<RenderActionParen>(a);
                s += paren->direction() == DirectionH::LEFT ? u":pl" : u":pr";
                break;
            }
            case RenderAction::RenderActionType::STOPHALIGN:
                // Internal, skip
                break;
        }
    }
    xml.tag(name, s);
}

//---------------------------------------------------------
//  read
//---------------------------------------------------------

void ChordToken::read(XmlReader &e, int msc_version) {
    String c = e.attribute("class");
    if (c == "quality") {
        token_class = ChordTokenClass::QUALITY;
    } else if (c == "extension") {
        token_class = ChordTokenClass::EXTENSION;
    } else if (c == "modifier") {
        token_class = ChordTokenClass::MODIFIER;
    } else if (c == "type") {
        token_class = ChordTokenClass::TYPE;
    } else if (c == "accidental") {
        token_class = ChordTokenClass::ACCIDENTAL;
    } else {
        token_class = ChordTokenClass::ALL;
    }
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            names << e.read_text();
        } else if (tag == "render") {
            read_render_list(e.read_text(), render_list, msc_version);
        }
    }
}

//---------------------------------------------------------
//  write
//---------------------------------------------------------

void ChordToken::write(XmlWriter &xml) const {
    XmlWriter::Attributes attrs;
    switch (token_class) {
        case ChordTokenClass::QUALITY: attrs.push_back({"class", "quality"}); break;
        case ChordTokenClass::EXTENSION: attrs.push_back({"class", "extension"}); break;
        case ChordTokenClass::MODIFIER: attrs.push_back({"class", "modifier"}); break;
        case ChordTokenClass::TYPE: attrs.push_back({"class", "type"}); break;
        case ChordTokenClass::ACCIDENTAL: attrs.push_back({"accidental", "type"}); break;
        default: break;
    }
    xml.start_element("token", attrs);
    for (const String &s : names) {
        xml.tag("name", s);
    }
    write_render_list(xml, render_list, "render");
    xml.end_element();
}

//---------------------------------------------------------
//  configure
//---------------------------------------------------------

void ParsedChord::configure(const ChordList *cl) {
    if (!cl) {
        return;
    }
    // TODO: allow this to be parameterized via chord list
    major_ << u"ma" << u"maj" << u"major" << u"t" << u"^";
    minor_ << u"mi" << u"min" << u"minor" << u"-" << u"=";
    diminished_ << u"dim" << u"dim." << u"o";
    augmented_ << u"aug" << u"+";
    lower_ << u"b" << u"-" << u"dim";
    raise_ << u"#" << u"+" << u"aug";
    mod_ << u"sus" << u"add" << u"no" << u"omit" << u"^" << u"type";
    symbols_ << u"t" << u"^" << u"-" << u"+" << u"o" << u"0";
}

//---------------------------------------------------------
//  correctXmlText
//    remove digits from _xmlText, optionally replace with s
//    needed for m(Maj9) et al
//---------------------------------------------------------

void ParsedChord::correct_xml_text(const String &s) {
    String xml_text = xml_text_;
    xml_text.remove(std::regex("[0-9]"));
    if (!s.empty()) {
        size_t pos = xml_text.last_index_of(u')');
        if (pos == muse::nidx) {
            pos = xml_text.size();
        }
        xml_text.insert(pos, s);
    }
    xml_text_ = xml_text;
}

//---------------------------------------------------------
//  parse
//    returns true if chord was parseable
//---------------------------------------------------------

bool ParsedChord::parse(const String &s, const ChordList *cl, bool syntax_only, bool prefer_minor) {
    String tok1, tok1_l, tok2, tok2_l;
    String extension_digits = u"123456789";
    String special = u"()[],/\\ ";
    String leading = u"([ ";
    String trailing = u")],/\\ ";
    String initial;
    bool take6 = false, take7 = false, take9 = false, take11 = false, take13 = false;
    size_t last_leading_token = 0;
    size_t len = s.size();
    size_t i = 0;
    int third_key = 0, seventh_key = 0;
    bool sus_chord = false;
    std::vector<HDegree> hdl;
    int key[] = {0, 0, 2, 4, 5, 7, 9, 11, 0, 2, 4, 5, 7, 9, 11};

    configure(cl);
    name_ = s;
    parseable_ = true;
    understandable_ = true;

    last_leading_token = token_list_.size();
    // get quality
    for (tok1 = u"", tok1_l = u"", initial = u""; i < len; ++i) {
        // up to first (non-zero) digit, paren, or comma
        if (extension_digits.contains(s.at(i)) || special.contains(s.at(i))) {
            break;
        }
        tok1.append(s.at(i));
        tok1_l.append(s.at(i).to_lower());
        if (tok1_l == u"m" || major_.contains(tok1_l) || minor_.contains(tok1_l) || diminished_.contains(tok1_l) ||
            augmented_.contains(tok1_l)) {
            initial = tok1;
        }
    }
    // special case for "madd", which needs to parse as m,add rather than ma,dd
    if (tok1_l.starts_with(u"madd")) {
        initial = tok1.at(0);
    }
    // quality and first modifier ran together with no separation - eg, mima7, augadd
    // keep quality portion, reset index to read modifier portion later
    // Prevent greedy capture of first character as chord quality in the following cases
    static const std::array<String, 3> modifiers_starting_with_quality_characters{u"tristan", u"omit", u"type"};
    if (!initial.empty() && initial != tok1 && !muse::contains(modifiers_starting_with_quality_characters, tok1_l)) {
        i -= (tok1.size() - initial.size());
        tok1 = initial;
        tok1_l = initial.to_lower();
    }
    // determine quality
    if (tok1 == u"M" || major_.contains(tok1_l)) {
        quality_ = u"major";
        take6 = true;
        take7 = true;
        take9 = true;
        take11 = true;
        take13 = true;
        if (!syntax_only) {
            chord_ = HChord(u"C E G");
        }
    } else if (tok1 == u"m" || minor_.contains(tok1_l)) {
        quality_ = u"minor";
        take6 = true;
        take7 = true;
        take9 = true;
        take11 = true;
        take13 = true;
        if (!syntax_only) {
            chord_ = HChord(u"C Eb G");
        }
    } else if (diminished_.contains(tok1_l)) {
        quality_ = u"diminished";
        take7 = true;
        if (!syntax_only) {
            chord_ = HChord(u"C Eb Gb");
        }
    } else if (augmented_.contains(tok1_l)) {
        quality_ = u"augmented";
        take7 = true;
        if (!syntax_only) {
            chord_ = HChord(u"C E G#");
        }
    } else if (tok1_l == "0") {
        quality_ = u"half-diminished";
        if (!syntax_only) {
            chord_ = HChord(u"C Eb Gb Bb");
        }
    } else if (tok1_l.empty()) {
        // empty quality - this will turn out to be major or dominant (or minor if preferMinor)
        quality_ = u"";
        if (!syntax_only) {
            chord_ = prefer_minor ? HChord(u"C Eb G") : HChord(u"C E G");
        }
        if (prefer_minor) {
            name_ = u"=" + name_;
        }
    } else {
        // anything else is not a quality after all, but a modifier
        // reset to read again as modifier
        quality_ = u"";
        tok1 = u"";
        tok1_l = u"";
        i = static_cast<int>(last_leading_token);
        if (!syntax_only) {
            chord_ = HChord(u"C E G");
        }
    }
    if (tok1_l == "=") {
        tok1 = u"";
        tok1_l = u"";
    }
    if (!tok1.empty()) {
        add_token(tok1, ChordTokenClass::QUALITY);
    }
    if (!syntax_only) {
        xml_kind_ = quality_;
        xml_parens_ = u"no";
        if (symbols_.contains(tok1)) {
            xml_symbols_ = u"yes";
            xml_text_ = u"";
        } else {
            xml_symbols_ = u"no";
            xml_text_ = tok1;
        }
    }
    // eat trailing parens and commas
    while (i < len && trailing.contains(s.at(i))) {
        add_token(String(s.at(i++)), ChordTokenClass::QUALITY);
    }

    size_t prev_idx = i;

    // Get type - either "typen" or "III"
    // Eat up to first number
    for (tok1 = u""; i < len; ++i) {
        if (s.at(i).is_digit()) {
            break;
        }
        tok1.append(s.at(i));
    }

    if (tok1 == "type") {
        add_token(tok1, ChordTokenClass::TYPE);
        // Read number
        for (tok1 = u""; i < len; ++i) {
            if (!s.at(i).is_digit()) {
                break;
            }
            add_token(s.at(i), ChordTokenClass::TYPE);
        }
    } else if (tok1.contains(std::wregex(L"[IVX]+"))) {
        // Get roman numerals
        for (i = 0; i < tok1.size(); i++) {
            add_token(tok1.at(i), ChordTokenClass::TYPE);
        }
    } else {
        i = prev_idx;
    }

    last_leading_token = token_list_.size();
    // get extension - up to first non-digit other than comma or slash
    for (tok1 = u""; i < len; ++i) {
        if (!s.at(i).is_digit() && s.at(i) != ',' && s.at(i) != '/') {
            break;
        }
        tok1.append(s.at(i));
    }
    extension_ = tok1;
    if (quality_.empty()) {
        if (extension_ == "7" || extension_ == "9" || extension_ == "11" || extension_ == "13") {
            quality_ = prefer_minor ? u"minor" : u"dominant";
            if (!syntax_only) {
                xml_kind_ = prefer_minor ? u"minor" : u"dominant";
            }
            take7 = true;
            take9 = true;
            take11 = true;
            take13 = true;
        } else {
            quality_ = prefer_minor ? u"minor" : u"major";
            if (!syntax_only) {
                xml_kind_ = prefer_minor ? u"minor" : u"major";
            }
            take6 = true;
            take7 = true;
            take9 = true;
            take11 = true;
            take13 = true;
        }
    }
    if (!tok1.empty()) {
        add_token(tok1, ChordTokenClass::EXTENSION);
    }
    if (!syntax_only) {
        if (quality_ == "minor") {
            third_key = 3;
        } else {
            third_key = 4;
        }
        if (quality_ == "major") {
            seventh_key = 11;
        } else if (quality_ == "diminished") {
            seventh_key = 9;
        } else {
            seventh_key = 10;
        }
        xml_text_ += extension_;
        StringList extl;
        if (tok1 == "2") {
            String d = u"add" + tok1;
            xml_degrees_ << d;
            xml_text_.remove(tok1);
            chord_ += 2;
        } else if (tok1 == "4") {
            String d = u"add" + tok1;
            xml_degrees_ << d;
            xml_text_.remove(tok1);
            chord_ += 5;
        } else if (tok1 == "5") {
            xml_kind_ = u"power";
            chord_ -= third_key;
        } else if (tok1 == "6") {
            if (take6) {
                xml_kind_ += u"-sixth";
            } else {
                extl << u"6";
            }
            chord_ += 9;
        } else if (tok1 == "7") {
            if (take7) {
                xml_kind_ += u"-seventh";
            } else if (xml_kind_ != "half-diminished") {
                extl << u"7";
            }
            chord_ += seventh_key;
        } else if (tok1 == "9") {
            if (take9) {
                xml_kind_ += u"-ninth";
            } else if (take7) {
                xml_kind_ += u"-seventh";
                extl << u"9";
                correct_xml_text(u"7");
            } else if (xml_kind_ == u"half-diminished") {
                extl << u"9";
                correct_xml_text();
            } else {
                extl << u"7" << u"9";
                correct_xml_text();
            }
            chord_ += seventh_key;
            chord_ += 2;
        } else if (tok1 == u"11") {
            if (take11) {
                xml_kind_ += u"-11th";
            } else if (take7) {
                xml_kind_ += u"-seventh";
                extl << u"9" << u"11";
                correct_xml_text(u"7");
            } else if (xml_kind_ == u"half-diminished") {
                extl << u"9" << u"11";
                correct_xml_text();
            } else {
                extl << u"7" << u"9" << u"11";
                correct_xml_text();
            }
            chord_ += seventh_key;
            chord_ += 2;
            chord_ += 5;
        } else if (tok1 == u"13") {
            if (take13) {
                xml_kind_ += u"-13th";
            } else if (take7) {
                xml_kind_ += u"-seventh";
                extl << u"9" << u"11" << u"13";
                correct_xml_text(u"7");
            } else if (xml_kind_ == u"half-diminished") {
                extl << u"9" << u"11" << u"13";
                correct_xml_text();
            } else {
                extl << u"7" << u"9" << u"11" << u"13";
                correct_xml_text();
            }
            chord_ += seventh_key;
            chord_ += 2;
            chord_ += 5;
            chord_ += 9;
        } else if (tok1 == u"69" || tok1 == u"6,9" || tok1 == u"6/9") {
            if (take6) {
                xml_kind_ += u"-sixth";
                extl << u"9";
                correct_xml_text(u"6");
            } else {
                extl << u"6" << u"9";
                correct_xml_text();
            }
            extension_ = u"69";
            chord_ += 9;
            chord_ += 2;
        }
        for (const String &e : extl) {
            String d = u"add" + e;
            xml_degrees_ << d;
        }
        if (xml_kind_ == u"dominant-seventh") {
            xml_kind_ = u"dominant";
        }
    }
    // eat trailing parens and commas
    while (i < len && trailing.contains(s.at(i))) {
        add_token(String(s.at(i++)), ChordTokenClass::EXTENSION);
    }

    // get modifiers
    bool add_pending = false;
    bool sus_pending = false;
    modifier_list_.clear();
    while (i < len) {
        // eat leading parens
        while (i < len && leading.contains(s.at(i))) {
            add_token(String(s.at(i++)), ChordTokenClass::MODIFIER);
            xml_parens_ = u"yes";
        }
        // get first token - up to first digit, paren, or comma
        for (tok1 = u"", tok1_l = u"", initial = u""; i < len; ++i) {
            if (s.at(i).is_digit() || special.contains(s.at(i))) {
                break;
            }
            tok1.append(s.at(i));
            tok1_l.append(s.at(i).to_lower());
            if (mod_.contains(tok1_l)) {
                initial = tok1;
            }
        }
        // if we reached the end of the string and never got a token,
        // then nothing to do, and no sense in looking for a second token
        if (i == len && tok1.empty()) {
            break;
        }
        if (!initial.empty() && initial != tok1) {
            // two modifiers ran together with no separation - eg, susb9
            // keep first, reset index to read second later
            i -= (tok1.size() - initial.size());
            tok1 = initial;
            tok1_l = initial.to_lower();
        }
        // for "add" and "sus", just add the token and then read argument as a separate modifier
        // this allows the argument to itself be a two-part string
        // thus allowing addb9 -> add;b,9
        if (tok1_l == "add") {
            add_token(tok1, ChordTokenClass::MODIFIER);
            add_pending = true;
            continue;
        }
        // When there are no numbers after, default to sus4 further down
        if (tok1_l == "sus" && i != len) {
            add_token(tok1, ChordTokenClass::MODIFIER);
            sus_pending = true;
            continue;
        }
        // eat spaces
        while (i < len && s.at(i) == u' ') {
            ++i;
        }
        // get second token - a number <= 13
        for (tok2 = u""; i < len; ++i) {
            if (!s.at(i).is_digit()) {
                break;
            }
            if (tok2.size() == 1 && (tok2.at(0) != u'1' || s.at(i) > u'3')) {
                break;
            }
            tok2.append(s.at(i));
        }
        // check suffix - 'st', 'nd', 'rd', 'th'
        String suffix;
        static const int suffix_len = 2;
        for (int j = 0; j < suffix_len && i + j < len; j++) {
            suffix.append(s.at(i + j));
        }

        if (suffix.contains(std::wregex(L"st|nd|rd|th"))) {
            i += suffix_len;
        } else {
            suffix.clear();
        }

        tok2_l = tok2.to_lower();
        // re-attach "add"
        if (add_pending) {
            if (raise_.contains(tok1_l)) {
                tok1_l = u"#";
            } else if (lower_.contains(tok1_l)) {
                tok1_l = u"b";
            } else if (tok1 == "M" || major_.contains(tok1_l)) {
                tok1_l = u"major";
            }
            tok2_l = tok1_l + tok2_l;
            tok1_l = u"add";
        }
        // re-attach "sus"
        if (sus_pending) {
            if (raise_.contains(tok1_l)) {
                tok1_l = u"#";
            } else if (lower_.contains(tok1_l)) {
                tok1_l = u"b";
            } else if (tok1 == "M" || major_.contains(tok1_l)) {
                tok1_l = u"major";
            }
            tok2_l = tok1_l + tok2_l;
            tok1_l = u"sus";
        }
        // standardize spelling
        if (tok1 == "M" || major_.contains(tok1_l)) {
            tok1_l = u"major";
        } else if (tok1_l == "omit") {
            tok1_l = u"no";
        } else if (tok1_l == "sus" && tok2_l.empty()) {
            tok2_l = u"4";
        } else if (augmented_.contains(tok1_l) && tok2_l.empty()) {
            if (quality_ == "dominant" && extension_ == "7") {
                quality_ = u"augmented";
                if (!syntax_only) {
                    xml_kind_ = u"augmented-seventh";
                    xml_text_ = extension_ + tok1;
                    chord_ -= 7;
                    chord_ += 8;
                }
                tok1_l = u"";
            } else {
                tok1_l = u"#";
                tok2_l = u"5";
            }
        } else if (diminished_.contains(tok1)) {
            quality_ = u"diminished";
            if (!syntax_only) {
                xml_kind_ = u"diminished";
                xml_text_ = extension_ + tok1;
                chord_ -= 4;
                chord_ += 3;
                chord_ -= 7;
                chord_ += 6;
            }
            tok1_l = u"";
        } else if ((lower_.contains(tok1_l) || raise_.contains(tok1_l)) && tok2_l.empty()) {
            // trailing alteration - treat as applying to extension (and convert to modifier)
            // this handles C5b, C9#, etc
            tok2_l = extension_;
            if (!syntax_only) {
                xml_kind_ = (quality_ == u"dominant") ? u"major" : quality_;
                xml_text_.remove(extension_);
                if (extension_ == "5") {
                    chord_ += third_key;
                } else {
                    chord_ -= seventh_key;
                }
            }
            if (quality_ == "dominant") {
                quality_ = u"major";
            }
            extension_ = u"";
            if (lower_.contains(tok1_l)) {
                tok1_l = u"b";
            } else {
                tok1_l = u"#";
            }
        } else if (lower_.contains(tok1_l)) {
            tok1_l = u"b";
        } else if (raise_.contains(tok1_l)) {
            tok1_l = u"#";
        }
        String m = tok1_l + tok2_l + suffix;
        if (!m.empty()) {
            modifier_list_ << m;
        }
        if (!tok1.empty()) {
            add_token(tok1, ChordTokenClass::MODIFIER);
        }
        if (!tok2.empty()) {
            add_token(tok2, ChordTokenClass::MODIFIER);
        }
        if (!suffix.empty()) {
            add_token(suffix, ChordTokenClass::MODIFIER);
        }
        if (!syntax_only) {
            int d;
            if (tok2_l.empty()) {
                d = 0;
            } else {
                d = tok2_l.to_int();
            }
            if (d > 13) {
                d = 13;
            }
            String degree;
            bool alter = false;
            if (tok1_l == "add") {
                if (d) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                } else if (!tok2_l.empty()) {
                    // this was result of addPending
                    // alteration; tok1 = alter, tok2 = value
                    d = tok2.to_int();
                    if (raise_.contains(tok1) || tok1 == "M" || major_.contains(tok1.to_lower())) {
                        if (d == 7) {
                            chord_ += 11;
                            tok2_l = u"#7";
                        } else if (raise_.contains(tok1)) {
                            hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                        }
                    } else if (lower_.contains(tok1)) {
                        if (d == 7) {
                            chord_ += 10;
                        } else {
                            hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                        }
                    } else if (d) {
                        hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                    }
                }
                degree = u"add" + tok2_l;
            } else if (tok1_l == u"no") {
                degree = u"sub" + tok2_l;
                if (d) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::SUBTRACT));
                }
            } else if (tok1_l == "sus") {
                // convert chords with sus into suspended "kind"
                // extension then becomes a series of degree adds
                if (tok2_l == "4") {
                    xml_kind_ = u"suspended-fourth";
                } else if (tok2_l == "2") {
                    xml_kind_ = u"suspended-second";
                }
                xml_text_ = tok1_l + tok2;
                if (extension_ == "7" || extension_ == "9" || extension_ == "11" || extension_ == "13") {
                    xml_degrees_ << ((quality_ == u"major") ? u"add#7" : u"add7");
                    // hack for programs that cannot assemble names well
                    // even though the kind is suspended, set text to also include the extension
                    // in export, we will set the degree text to null
                    xml_text_ = extension_ + xml_text_;
                    degree = u"";
                } else if (!extension_.empty()) {
                    degree = u"add" + extension_;
                }
                if (extension_ == u"13") {
                    xml_degrees_ << u"add9";
                    xml_degrees_ << u"add11";
                    xml_degrees_ << u"add13";
                } else if (extension_ == u"11") {
                    xml_degrees_ << u"add9";
                    xml_degrees_ << u"add11";
                } else if (extension_ == u"9") {
                    xml_degrees_ << u"add9";
                }
                sus_chord = true;
                chord_ -= third_key;
                if (d) {
                    chord_ += key[d];
                }
            } else if (tok1_l == u"major") {
                if (xml_kind_.starts_with(u"minor")) {
                    xml_kind_ = u"major-minor";
                    if (extension_ == u"9" || tok2_l == u"9") {
                        xml_degrees_ << u"add9";
                    }
                    if (extension_ == "11" || tok2_l == u"11") {
                        xml_degrees_ << u"add9";
                        xml_degrees_ << u"add11";
                    }
                    if (extension_ == u"13" || tok2_l == u"13") {
                        xml_degrees_ << u"add9";
                        xml_degrees_ << u"add11";
                        xml_degrees_ << u"add13";
                    }
                    xml_text_ += tok1 + tok2;
                    correct_xml_text(u"7");
                } else {
                    tok1_l = u"add";
                }
                chord_ -= 10;
                chord_ += 11;
                if (d && d != 7) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                }
            } else if (tok1_l == u"alt") {
                xml_degrees_ << u"altb5";
                xml_degrees_ << u"add#5";
                xml_degrees_ << u"addb9";
                xml_degrees_ << u"add#9";
                chord_ -= 7;
                chord_ += 6;
                chord_ += 8;
                chord_ += 1;
                chord_ += 3;
            } else if (tok1_l == u"blues") {
                // this isn't really well-defined, but it might as well mean something
                if (extension_ == u"11" || extension_ == u"13") {
                    xml_degrees_ << u"alt#9";
                } else {
                    xml_degrees_ << u"add#9";
                }
                chord_ += 3;
            } else if (tok1_l == u"lyd") {
                if (extension_ == u"13") {
                    xml_degrees_ << u"alt#11";
                } else {
                    xml_degrees_ << u"add#11";
                }
                chord_ += 6;
            } else if (tok1_l == "phryg") {
                if (!xml_kind_.starts_with(u"minor")) {
                    xml_kind_ = u"minor-seventh";
                }
                if (extension_ == "11" || extension_ == "13") {
                    xml_degrees_ << u"altb9";
                } else {
                    xml_degrees_ << u"addb9";
                }
                xml_text_ += tok1;
                chord_ = HChord(u"C Db Eb G Bb");
            } else if (tok1_l == "tristan") {
                xml_kind_ = u"Tristan";
                xml_text_ = tok1;
                chord_ = HChord(u"C F# A# D#");
            } else if (add_pending) {
                degree = u"add" + tok1_l + tok2_l;
                if (raise_.contains(tok1_l)) {
                    hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                } else if (lower_.contains(tok1_l)) {
                    hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                } else {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                }
            } else if (sus_pending) {
                degree = u"sus" + tok1_l + tok2_l;
                if (raise_.contains(tok1_l)) {
                    hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                } else if (lower_.contains(tok1_l)) {
                    hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                } else {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                }
            } else if (tok1_l.empty() && !tok2_l.empty()) {
                degree = u"add" + tok2_l;
                hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
            } else if (lower_.contains(tok1_l)) {
                tok1_l = u"b";
                alter = true;
            } else if (raise_.contains(tok1_l)) {
                tok1_l = u"#";
                alter = true;
            } else if (tok1_l.empty()) {
                // token was already handled fully
            } else {
                understandable_ = false;
                if (s.starts_with(tok1)) {
                    // unrecognized token right from very beginning
                    xml_kind_ = u"other";
                    xml_text_ = tok1;
                }
            }
            if (alter) {
                if (tok2_l == "4" && xml_kind_ == "suspended-fourth") {
                    degree = u"alt";
                } else if (tok2_l == "5") {
                    degree = u"alt";
                } else if (tok2_l == "9" && (extension_ == "11" || extension_ == "13")) {
                    degree = u"alt";
                } else if (tok2_l == "11" && extension_ == "13") {
                    degree = u"alt";
                } else {
                    degree = u"add";
                }
                degree += tok1_l + tok2_l;
                if (chord_.contains(key[d]) && !(sus_chord && (d == 11))) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::SUBTRACT));
                }
                if (tok1_l == "#") {
                    if (d == 7) {
                        chord_ += 11;
                    } else {
                        hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                    }
                } else if (tok1_l == "b") {
                    if (d == 7) {
                        chord_ += 10;
                    } else {
                        hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                    }
                }
            }
            if (!degree.empty()) {
                xml_degrees_ << degree;
            }
        }
        // eat trailing parens and commas
        while (i < len && trailing.contains(s.at(i))) {
            add_token(String(s.at(i++)), ChordTokenClass::MODIFIER);
        }
        add_pending = false;
        sus_pending = false;
    }
    if (!syntax_only) {
        chord_.add(hdl);
        // fix "add" / "alt" conflicts
        // so add9,altb9 -> addb9
        StringList alt_list = xml_degrees_.filter(u"alt");
        for (const String &d : alt_list) {
            String unalt(d);
            unalt.replace(std::regex("alt[b#]"), u"add");
            if (xml_degrees_.remove_all(unalt)) {
                String alt(d);
                alt.replace(u"alt", u"add");
                size_t i1 = xml_degrees_.index_of(d);
                xml_degrees_.replace(i1, alt);
            }
        }
    }

    // construct handle
    if (!modifier_list_.empty()) {
        StringList sorted_modifiers = modifier_list_;
        std::sort(sorted_modifiers.begin(), sorted_modifiers.end());
        modifiers_ = u"<" + sorted_modifiers.join(u"><") + u">";
    }
    handle_ = u"<" + quality_ + u"><" + extension_ + u">" + modifiers_;

    // force <minor><7><b5> to export as half-diminished
    if (!syntax_only && handle_ == u"<minor><7><b5>") {
        xml_kind_ = u"half-diminished";
        xml_text_ = s;
        xml_degrees_.clear();
    }
    if (MScore::debug_mode) {
        LOGD("parse: source = <%s>, handle = %s", muPrintable(s), muPrintable(handle_));
        if (!syntax_only) {
            LOGD("parse: HChord = <%s> (%d)", muPrintable(chord_.voicing()), chord_.get_keys());
            LOGD("parse: xmlKind = <%s>, text = <%s>", muPrintable(xml_kind_), muPrintable(xml_text_));
            LOGD("parse: xmlSymbols = %s, xmlParens = %s", muPrintable(xml_symbols_), muPrintable(xml_parens_));
            LOGD("parse: xmlDegrees = <%s>", muPrintable(xml_degrees_.join(u",")));
        }
    }
    return parseable_;
}

//---------------------------------------------------------
//   fromXml
//---------------------------------------------------------

String ParsedChord::from_xml(const String &raw_kind, const String &raw_kind_text, const String &use_symbols, const String &use_parens,
                             const std::vector<HDegree> &dl, const ChordList *cl) {
    String kind = raw_kind;
    String kind_text = raw_kind_text;
    bool syms = (use_symbols == "yes");
    bool parens = (use_parens == "yes");
    bool implied = false;
    bool extend = false;
    int extension = 0;
    parseable_ = true;
    understandable_ = true;

    // get quality info from kind
    if (kind == "major-minor") {
        quality_ = u"minor";
        modifier_list_ << u"major7";
        extend = true;
    } else if (kind.contains(u"major")) {
        quality_ = u"major";
        if (kind == "major" || kind == "major-sixth") {
            implied = true;
        }
    } else if (kind.contains(u"minor")) {
        quality_ = u"minor";
    } else if (kind.contains(u"dominant")) {
        quality_ = u"dominant";
        implied = true;
        extension = 7;
    } else if (kind == "augmented-seventh") {
        quality_ = u"augmented";
        extension = 7;
        extend = true;
    } else if (kind == "augmented") {
        quality_ = u"augmented";
    } else if (kind == "half-diminished") {
        quality_ = u"half-diminished";
        if (syms) {
            extension = 7;
            extend = true;
        }
    } else if (kind == "diminished-seventh") {
        quality_ = u"diminished";
        extension = 7;
    } else if (kind == "diminished") {
        quality_ = u"diminished";
    } else if (kind == "suspended-fourth") {
        quality_ = u"major";
        implied = true;
        modifier_list_ << u"sus4";
    } else if (kind == "suspended-second") {
        quality_ = u"major";
        implied = true;
        modifier_list_ << u"sus2";
    } else if (kind == "power") {
        quality_ = u"major";
        implied = true;
        extension = 5;
    } else if (kind == "pedal") {
        // Ignore, assume major
        quality_ = u"major";
        implied = true;
    } else {
        quality_ = kind;
    }

    // get extension info from kind
    if (kind.contains(u"seventh")) {
        extension = 7;
    } else if (kind.contains(u"ninth")) {
        extension = 9;
    } else if (kind.contains(u"11th")) {
        extension = 11;
    } else if (kind.contains(u"13th")) {
        extension = 13;
    } else if (kind.contains(u"sixth")) {
        extension = 6;
    }

    // get modifier info from degree list
    for (const HDegree &d : dl) {
        String mod;
        int v = d.value();
        switch (d.type()) {
            case HDegreeType::ADD:
            case HDegreeType::ALTER:
                switch (d.alter()) {
                    case -1: mod = u"b"; break;
                    case 1: mod = u"#"; break;
                    case 0: mod = u"add"; break;
                }
                break;
            case HDegreeType::SUBTRACT: mod = u"no"; break;
            case HDegreeType::UNDEF:
            default: break;
        }
        mod += String::number(v);
        if (mod == "add7" && kind.contains(u"suspended")) {
            quality_ = u"dominant";
            implied = true;
            extension = 7;
            extend = true;
            mod = u"";
        } else if (mod == "add#7" && kind.contains(u"suspended")) {
            quality_ = u"major";
            implied = false;
            extension = 7;
            extend = true;
            mod = u"";
        } else if (mod == "add9" && extend) {
            if (extension < 9) {
                extension = 9;
            }
            mod = u"";
        } else if (mod == "add11" && extend) {
            if (extension < 11) {
                extension = 11;
            }
            mod = u"";
        } else if (mod == "add13" && extend) {
            extension = 13;
            mod = u"";
        } else if (mod == "add9" && kind.contains(u"sixth")) {
            extension = 69;
            mod = u"";
        }
        if (!mod.empty()) {
            modifier_list_ << mod;
        }
    }
    // convert no3,add[42] into sus[42]
    size_t no3 = modifier_list_.index_of(u"no3");
    if (no3 != muse::nidx) {
        size_t addn = modifier_list_.index_of(u"add4");
        if (addn == muse::nidx) {
            addn = modifier_list_.index_of(u"add2");
        }
        if (addn != muse::nidx) {
            String &s = modifier_list_[addn];
            s.replace(u"add", u"sus");
            modifier_list_.remove_at(no3);
        }
    }
    // convert kind=minor-seventh, degree=altb5 to kind=half-diminished (suppression of degree=altb comes later)
    if (kind == "minor-seventh" && modifier_list_.size() == 1 && modifier_list_.front() == "b5") {
        kind = u"half-diminished";
    }
    // force parens where necessary)
    if (!parens && extension == 0 && !modifier_list_.empty()) {
        String first_mod = modifier_list_.front();
        if (!first_mod.empty() && (first_mod.starts_with(u'#') || first_mod.starts_with(u'b'))) {
            parens = true;
        }
    }

    // record extension
    if (extension) {
        extension_ = String::number(extension);
    }

    // validate kindText
    if (!kind_text.empty() && kind != "none" && kind != "other") {
        ParsedChord validate;
        validate.parse(kind_text, cl, false);
        // kindText should parse to produce same kind, no degrees
        if (validate.xml_kind_ != kind || !validate.xml_degrees_.empty()) {
            kind_text = u"";
        }
    }

    // construct name & handle
    name_ = u"";
    if (!kind_text.empty()) {
        if (!extension_.empty() && kind.contains(u"suspended")) {
            name_ += extension_;
        }
        name_ += kind_text;
        if (extension == 69) {
            name_ += u"9";
        }
    } else if (implied) {
        name_ = extension_;
    } else {
        if (quality_ == "major") {
            name_ = syms ? u"^" : u"maj";
        } else if (quality_ == "minor") {
            name_ = syms ? u"-" : u"m";
        } else if (quality_ == "augmented") {
            name_ = syms ? u"+" : u"aug";
        } else if (quality_ == "diminished") {
            name_ = syms ? u"o" : u"dim";
        } else if (quality_ == "half-diminished") {
            name_ = syms ? u"0" : u"m7b5";
        } else {
            name_ = quality_;
        }
        name_ += extension_;
    }
    if (parens) {
        name_ += u"(";
    }
    for (String mod : modifier_list_) {
        mod.replace(u"major", u"maj");
        if (!kind_text.empty() && kind.contains(u"suspended") && mod.starts_with(u"sus")) {
            continue;
        } else if (!kind_text.empty() && kind == "major-minor" && mod.starts_with(u"maj")) {
            continue;
        }
        name_ += mod;
    }
    if (parens) {
        name_ += u")";
    }

    // parse name to construct handle & tokenList
    parse(name_, cl, true);

    // record original MusicXML
    xml_kind_ = kind;
    xml_text_ = kind_text;
    xml_symbols_ = use_symbols;
    xml_parens_ = use_parens;
    for (const HDegree &d : dl) {
        if (kind == "half-diminished" && d.type() == HDegreeType::ALTER && d.alter() == -1 && d.value() == 5) {
            continue;
        }
        xml_degrees_ << d.text();
    }

    return name_;
}

//---------------------------------------------------------
//   position
//---------------------------------------------------------

double ChordList::position(const StringList &names, bool stack_modifiers, bool super_script, ChordTokenClass ctc, size_t modifier_idx,
                           size_t nmodifiers) const {
    String name = names.empty() ? u"" : names.front();
    switch (ctc) {
        case ChordTokenClass::EXTENSION: return eadjust_;
        case ChordTokenClass::MODIFIER: {
            double y_adj = 0.0;
            if (super_script) {
                // Fake superscript by aligning with capheight
                y_adj += 0.15;
            } else if (stack_modifiers && nmodifiers > 1) {
                static constexpr double line_spacing = 0.4;          // Space between modifiers in units of modiferHeight
                const double modifier_height = mmag_ * stackedmmag_; // Modifier height in units of root capheight
                const double stack_height =
                    (nmodifiers * modifier_height) + ((nmodifiers - 1) * modifier_height *
                                                      line_spacing); // Height of total modifier stack (bottom baseline to top capheight)
                const double base = stack_height / 2;                // Baseline of bottom modifier in the stack
                y_adj += base - modifier_idx * modifier_height * (1 + line_spacing);
            }

            if (!name.is_empty()) {
                y_adj += madjust_;
            }
            return y_adj;
        }
        default: return 0.0;
    }
}

//---------------------------------------------------------
//   renderList
//---------------------------------------------------------

const std::vector<RenderActionPtr> &ParsedChord::render_list(const ChordList *cl, bool stacked) {
    // generate anew on each call,
    // in case chord list has changed since last time
    if (!render_list_.empty()) {
        render_list_.clear();
    }

    if (token_list_.empty()) {
        return render_list_;
    }

    size_t mod_idx = 0;
    const double mod_list_size = modifier_list_.size();
    const size_t final_mod_idx = mod_list_size - 1;
    const bool stack_modifiers_enabled = modifier_list_.size() > 1 && stacked;
    const bool super_script_enabled = !stack_modifiers_enabled && cl->chord_preset() == ChordStylePreset::JAZZ;

    // Special stack layout if there is a single 'add' or 'sus' with multiple degrees
    // Standard stack layout if there is only one
    static const std::wregex sus_add_regex = std::wregex(L"sus|add");
    bool stack_sus_or_add = false;
    for (const String &mod : modifier_list_) {
        if (!(stack_modifiers_enabled || super_script_enabled) || !mod.contains(sus_add_regex)) {
            continue;
        }
        if (!stack_sus_or_add) {
            stack_sus_or_add = true;
        } else {
            stack_sus_or_add = false;
            break;
        }
    }

    bool adjust = cl ? cl->auto_adjust() : false;
    bool first_modifier_token = true;
    bool closing_paren_pending = false;
    for (auto tok_it = token_list_.begin(); tok_it != token_list_.end(); tok_it++) {
        const ChordToken &tok = *tok_it;
        const String n = tok.names.front();
        if ((n == u"/" || n == u"," || n == u"\\" || n == u" ") &&
            (stack_sus_or_add || (stack_modifiers_enabled && tok.token_class == ChordTokenClass::MODIFIER))) {
            continue;
        }

        const bool tok_is_sus_or_add = std::regex_match(n.to_std_w_string(), sus_add_regex);
        const bool stack_modifier = stack_modifiers_enabled && !(stack_sus_or_add && tok_is_sus_or_add);
        const bool super_script_modifier = super_script_enabled && !(stack_sus_or_add && tok_is_sus_or_add);
        String cur_mod = modifier_list_.size() > 1 ? modifier_list_.at(mod_idx) : u"";
        if (stack_sus_or_add && stack_modifier && mod_idx == 0) {
            cur_mod.remove(sus_add_regex);
        }
        const bool modifier_end = cur_mod.ends_with(n) && mod_idx != final_mod_idx;

        std::vector<RenderActionPtr> rl;
        std::vector<ChordToken> defined_tokens;
        bool found = false;
        // potential definitions for token
        if (cl) {
            for (const ChordToken &ct : cl->chord_token_list) {
                for (const String &ctn : ct.names) {
                    if (ctn == n) {
                        defined_tokens.push_back(ct);
                    }
                }
            }
        }
        // find matching class, fallback on ChordTokenClass::ALL
        ChordTokenClass ctc = ChordTokenClass::ALL;
        for (const ChordToken &matching_tok : defined_tokens) {
            if (tok.token_class == matching_tok.token_class) {
                rl = matching_tok.render_list;
                ctc = tok.token_class;
                found = true;
                break;
            } else if (matching_tok.token_class == ChordTokenClass::ALL) {
                rl = matching_tok.render_list;
                found = true;
            }
        }

        // build render list
        // check for adjustments
        double y_adjust =
            adjust ? cl->position(tok.names, stack_modifier, super_script_modifier, ctc, final_mod_idx - mod_idx, mod_list_size) : 0.0;

        // Modifier behaviour
        if (tok.token_class == ChordTokenClass::MODIFIER) {
            // Stop adjusting when first non-adjusted modifier found
            if (real_is_null(y_adjust) && !(stack_sus_or_add && tok_is_sus_or_add)) {
                adjust = false;
            }

            // This is the first modifier. Discount subsequent items from horizontal alignment
            if (cl->exclude_mods_h_align() && mod_idx == 0) {
                render_list_.emplace_back(new RenderActionStopHAlign());
            }

            // Jazz superscript
            if (super_script_modifier) {
                // Set scale
                render_list_.emplace_back(new RenderActionScale(cl->stacked_modifier_mag()));
                // Move to x-height
                render_list_.emplace_back(new RenderActionPush());
                render_list_.emplace_back(new RenderActionMoveXHeight(true));
            }

            // Stacked modifiers
            if (stack_modifier) {
                // Pad modifier stack before
                if (first_modifier_token) {
                    render_list_.emplace_back(new RenderActionMove(0.15, 0));
                    first_modifier_token = false;
                }

                // Check if the next token is a closing paren
                // Delay starting a new line until after the paren
                // (parens are excluded from the modifier string)
                const auto &next_tok_it = std::next(tok_it);
                if (next_tok_it != token_list_.end() && modifier_end) {
                    const ChordToken &next_tok = *next_tok_it;
                    if (next_tok.names.front() == ")") {
                        closing_paren_pending = true;
                    }
                }

                auto starts_with_acc = [](const String &s) -> bool { return s.starts_with(u"b") || s.starts_with(u"#"); };
                auto starts_with_sus_or_add = [](const String &s) -> bool { return s.starts_with(u"sus") || s.starts_with(u"add"); };
                // Align vertically stacked modifier's x position by pushing it at the start of every modifier and popping at the end
                // Make sure degrees are aligned when there are accidentals
                // -- nextMod now searches to the end of modifierList to support stacks of 3+ modifiers
                auto it = std::find_if(modifier_list_.begin() + (mod_idx + 1), modifier_list_.end(),
                                       [&starts_with_acc](const String &s) { return starts_with_acc(s); });
                const String &next_mod = (it != modifier_list_.end()) ? *it : u"";
                bool next_mod_starts_with_acc = (it != modifier_list_.end());
                bool cur_mod_starts_with_acc = starts_with_acc(cur_mod);
                bool n_is_acc = starts_with_acc(n);
                bool cur_mod_starts_with_sus_or_add = starts_with_sus_or_add(cur_mod);
                bool n_is_sus_or_add = starts_with_sus_or_add(n);
                bool n_is_paren = (n == u"(") || (n == u")");

                if (cur_mod_starts_with_sus_or_add) { // Align sus or add left
                    if (n_is_sus_or_add) {
                        render_list_.emplace_back(new RenderActionPush());
                    }
                } else if (n_is_acc &&
                           next_mod_starts_with_acc) { // current line has accidental, next line has accidental -> push before accidental
                    render_list_.emplace_back(new RenderActionPush());
                } else if (!cur_mod_starts_with_acc &&
                           !next_mod_starts_with_acc) { // current line has no accidental, next line has no accidental -> push before degree
                    render_list_.emplace_back(new RenderActionPush());
                } else if (cur_mod_starts_with_acc && !next_mod_starts_with_acc &&
                           !n_is_acc) { // current line has accidental, next line has no accidental ->  push before degree
                    render_list_.emplace_back(new RenderActionPush());
                } else if (!cur_mod_starts_with_acc && next_mod_starts_with_acc &&
                           !n_is_paren) { // current line has no accidental, next line has accidental -> push before degree
                    render_list_.emplace_back(new RenderActionPush()); // and move by next line's accidental's width to align
                    String next_acc = next_mod;
                    static const std::wregex degree_regex = std::wregex(L"[0-9]+");
                    next_acc.replace(degree_regex, u"");
                    // Apply scaling and use renderList to match the behavior of standard accidental rendering
                    render_list_.emplace_back(new RenderActionScale(cl->stacked_modifier_mag()));
                    const ChordToken acc_tok = cl->token(next_acc, ChordTokenClass::MODIFIER);
                    if (acc_tok.is_valid()) {
                        for (const RenderActionPtr &a : acc_tok.render_list) {
                            if (a->action_type() == RenderAction::RenderActionType::SET) {
                                render_list_.emplace_back(new RenderActionMoveTextWidth(u"s" + next_acc));
                            } else {
                                render_list_.emplace_back(a);
                            }
                        }
                    }
                    render_list_.emplace_back(new RenderActionScale(1 / cl->stacked_modifier_mag()));
                }

                // Set scale
                render_list_.emplace_back(new RenderActionScale(cl->stacked_modifier_mag()));
                // Move to x-height
                render_list_.emplace_back(new RenderActionPush());
                render_list_.emplace_back(new RenderActionMoveXHeight(true));
            }
        }

        if (!real_is_null(y_adjust)) {
            render_list_.emplace_back(new RenderActionMove(0.0, y_adjust));
        }
        if (found) {
            render_list_.insert(render_list_.end(), rl.begin(), rl.end());
        } else {
            // no definition for token, so render as literal
            render_list_.emplace_back(new RenderActionSet(tok.names.front()));
        }
        // Reset adjust
        if (!real_is_null(y_adjust)) {
            render_list_.emplace_back(new RenderActionMove(0.0, -y_adjust));
        }

        if (tok.token_class == ChordTokenClass::MODIFIER && !n.empty() && super_script_modifier) {
            render_list_.emplace_back(new RenderActionPopY());
            render_list_.emplace_back(new RenderActionScale(1 / cl->stacked_modifier_mag()));
        }

        // Stacked modifiers
        if (tok.token_class == ChordTokenClass::MODIFIER && !n.empty() && stack_modifier) {
            // Reset move to x-height
            render_list_.emplace_back(new RenderActionPopY());
            // Reset scale
            render_list_.emplace_back(new RenderActionScale(1 / cl->stacked_modifier_mag()));
            if (modifier_end != closing_paren_pending) {
                mod_idx++;
                closing_paren_pending = false;

                // Restore x position
                render_list_.emplace_back(new RenderActionPopX());
            }
        }
    }

    return render_list_;
}

//---------------------------------------------------------
//   addToken
//---------------------------------------------------------

void ParsedChord::add_token(String s, ChordTokenClass tc) {
    if (s.empty()) {
        return;
    }
    ChordToken tok;
    tok.names << s;
    tok.token_class = tc;
    token_list_.push_back(tok);
}

//---------------------------------------------------------
//   ChordDescription
//    this form is used when reading from file
//    a private id is assigned for id = 0
//---------------------------------------------------------

ChordDescription::ChordDescription(int i) {
    if (!i) {
        //            i = --(cl->privateID);
        i = --ChordList::private_id;
    }
    id = i;
    generated = false;
    render_list_generated = false;
    export_ok = true;
}

//---------------------------------------------------------
//   ChordDescription
//    this form is used when generating from name
//    a private id is always assigned
//---------------------------------------------------------

ChordDescription::ChordDescription(const String &name) {
    id = --ChordList::private_id;
    generated = true;
    names.push_back(name);
    render_list_generated = false;
    export_ok = false;
}

//---------------------------------------------------------
//   complete
//    generate missing renderList and semantic (Xml) info
//---------------------------------------------------------

void ChordDescription::complete(ParsedChord *pc, const ChordList *cl) {
    ParsedChord temp_pc;
    if (!pc) {
        // generate parsed chord for its rendering & semantic (xml) info
        pc = &temp_pc;
        String n;
        if (!names.empty()) {
            n = names.front();
        }
        pc->parse(n, cl);
    }
    parsed_chords.push_back(*pc);
    if (render_list.empty() || render_list_stacked.empty() || render_list_generated) {
        render_list = pc->render_list(cl, false);
        render_list_stacked = pc->render_list(cl, true);
        render_list_generated = true;
    }
    if (xml_kind.empty()) {
        xml_kind = pc->xml_kind();
        xml_degrees = pc->xml_degrees();
    }
    // these fields are not read from chord description files
    // so get them from the parsed representation in all cases
    xml_text = pc->xml_text();
    xml_symbols = pc->xml_symbols();
    xml_parens = pc->xml_parens();
    if (chord.get_keys() == 0) {
        chord = HChord(pc->keys());
    }
    quality_ = pc->quality();
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ChordDescription::read(XmlReader &e, int msc_version) {
    int ni = 0;
    id = e.attribute("id").to_int();
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            String n = e.read_text();
            // stack names for this file on top of the list
            names.insert(ni++, n);
        } else if (tag == "xml") {
            xml_kind = e.read_text();
        } else if (tag == "degree") {
            xml_degrees.push_back(e.read_text());
        } else if (tag == "voicing") {
            chord = HChord(e.read_text());
        } else if (tag == "render") {
            read_render_list(e.read_text(), render_list, msc_version);
            render_list_generated = false;
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void ChordDescription::write(XmlWriter &xml) const {
    if (generated && !export_ok) {
        return;
    }
    if (id > 0) {
        xml.start_element("chord", {{"id", id}});
    } else {
        xml.start_element("chord");
    }
    for (const String &s : names) {
        xml.tag("name", s);
    }
    xml.tag("xml", xml_kind);
    xml.tag("voicing", chord.voicing());
    for (const String &s : xml_degrees) {
        xml.tag("degree", s);
    }
    write_render_list(xml, render_list, "render");
    xml.end_element();
}

//---------------------------------------------------------
//   ChordList
//---------------------------------------------------------

int ChordList::private_id = -1000;

//---------------------------------------------------------
//   configureAutoAdjust
//---------------------------------------------------------

void ChordList::configure_auto_adjust(double emag, double eadjust, double mmag, double madjust, double stackedmmag, bool stack_modifiers,
                                      bool exclude_mods_h_align, String symbol_font, const ChordStylePreset &preset) {
    stack_modifiers_ = stack_modifiers;
    exclude_mods_h_align_ = exclude_mods_h_align;
    emag_ = emag;
    eadjust_ = eadjust;
    mmag_ = mmag;
    stackedmmag_ = stackedmmag;
    madjust_ = madjust;
    symbol_text_font_ = symbol_font;
    chord_preset_ = preset;
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ChordList::read(XmlReader &e, int msc_version) {
    int font_idx = static_cast<int>(fonts.size());
    auto_adjust_ = false;
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "font") {
            ChordFont f;
            f.family = e.attribute("family", u"default");
            if (f.family == u"MuseJazz") {
                f.family = u"MuseJazz Text";
            } else if (f.family == u"ScoreText") {
                f.family = symbol_text_font_;
                f.music_symbol_text = true;
            }
            f.mag = 1.0;
            f.font_class = e.attribute("class");
            while (e.read_next_start_element()) {
                if (e.name() == "sym") {
                    ChordSymbol cs;
                    cs.font_idx = font_idx;
                    cs.name = e.attribute("name");
                    cs.value = e.attribute("value");
                    String code = e.attribute("code");
                    String sym_class = e.attribute("class");
                    if (!code.empty()) {
                        bool ok = true;
                        char32_t val = code.to_u_int(&ok, 0);
                        if (!ok && msc_version >= 400 && msc_version < 440) {
                            val = code.to_u_int(&ok, 16);
                        }
                        if (!ok) {
                            cs.code = 0;
                            cs.value = code;
                        } else if (Char::requires_surrogates(val)) {
                            cs.code = 0;
                            cs.value = String::from_ucs4(val);
                        } else {
                            cs.code = val;
                            cs.value = String(cs.code);
                        }
                    } else {
                        cs.code = 0;
                    }
                    if (cs.value.empty()) {
                        cs.value = cs.name;
                    }
                    cs.name = sym_class + cs.name;
                    symbols_.insert({cs.name, cs});
                    e.read_next();
                } else if (e.name() == "mag") {
                    f.mag = e.read_double();
                } else {
                    e.unknown();
                }
            }
            if (auto_adjust_) {
                if (f.font_class == "extension") {
                    f.mag *= emag_;
                } else if (f.font_class == "modifier") {
                    f.mag *= mmag_;
                }
            }
            fonts.push_back(f);
            ++font_idx;
        } else if (tag == "autoAdjust") {
            String nmag = e.attribute("mag");
            if (!nmag.empty()) {
                nmag_ = nmag.to_double();
            }
            String nadjust = e.attribute("adjust");
            if (!nadjust.empty()) {
                nadjust_ = nadjust.to_double();
            }
            auto_adjust_ = e.read_bool();
        } else if (tag == "token") {
            ChordToken t;
            t.read(e, msc_version);
            chord_token_list.push_back(t);
        } else if (tag == "chord") {
            int id = e.int_attribute("id");
            // if no id attribute (id == 0), then assign it a private id
            // user chords that match these ChordDescriptions will be treated as normal recognized chords
            // except that the id will not be written to the score file
            ChordDescription cd = (id && muse::contains(*this, id)) ? muse::take(*this, id) : ChordDescription(id);

            // record updated id
            id = cd.id;
            // read rest of description
            cd.read(e, msc_version);
            // restore updated id
            cd.id = id;
            // throw away previously parsed chords
            cd.parsed_chords.clear();
            // generate any missing info (including new parsed chords)
            cd.complete(0, this);
            // add to list
            insert({id, cd});
        } else if (tag == "renderRoot") {
            read_render_list(e.read_text(), render_list_root, msc_version);
        } else if (tag == "renderFunction") {
            read_render_list(e.read_text(), render_list_function, msc_version);
        } else if ((tag == "renderBase" && msc_version < 460) || tag == "renderBass") {
            read_render_list(e.read_text(), render_list_bass, msc_version);
        } else if (tag == "renderBassOffset") {
            read_render_list(e.read_text(), render_list_bass_offset, msc_version);
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void ChordList::write(XmlWriter &xml) const {
    int font_idx = 0;
    for (const ChordFont &f : fonts) {
        xml.start_element("font", {{"id", font_idx}, {"family", f.family}});
        xml.tag("mag", f.mag);
        for (const auto &p : symbols_) {
            const ChordSymbol &s = p.second;
            if (s.font_idx == font_idx) {
                if (s.code.is_null()) {
                    xml.tag("sym", {{"name", s.name}, {"value", s.value}});
                } else {
                    // write hex numbers with a "0x" prefix, so they can convert back properly on read
                    xml.tag("sym", {{"name", s.name}, {"code", u"0x" + String::number(s.code.unicode(), 16)}});
                }
            }
        }
        xml.end_element();
        ++font_idx;
    }
    if (auto_adjust_) {
        xml.tag("autoAdjust", {{"mag", nmag_}, {"adjust", nadjust_}});
    }
    for (const ChordToken &t : chord_token_list) {
        t.write(xml);
    }
    if (!render_list_root.empty()) {
        write_render_list(xml, render_list_root, "renderRoot");
    }
    if (!render_list_function.empty()) {
        write_render_list(xml, render_list_function, "renderFunction");
    }
    if (!render_list_bass.empty()) {
        write_render_list(xml, render_list_bass, "renderBass");
    }
    if (!render_list_bass_offset.empty()) {
        write_render_list(xml, render_list_bass_offset, "renderBassOffset");
    }
    for (const auto &p : *this) {
        const ChordDescription &cd = p.second;
        cd.write(xml);
    }
}

//---------------------------------------------------------
//   read
//    read Chord List, return false on error
//---------------------------------------------------------

bool ChordList::read(const String &name) {
    //      LOGD("ChordList::read <%s>", muPrintable(name));
    muse::io::PathT path;
    FileInfo ftest(name);
    if (ftest.is_absolute()) {
        path = name;
    } else {
        path = u":/engraving/styles/" + name;
    }

    // default to chords_std.xml
    if (!FileInfo::exists(path)) {
        path = u":/engraving/styles/chords_std.xml";
    }

    if (name.is_empty()) {
        return false;
    }
    File f(path);
    if (!f.open(IODevice::READ_ONLY)) {
        LOGE() << "Cannot open chord description: " << f.file_path();
        return false;
    }

    return read(&f);
}

bool ChordList::read(IODevice *device) {
    XmlReader e(device);

    while (e.read_next_start_element()) {
        if (e.name() == "museScore") {
            const String version = e.attribute("version");
            const StringList sl = version.split(u'.');
            const int msc_version = sl.size() == 2 ? sl[0].to_int() * 100 + sl[1].to_int() : 0;
            read(e, msc_version);
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   writeChordList
//---------------------------------------------------------

bool ChordList::write(const String &name) const {
    FileInfo info(name);

    if (info.suffix().is_empty()) {
        String path = info.file_path();
        path += u".xml";
        info = FileInfo(path);
    }

    auto out_buf = Buffer::opened(IODevice::WRITE_ONLY);
    write(&out_buf);
    out_buf.close();

    Ret ret = File::write_file(info.file_path(), out_buf.data());
    if (!ret) {
        LOGE() << "failed to write chord list: " << ret.to_string();
        return false;
    }

    return true;
}

bool ChordList::write(IODevice *device) const {
    XmlWriter xml(device);
    xml.start_document();
    xml.start_element("museScore", {{"version", Constants::msc_version_str}});

    write(xml);

    xml.end_element();

    return true;
}

//---------------------------------------------------------
//   loaded
//---------------------------------------------------------

bool ChordList::loaded() const {
    // track whether a description file has been loaded
    // since chords.xml really doesn't load enough to stand alone,
    // we need a way to track when a "real" chord list has been loaded
    // for lack of anything better, key off renderListRoot
    return !render_list_root.empty();
}

//---------------------------------------------------------
//   unload
//---------------------------------------------------------

void ChordList::unload() {
    clear();
    symbols_.clear();
    fonts.clear();
    render_list_root.clear();
    render_list_bass.clear();
    chord_token_list.clear();
    auto_adjust_ = false;
}

const ChordDescription *ChordList::description(int id) const {
    auto it = this->find(id);
    if (it == this->end()) {
        return nullptr;
    }
    return &it->second;
}

ChordSymbol ChordList::symbol(const String &s) const { return muse::value(symbols_, s); }

ChordToken ChordList::token(const String &s, ChordTokenClass type) const {
    for (const ChordToken &tok : chord_token_list) {
        if (tok.token_class != type || !tok.names.contains(s)) {
            continue;
        }

        return tok;
    }

    return ChordToken();
}

void ChordList::check_chord_list(const MStyle &style) {
    // make sure we have a chordlist
    if (!loaded()) {
        double emag = style.style_d(Sid::CHORD_EXTENSION_MAG);
        double eadjust = style.style_d(Sid::CHORD_EXTENSION_ADJUST);
        double mmag = style.style_d(Sid::CHORD_MODIFIER_MAG);
        double madjust = style.style_d(Sid::CHORD_MODIFIER_ADJUST);
        double stackedmmag = style.style_d(Sid::CHORD_STACKED_MODIFIER_MAG);
        bool stack_modifiers = style.style_b(Sid::VERTICALLY_STACK_MODIFIERS);
        bool exclude_mods_h_align = style.style_b(Sid::CHORD_ALIGNMENT_EXCLUDE_MODIFIERS);
        String symbol_font = style.style_st(Sid::MUSICAL_TEXT_FONT);
        ChordStylePreset preset = style.style_v(Sid::CHORD_STYLE).value<ChordStylePreset>();
        configure_auto_adjust(emag, eadjust, mmag, madjust, stackedmmag, stack_modifiers, exclude_mods_h_align, symbol_font, preset);

        if (style.value(Sid::CHORDS_XML_FILE).to_bool()) {
            read(u"chords.xml");
        }

        read(style.value(Sid::CHORD_DESCRIPTION_FILE).value<String>());
    }
}

//---------------------------------------------------------
//   print
//    only for debugging
//---------------------------------------------------------

void RenderAction::print(RenderActionType type, const String &info) const {
    static const char *names[] = {"SET", "MOVE", "MOVEXHEIGHT", "PUSH", "POP", "NOTE", "ACCIDENTAL", "STOPHALIGN", "SCALE", "PAREN"};
    LOGD("%10s %s", names[int(type)], muPrintable(info));
}

void RenderActionMove::print() const {
    String info = String(u"%1 %2").arg(vec_.x(), vec_.y());
    RenderAction::print(action_type(), info);
}

void RenderActionSet::print() const {
    String info = String(u"<%1>").arg(text_);
    RenderAction::print(action_type(), info);
}

void RenderActionPop::print() const {
    String info = String(u"pop x: %1 pop y: %2").arg(popx_, popy_);
    RenderAction::print(action_type(), info);
}

void RenderActionScale::print() const {
    String info = String(u"%1").arg(scale_);
    RenderAction::print(action_type(), info);
}

void RenderActionMoveScaled::print() const {
    String info = String(u"SCALED %1 %2").arg(x(), y());
    RenderAction::print(action_type(), info);
}

void RenderActionMoveXHeight::print() const {
    String info = String(u"up: %1").arg(up_);
    RenderAction::print(action_type(), info);
}

void RenderActionParen::print() const {
    String info = String(u"direction: %1").arg(direction() == DirectionH::LEFT ? u"left" : u"right");
    RenderAction::print(action_type(), info);
}
} // namespace mu::engraving
