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

#include "types/constants.h"

#include "rw/compat/compatutils.h"
#include "rw/xmlreader.h"
#include "rw/xmlwriter.h"

#include "style/style.h"

#include "mscore.h"
#include "pitchspelling.h"

#include "log.h"

using namespace mu;
using namespace muse;
using namespace muse::io;

namespace mu::engraving {
//---------------------------------------------------------
//   HChord
//---------------------------------------------------------

HChord::HChord(const String &str) {
    static const char *const scaleNames[2][12] = {{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"},
                                                  {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"}};
    keys_ = 0;
    StringList sl = str.split(u' ', muse::SkipEmptyParts);
    for (const String &s : sl) {
        for (int i = 0; i < 12; ++i) {
            if (s == scaleNames[0][i] || s == scaleNames[1][i]) {
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

void HChord::rotate(int semiTones) {
    while (semiTones > 0) {
        if (keys_ & 0x800) {
            keys_ = ((keys_ & ~0x800) << 1) + 1;
        } else {
            keys_ <<= 1;
        }
        --semiTones;
    }
    while (semiTones < 0) {
        if (keys_ & 1) {
            keys_ = (keys_ >> 1) | 0x800;
        } else {
            keys_ >>= 1;
        }
        ++semiTones;
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
            s += String::fromAscii(names[i]);
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

void HChord::add(const std::vector<HDegree> &degreeList) {
    // LOGD("HChord::add   ");print();
    // convert degrees to semitones
    static const int degreeTable[] = {// 1  2  3  4  5  6   7
                                      // C  D  E  F  G  A   B
                                      0, 2, 4, 5, 7, 9, 11};
    // factor in the degrees
    for (const HDegree &d : degreeList) {
        int i = (d.value() - 1) % 7;
        if (i < 0) {
            continue; // ?
        }
        int dv = degreeTable[i] + d.alter();
        int dv1 = degreeTable[i];

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

static void readRenderList(String val, std::vector<RenderActionPtr> &renderList, int mscVersion) {
    renderList.clear();
    StringList sl = val.split(u' ', muse::SkipEmptyParts);
    for (const String &s : sl) {
        if (s.startsWith(u"m:")) {
            StringList ssl = s.split(u':', muse::SkipEmptyParts);
            if (ssl.size() == 3) {
                // m:x:y
                double movex = ssl.at(1).toDouble();
                double movey = ssl.at(2).toDouble();

                if (mscVersion < 460) {
                    movex = compat::CompatUtils::convertChordExtModUnits(movex);
                    movey = compat::CompatUtils::convertChordExtModUnits(movey);
                }

                renderList.emplace_back(new RenderActionMove(movex, movey));
            }
        } else if (s.startsWith(u"ms:")) {
            StringList ssl = s.split(u':', muse::SkipEmptyParts);
            if (ssl.size() == 3) {
                // ms:x:y
                double movex = ssl[1].toDouble();
                double movey = ssl[2].toDouble();

                renderList.emplace_back(new RenderActionMoveScaled(movex, movey));
            }
        } else if (s == u":mx") {
            renderList.emplace_back(new RenderActionMoveXHeight(true));
        } else if (s == u":mxs") {
            renderList.emplace_back(new RenderActionMoveXHeightScaled(true));
        } else if (s.startsWith(u"sc:")) {
            StringList ssl = s.split(u':', muse::SkipEmptyParts);
            if (ssl.size() == 2) {
                double scale = ssl.at(1).toDouble();
                renderList.emplace_back(new RenderActionScale(scale));
            }
        } else if (s == u":push") {
            renderList.emplace_back(new RenderActionPush());
        } else if (s == u":pop") {
            renderList.emplace_back(new RenderActionPop());
        } else if (s == u":popx") {
            renderList.emplace_back(new RenderActionPopX());
        } else if (s == u":popy") {
            renderList.emplace_back(new RenderActionPopY());
        } else if (s == u":n") {
            renderList.emplace_back(new RenderActionNote());
        } else if (s == u":a") {
            renderList.emplace_back(new RenderActionAccidental());
        } else if (s == u":pl") {
            renderList.emplace_back(new RenderActionParenLeft());
        } else if (s == u":pr") {
            renderList.emplace_back(new RenderActionParenRight());
        } else {
            renderList.emplace_back(new RenderActionSet(s));
        }
    }
}

//---------------------------------------------------------
//   writeRenderList
//---------------------------------------------------------

static void writeRenderList(XmlWriter &xml, const std::vector<RenderActionPtr> &al, const AsciiStringView &name) {
    String s;

    for (const RenderActionPtr &a : al) {
        if (!s.isEmpty()) {
            s += u" ";
        }
        switch (a->actionType()) {
            case RenderAction::RenderActionType::SET: {
                const RenderActionSetPtr set = std::static_pointer_cast<RenderActionSet>(a);
                s += set->text();
                break;
            }
            case RenderAction::RenderActionType::MOVE: {
                const RenderActionMovePtr move = std::static_pointer_cast<RenderActionMove>(a);

                if (!RealIsNull(move->x()) || !RealIsNull(move->y())) {
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
                String coord = pop->popX() && pop->popY() ? u"" : (pop->popX() ? u"x" : u"y");
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

void ChordToken::read(XmlReader &e, int mscVersion) {
    String c = e.attribute("class");
    if (c == "quality") {
        tokenClass = ChordTokenClass::QUALITY;
    } else if (c == "extension") {
        tokenClass = ChordTokenClass::EXTENSION;
    } else if (c == "modifier") {
        tokenClass = ChordTokenClass::MODIFIER;
    } else if (c == "type") {
        tokenClass = ChordTokenClass::TYPE;
    } else if (c == "accidental") {
        tokenClass = ChordTokenClass::ACCIDENTAL;
    } else {
        tokenClass = ChordTokenClass::ALL;
    }
    while (e.readNextStartElement()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            names << e.readText();
        } else if (tag == "render") {
            readRenderList(e.readText(), renderList, mscVersion);
        }
    }
}

//---------------------------------------------------------
//  write
//---------------------------------------------------------

void ChordToken::write(XmlWriter &xml) const {
    XmlWriter::Attributes attrs;
    switch (tokenClass) {
        case ChordTokenClass::QUALITY: attrs.push_back({"class", "quality"}); break;
        case ChordTokenClass::EXTENSION: attrs.push_back({"class", "extension"}); break;
        case ChordTokenClass::MODIFIER: attrs.push_back({"class", "modifier"}); break;
        case ChordTokenClass::TYPE: attrs.push_back({"class", "type"}); break;
        case ChordTokenClass::ACCIDENTAL: attrs.push_back({"accidental", "type"}); break;
        default: break;
    }
    xml.startElement("token", attrs);
    for (const String &s : names) {
        xml.tag("name", s);
    }
    writeRenderList(xml, renderList, "render");
    xml.endElement();
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

void ParsedChord::correctXmlText(const String &s) {
    String xmlText = xmlText_;
    xmlText.remove(std::regex("[0-9]"));
    if (!s.empty()) {
        size_t pos = xmlText.lastIndexOf(u')');
        if (pos == muse::nidx) {
            pos = xmlText.size();
        }
        xmlText.insert(pos, s);
    }
    xmlText_ = xmlText;
}

//---------------------------------------------------------
//  parse
//    returns true if chord was parseable
//---------------------------------------------------------

bool ParsedChord::parse(const String &s, const ChordList *cl, bool syntaxOnly, bool preferMinor) {
    String tok1, tok1L, tok2, tok2L;
    String extensionDigits = u"123456789";
    String special = u"()[],/\\ ";
    String leading = u"([ ";
    String trailing = u")],/\\ ";
    String initial;
    bool take6 = false, take7 = false, take9 = false, take11 = false, take13 = false;
    size_t lastLeadingToken = 0;
    size_t len = s.size();
    size_t i = 0;
    int thirdKey = 0, seventhKey = 0;
    bool susChord = false;
    std::vector<HDegree> hdl;
    int key[] = {0, 0, 2, 4, 5, 7, 9, 11, 0, 2, 4, 5, 7, 9, 11};

    configure(cl);
    name_ = s;
    parseable_ = true;
    understandable_ = true;

    lastLeadingToken = tokenList_.size();
    // get quality
    for (tok1 = u"", tok1L = u"", initial = u""; i < len; ++i) {
        // up to first (non-zero) digit, paren, or comma
        if (extensionDigits.contains(s.at(i)) || special.contains(s.at(i))) {
            break;
        }
        tok1.append(s.at(i));
        tok1L.append(s.at(i).toLower());
        if (tok1L == u"m" || major_.contains(tok1L) || minor_.contains(tok1L) || diminished_.contains(tok1L) ||
            augmented_.contains(tok1L)) {
            initial = tok1;
        }
    }
    // special case for "madd", which needs to parse as m,add rather than ma,dd
    if (tok1L.startsWith(u"madd")) {
        initial = tok1.at(0);
    }
    // quality and first modifier ran together with no separation - eg, mima7, augadd
    // keep quality portion, reset index to read modifier portion later
    // Prevent greedy capture of first character as chord quality in the following cases
    static const std::array<String, 3> modifiersStartingWithQualityCharacters{u"tristan", u"omit", u"type"};
    if (!initial.empty() && initial != tok1 && !muse::contains(modifiersStartingWithQualityCharacters, tok1L)) {
        i -= (tok1.size() - initial.size());
        tok1 = initial;
        tok1L = initial.toLower();
    }
    // determine quality
    if (tok1 == u"M" || major_.contains(tok1L)) {
        quality_ = u"major";
        take6 = true;
        take7 = true;
        take9 = true;
        take11 = true;
        take13 = true;
        if (!syntaxOnly) {
            chord_ = HChord(u"C E G");
        }
    } else if (tok1 == u"m" || minor_.contains(tok1L)) {
        quality_ = u"minor";
        take6 = true;
        take7 = true;
        take9 = true;
        take11 = true;
        take13 = true;
        if (!syntaxOnly) {
            chord_ = HChord(u"C Eb G");
        }
    } else if (diminished_.contains(tok1L)) {
        quality_ = u"diminished";
        take7 = true;
        if (!syntaxOnly) {
            chord_ = HChord(u"C Eb Gb");
        }
    } else if (augmented_.contains(tok1L)) {
        quality_ = u"augmented";
        take7 = true;
        if (!syntaxOnly) {
            chord_ = HChord(u"C E G#");
        }
    } else if (tok1L == "0") {
        quality_ = u"half-diminished";
        if (!syntaxOnly) {
            chord_ = HChord(u"C Eb Gb Bb");
        }
    } else if (tok1L.empty()) {
        // empty quality - this will turn out to be major or dominant (or minor if preferMinor)
        quality_ = u"";
        if (!syntaxOnly) {
            chord_ = preferMinor ? HChord(u"C Eb G") : HChord(u"C E G");
        }
        if (preferMinor) {
            name_ = u"=" + name_;
        }
    } else {
        // anything else is not a quality after all, but a modifier
        // reset to read again as modifier
        quality_ = u"";
        tok1 = u"";
        tok1L = u"";
        i = static_cast<int>(lastLeadingToken);
        if (!syntaxOnly) {
            chord_ = HChord(u"C E G");
        }
    }
    if (tok1L == "=") {
        tok1 = u"";
        tok1L = u"";
    }
    if (!tok1.empty()) {
        addToken(tok1, ChordTokenClass::QUALITY);
    }
    if (!syntaxOnly) {
        xmlKind_ = quality_;
        xmlParens_ = u"no";
        if (symbols_.contains(tok1)) {
            xmlSymbols_ = u"yes";
            xmlText_ = u"";
        } else {
            xmlSymbols_ = u"no";
            xmlText_ = tok1;
        }
    }
    // eat trailing parens and commas
    while (i < len && trailing.contains(s.at(i))) {
        addToken(String(s.at(i++)), ChordTokenClass::QUALITY);
    }

    size_t prevIdx = i;

    // Get type - either "typen" or "III"
    // Eat up to first number
    for (tok1 = u""; i < len; ++i) {
        if (s.at(i).isDigit()) {
            break;
        }
        tok1.append(s.at(i));
    }

    if (tok1 == "type") {
        addToken(tok1, ChordTokenClass::TYPE);
        // Read number
        for (tok1 = u""; i < len; ++i) {
            if (!s.at(i).isDigit()) {
                break;
            }
            addToken(s.at(i), ChordTokenClass::TYPE);
        }
    } else if (tok1.contains(std::wregex(L"[IVX]+"))) {
        // Get roman numerals
        for (i = 0; i < tok1.size(); i++) {
            addToken(tok1.at(i), ChordTokenClass::TYPE);
        }
    } else {
        i = prevIdx;
    }

    lastLeadingToken = tokenList_.size();
    // get extension - up to first non-digit other than comma or slash
    for (tok1 = u""; i < len; ++i) {
        if (!s.at(i).isDigit() && s.at(i) != ',' && s.at(i) != '/') {
            break;
        }
        tok1.append(s.at(i));
    }
    extension_ = tok1;
    if (quality_.empty()) {
        if (extension_ == "7" || extension_ == "9" || extension_ == "11" || extension_ == "13") {
            quality_ = preferMinor ? u"minor" : u"dominant";
            if (!syntaxOnly) {
                xmlKind_ = preferMinor ? u"minor" : u"dominant";
            }
            take7 = true;
            take9 = true;
            take11 = true;
            take13 = true;
        } else {
            quality_ = preferMinor ? u"minor" : u"major";
            if (!syntaxOnly) {
                xmlKind_ = preferMinor ? u"minor" : u"major";
            }
            take6 = true;
            take7 = true;
            take9 = true;
            take11 = true;
            take13 = true;
        }
    }
    if (!tok1.empty()) {
        addToken(tok1, ChordTokenClass::EXTENSION);
    }
    if (!syntaxOnly) {
        if (quality_ == "minor") {
            thirdKey = 3;
        } else {
            thirdKey = 4;
        }
        if (quality_ == "major") {
            seventhKey = 11;
        } else if (quality_ == "diminished") {
            seventhKey = 9;
        } else {
            seventhKey = 10;
        }
        xmlText_ += extension_;
        StringList extl;
        if (tok1 == "2") {
            String d = u"add" + tok1;
            xmlDegrees_ << d;
            xmlText_.remove(tok1);
            chord_ += 2;
        } else if (tok1 == "4") {
            String d = u"add" + tok1;
            xmlDegrees_ << d;
            xmlText_.remove(tok1);
            chord_ += 5;
        } else if (tok1 == "5") {
            xmlKind_ = u"power";
            chord_ -= thirdKey;
        } else if (tok1 == "6") {
            if (take6) {
                xmlKind_ += u"-sixth";
            } else {
                extl << u"6";
            }
            chord_ += 9;
        } else if (tok1 == "7") {
            if (take7) {
                xmlKind_ += u"-seventh";
            } else if (xmlKind_ != "half-diminished") {
                extl << u"7";
            }
            chord_ += seventhKey;
        } else if (tok1 == "9") {
            if (take9) {
                xmlKind_ += u"-ninth";
            } else if (take7) {
                xmlKind_ += u"-seventh";
                extl << u"9";
                correctXmlText(u"7");
            } else if (xmlKind_ == u"half-diminished") {
                extl << u"9";
                correctXmlText();
            } else {
                extl << u"7" << u"9";
                correctXmlText();
            }
            chord_ += seventhKey;
            chord_ += 2;
        } else if (tok1 == u"11") {
            if (take11) {
                xmlKind_ += u"-11th";
            } else if (take7) {
                xmlKind_ += u"-seventh";
                extl << u"9" << u"11";
                correctXmlText(u"7");
            } else if (xmlKind_ == u"half-diminished") {
                extl << u"9" << u"11";
                correctXmlText();
            } else {
                extl << u"7" << u"9" << u"11";
                correctXmlText();
            }
            chord_ += seventhKey;
            chord_ += 2;
            chord_ += 5;
        } else if (tok1 == u"13") {
            if (take13) {
                xmlKind_ += u"-13th";
            } else if (take7) {
                xmlKind_ += u"-seventh";
                extl << u"9" << u"11" << u"13";
                correctXmlText(u"7");
            } else if (xmlKind_ == u"half-diminished") {
                extl << u"9" << u"11" << u"13";
                correctXmlText();
            } else {
                extl << u"7" << u"9" << u"11" << u"13";
                correctXmlText();
            }
            chord_ += seventhKey;
            chord_ += 2;
            chord_ += 5;
            chord_ += 9;
        } else if (tok1 == u"69" || tok1 == u"6,9" || tok1 == u"6/9") {
            if (take6) {
                xmlKind_ += u"-sixth";
                extl << u"9";
                correctXmlText(u"6");
            } else {
                extl << u"6" << u"9";
                correctXmlText();
            }
            extension_ = u"69";
            chord_ += 9;
            chord_ += 2;
        }
        for (const String &e : extl) {
            String d = u"add" + e;
            xmlDegrees_ << d;
        }
        if (xmlKind_ == u"dominant-seventh") {
            xmlKind_ = u"dominant";
        }
    }
    // eat trailing parens and commas
    while (i < len && trailing.contains(s.at(i))) {
        addToken(String(s.at(i++)), ChordTokenClass::EXTENSION);
    }

    // get modifiers
    bool addPending = false;
    bool susPending = false;
    modifierList_.clear();
    while (i < len) {
        // eat leading parens
        while (i < len && leading.contains(s.at(i))) {
            addToken(String(s.at(i++)), ChordTokenClass::MODIFIER);
            xmlParens_ = u"yes";
        }
        // get first token - up to first digit, paren, or comma
        for (tok1 = u"", tok1L = u"", initial = u""; i < len; ++i) {
            if (s.at(i).isDigit() || special.contains(s.at(i))) {
                break;
            }
            tok1.append(s.at(i));
            tok1L.append(s.at(i).toLower());
            if (mod_.contains(tok1L)) {
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
            tok1L = initial.toLower();
        }
        // for "add" and "sus", just add the token and then read argument as a separate modifier
        // this allows the argument to itself be a two-part string
        // thus allowing addb9 -> add;b,9
        if (tok1L == "add") {
            addToken(tok1, ChordTokenClass::MODIFIER);
            addPending = true;
            continue;
        }
        // When there are no numbers after, default to sus4 further down
        if (tok1L == "sus" && i != len) {
            addToken(tok1, ChordTokenClass::MODIFIER);
            susPending = true;
            continue;
        }
        // eat spaces
        while (i < len && s.at(i) == u' ') {
            ++i;
        }
        // get second token - a number <= 13
        for (tok2 = u""; i < len; ++i) {
            if (!s.at(i).isDigit()) {
                break;
            }
            if (tok2.size() == 1 && (tok2.at(0) != u'1' || s.at(i) > u'3')) {
                break;
            }
            tok2.append(s.at(i));
        }
        // check suffix - 'st', 'nd', 'rd', 'th'
        String suffix;
        static const int SUFFIX_LEN = 2;
        for (int j = 0; j < SUFFIX_LEN && i + j < len; j++) {
            suffix.append(s.at(i + j));
        }

        if (suffix.contains(std::wregex(L"st|nd|rd|th"))) {
            i += SUFFIX_LEN;
        } else {
            suffix.clear();
        }

        tok2L = tok2.toLower();
        // re-attach "add"
        if (addPending) {
            if (raise_.contains(tok1L)) {
                tok1L = u"#";
            } else if (lower_.contains(tok1L)) {
                tok1L = u"b";
            } else if (tok1 == "M" || major_.contains(tok1L)) {
                tok1L = u"major";
            }
            tok2L = tok1L + tok2L;
            tok1L = u"add";
        }
        // re-attach "sus"
        if (susPending) {
            if (raise_.contains(tok1L)) {
                tok1L = u"#";
            } else if (lower_.contains(tok1L)) {
                tok1L = u"b";
            } else if (tok1 == "M" || major_.contains(tok1L)) {
                tok1L = u"major";
            }
            tok2L = tok1L + tok2L;
            tok1L = u"sus";
        }
        // standardize spelling
        if (tok1 == "M" || major_.contains(tok1L)) {
            tok1L = u"major";
        } else if (tok1L == "omit") {
            tok1L = u"no";
        } else if (tok1L == "sus" && tok2L.empty()) {
            tok2L = u"4";
        } else if (augmented_.contains(tok1L) && tok2L.empty()) {
            if (quality_ == "dominant" && extension_ == "7") {
                quality_ = u"augmented";
                if (!syntaxOnly) {
                    xmlKind_ = u"augmented-seventh";
                    xmlText_ = extension_ + tok1;
                    chord_ -= 7;
                    chord_ += 8;
                }
                tok1L = u"";
            } else {
                tok1L = u"#";
                tok2L = u"5";
            }
        } else if (diminished_.contains(tok1)) {
            quality_ = u"diminished";
            if (!syntaxOnly) {
                xmlKind_ = u"diminished";
                xmlText_ = extension_ + tok1;
                chord_ -= 4;
                chord_ += 3;
                chord_ -= 7;
                chord_ += 6;
            }
            tok1L = u"";
        } else if ((lower_.contains(tok1L) || raise_.contains(tok1L)) && tok2L.empty()) {
            // trailing alteration - treat as applying to extension (and convert to modifier)
            // this handles C5b, C9#, etc
            tok2L = extension_;
            if (!syntaxOnly) {
                xmlKind_ = (quality_ == u"dominant") ? u"major" : quality_;
                xmlText_.remove(extension_);
                if (extension_ == "5") {
                    chord_ += thirdKey;
                } else {
                    chord_ -= seventhKey;
                }
            }
            if (quality_ == "dominant") {
                quality_ = u"major";
            }
            extension_ = u"";
            if (lower_.contains(tok1L)) {
                tok1L = u"b";
            } else {
                tok1L = u"#";
            }
        } else if (lower_.contains(tok1L)) {
            tok1L = u"b";
        } else if (raise_.contains(tok1L)) {
            tok1L = u"#";
        }
        String m = tok1L + tok2L + suffix;
        if (!m.empty()) {
            modifierList_ << m;
        }
        if (!tok1.empty()) {
            addToken(tok1, ChordTokenClass::MODIFIER);
        }
        if (!tok2.empty()) {
            addToken(tok2, ChordTokenClass::MODIFIER);
        }
        if (!suffix.empty()) {
            addToken(suffix, ChordTokenClass::MODIFIER);
        }
        if (!syntaxOnly) {
            int d;
            if (tok2L.empty()) {
                d = 0;
            } else {
                d = tok2L.toInt();
            }
            if (d > 13) {
                d = 13;
            }
            String degree;
            bool alter = false;
            if (tok1L == "add") {
                if (d) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                } else if (!tok2L.empty()) {
                    // this was result of addPending
                    // alteration; tok1 = alter, tok2 = value
                    d = tok2.toInt();
                    if (raise_.contains(tok1) || tok1 == "M" || major_.contains(tok1.toLower())) {
                        if (d == 7) {
                            chord_ += 11;
                            tok2L = u"#7";
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
                degree = u"add" + tok2L;
            } else if (tok1L == u"no") {
                degree = u"sub" + tok2L;
                if (d) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::SUBTRACT));
                }
            } else if (tok1L == "sus") {
                // convert chords with sus into suspended "kind"
                // extension then becomes a series of degree adds
                if (tok2L == "4") {
                    xmlKind_ = u"suspended-fourth";
                } else if (tok2L == "2") {
                    xmlKind_ = u"suspended-second";
                }
                xmlText_ = tok1L + tok2;
                if (extension_ == "7" || extension_ == "9" || extension_ == "11" || extension_ == "13") {
                    xmlDegrees_ << ((quality_ == u"major") ? u"add#7" : u"add7");
                    // hack for programs that cannot assemble names well
                    // even though the kind is suspended, set text to also include the extension
                    // in export, we will set the degree text to null
                    xmlText_ = extension_ + xmlText_;
                    degree = u"";
                } else if (!extension_.empty()) {
                    degree = u"add" + extension_;
                }
                if (extension_ == u"13") {
                    xmlDegrees_ << u"add9";
                    xmlDegrees_ << u"add11";
                    xmlDegrees_ << u"add13";
                } else if (extension_ == u"11") {
                    xmlDegrees_ << u"add9";
                    xmlDegrees_ << u"add11";
                } else if (extension_ == u"9") {
                    xmlDegrees_ << u"add9";
                }
                susChord = true;
                chord_ -= thirdKey;
                if (d) {
                    chord_ += key[d];
                }
            } else if (tok1L == u"major") {
                if (xmlKind_.startsWith(u"minor")) {
                    xmlKind_ = u"major-minor";
                    if (extension_ == u"9" || tok2L == u"9") {
                        xmlDegrees_ << u"add9";
                    }
                    if (extension_ == "11" || tok2L == u"11") {
                        xmlDegrees_ << u"add9";
                        xmlDegrees_ << u"add11";
                    }
                    if (extension_ == u"13" || tok2L == u"13") {
                        xmlDegrees_ << u"add9";
                        xmlDegrees_ << u"add11";
                        xmlDegrees_ << u"add13";
                    }
                    xmlText_ += tok1 + tok2;
                    correctXmlText(u"7");
                } else {
                    tok1L = u"add";
                }
                chord_ -= 10;
                chord_ += 11;
                if (d && d != 7) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                }
            } else if (tok1L == u"alt") {
                xmlDegrees_ << u"altb5";
                xmlDegrees_ << u"add#5";
                xmlDegrees_ << u"addb9";
                xmlDegrees_ << u"add#9";
                chord_ -= 7;
                chord_ += 6;
                chord_ += 8;
                chord_ += 1;
                chord_ += 3;
            } else if (tok1L == u"blues") {
                // this isn't really well-defined, but it might as well mean something
                if (extension_ == u"11" || extension_ == u"13") {
                    xmlDegrees_ << u"alt#9";
                } else {
                    xmlDegrees_ << u"add#9";
                }
                chord_ += 3;
            } else if (tok1L == u"lyd") {
                if (extension_ == u"13") {
                    xmlDegrees_ << u"alt#11";
                } else {
                    xmlDegrees_ << u"add#11";
                }
                chord_ += 6;
            } else if (tok1L == "phryg") {
                if (!xmlKind_.startsWith(u"minor")) {
                    xmlKind_ = u"minor-seventh";
                }
                if (extension_ == "11" || extension_ == "13") {
                    xmlDegrees_ << u"altb9";
                } else {
                    xmlDegrees_ << u"addb9";
                }
                xmlText_ += tok1;
                chord_ = HChord(u"C Db Eb G Bb");
            } else if (tok1L == "tristan") {
                xmlKind_ = u"Tristan";
                xmlText_ = tok1;
                chord_ = HChord(u"C F# A# D#");
            } else if (addPending) {
                degree = u"add" + tok1L + tok2L;
                if (raise_.contains(tok1L)) {
                    hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                } else if (lower_.contains(tok1L)) {
                    hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                } else {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                }
            } else if (susPending) {
                degree = u"sus" + tok1L + tok2L;
                if (raise_.contains(tok1L)) {
                    hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                } else if (lower_.contains(tok1L)) {
                    hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                } else {
                    hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
                }
            } else if (tok1L.empty() && !tok2L.empty()) {
                degree = u"add" + tok2L;
                hdl.push_back(HDegree(d, 0, HDegreeType::ADD));
            } else if (lower_.contains(tok1L)) {
                tok1L = u"b";
                alter = true;
            } else if (raise_.contains(tok1L)) {
                tok1L = u"#";
                alter = true;
            } else if (tok1L.empty()) {
                // token was already handled fully
            } else {
                understandable_ = false;
                if (s.startsWith(tok1)) {
                    // unrecognized token right from very beginning
                    xmlKind_ = u"other";
                    xmlText_ = tok1;
                }
            }
            if (alter) {
                if (tok2L == "4" && xmlKind_ == "suspended-fourth") {
                    degree = u"alt";
                } else if (tok2L == "5") {
                    degree = u"alt";
                } else if (tok2L == "9" && (extension_ == "11" || extension_ == "13")) {
                    degree = u"alt";
                } else if (tok2L == "11" && extension_ == "13") {
                    degree = u"alt";
                } else {
                    degree = u"add";
                }
                degree += tok1L + tok2L;
                if (chord_.contains(key[d]) && !(susChord && (d == 11))) {
                    hdl.push_back(HDegree(d, 0, HDegreeType::SUBTRACT));
                }
                if (tok1L == "#") {
                    if (d == 7) {
                        chord_ += 11;
                    } else {
                        hdl.push_back(HDegree(d, 1, HDegreeType::ADD));
                    }
                } else if (tok1L == "b") {
                    if (d == 7) {
                        chord_ += 10;
                    } else {
                        hdl.push_back(HDegree(d, -1, HDegreeType::ADD));
                    }
                }
            }
            if (!degree.empty()) {
                xmlDegrees_ << degree;
            }
        }
        // eat trailing parens and commas
        while (i < len && trailing.contains(s.at(i))) {
            addToken(String(s.at(i++)), ChordTokenClass::MODIFIER);
        }
        addPending = false;
        susPending = false;
    }
    if (!syntaxOnly) {
        chord_.add(hdl);
        // fix "add" / "alt" conflicts
        // so add9,altb9 -> addb9
        StringList altList = xmlDegrees_.filter(u"alt");
        for (const String &d : altList) {
            String unalt(d);
            unalt.replace(std::regex("alt[b#]"), u"add");
            if (xmlDegrees_.removeAll(unalt)) {
                String alt(d);
                alt.replace(u"alt", u"add");
                size_t i1 = xmlDegrees_.indexOf(d);
                xmlDegrees_.replace(i1, alt);
            }
        }
    }

    // construct handle
    if (!modifierList_.empty()) {
        StringList sortedModifiers = modifierList_;
        std::sort(sortedModifiers.begin(), sortedModifiers.end());
        modifiers_ = u"<" + sortedModifiers.join(u"><") + u">";
    }
    handle_ = u"<" + quality_ + u"><" + extension_ + u">" + modifiers_;

    // force <minor><7><b5> to export as half-diminished
    if (!syntaxOnly && handle_ == u"<minor><7><b5>") {
        xmlKind_ = u"half-diminished";
        xmlText_ = s;
        xmlDegrees_.clear();
    }
    if (MScore::debugMode) {
        LOGD("parse: source = <%s>, handle = %s", muPrintable(s), muPrintable(handle_));
        if (!syntaxOnly) {
            LOGD("parse: HChord = <%s> (%d)", muPrintable(chord_.voicing()), chord_.getKeys());
            LOGD("parse: xmlKind = <%s>, text = <%s>", muPrintable(xmlKind_), muPrintable(xmlText_));
            LOGD("parse: xmlSymbols = %s, xmlParens = %s", muPrintable(xmlSymbols_), muPrintable(xmlParens_));
            LOGD("parse: xmlDegrees = <%s>", muPrintable(xmlDegrees_.join(u",")));
        }
    }
    return parseable_;
}

//---------------------------------------------------------
//   fromXml
//---------------------------------------------------------

String ParsedChord::fromXml(const String &rawKind, const String &rawKindText, const String &useSymbols, const String &useParens,
                            const std::vector<HDegree> &dl, const ChordList *cl) {
    String kind = rawKind;
    String kindText = rawKindText;
    bool syms = (useSymbols == "yes");
    bool parens = (useParens == "yes");
    bool implied = false;
    bool extend = false;
    int extension = 0;
    parseable_ = true;
    understandable_ = true;

    // get quality info from kind
    if (kind == "major-minor") {
        quality_ = u"minor";
        modifierList_ << u"major7";
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
        modifierList_ << u"sus4";
    } else if (kind == "suspended-second") {
        quality_ = u"major";
        implied = true;
        modifierList_ << u"sus2";
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
            modifierList_ << mod;
        }
    }
    // convert no3,add[42] into sus[42]
    size_t no3 = modifierList_.indexOf(u"no3");
    if (no3 != muse::nidx) {
        size_t addn = modifierList_.indexOf(u"add4");
        if (addn == muse::nidx) {
            addn = modifierList_.indexOf(u"add2");
        }
        if (addn != muse::nidx) {
            String &s = modifierList_[addn];
            s.replace(u"add", u"sus");
            modifierList_.removeAt(no3);
        }
    }
    // convert kind=minor-seventh, degree=altb5 to kind=half-diminished (suppression of degree=altb comes later)
    if (kind == "minor-seventh" && modifierList_.size() == 1 && modifierList_.front() == "b5") {
        kind = u"half-diminished";
    }
    // force parens where necessary)
    if (!parens && extension == 0 && !modifierList_.empty()) {
        String firstMod = modifierList_.front();
        if (!firstMod.empty() && (firstMod.startsWith(u'#') || firstMod.startsWith(u'b'))) {
            parens = true;
        }
    }

    // record extension
    if (extension) {
        extension_ = String::number(extension);
    }

    // validate kindText
    if (!kindText.empty() && kind != "none" && kind != "other") {
        ParsedChord validate;
        validate.parse(kindText, cl, false);
        // kindText should parse to produce same kind, no degrees
        if (validate.xmlKind_ != kind || !validate.xmlDegrees_.empty()) {
            kindText = u"";
        }
    }

    // construct name & handle
    name_ = u"";
    if (!kindText.empty()) {
        if (!extension_.empty() && kind.contains(u"suspended")) {
            name_ += extension_;
        }
        name_ += kindText;
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
    for (String mod : modifierList_) {
        mod.replace(u"major", u"maj");
        if (!kindText.empty() && kind.contains(u"suspended") && mod.startsWith(u"sus")) {
            continue;
        } else if (!kindText.empty() && kind == "major-minor" && mod.startsWith(u"maj")) {
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
    xmlKind_ = kind;
    xmlText_ = kindText;
    xmlSymbols_ = useSymbols;
    xmlParens_ = useParens;
    for (const HDegree &d : dl) {
        if (kind == "half-diminished" && d.type() == HDegreeType::ALTER && d.alter() == -1 && d.value() == 5) {
            continue;
        }
        xmlDegrees_ << d.text();
    }

    return name_;
}

//---------------------------------------------------------
//   position
//---------------------------------------------------------

double ChordList::position(const StringList &names, bool stackModifiers, bool superScript, ChordTokenClass ctc, size_t modifierIdx,
                           size_t nmodifiers) const {
    String name = names.empty() ? u"" : names.front();
    switch (ctc) {
        case ChordTokenClass::EXTENSION: return eadjust_;
        case ChordTokenClass::MODIFIER: {
            double yAdj = 0.0;
            if (superScript) {
                // Fake superscript by aligning with capheight
                yAdj += 0.15;
            } else if (stackModifiers && nmodifiers > 1) {
                static constexpr double LINE_SPACING = 0.4;         // Space between modifiers in units of modiferHeight
                const double modifierHeight = mmag_ * stackedmmag_; // Modifier height in units of root capheight
                const double stackHeight =
                    (nmodifiers * modifierHeight) +
                    ((nmodifiers - 1) * modifierHeight * LINE_SPACING); // Height of total modifier stack (bottom baseline to top capheight)
                const double base = stackHeight / 2;                    // Baseline of bottom modifier in the stack
                yAdj += base - modifierIdx * modifierHeight * (1 + LINE_SPACING);
            }

            if (!name.isEmpty()) {
                yAdj += madjust_;
            }
            return yAdj;
        }
        default: return 0.0;
    }
}

//---------------------------------------------------------
//   renderList
//---------------------------------------------------------

const std::vector<RenderActionPtr> &ParsedChord::renderList(const ChordList *cl, bool stacked) {
    // generate anew on each call,
    // in case chord list has changed since last time
    if (!renderList_.empty()) {
        renderList_.clear();
    }

    if (tokenList_.empty()) {
        return renderList_;
    }

    size_t modIdx = 0;
    const double modListSize = modifierList_.size();
    const size_t finalModIdx = modListSize - 1;
    const bool stackModifiersEnabled = modifierList_.size() > 1 && stacked;
    const bool superScriptEnabled = !stackModifiersEnabled && cl->chordPreset() == ChordStylePreset::JAZZ;

    // Special stack layout if there is a single 'add' or 'sus' with multiple degrees
    // Standard stack layout if there is only one
    static const std::wregex SUS_ADD_REGEX = std::wregex(L"sus|add");
    bool stackSusOrAdd = false;
    for (const String &mod : modifierList_) {
        if (!(stackModifiersEnabled || superScriptEnabled) || !mod.contains(SUS_ADD_REGEX)) {
            continue;
        }
        if (!stackSusOrAdd) {
            stackSusOrAdd = true;
        } else {
            stackSusOrAdd = false;
            break;
        }
    }

    bool adjust = cl ? cl->autoAdjust() : false;
    bool firstModifierToken = true;
    bool closingParenPending = false;
    for (auto tokIt = tokenList_.begin(); tokIt != tokenList_.end(); tokIt++) {
        const ChordToken &tok = *tokIt;
        const String n = tok.names.front();
        if ((n == u"/" || n == u"," || n == u"\\" || n == u" ") &&
            (stackSusOrAdd || (stackModifiersEnabled && tok.tokenClass == ChordTokenClass::MODIFIER))) {
            continue;
        }

        const bool tokIsSusOrAdd = std::regex_match(n.toStdWString(), SUS_ADD_REGEX);
        const bool stackModifier = stackModifiersEnabled && !(stackSusOrAdd && tokIsSusOrAdd);
        const bool superScriptModifier = superScriptEnabled && !(stackSusOrAdd && tokIsSusOrAdd);
        String curMod = modifierList_.size() > 1 ? modifierList_.at(modIdx) : u"";
        if (stackSusOrAdd && stackModifier && modIdx == 0) {
            curMod.remove(SUS_ADD_REGEX);
        }
        const bool modifierEnd = curMod.endsWith(n) && modIdx != finalModIdx;

        std::vector<RenderActionPtr> rl;
        std::vector<ChordToken> definedTokens;
        bool found = false;
        // potential definitions for token
        if (cl) {
            for (const ChordToken &ct : cl->chordTokenList) {
                for (const String &ctn : ct.names) {
                    if (ctn == n) {
                        definedTokens.push_back(ct);
                    }
                }
            }
        }
        // find matching class, fallback on ChordTokenClass::ALL
        ChordTokenClass ctc = ChordTokenClass::ALL;
        for (const ChordToken &matchingTok : definedTokens) {
            if (tok.tokenClass == matchingTok.tokenClass) {
                rl = matchingTok.renderList;
                ctc = tok.tokenClass;
                found = true;
                break;
            } else if (matchingTok.tokenClass == ChordTokenClass::ALL) {
                rl = matchingTok.renderList;
                found = true;
            }
        }

        // build render list
        // check for adjustments
        double yAdjust = adjust ? cl->position(tok.names, stackModifier, superScriptModifier, ctc, finalModIdx - modIdx, modListSize) : 0.0;

        // Modifier behaviour
        if (tok.tokenClass == ChordTokenClass::MODIFIER) {
            // Stop adjusting when first non-adjusted modifier found
            if (RealIsNull(yAdjust) && !(stackSusOrAdd && tokIsSusOrAdd)) {
                adjust = false;
            }

            // This is the first modifier. Discount subsequent items from horizontal alignment
            if (cl->excludeModsHAlign() && modIdx == 0) {
                renderList_.emplace_back(new RenderActionStopHAlign());
            }

            // Jazz superscript
            if (superScriptModifier) {
                // Set scale
                renderList_.emplace_back(new RenderActionScale(cl->stackedModifierMag()));
                // Move to x-height
                renderList_.emplace_back(new RenderActionPush());
                renderList_.emplace_back(new RenderActionMoveXHeight(true));
            }

            // Stacked modifiers
            if (stackModifier) {
                // Pad modifier stack before
                if (firstModifierToken) {
                    renderList_.emplace_back(new RenderActionMove(0.15, 0));
                    firstModifierToken = false;
                }

                // Check if the next token is a closing paren
                // Delay starting a new line until after the paren
                // (parens are excluded from the modifier string)
                const auto &nextTokIt = std::next(tokIt);
                if (nextTokIt != tokenList_.end() && modifierEnd) {
                    const ChordToken &nextTok = *nextTokIt;
                    if (nextTok.names.front() == ")") {
                        closingParenPending = true;
                    }
                }

                auto startsWithAcc = [](const String &s) -> bool { return s.startsWith(u"b") || s.startsWith(u"#"); };
                auto startsWithSusOrAdd = [](const String &s) -> bool { return s.startsWith(u"sus") || s.startsWith(u"add"); };
                // Align vertically stacked modifier's x position by pushing it at the start of every modifier and popping at the end
                // Make sure degrees are aligned when there are accidentals
                // -- nextMod now searches to the end of modifierList to support stacks of 3+ modifiers
                auto it = std::find_if(modifierList_.begin() + (modIdx + 1), modifierList_.end(),
                                       [&startsWithAcc](const String &s) { return startsWithAcc(s); });
                const String &nextMod = (it != modifierList_.end()) ? *it : u"";
                bool nextModStartsWithAcc = (it != modifierList_.end());
                bool curModStartsWithAcc = startsWithAcc(curMod);
                bool nIsAcc = startsWithAcc(n);
                bool curModStartsWithSusOrAdd = startsWithSusOrAdd(curMod);
                bool nIsSusOrAdd = startsWithSusOrAdd(n);
                bool nIsParen = (n == u"(") || (n == u")");

                if (curModStartsWithSusOrAdd) { // Align sus or add left
                    if (nIsSusOrAdd) {
                        renderList_.emplace_back(new RenderActionPush());
                    }
                } else if (nIsAcc &&
                           nextModStartsWithAcc) { // current line has accidental, next line has accidental -> push before accidental
                    renderList_.emplace_back(new RenderActionPush());
                } else if (!curModStartsWithAcc &&
                           !nextModStartsWithAcc) { // current line has no accidental, next line has no accidental -> push before degree
                    renderList_.emplace_back(new RenderActionPush());
                } else if (curModStartsWithAcc && !nextModStartsWithAcc &&
                           !nIsAcc) { // current line has accidental, next line has no accidental ->  push before degree
                    renderList_.emplace_back(new RenderActionPush());
                } else if (!curModStartsWithAcc && nextModStartsWithAcc &&
                           !nIsParen) { // current line has no accidental, next line has accidental -> push before degree
                    renderList_.emplace_back(new RenderActionPush()); // and move by next line's accidental's width to align
                    String nextAcc = nextMod;
                    static const std::wregex DEGREE_REGEX = std::wregex(L"[0-9]+");
                    nextAcc.replace(DEGREE_REGEX, u"");
                    // Apply scaling and use renderList to match the behavior of standard accidental rendering
                    renderList_.emplace_back(new RenderActionScale(cl->stackedModifierMag()));
                    const ChordToken accTok = cl->token(nextAcc, ChordTokenClass::MODIFIER);
                    if (accTok.isValid()) {
                        for (const RenderActionPtr &a : accTok.renderList) {
                            if (a->actionType() == RenderAction::RenderActionType::SET) {
                                renderList_.emplace_back(new RenderActionMoveTextWidth(u"s" + nextAcc));
                            } else {
                                renderList_.emplace_back(a);
                            }
                        }
                    }
                    renderList_.emplace_back(new RenderActionScale(1 / cl->stackedModifierMag()));
                }

                // Set scale
                renderList_.emplace_back(new RenderActionScale(cl->stackedModifierMag()));
                // Move to x-height
                renderList_.emplace_back(new RenderActionPush());
                renderList_.emplace_back(new RenderActionMoveXHeight(true));
            }
        }

        if (!RealIsNull(yAdjust)) {
            renderList_.emplace_back(new RenderActionMove(0.0, yAdjust));
        }
        if (found) {
            renderList_.insert(renderList_.end(), rl.begin(), rl.end());
        } else {
            // no definition for token, so render as literal
            renderList_.emplace_back(new RenderActionSet(tok.names.front()));
        }
        // Reset adjust
        if (!RealIsNull(yAdjust)) {
            renderList_.emplace_back(new RenderActionMove(0.0, -yAdjust));
        }

        if (tok.tokenClass == ChordTokenClass::MODIFIER && !n.empty() && superScriptModifier) {
            renderList_.emplace_back(new RenderActionPopY());
            renderList_.emplace_back(new RenderActionScale(1 / cl->stackedModifierMag()));
        }

        // Stacked modifiers
        if (tok.tokenClass == ChordTokenClass::MODIFIER && !n.empty() && stackModifier) {
            // Reset move to x-height
            renderList_.emplace_back(new RenderActionPopY());
            // Reset scale
            renderList_.emplace_back(new RenderActionScale(1 / cl->stackedModifierMag()));
            if (modifierEnd != closingParenPending) {
                modIdx++;
                closingParenPending = false;

                // Restore x position
                renderList_.emplace_back(new RenderActionPopX());
            }
        }
    }

    return renderList_;
}

//---------------------------------------------------------
//   addToken
//---------------------------------------------------------

void ParsedChord::addToken(String s, ChordTokenClass tc) {
    if (s.empty()) {
        return;
    }
    ChordToken tok;
    tok.names << s;
    tok.tokenClass = tc;
    tokenList_.push_back(tok);
}

//---------------------------------------------------------
//   ChordDescription
//    this form is used when reading from file
//    a private id is assigned for id = 0
//---------------------------------------------------------

ChordDescription::ChordDescription(int i) {
    if (!i) {
        //            i = --(cl->privateID);
        i = --ChordList::privateID;
    }
    id = i;
    generated = false;
    renderListGenerated = false;
    exportOk = true;
}

//---------------------------------------------------------
//   ChordDescription
//    this form is used when generating from name
//    a private id is always assigned
//---------------------------------------------------------

ChordDescription::ChordDescription(const String &name) {
    id = --ChordList::privateID;
    generated = true;
    names.push_back(name);
    renderListGenerated = false;
    exportOk = false;
}

//---------------------------------------------------------
//   complete
//    generate missing renderList and semantic (Xml) info
//---------------------------------------------------------

void ChordDescription::complete(ParsedChord *pc, const ChordList *cl) {
    ParsedChord tempPc;
    if (!pc) {
        // generate parsed chord for its rendering & semantic (xml) info
        pc = &tempPc;
        String n;
        if (!names.empty()) {
            n = names.front();
        }
        pc->parse(n, cl);
    }
    parsedChords.push_back(*pc);
    if (renderList.empty() || renderListStacked.empty() || renderListGenerated) {
        renderList = pc->renderList(cl, false);
        renderListStacked = pc->renderList(cl, true);
        renderListGenerated = true;
    }
    if (xmlKind.empty()) {
        xmlKind = pc->xmlKind();
        xmlDegrees = pc->xmlDegrees();
    }
    // these fields are not read from chord description files
    // so get them from the parsed representation in all cases
    xmlText = pc->xmlText();
    xmlSymbols = pc->xmlSymbols();
    xmlParens = pc->xmlParens();
    if (chord.getKeys() == 0) {
        chord = HChord(pc->keys());
    }
    quality_ = pc->quality();
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ChordDescription::read(XmlReader &e, int mscVersion) {
    int ni = 0;
    id = e.attribute("id").toInt();
    while (e.readNextStartElement()) {
        const AsciiStringView tag(e.name());
        if (tag == "name") {
            String n = e.readText();
            // stack names for this file on top of the list
            names.insert(ni++, n);
        } else if (tag == "xml") {
            xmlKind = e.readText();
        } else if (tag == "degree") {
            xmlDegrees.push_back(e.readText());
        } else if (tag == "voicing") {
            chord = HChord(e.readText());
        } else if (tag == "render") {
            readRenderList(e.readText(), renderList, mscVersion);
            renderListGenerated = false;
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void ChordDescription::write(XmlWriter &xml) const {
    if (generated && !exportOk) {
        return;
    }
    if (id > 0) {
        xml.startElement("chord", {{"id", id}});
    } else {
        xml.startElement("chord");
    }
    for (const String &s : names) {
        xml.tag("name", s);
    }
    xml.tag("xml", xmlKind);
    xml.tag("voicing", chord.voicing());
    for (const String &s : xmlDegrees) {
        xml.tag("degree", s);
    }
    writeRenderList(xml, renderList, "render");
    xml.endElement();
}

//---------------------------------------------------------
//   ChordList
//---------------------------------------------------------

int ChordList::privateID = -1000;

//---------------------------------------------------------
//   configureAutoAdjust
//---------------------------------------------------------

void ChordList::configureAutoAdjust(double emag, double eadjust, double mmag, double madjust, double stackedmmag, bool stackModifiers,
                                    bool excludeModsHAlign, String symbolFont, const ChordStylePreset &preset) {
    stackModifiers_ = stackModifiers;
    excludeModsHAlign_ = excludeModsHAlign;
    emag_ = emag;
    eadjust_ = eadjust;
    mmag_ = mmag;
    stackedmmag_ = stackedmmag;
    madjust_ = madjust;
    symbolTextFont_ = symbolFont;
    chordPreset_ = preset;
}

//---------------------------------------------------------
//   read
//---------------------------------------------------------

void ChordList::read(XmlReader &e, int mscVersion) {
    int fontIdx = static_cast<int>(fonts.size());
    autoAdjust_ = false;
    while (e.readNextStartElement()) {
        const AsciiStringView tag(e.name());
        if (tag == "font") {
            ChordFont f;
            f.family = e.attribute("family", u"default");
            if (f.family == u"MuseJazz") {
                f.family = u"MuseJazz Text";
            } else if (f.family == u"ScoreText") {
                f.family = symbolTextFont_;
                f.musicSymbolText = true;
            }
            f.mag = 1.0;
            f.fontClass = e.attribute("class");
            while (e.readNextStartElement()) {
                if (e.name() == "sym") {
                    ChordSymbol cs;
                    cs.fontIdx = fontIdx;
                    cs.name = e.attribute("name");
                    cs.value = e.attribute("value");
                    String code = e.attribute("code");
                    String symClass = e.attribute("class");
                    if (!code.empty()) {
                        bool ok = true;
                        char32_t val = code.toUInt(&ok, 0);
                        if (!ok && mscVersion >= 400 && mscVersion < 440) {
                            val = code.toUInt(&ok, 16);
                        }
                        if (!ok) {
                            cs.code = 0;
                            cs.value = code;
                        } else if (Char::requiresSurrogates(val)) {
                            cs.code = 0;
                            cs.value = String::fromUcs4(val);
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
                    cs.name = symClass + cs.name;
                    symbols_.insert({cs.name, cs});
                    e.readNext();
                } else if (e.name() == "mag") {
                    f.mag = e.readDouble();
                } else {
                    e.unknown();
                }
            }
            if (autoAdjust_) {
                if (f.fontClass == "extension") {
                    f.mag *= emag_;
                } else if (f.fontClass == "modifier") {
                    f.mag *= mmag_;
                }
            }
            fonts.push_back(f);
            ++fontIdx;
        } else if (tag == "autoAdjust") {
            String nmag = e.attribute("mag");
            if (!nmag.empty()) {
                nmag_ = nmag.toDouble();
            }
            String nadjust = e.attribute("adjust");
            if (!nadjust.empty()) {
                nadjust_ = nadjust.toDouble();
            }
            autoAdjust_ = e.readBool();
        } else if (tag == "token") {
            ChordToken t;
            t.read(e, mscVersion);
            chordTokenList.push_back(t);
        } else if (tag == "chord") {
            int id = e.intAttribute("id");
            // if no id attribute (id == 0), then assign it a private id
            // user chords that match these ChordDescriptions will be treated as normal recognized chords
            // except that the id will not be written to the score file
            ChordDescription cd = (id && muse::contains(*this, id)) ? muse::take(*this, id) : ChordDescription(id);

            // record updated id
            id = cd.id;
            // read rest of description
            cd.read(e, mscVersion);
            // restore updated id
            cd.id = id;
            // throw away previously parsed chords
            cd.parsedChords.clear();
            // generate any missing info (including new parsed chords)
            cd.complete(0, this);
            // add to list
            insert({id, cd});
        } else if (tag == "renderRoot") {
            readRenderList(e.readText(), renderListRoot, mscVersion);
        } else if (tag == "renderFunction") {
            readRenderList(e.readText(), renderListFunction, mscVersion);
        } else if ((tag == "renderBase" && mscVersion < 460) || tag == "renderBass") {
            readRenderList(e.readText(), renderListBass, mscVersion);
        } else if (tag == "renderBassOffset") {
            readRenderList(e.readText(), renderListBassOffset, mscVersion);
        } else {
            e.unknown();
        }
    }
}

//---------------------------------------------------------
//   write
//---------------------------------------------------------

void ChordList::write(XmlWriter &xml) const {
    int fontIdx = 0;
    for (const ChordFont &f : fonts) {
        xml.startElement("font", {{"id", fontIdx}, {"family", f.family}});
        xml.tag("mag", f.mag);
        for (const auto &p : symbols_) {
            const ChordSymbol &s = p.second;
            if (s.fontIdx == fontIdx) {
                if (s.code.isNull()) {
                    xml.tag("sym", {{"name", s.name}, {"value", s.value}});
                } else {
                    // write hex numbers with a "0x" prefix, so they can convert back properly on read
                    xml.tag("sym", {{"name", s.name}, {"code", u"0x" + String::number(s.code.unicode(), 16)}});
                }
            }
        }
        xml.endElement();
        ++fontIdx;
    }
    if (autoAdjust_) {
        xml.tag("autoAdjust", {{"mag", nmag_}, {"adjust", nadjust_}});
    }
    for (const ChordToken &t : chordTokenList) {
        t.write(xml);
    }
    if (!renderListRoot.empty()) {
        writeRenderList(xml, renderListRoot, "renderRoot");
    }
    if (!renderListFunction.empty()) {
        writeRenderList(xml, renderListFunction, "renderFunction");
    }
    if (!renderListBass.empty()) {
        writeRenderList(xml, renderListBass, "renderBass");
    }
    if (!renderListBassOffset.empty()) {
        writeRenderList(xml, renderListBassOffset, "renderBassOffset");
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
    muse::io::path_t path;
    FileInfo ftest(name);
    if (ftest.isAbsolute()) {
        path = name;
    } else {
        path = u":/engraving/styles/" + name;
    }

    // default to chords_std.xml
    if (!FileInfo::exists(path)) {
        path = u":/engraving/styles/chords_std.xml";
    }

    if (name.isEmpty()) {
        return false;
    }
    File f(path);
    if (!f.open(IODevice::ReadOnly)) {
        LOGE() << "Cannot open chord description: " << f.filePath();
        return false;
    }

    return read(&f);
}

bool ChordList::read(IODevice *device) {
    XmlReader e(device);

    while (e.readNextStartElement()) {
        if (e.name() == "museScore") {
            const String version = e.attribute("version");
            const StringList sl = version.split(u'.');
            const int mscVersion = sl.size() == 2 ? sl[0].toInt() * 100 + sl[1].toInt() : 0;
            read(e, mscVersion);
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

    if (info.suffix().isEmpty()) {
        String path = info.filePath();
        path += u".xml";
        info = FileInfo(path);
    }

    auto outBuf = Buffer::opened(IODevice::WriteOnly);
    write(&outBuf);
    outBuf.close();

    Ret ret = File::writeFile(info.filePath(), outBuf.data());
    if (!ret) {
        LOGE() << "failed to write chord list: " << ret.toString();
        return false;
    }

    return true;
}

bool ChordList::write(IODevice *device) const {
    XmlWriter xml(device);
    xml.startDocument();
    xml.startElement("museScore", {{"version", Constants::MSC_VERSION_STR}});

    write(xml);

    xml.endElement();

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
    return !renderListRoot.empty();
}

//---------------------------------------------------------
//   unload
//---------------------------------------------------------

void ChordList::unload() {
    clear();
    symbols_.clear();
    fonts.clear();
    renderListRoot.clear();
    renderListBass.clear();
    chordTokenList.clear();
    autoAdjust_ = false;
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
    for (const ChordToken &tok : chordTokenList) {
        if (tok.tokenClass != type || !tok.names.contains(s)) {
            continue;
        }

        return tok;
    }

    return ChordToken();
}

void ChordList::checkChordList(const MStyle &style) {
    // make sure we have a chordlist
    if (!loaded()) {
        double emag = style.styleD(Sid::chordExtensionMag);
        double eadjust = style.styleD(Sid::chordExtensionAdjust);
        double mmag = style.styleD(Sid::chordModifierMag);
        double madjust = style.styleD(Sid::chordModifierAdjust);
        double stackedmmag = style.styleD(Sid::chordStackedModifierMag);
        bool stackModifiers = style.styleB(Sid::verticallyStackModifiers);
        bool excludeModsHAlign = style.styleB(Sid::chordAlignmentExcludeModifiers);
        String symbolFont = style.styleSt(Sid::musicalTextFont);
        ChordStylePreset preset = style.styleV(Sid::chordStyle).value<ChordStylePreset>();
        configureAutoAdjust(emag, eadjust, mmag, madjust, stackedmmag, stackModifiers, excludeModsHAlign, symbolFont, preset);

        if (style.value(Sid::chordsXmlFile).toBool()) {
            read(u"chords.xml");
        }

        read(style.value(Sid::chordDescriptionFile).value<String>());
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
    RenderAction::print(actionType(), info);
}

void RenderActionSet::print() const {
    String info = String(u"<%1>").arg(text_);
    RenderAction::print(actionType(), info);
}

void RenderActionPop::print() const {
    String info = String(u"pop x: %1 pop y: %2").arg(popx_, popy_);
    RenderAction::print(actionType(), info);
}

void RenderActionScale::print() const {
    String info = String(u"%1").arg(scale_);
    RenderAction::print(actionType(), info);
}

void RenderActionMoveScaled::print() const {
    String info = String(u"SCALED %1 %2").arg(x(), y());
    RenderAction::print(actionType(), info);
}

void RenderActionMoveXHeight::print() const {
    String info = String(u"up: %1").arg(up_);
    RenderAction::print(actionType(), info);
}

void RenderActionParen::print() const {
    String info = String(u"direction: %1").arg(direction() == DirectionH::LEFT ? u"left" : u"right");
    RenderAction::print(actionType(), info);
}
} // namespace mu::engraving
