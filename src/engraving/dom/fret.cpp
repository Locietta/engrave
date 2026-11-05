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

#include "fret.h"

#include "translation.h"

#include "io/file.h"

#include "../editing/addremoveelement.h"
#include "../editing/editfretboarddiagram.h"

#include "anchors.h"
#include "chord.h"
#include "factory.h"
#include "harmony.h"
#include "measure.h"
#include "note.h"
#include "pitchspelling.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stringdata.h"
#include "system.h"

#include "rw/harmonytodiagramreader.h"
#include "rw/read460/tread.h"

#include "log.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//    parent() is Segment or Box
//

//---------------------------------------------------------
//   fretStyle
//---------------------------------------------------------

static const ElementStyle fretStyle{
    {Sid::fretNumPos, Pid::FRET_NUM_POS},
    {Sid::fretMag, Pid::MAG},
    {Sid::fretPlacement, Pid::PLACEMENT},
    {Sid::fretStrings, Pid::FRET_STRINGS},
    {Sid::fretFrets, Pid::FRET_FRETS},
    {Sid::fretNut, Pid::FRET_NUT},
    {Sid::fretMinDistance, Pid::MIN_DISTANCE},
    {Sid::fretOrientation, Pid::ORIENTATION},
    {Sid::fretShowFingerings, Pid::FRET_SHOW_FINGERINGS},
};

//---------------------------------------------------------
//   FretDiagram
//---------------------------------------------------------

struct HarmonyMapKey {
    HarmonyMapKey() {}

    HarmonyMapKey(int k, int r, int b) : keys(k), rootTpc(r), bassTpc(b) {}

    int keys = 0;
    int rootTpc = Tpc::TPC_INVALID;
    int bassTpc = Tpc::TPC_INVALID;

    bool operator<(const HarmonyMapKey &other) const {
        return std::tie(keys, rootTpc, bassTpc) < std::tie(other.keys, other.rootTpc, other.bassTpc);
    }

    bool operator==(const HarmonyMapKey &other) const {
        return std::tie(keys, rootTpc, bassTpc) == std::tie(other.keys, other.rootTpc, other.bassTpc);
    }
};

static std::map<HarmonyMapKey /*key*/, std::vector<DiagramInfo>> s_harmonyToDiagramMap;
static std::unordered_map<String /*pattern*/, std::vector<String /*harmonyName*/>> s_diagramPatternToHarmoniesMap;

static const muse::io::path_t HARMONY_TO_DIAGRAM_FILE_PATH(":/data/harmony_to_diagram.xml");

static int canonicalEnharmonicTpc(int tpc) {
    // Normalize to one canonical TPC per pitch class
    int mod = (tpc - Tpc::TPC_G_B) % TPC_DELTA_ENHARMONIC;
    return Tpc::TPC_G_B + (mod < 0 ? mod + TPC_DELTA_ENHARMONIC : mod);
}

static HarmonyMapKey createHarmonyMapKey(const String &harmony, const NoteSpellingType &spellingType, const ChordList *cl) {
    String s = harmony;
    NoteCaseType noteCase;
    size_t idx;
    int rootTpc = convertNote(s, spellingType, noteCase, idx);

    int bassTpc = Tpc::TPC_INVALID;
    size_t slash = s.lastIndexOf(u'/');
    if (slash != muse::nidx) {
        String bs = s.mid(slash + 1).simplified();
        s = s.mid(idx, slash - idx).simplified();
        size_t idx2 = 0;
        bassTpc = convertNote(bs, spellingType, noteCase, idx2);

        if (!tpcIsValid(bassTpc)) {
            // if what follows after slash is not (just) a TPC
            // then reassemble chord and try to parse with the slash
            s = s + u"/" + bs;
        }
    } else {
        s = s.mid(idx);
    }

    ParsedChord chord;
    if (!chord.parse(s, cl)) {
        LOGE() << "Error parse " << harmony;
    }

    int keys = chord.keys();

    return HarmonyMapKey(keys, rootTpc, bassTpc);
}

static DiagramInfo resolveDiagram(const std::vector<DiagramInfo> &diagrams) {
    auto isSimpler = [](const DiagramInfo &a, const DiagramInfo &b) {
        const String &harmonyNameA = a.harmonyName;
        const String &harmonyNameB = b.harmonyName;

        if (harmonyNameA.size() != harmonyNameB.size()) {
            return harmonyNameA.size() < harmonyNameB.size();
        }

        return harmonyNameA < harmonyNameB;
    };

    DiagramInfo result = diagrams.front();
    for (size_t i = 1; i < diagrams.size(); ++i) {
        if (!isSimpler(result, diagrams[i])) {
            result = diagrams[i];
        }
    }

    return result;
}

FretDiagram::FretDiagram(DummyParentOr<Segment> parent)
    : EngravingItem(ElementType::FRET_DIAGRAM, parent, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    initDefaultValues();

    initElementStyle(&fretStyle);
}

FretDiagram::FretDiagram(const FretDiagram &f) : EngravingItem(f) {
    strings_ = f.strings_;
    frets_ = f.frets_;
    fretOffset_ = f.fretOffset_;
    maxFrets_ = f.maxFrets_;
    userMag_ = f.userMag_;
    dots_ = f.dots_;
    markers_ = f.markers_;
    barres_ = f.barres_;
    showNut_ = f.showNut_;
    orientation_ = f.orientation_;
    showFingering_ = f.showFingering_;
    fingering_ = f.fingering_;

    if (f.harmony_) {
        Harmony *h = new Harmony(*f.harmony_);
        add(h);
    }
}

void FretDiagram::initDefaultValues() {
    strings_ = 6;
    frets_ = 4;
    fretOffset_ = 0;
    maxFrets_ = 24;
    showNut_ = true;
    orientation_ = Orientation::VERTICAL;

    userMag_ = 1.0;

    showFingering_ = false;
    fingering_ = std::vector<int>(strings_, 0);
}

FretDiagram::~FretDiagram() {
    if (harmony_) {
        delete harmony_;
    }
}

//---------------------------------------------------------
//   linkedClone
//---------------------------------------------------------

EngravingItem *FretDiagram::linkedClone() {
    FretDiagram *e = clone();
    e->setAutoplace(true);
    if (harmony_) {
        EngravingItem *newHarmony = harmony_->linkedClone();
        e->add(newHarmony);
    }
    score()->undo(new Link(e, this));
    return e;
}

Segment *FretDiagram::segment() const {
    EngravingObject *parent = ownershipParent();
    if (!parent || !parent->isSegment()) {
        return nullptr;
    }

    return toSegment(ownershipParent());
}

void FretDiagram::updateDiagram(const String &harmonyName) {
    std::vector<DiagramInfo> availableDiagrams = patternsFromHarmony(harmonyName);
    if (availableDiagrams.empty()) {
        return;
    }

    String diagramXml = resolveDiagram(availableDiagrams).diagramXml;

    if (diagramXml.empty()) {
        return;
    }

    clear();

    read460::ReadContext ctx(score());
    XmlReader reader(diagramXml.toUtf8());

    read460::TRead::read(this, reader, ctx);

    triggerLayout();
    return;
}

double FretDiagram::mainWidth() const {
    double mainWidth = 0.0;
    if (orientation() == Orientation::VERTICAL) {
        mainWidth = ldata()->stringDist * (strings() - 1);
    } else if (orientation() == Orientation::HORIZONTAL) {
        mainWidth = ldata()->fretDist * frets();
    }
    return mainWidth;
}

//---------------------------------------------------------
//   setStrings
//---------------------------------------------------------

void FretDiagram::setStrings(int n) {
    int difference = n - strings_;
    if (difference == 0 || n <= 0) {
        return;
    }

    // Move all dots, makers, barres to the RIGHT, so we add strings to the left
    // This is more useful - few instruments need strings added to the right.
    DotMap tempDots;
    MarkerMap tempMarkers;

    for (int string = 0; string < strings_; ++string) {
        if (string + difference < 0) {
            continue;
        }

        for (auto const &d : dot(string)) {
            if (d.exists()) {
                tempDots[string + difference].push_back(FretItem::Dot(d));
            }
        }

        if (marker(string).exists()) {
            tempMarkers[string + difference] = marker(string);
        }
    }

    dots_ = tempDots;
    markers_ = tempMarkers;

    for (int fret = 1; fret <= frets_; ++fret) {
        if (barre(fret).exists()) {
            if (barres_[fret].startString + difference <= 0) {
                removeBarre(fret);
                continue;
            }

            barres_[fret].startString = std::max(0, barres_[fret].startString + difference);
            barres_[fret].endString = barres_[fret].endString == -1 ? -1 : barres_[fret].endString + difference;
        }
    }

    if (difference > 0) {
        for (int i = 0; i < difference; ++i) {
            fingering_.push_back(0);
        }
    } else {
        fingering_.erase(fingering_.end() + difference, fingering_.end());
    }

    strings_ = n;
}

//---------------------------------------------------------
//   init
//---------------------------------------------------------

void FretDiagram::init(StringData *stringData, Chord *chord) {
    if (!stringData) {
        setStrings(6);
    } else {
        setStrings(static_cast<int>(stringData->strings()));
    }
    if (stringData) {
        for (int string = 0; string < strings_; ++string) {
            setMarker(string, FretMarkerType::CROSS);
        }
        for (const Note *note : chord->notes()) {
            int string;
            int fret;
            if (stringData->convertPitch(note->pitch(), chord->staff(), &string, &fret)) {
                setDot(string, fret);
            }
        }
        frets_ = stringData->frets();
    } else {
        maxFrets_ = 6;
    }
}

//---------------------------------------------------------
//   setDot
//    take a fret value of 0 to mean remove the dot, except with add
//    where we actually need to pass a fret val.
//---------------------------------------------------------

void FretDiagram::setDot(int string, int fret, bool add /*= false*/, FretDotType dtype /*= FretDotType::NORMAL*/) {
    if (fret == 0) {
        removeDot(string, fret);
    } else if (string >= 0 && string < strings_) {
        // Special case - with add, if there is a dot in the position, remove it
        // If not, add it.
        if (add) {
            if (dot(string, fret)[0].exists()) {
                removeDot(string, fret);
                return; // We are done here, all we needed to do was remove a single dot
            }
        } else {
            dots_[string].clear();
        }

        dots_[string].push_back(FretItem::Dot(fret, dtype));
        if (!add) {
            setMarker(string, FretMarkerType::NONE);
        }
    }
}

void FretDiagram::addDotForDotStyleBarre(int string, int fret) {
    if (dots_[string].empty()) {
        dots_[string].push_back(FretItem::Dot(fret, FretDotType::NORMAL, /*isPartOfSlurBarre*/ true));
    }
}

void FretDiagram::removeDotForDotStyleBarre(int string, int fret) { removeDot(string, fret); }

//---------------------------------------------------------
//   setMarker
//    Removal of dots and barres if "Multiple dots" is inactive
//    is handled in FretCanvas::mousePressEvent()
//---------------------------------------------------------

void FretDiagram::setMarker(int string, FretMarkerType mtype) {
    if (string >= 0 && string < strings_) {
        markers_[string] = FretItem::Marker(mtype);
        if (mtype != FretMarkerType::NONE) {
            removeDot(string);
            removeBarres(string);
        }
    }
}

//---------------------------------------------------------
//   setBarre
//    We'll accept a value of -1 for the end string, to denote
//    that the barre goes as far right as possible.
//    Take a start string value of -1 to mean 'remove this barre'
//---------------------------------------------------------

void FretDiagram::setBarre(int startString, int endString, int fret) {
    if (startString == -1) {
        removeBarre(fret);
    } else if (startString >= 0 && endString >= -1 && startString < strings_ && endString < strings_) {
        barres_[fret] = FretItem::Barre(startString, endString);
    }
}

//---------------------------------------------------------
//    This version is for clicks on a dot with shift.
//    If there is no barre at fret, then add one with the string as the start.
//    If there is a barre with a -1 end string, set the end string to string.
//    If there is a barre with a set start and end, remove it.
//    Add may be used in the future if we decide to add dots as default with barres.
//---------------------------------------------------------

void FretDiagram::setBarre(int string, int fret, bool add /*= false*/) {
    UNUSED(add);

    FretItem::Barre b = barre(fret);
    if (!b.exists()) {
        if (string < strings_ - 1) {
            barres_[fret] = FretItem::Barre(string, -1);
            removeDotsMarkers(string, -1, fret);
        }
    } else if (b.endString == -1 && b.startString < string) {
        barres_[fret].endString = string;
    } else {
        removeDotsMarkers(b.startString, b.endString, fret);
        removeBarre(fret);
    }
}

//---------------------------------------------------------
//   undoSetFretDot
//---------------------------------------------------------

void FretDiagram::undoSetFretDot(int _string, int _fret, bool _add /*= true*/, FretDotType _dtype /*= FretDotType::NORMAl*/) {
    for (EngravingObject *e : linkList()) {
        FretDiagram *fd = toFretDiagram(e);
        fd->score()->undo(new FretDot(fd, _string, _fret, _add, _dtype));
    }
}

//---------------------------------------------------------
//   undoSetFretMarker
//---------------------------------------------------------

void FretDiagram::undoSetFretMarker(int _string, FretMarkerType _mtype) {
    for (EngravingObject *e : linkList()) {
        FretDiagram *fd = toFretDiagram(e);
        fd->score()->undo(new FretMarker(fd, _string, _mtype));
    }
}

//---------------------------------------------------------
//   undoSetFretBarre
//    add refers to using multiple dots per string when adding dots automatically
//---------------------------------------------------------

void FretDiagram::undoSetFretBarre(int _string, int _fret, bool _add /*= false*/) {
    for (EngravingObject *e : linkList()) {
        FretDiagram *fd = toFretDiagram(e);
        fd->score()->undo(new FretBarre(fd, _string, _fret, _add));
    }
}

//---------------------------------------------------------
//   removeBarre
//    Remove a barre on a given fret.
//---------------------------------------------------------

void FretDiagram::removeBarre(int f) { barres_.erase(f); }

//---------------------------------------------------------
//   removeBarres
//    Remove barres crossing a certain point. Fret of 0 means any point along
//    the string.
//---------------------------------------------------------

void FretDiagram::removeBarres(int string, int fret /*= 0*/) {
    auto iter = barres_.begin();
    while (iter != barres_.end()) {
        int bfret = iter->first;
        FretItem::Barre b = iter->second;

        if (b.exists() && b.startString <= string && (b.endString >= string || b.endString == -1)) {
            if (fret > 0 && fret != bfret) {
                ++iter;
            } else {
                iter = barres_.erase(iter);
            }
        } else {
            ++iter;
        }
    }
}

//---------------------------------------------------------
//   removeMarker
//---------------------------------------------------------

void FretDiagram::removeMarker(int s) {
    auto it = markers_.find(s);
    markers_.erase(it);
}

//---------------------------------------------------------
//   removeDot
//    take a fret value of 0 to mean remove all dots
//---------------------------------------------------------

void FretDiagram::removeDot(int s, int f /*= 0*/) {
    if (f > 0) {
        std::vector<FretItem::Dot> tempDots;
        for (auto const &d : dot(s)) {
            if (d.exists() && d.fret != f) {
                tempDots.push_back(FretItem::Dot(d));
            }
        }

        dots_[s] = tempDots;
    } else {
        dots_[s].clear();
    }

    if (dots_[s].size() == 0) {
        auto it = dots_.find(s);
        dots_.erase(it);
    }
}

//---------------------------------------------------------
//   removeDotsMarkers
//    removes all markers between [ss, es] and dots between [ss, es],
//    where the dots have a fret of fret.
//---------------------------------------------------------

void FretDiagram::removeDotsMarkers(int ss, int es, int fret) {
    if (ss == -1) {
        return;
    }

    int end = es == -1 ? strings_ : es;
    for (int string = ss; string <= end; ++string) {
        removeDot(string, fret);

        if (marker(string).exists()) {
            removeMarker(string);
        }
    }
}

//---------------------------------------------------------
//   Fill diagram from string like "XO-[1-O][1-X,2-O,3-S][3-T];B1[0-5];B..."
//   - Each character or bracketed block represents a string, from lowest to highest.
//   - 'X' = muted string (cross marker)
//   - 'O' = open string (circle marker)
//   - '-' = empty or unused string
//   - [fret-type,...] = one or more fretted dots on that string:
//       • fret is absolute (already includes offset)
//       • type is:
//           - O = circle
//           - X = cross
//           - S = square
//           - T = triangle
//   - Example: [1-X,2-O,3-S] means three dots on frets 1, 2, and 3 with different types
//   - Barre chords follow after ';', in the format: B{fret}[{start}-{end}]
//     e.g. B1[0-5] = barre on fret 1 from string 0 to 5
//---------------------------------------------------------

void FretDiagram::applyDiagramPattern(FretDiagram *diagram, const String &pattern) {
    diagram->clear();

    const std::vector<String> parts = pattern.split(';');
    if (parts.empty()) {
        return;
    }

    const String &mainPart = parts[0];
    std::vector<String> stringTokens;

    for (size_t i = 0; i < mainPart.size();) {
        if (mainPart[i] == u'[') {
            size_t rb = mainPart.indexOf(u']', i);
            if (rb != muse::nidx) {
                stringTokens.push_back(mainPart.mid(i, rb - i + 1));
                i = rb + 1;
                continue;
            }
        }
        stringTokens.push_back(mainPart.mid(i, 1));
        ++i;
    }

    const int stringCount = static_cast<int>(stringTokens.size());
    diagram->setStrings(stringCount);
    diagram->setFrets(4);
    diagram->setPropertyFlags(Pid::FRET_STRINGS, PropertyFlags::UNSTYLED);
    diagram->setPropertyFlags(Pid::FRET_FRETS, PropertyFlags::UNSTYLED);

    int offset = 0;

    for (int i = 0; i < stringCount; ++i) {
        const String &token = stringTokens[i];

        if (token == u"-") {
            continue;
        }

        if (token.startsWith(u"[") && token.endsWith(u"]")) {
            // Example: [1-O,2-X,3-S]
            String inner = token.mid(1, token.size() - 2);
            std::vector<String> pairs = inner.split(u',');

            for (const String &p : pairs) {
                size_t dash = p.indexOf(u'-');
                if (dash > 0) {
                    int fret = p.left(dash).toInt();
                    Char typeChar = p.mid(dash + 1, 1).at(0);
                    FretDotType dt;

                    switch (typeChar.unicode()) {
                        case 'X': dt = FretDotType::CROSS; break;
                        case 'O': dt = FretDotType::NORMAL; break;
                        case 'S': dt = FretDotType::SQUARE; break;
                        case 'T': dt = FretDotType::TRIANGLE; break;
                        default: dt = FretDotType::NORMAL; break;
                    }

                    diagram->setDot(i, fret - offset, true, dt);
                    if (fret - 3 > 0 && offset < fret - 3) {
                        offset = fret - 3;
                    }
                }
            }
        } else if (token == u"X") {
            diagram->setMarker(i, FretMarkerType::CROSS);
        } else if (token == u"O") {
            diagram->setMarker(i, FretMarkerType::CIRCLE);
        }
    }

    if (offset > 0) {
        diagram->setFretOffset(offset);
    }

    for (size_t i = 1; i < parts.size(); ++i) {
        const String &barrePart = parts[i];
        if (!barrePart.startsWith(u'B')) {
            continue;
        }

        size_t lb = barrePart.indexOf(u'[');
        size_t rb = barrePart.indexOf(u']');
        if (lb < 2 || rb < lb) {
            continue;
        }

        int fret = barrePart.mid(1, lb - 1).toInt();
        if (fret <= 0) {
            continue;
        }

        String range = barrePart.mid(lb + 1, rb - lb - 1);
        size_t dash = range.indexOf('-');
        if (dash > 0) {
            int start = range.left(dash).toInt();
            int end = range.mid(dash + 1).toInt();
            if (start >= 0 && end >= start && end < stringCount) {
                diagram->setBarre(start, end, fret - offset);
            }
        }
    }
}

String FretDiagram::patternFromDiagram() const {
    const int diagramStrings = strings();
    const int offset = fretOffset();

    StringList patternParts;
    const DotMap &dotsMap = dots();

    for (int i = 0; i < diagramStrings; ++i) {
        const FretItem::Marker fretMarker = marker(i);

        if (fretMarker.mtype == FretMarkerType::CROSS) {
            patternParts.push_back(u"X");
            continue;
        } else if (fretMarker.mtype == FretMarkerType::CIRCLE) {
            patternParts.push_back(u"O");
            continue;
        }

        const auto it = dotsMap.find(i);
        std::vector<FretItem::Dot> dotList;
        if (it != dotsMap.end()) {
            const std::vector<FretItem::Dot> &dots = it->second;
            for (const FretItem::Dot &dot : dots) {
                if (!dot.isPartOfSlurBarre) { // Don't write dot if part of slur barré
                    dotList.push_back(dot);
                }
            }
        }

        if (!dotList.empty()) {
            const auto &dotList2 = it->second;
            StringList dotDescriptions;
            for (const auto &dot : dotList2) {
                if (dot.isPartOfSlurBarre) {
                    continue;
                }

                int actualFret = dot.fret + offset;
                Char typeChar = u'O';

                switch (dot.dtype) {
                    case FretDotType::NORMAL: typeChar = u'O'; break;
                    case FretDotType::CROSS: typeChar = u'X'; break;
                    case FretDotType::SQUARE: typeChar = u'S'; break;
                    case FretDotType::TRIANGLE: typeChar = u'T'; break;
                    default: break;
                }

                dotDescriptions.push_back(String::number(actualFret) + u'-' + typeChar);
            }

            patternParts.push_back(u'[' + dotDescriptions.join(u",") + u']');
        } else {
            patternParts.push_back(u"-");
        }
    }

    String pattern = patternParts.join(u"");

    const BarreMap &_barres = barres();
    StringList barreParts;
    for (const auto &[fret, b] : _barres) {
        if (!b.exists()) {
            continue;
        }

        int adjustedFret = fret + offset;
        int start = b.startString;
        int end = (b.endString != -1) ? b.endString : diagramStrings - 1;

        barreParts.push_back(u'B' + String::number(adjustedFret) + u'[' + String::number(start) + u'-' + String::number(end) + u']');
    }

    if (!barreParts.empty()) {
        pattern += u';' + barreParts.join(u";");
    }

    return pattern;
}

std::vector<String> FretDiagram::harmoniesFromPattern(const String &pattern) const {
    if (s_diagramPatternToHarmoniesMap.empty()) {
        readHarmonyToDiagramFile(HARMONY_TO_DIAGRAM_FILE_PATH);
    }
    return muse::value(s_diagramPatternToHarmoniesMap, pattern);
}

std::vector<DiagramInfo> FretDiagram::patternsFromHarmony(const String &harmonyName) {
    if (s_harmonyToDiagramMap.empty()) {
        readHarmonyToDiagramFile(HARMONY_TO_DIAGRAM_FILE_PATH);
    }

    NoteSpellingType spellingType = style().styleV(Sid::chordSymbolSpelling).value<NoteSpellingType>();
    HarmonyMapKey key = createHarmonyMapKey(harmonyName, spellingType, score()->chordList());

    if (tpcIsValid(key.rootTpc)) {
        key.rootTpc = canonicalEnharmonicTpc(key.rootTpc);
    }
    if (tpcIsValid(key.bassTpc)) {
        key.bassTpc = canonicalEnharmonicTpc(key.bassTpc);
    }

    return muse::value(s_harmonyToDiagramMap, key);
}

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void FretDiagram::clear() {
    barres_.clear();
    dots_.clear();
    markers_.clear();
    fretOffset_ = 0;
}

bool FretDiagram::isClear() const { return barres_.empty() && dots_.empty() && markers_.empty(); }

//---------------------------------------------------------
//   undoFretClear
//---------------------------------------------------------

void FretDiagram::undoFretClear() {
    for (EngravingObject *e : linkList()) {
        FretDiagram *fd = toFretDiagram(e);
        fd->score()->undo(new FretClear(fd));
    }
}

int FretDiagram::numPos() const { return style().styleI(Sid::fretNumPos); }

//---------------------------------------------------------
//   dot
//    take fret value of zero to mean all dots
//---------------------------------------------------------

std::vector<FretItem::Dot> FretDiagram::dot(int s, int f /*= 0*/) const {
    if (dots_.find(s) != dots_.end()) {
        if (f != 0) {
            for (auto const &d : dots_.at(s)) {
                if (d.fret == f) {
                    return std::vector<FretItem::Dot>{FretItem::Dot(d)};
                }
            }
        } else {
            return dots_.at(s);
        }
    }
    return std::vector<FretItem::Dot>{FretItem::Dot(0)};
}

//---------------------------------------------------------
//   marker
//---------------------------------------------------------

FretItem::Marker FretDiagram::marker(int s) const {
    if (markers_.find(s) != markers_.end()) {
        return markers_.at(s);
    }
    return FretItem::Marker(FretMarkerType::NONE);
}

//---------------------------------------------------------
//   barre
//---------------------------------------------------------

FretItem::Barre FretDiagram::barre(int f) const {
    if (barres_.find(f) != barres_.end()) {
        return barres_.at(f);
    }
    return FretItem::Barre(-1, -1);
}

Font FretDiagram::fretNumFont() const {
    const MStyle &st = style();
    Font f(st.styleSt(Sid::fretDiagramFretNumberFontFace), Font::Type::Text);
    f.setPointSizeF(st.styleD(Sid::fretDiagramFretNumberFontSize) * userMag());
    FontStyle fStyle = st.styleV(Sid::fretDiagramFretNumberFontStyle).value<FontStyle>();
    f.setBold(fStyle & FontStyle::Bold);
    f.setItalic(fStyle & FontStyle::Italic);
    f.setUnderline(fStyle & FontStyle::Underline);
    f.setStrike(fStyle & FontStyle::Strike);
    return f;
}

Font FretDiagram::fingeringFont() const {
    const MStyle &st = style();
    Font f(st.styleSt(Sid::fretDiagramFingeringFontFace), Font::Type::Text);
    f.setPointSizeF(st.styleD(Sid::fretDiagramFingeringFontSize) * userMag());
    FontStyle fStyle = st.styleV(Sid::fretDiagramFingeringFontStyle).value<FontStyle>();
    f.setBold(fStyle & FontStyle::Bold);
    f.setItalic(fStyle & FontStyle::Italic);
    f.setUnderline(fStyle & FontStyle::Underline);
    f.setStrike(fStyle & FontStyle::Strike);
    return f;
}

String FretDiagram::harmonyPlainText() const { return harmony_ ? harmony_->plainText() : String(); }

String FretDiagram::harmonyDisplayText() const { return harmony_ ? harmony_->displayText() : String(); }

//---------------------------------------------------------
//   setHarmony
///   if this is being done by the user, use undoSetHarmony instead
//---------------------------------------------------------

void FretDiagram::setHarmony(String harmonyText) {
    if (!harmony_) {
        Harmony *h = new Harmony(this);
        add(h);
    }

    harmony_->setHarmony(harmonyText);
    harmony_->setXmlText(harmony_->harmonyName());
    triggerLayout();
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void FretDiagram::add(EngravingItem *e) {
    e->setOwnershipParent(this);
    if (e->isHarmony()) {
        harmony_ = toHarmony(e);

        harmony_->setTrack(track());

        if (harmony_->harmonyName().empty()) {
            if (s_diagramPatternToHarmoniesMap.empty()) {
                readHarmonyToDiagramFile(HARMONY_TO_DIAGRAM_FILE_PATH);
            }

            String pattern = patternFromDiagram();
            if (!pattern.empty()) {
                std::vector<String> matchedHarmonies = muse::value(s_diagramPatternToHarmoniesMap, pattern);
                if (!matchedHarmonies.empty()) {
                    harmony_->setHarmony(matchedHarmonies.front());
                }
            }
        }

        e->added();
    } else {
        LOGW("FretDiagram: cannot add <%s>\n", e->typeName());
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void FretDiagram::remove(EngravingItem *e) {
    if (e == harmony_) {
        harmony_ = nullptr;
        e->removed();
    } else {
        LOGW("FretDiagram: cannot remove <%s>\n", e->typeName());
    }
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool FretDiagram::acceptDrop(EditData &data) const { return data.dropElement->isHarmony(); }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *FretDiagram::drop(Transaction &, EditData &data) {
    EngravingItem *e = data.dropElement;
    if (e->isHarmony()) {
        Harmony *h = toHarmony(e);
        h->setOwnershipParent(ownershipParent());
        h->setTrack(track());
        score()->undoAddElement(h);
    } else {
        LOGW("FretDiagram: cannot drop <%s>\n", e->typeName());
        delete e;
        e = 0;
    }
    return e;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void FretDiagram::scanElements(std::function<void(EngravingItem *)> func) {
    func(this);

    // don't display harmony in palette
    if (harmony_) {
        func(harmony_);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

void FretDiagram::undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::EXCLUDE_VERTICAL_ALIGN) {
        EngravingItem::undoChangeProperty(id, v, ps);

        bool val = v.toBool();
        Harmony *h = harmony();
        if (h && h->excludeVerticalAlign() != val) {
            h->undoChangeProperty(Pid::EXCLUDE_VERTICAL_ALIGN, val, ps);
        }
        Segment *parentSeg = segment();
        if (!parentSeg) {
            return;
        }

        for (EngravingItem *item : parentSeg->annotations()) {
            if ((!item->isFretDiagram() && !item->isHarmony()) || item == this || track2staff(item->track()) != staffIdx()) {
                continue;
            }

            if (item->excludeVerticalAlign() != val) {
                item->undoChangeProperty(Pid::EXCLUDE_VERTICAL_ALIGN, val, ps);
            }
        }
        return;
    }

    EngravingItem::undoChangeProperty(id, v, ps);
}

PropertyValue FretDiagram::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::MAG: return userMag();
        case Pid::FRET_STRINGS: return strings();
        case Pid::FRET_FRETS: return frets();
        case Pid::FRET_NUT: return showNut();
        case Pid::FRET_OFFSET: return fretOffset();
        case Pid::ORIENTATION: return orientation_;
        case Pid::FRET_SHOW_FINGERINGS: return showFingering_;
        case Pid::FRET_FINGERING: return fingering_;
        default: return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool FretDiagram::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::MAG: setUserMag(v.toDouble()); break;
        case Pid::FRET_STRINGS: setStrings(v.toInt()); break;
        case Pid::FRET_FRETS: setFrets(v.toInt()); break;
        case Pid::FRET_NUT: setShowNut(v.toBool()); break;
        case Pid::FRET_OFFSET: setFretOffset(v.toInt()); break;
        case Pid::ORIENTATION: orientation_ = v.value<Orientation>(); break;
        case Pid::FRET_SHOW_FINGERINGS: setShowFingering(v.toBool()); break;
        case Pid::FRET_FINGERING: setFingering(v.value<std::vector<int>>()); break;
        case Pid::EXCLUDE_VERTICAL_ALIGN: {
            setExcludeVerticalAlign(v.toBool());
            break;
        }
        default: return EngravingItem::setProperty(propertyId, v);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue FretDiagram::propertyDefault(Pid pid) const {
    // We shouldn't style the fret offset
    if (pid == Pid::FRET_OFFSET) {
        return PropertyValue(0);
    } else if (pid == Pid::FRET_FINGERING) {
        return std::vector<int>(strings_, 0);
    }

    for (const StyledProperty &p : *styledProperties()) {
        if (p.pid == pid) {
            if (propertyType(pid) == P_TYPE::ABSOLUTE) {
                return style().styleAbsolute(p.sid);
            }
            return style().styleV(p.sid);
        }
    }
    return EngravingItem::propertyDefault(pid);
}

void FretDiagram::setTrack(track_idx_t val) {
    EngravingItem::setTrack(val);

    if (harmony_) {
        harmony_->setTrack(val);
    }
}

//---------------------------------------------------------
//   screenReaderInfo
//---------------------------------------------------------

void FretDiagram::setFingering(std::vector<int> v) { fingering_ = std::move(v); }

FretDiagram *FretDiagram::makeFromHarmonyOrFretDiagram(const EngravingItem *harmonyOrFretDiagram) {
    IF_ASSERT_FAILED(!harmonyOrFretDiagram || !harmonyOrFretDiagram->isHarmony() || !harmonyOrFretDiagram->isFretDiagram()) {
        return nullptr;
    }

    FretDiagram *fretDiagram = nullptr;

    if (harmonyOrFretDiagram->isHarmony() && !harmonyOrFretDiagram->ownershipParent()->isFretDiagram()) {
        Harmony *harmony = toHarmony(harmonyOrFretDiagram)->clone();

        fretDiagram = Factory::createFretDiagram(harmonyOrFretDiagram->score()->dummy());

        fretDiagram->updateDiagram(harmony->plainText());

        harmony->resetProperty(Pid::OFFSET);
        harmony->setProperty(Pid::ALIGN, Align(AlignH::HCENTER, AlignV::BASELINE));
        harmony->setPropertyFlags(Pid::ALIGN, PropertyFlags::UNSTYLED);

        fretDiagram->add(harmony);
    } else if (harmonyOrFretDiagram->isHarmony() && harmonyOrFretDiagram->ownershipParent()->isFretDiagram()) {
        fretDiagram = toFretDiagram(harmonyOrFretDiagram->ownershipParent())->clone();
    } else if (harmonyOrFretDiagram->isFretDiagram()) {
        fretDiagram = toFretDiagram(harmonyOrFretDiagram)->clone();
        fretDiagram->setOffset(PointF());
        if (!fretDiagram->harmony()) {
            //! generate from diagram and add harmony
            fretDiagram->add(Factory::createHarmony(fretDiagram));
        }
    }

    return fretDiagram;
}

bool FretDiagram::isInFretBox() const {
    EngravingObject *parent = ownershipParent();
    return parent ? parent->isFBox() : false;
}

bool FretDiagram::isCustom(const String &harmonyNameForCompare) const {
    if (harmonyNameForCompare.empty()) {
        return false;
    }

    static constexpr std::array props{Pid::FRET_STRINGS, Pid::FRET_FRETS, Pid::FRET_NUT, Pid::FRET_FINGERING};

    for (const Pid &pid : props) {
        if (pid == Pid::FRET_FINGERING) {
            if (custom(Pid::FRET_SHOW_FINGERINGS)) {
                return true;
            }
        } else if (custom(pid)) {
            return true;
        }
    }

    if (s_harmonyToDiagramMap.empty()) {
        readHarmonyToDiagramFile(HARMONY_TO_DIAGRAM_FILE_PATH);
    }

    NoteSpellingType spellingType = style().styleV(Sid::chordSymbolSpelling).value<NoteSpellingType>();
    HarmonyMapKey key = createHarmonyMapKey(harmonyNameForCompare, spellingType, score()->chordList());

    if (tpcIsValid(key.rootTpc)) {
        key.rootTpc = canonicalEnharmonicTpc(key.rootTpc);
    }
    if (tpcIsValid(key.bassTpc)) {
        key.bassTpc = canonicalEnharmonicTpc(key.bassTpc);
    }

    std::vector<DiagramInfo> availableDiagrams = muse::value(s_harmonyToDiagramMap, key);
    if (availableDiagrams.empty()) {
        return true;
    }

    String currentPattern = patternFromDiagram();

    for (const DiagramInfo &diagram : availableDiagrams) {
        if (diagram.diagramPattern == currentPattern) {
            return false;
        }
    }

    return true;
}

void FretDiagram::readHarmonyToDiagramFile(const muse::io::path_t &filePath) const {
    TRACEFUNC;

    muse::io::File file(filePath);
    if (!file.open()) {
        LOGE() << file.errorString();
        return;
    }

    XmlReader reader(&file);

    std::unordered_map<String, rw::HarmonyToDiagramReader::FretDiagramInfo> harmonyToDiagramMap = rw::HarmonyToDiagramReader::read(reader);

    const ChordList *chordList = score()->chordList();
    const NoteSpellingType spellingType = NoteSpellingType::STANDARD;

    for (auto &[key, value] : harmonyToDiagramMap) {
        HarmonyMapKey mapKey = createHarmonyMapKey(key, spellingType, chordList);

        if (tpcIsValid(mapKey.rootTpc)) {
            mapKey.rootTpc = canonicalEnharmonicTpc(mapKey.rootTpc);
        }
        if (tpcIsValid(mapKey.bassTpc)) {
            mapKey.bassTpc = canonicalEnharmonicTpc(mapKey.bassTpc);
        }

        s_harmonyToDiagramMap[mapKey].push_back({key, value.xml, value.pattern});
        s_diagramPatternToHarmoniesMap[value.pattern].push_back(key);
    }
}

//---------------------------------------------------------
//   markerToChar
//---------------------------------------------------------

Char FretItem::markerToChar(FretMarkerType t) {
    switch (t) {
        case FretMarkerType::CIRCLE: return Char(u'O');
        case FretMarkerType::CROSS: return Char(u'X');
        case FretMarkerType::NONE:
        default: return Char();
    }
}

//---------------------------------------------------------
//   markerTypeToName
//---------------------------------------------------------

const std::vector<FretItem::MarkerTypeNameItem> FretItem::markerTypeNameMap = {
    {FretMarkerType::CIRCLE, "circle"}, {FretMarkerType::CROSS, "cross"}, {FretMarkerType::NONE, "none"}};

String FretItem::markerTypeToName(FretMarkerType t) {
    for (auto i : FretItem::markerTypeNameMap) {
        if (i.mtype == t) {
            return String::fromAscii(i.name);
        }
    }

    ASSERT_X("Unrecognised FretMarkerType!");
    return String(); // prevent compiler warnings
}

//---------------------------------------------------------
//   nameToMarkerType
//---------------------------------------------------------

FretMarkerType FretItem::nameToMarkerType(String n) {
    for (auto i : FretItem::markerTypeNameMap) {
        if (String::fromAscii(i.name) == n) {
            return i.mtype;
        }
    }
    LOGW("Unrecognised marker name!");
    return FretMarkerType::NONE; // default
}

//---------------------------------------------------------
//   dotTypeToName
//---------------------------------------------------------

const std::vector<FretItem::DotTypeNameItem> FretItem::dotTypeNameMap = {
    {FretDotType::NORMAL, "normal"},
    {FretDotType::CROSS, "cross"},
    {FretDotType::SQUARE, "square"},
    {FretDotType::TRIANGLE, "triangle"},
};

String FretItem::dotTypeToName(FretDotType t) {
    for (auto i : FretItem::dotTypeNameMap) {
        if (i.dtype == t) {
            return String::fromAscii(i.name);
        }
    }

    ASSERT_X("Unrecognised FretDotType!");
    return String(); // prevent compiler warnings
}

//---------------------------------------------------------
//   nameToDotType
//---------------------------------------------------------

FretDotType FretItem::nameToDotType(String n) {
    for (auto i : FretItem::dotTypeNameMap) {
        if (String::fromAscii(i.name) == n) {
            return i.dtype;
        }
    }
    LOGW("Unrecognised dot name!");
    return FretDotType::NORMAL; // default
}
} // namespace mu::engraving
