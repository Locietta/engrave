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

#include <vector>

#include "engravingitem.h"

#include "draw/types/font.h"

namespace muse::io {
struct path_t;
} // namespace muse::io

namespace mu::engraving {
class Chord;
class Factory;
class Harmony;
class StringData;
class Transaction;

// Keep this in order - not used directly for comparisons, but the dots will appear in
// this order in fret multidot mode. See fretproperties.cpp.
enum class FretDotType : signed char { NORMAL = 0, CROSS, SQUARE, TRIANGLE = 3 };

enum class FretMarkerType : signed char { NONE, CIRCLE, CROSS };

class FretItem {
public:
    struct Barre {
        int startString = -1;
        int endString = -1;

        Barre() = default;

        Barre(int s, int e) : startString(s), endString(e) {}

        bool exists() const { return startString > -1; }
    };

    struct Dot {
        int fret = 0;
        FretDotType dtype = FretDotType::NORMAL;
        int fingering = 0; // NOTE:JT - possible future feature?

        Dot() = default;

        Dot(int f, FretDotType t = FretDotType::NORMAL, bool isPartOfSlurBarre = false)
            : fret(f), dtype(t), isPartOfSlurBarre(isPartOfSlurBarre) {}

        bool exists() const { return fret > 0; }

        bool isPartOfSlurBarre = false;
    };

    struct Marker {
        FretMarkerType mtype = FretMarkerType::NONE;

        Marker() = default;

        Marker(FretMarkerType t) : mtype(t) {}

        bool exists() const { return mtype != FretMarkerType::NONE; }
    };

    struct MarkerTypeNameItem {
        FretMarkerType mtype = FretMarkerType::NONE;
        const char *name = nullptr;
    };

    struct DotTypeNameItem {
        FretDotType dtype = FretDotType::NORMAL;
        const char *name = nullptr;
    };

    static const std::vector<FretItem::MarkerTypeNameItem> markerTypeNameMap;
    static const std::vector<FretItem::DotTypeNameItem> dotTypeNameMap;

    static Char markerToChar(FretMarkerType t);
    static String markerTypeToName(FretMarkerType t);
    static FretMarkerType nameToMarkerType(String n);
    static String dotTypeToName(FretDotType t);
    static FretDotType nameToDotType(String n);
};

// The three main storage containers used by fret diagrams
typedef std::map<int, FretItem::Barre> BarreMap;
typedef std::map<int, FretItem::Marker> MarkerMap;
typedef std::map<int, std::vector<FretItem::Dot>> DotMap;

struct DiagramInfo {
    String harmonyName;
    String diagramXml;
    String diagramPattern;
};

//---------------------------------------------------------
//   @@ FretDiagram
///    Fretboard diagram
//
//   @P userMag    double
//   @P strings    int  number of strings
//   @P frets      int  number of frets
//   @P fretOffset int
//
//   Note that, while strings are zero-indexed, frets are one-indexed
//---------------------------------------------------------

class FretDiagram final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, FretDiagram)
    DECLARE_CLASSOF(ElementType::FRET_DIAGRAM)

public:

    ~FretDiagram();

    EngravingItem *linkedClone() override;

    FretDiagram *clone() const override { return new FretDiagram(*this); }

    Segment *segment() const;

    String patternFromDiagram() const;
    std::vector<String> harmoniesFromPattern(const String &pattern) const;
    std::vector<DiagramInfo> patternsFromHarmony(const String &harmonyName);

    void updateDiagram(const String &harmonyName);

    double mainWidth() const;

    int strings() const { return strings_; }

    int frets() const { return frets_; }

    void setStrings(int n);

    void setFrets(int n) { frets_ = n; }

    void setDot(int string, int fret, bool add = false, FretDotType dtype = FretDotType::NORMAL);
    void addDotForDotStyleBarre(int string, int fret);
    void removeDotForDotStyleBarre(int string, int fret);
    void setBarre(int startString, int endString, int fret);
    void setBarre(int string, int fret, bool add = false);
    void setMarker(int string, FretMarkerType marker);
    /*void setFingering(int string, int finger);*/

    void clear();
    bool isClear() const;

    void undoSetFretDot(int _string, int _fret, bool _add = false, FretDotType _dtype = FretDotType::NORMAL);
    void undoSetFretMarker(int _string, FretMarkerType _mtype);
    void undoSetFretBarre(int _string, int _fret, bool _add = false);
    void undoFretClear();

    int fretOffset() const { return fretOffset_; }

    void setFretOffset(int val) { fretOffset_ = val; }

    int maxFrets() const { return maxFrets_; }

    void setMaxFrets(int val) { maxFrets_ = val; }

    bool showNut() const { return showNut_; }

    void setShowNut(bool val) { showNut_ = val; }

    double userMag() const { return userMag_; }

    void setUserMag(double m) { userMag_ = m; }

    int numPos() const;

    Orientation orientation() const { return orientation_; }

    String harmonyPlainText() const;
    String harmonyDisplayText() const;

    Harmony *harmony() const { return harmony_; }

    void setHarmony(String harmonyText);

    std::vector<FretItem::Dot> dot(int s, int f = 0) const;
    FretItem::Marker marker(int s) const;
    FretItem::Barre barre(int fret) const;

    const BarreMap &barres() const { return barres_; }

    const DotMap &dots() const { return dots_; }

    const MarkerMap &markers() const { return markers_; }

    muse::draw::Font fretNumFont() const;
    muse::draw::Font fingeringFont() const;

    void init(StringData *, Chord *);
    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    bool acceptDrop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    void scanElements(std::function<void(EngravingItem *)> func) override;

    void undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    using EngravingObject::undoChangeProperty;
    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;

    void setTrack(track_idx_t val) override;

    bool showFingering() const { return showFingering_; }

    void setShowFingering(bool v) { showFingering_ = v; }

    const std::vector<int> &fingering() const { return fingering_; }

    void setFingering(std::vector<int> v);

    static FretDiagram *makeFromHarmonyOrFretDiagram(const EngravingItem *harmonyOrFretDiagram);

    bool isInFretBox() const;
    bool isCustom(const String &harmonyNameForCompare) const;

    bool allowTimeAnchor() const override { return ownershipParent() && parent()->isSegment(); }

    friend class FretUndoData;

    struct FingeringItem {
        String fingerNumber;
        PointF pos;

        FingeringItem(String s, PointF p) : fingerNumber(s), pos(p) {}
    };

    struct LayoutData : public EngravingItem::LayoutData {
        double stringLineWidth = 0.0;
        double nutLineWidth = 0.0;
        double nutY = 0.0;
        double stringDist = 0.0;
        double fretDist = 0.0;
        double markerSize = 0.0;
        double markerY = 0.0;
        double stringExtendTop = 0.0;
        double stringExtendBottom = 0.0;
        double dotDiameter = 0.0;
        double fretNumPadding = 0.0;
        double gridHeight = 0.0;
        std::vector<FingeringItem> fingeringItems;
        std::vector<PainterPath> slurPaths;
        String fretText = String();
    };
    DECLARE_LAYOUTDATA_METHODS(FretDiagram)

private:
    friend class Factory;

    FretDiagram(DummyParentOr<Segment> parent);
    FretDiagram(const FretDiagram &);

    void readHarmonyToDiagramFile(const muse::io::path_t &filePath) const;

    void initDefaultValues();

    void removeDot(int s, int f = 0);
    void removeBarre(int f);
    void removeBarres(int string, int fret = 0);
    void removeMarker(int s);
    void removeDotsMarkers(int ss, int es, int fret);

    static void applyDiagramPattern(FretDiagram *diagram, const String &pattern);

    int strings_ = 0;
    int frets_ = 0;
    int fretOffset_ = 0;
    int maxFrets_ = 0;
    bool showNut_ = false;
    Orientation orientation_ = Orientation::VERTICAL;

    // Barres are stored in the format: K: fret, V: barre struct
    BarreMap barres_;

    // Dots stored as K: string, V: dot struct
    DotMap dots_;

    // Markers stored as K: string, V: marker struct
    MarkerMap markers_;

    Harmony *harmony_ = nullptr;

    double userMag_ = 1.0; // allowed 0.1 - 10.0

    bool showFingering_ = false;
    std::vector<int> fingering_ = std::vector<int>(strings_, 0);
};
} // namespace mu::engraving
