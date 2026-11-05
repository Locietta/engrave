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

#ifndef MU_ENGRAVING_ARTICULATION_H
#define MU_ENGRAVING_ARTICULATION_H

#include <set>

#include "chordrest.h"
#include "engravingitem.h"

namespace mu::engraving {
class Factory;

class ChordRest;
class Measure;
class Page;
class Segment;
class System;

//---------------------------------------------------------
//   ArticulationInfo
//    gives infos about note attributes
//---------------------------------------------------------

enum class ArticulationCategory : unsigned char {
    NONE = 0x0,
    DOUBLE = 0x1,
    TENUTO = 0x2,
    STACCATO = 0x4,
    ACCENT = 0x8,
    MARCATO = 0x10,
    LUTE_FINGERING = 0x20,
    LAISSEZ_VIB = 0x40,
    HANDBELLS = 0x80,
};
DECLARE_FLAGS(ArticulationCategories, ArticulationCategory)
DECLARE_OPERATORS_FOR_FLAGS(ArticulationCategories)

enum class ArticulationAnchor : char {
    TOP,    // attribute is always placed at top
    BOTTOM, // attribute is placed at bottom
    AUTO,   // anchor depends on chord direction, away from stem
};

enum class ArticulationStemSideAlign : char {
    // horizontal align for stem-side articulation layout
    STEM,     // attribute is placed directly over the stem
    NOTEHEAD, // attribute is centered on the notehead
    AVERAGE,  // attribute is placed at the average of stem pos and notehead center
};

// flags:
enum class ArticulationShowIn : char { PITCHED_STAFF = 1, TABLATURE = 2 };

constexpr ArticulationShowIn operator|(ArticulationShowIn a1, ArticulationShowIn a2) {
    return static_cast<ArticulationShowIn>(static_cast<unsigned char>(a1) | static_cast<unsigned char>(a2));
}

constexpr bool operator&(ArticulationShowIn a1, ArticulationShowIn a2) {
    return static_cast<unsigned char>(a1) & static_cast<unsigned char>(a2);
}

enum class ArticulationsUpdateMode : unsigned char { Insert, Remove };

std::set<SymId> updateArticulations(const std::set<SymId> &articulationSymbolIds, SymId articulationSymbolId,
                                    ArticulationsUpdateMode updateMode = ArticulationsUpdateMode::Insert);
std::set<SymId> splitArticulations(const std::set<SymId> &articulationSymbolIds);
std::set<SymId> joinArticulations(const std::set<SymId> &articulationSymbolIds);
std::set<SymId> flipArticulations(const std::set<SymId> &articulationSymbolIds, PlacementV placement);

//---------------------------------------------------------
//   @@ Articulation
///    articulation marks
//---------------------------------------------------------

class Articulation : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, Articulation)
    DECLARE_CLASSOF(ElementType::ARTICULATION)

public:

    Articulation(const Articulation &) = default;
    Articulation &operator=(const Articulation &) = delete;

    virtual Articulation *clone() const override { return new Articulation(*this); }

    virtual double mag() const override;

    SymId symId() const { return symId_; }

    void setSymId(SymId id);
    virtual int subtype() const override;

    void setTextType(ArticulationTextType textType);

    ArticulationTextType textType() const { return textType_; }

    Text *text() const { return text_; }

    void setText(Text *t) { text_ = t; }

    void setSelected(bool f) override;
    void setVisible(bool f) override;

    TranslatableString typeUserName() const override;
    TranslatableString subtypeUserName() const override;
    String articulationName() const; // type-name of articulation; used for midi rendering
    static String symId2ArticulationName(SymId symId);

    bool layoutCloseToNote() const;

    bool isHiddenOnTabStaff() const;

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid) const override;
    void resetProperty(Pid id) override;
    Sid getPropertyStyle(Pid id) const override;

    void setDirection(DirectionV d) { direction_ = d; }

    DirectionV direction() const { return direction_; }

    ChordRest *chordRest() const;
    Segment *segment() const;
    Measure *measure() const;
    System *system() const;
    Page *page() const;

    ArticulationAnchor anchor() const { return anchor_; }

    void setAnchor(ArticulationAnchor v) { anchor_ = v; }

    OrnamentStyle ornamentStyle() const { return ornamentStyle_; }

    void setOrnamentStyle(OrnamentStyle val) { ornamentStyle_ = val; }

    bool playArticulation() const { return playArticulation_; }

    void setPlayArticulation(bool val) { playArticulation_ = val; }

    String channelName() const { return channelName_; }

    void setChannelName(const String &s) { channelName_ = s; }

    bool isDouble() const { return categories_ & ArticulationCategory::DOUBLE; }

    bool isTenuto() const { return categories_ & ArticulationCategory::TENUTO; }

    bool isStaccato() const { return categories_ & ArticulationCategory::STACCATO; }

    bool isAccent() const { return categories_ & ArticulationCategory::ACCENT; }

    bool isMarcato() const { return categories_ & ArticulationCategory::MARCATO; }

    bool isLuteFingering() const { return categories_ & ArticulationCategory::LUTE_FINGERING; }

    bool isLaissezVib() const { return categories_ & ArticulationCategory::LAISSEZ_VIB; }

    bool isHandbellsArticulation() const { return categories_ & ArticulationCategory::HANDBELLS; }

    bool isBasicArticulation() const;

    void styleChanged() override;

    bool isOnCrossBeamSide() const;

    staff_idx_t vStaffIdx() const override;

    struct LayoutData : public EngravingItem::LayoutData {
        ld_field<bool> up = {"[Articulation] up", true};
        ld_field<SymId> symId = {"[Articulation] symId", SymId::noSym};

        void reset() override {
            EngravingItem::LayoutData::reset();
            up.reset();
            symId.reset();
        }

        double opticalCenter() const;
    };
    DECLARE_LAYOUTDATA_METHODS(Articulation)

    void setUp(bool val);

    //! --- DEPRECATED ---
    bool up() const { return ldata()->up; }

    //! ------------------

protected:
    friend class mu::engraving::Factory;
    Articulation(DummyParentOr<ChordRest> parent, ElementType type = ElementType::ARTICULATION);

private:

    void setupShowOnTabStyles();

    enum class AnchorGroup : unsigned char { ARTICULATION, LUTE_FINGERING, OTHER };
    static AnchorGroup anchorGroup(SymId);

    void computeCategories();

    ArticulationCategories categories_ = ArticulationCategory::NONE;

    SymId symId_ = SymId::noSym;
    DirectionV direction_ = DirectionV::AUTO;
    String channelName_;

    ArticulationTextType textType_ = ArticulationTextType::NO_TEXT;
    Text *text_ = nullptr;

    ArticulationAnchor anchor_ = ArticulationAnchor::AUTO;

    OrnamentStyle ornamentStyle_ = OrnamentStyle::DEFAULT; // for use in ornaments such as trill
    bool playArticulation_ = true;

    std::pair<Sid, Sid> showOnTabStyles_ = {Sid::NOSTYLE, Sid::NOSTYLE};
};
} // namespace mu::engraving

#endif
