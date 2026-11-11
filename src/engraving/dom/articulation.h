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
struct Factory;

struct ChordRest;
struct Measure;
struct Page;
struct Segment;
struct System;

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

enum class ArticulationsUpdateMode : unsigned char { INSERT, REMOVE };

std::set<SymId> update_articulations(const std::set<SymId> &articulation_symbol_ids, SymId articulation_symbol_id,
                                     ArticulationsUpdateMode update_mode = ArticulationsUpdateMode::INSERT);
std::set<SymId> split_articulations(const std::set<SymId> &articulation_symbol_ids);
std::set<SymId> join_articulations(const std::set<SymId> &articulation_symbol_ids);
std::set<SymId> flip_articulations(const std::set<SymId> &articulation_symbol_ids, PlacementV placement);

//---------------------------------------------------------
//   @@ Articulation
///    articulation marks
//---------------------------------------------------------

struct Articulation : EngravingItem {
private:
    DECLARE_CLASSOF(ElementType::ARTICULATION)

public:

    Articulation(const Articulation &) = default;
    Articulation &operator=(const Articulation &) = delete;

    virtual Articulation *clone() const override { return new Articulation(*this); }

    virtual double mag() const override;

    SymId sym_id() const { return sym_id_; }

    void set_sym_id(SymId id);
    virtual int subtype() const override;

    void set_text_type(ArticulationTextType text_type);

    ArticulationTextType text_type() const { return text_type_; }

    Text *text() const { return text_; }

    void set_text(Text *t) { text_ = t; }

    void set_selected(bool f) override;
    void set_visible(bool f) override;

    TranslatableString type_user_name() const override;
    TranslatableString subtype_user_name() const override;
    String articulation_name() const; // type-name of articulation; used for midi rendering
    static String sym_id2_articulation_name(SymId sym_id);

    bool layout_close_to_note() const;

    bool is_hidden_on_tab_staff() const;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;
    void reset_property(Pid id) override;
    Sid get_property_style(Pid id) const override;

    void set_direction(DirectionV d) { direction_ = d; }

    DirectionV direction() const { return direction_; }

    ChordRest *chord_rest() const;
    Segment *segment() const;
    Measure *measure() const;
    System *system() const;
    Page *page() const;

    ArticulationAnchor anchor() const { return anchor_; }

    void set_anchor(ArticulationAnchor v) { anchor_ = v; }

    OrnamentStyle ornament_style() const { return ornament_style_; }

    void set_ornament_style(OrnamentStyle val) { ornament_style_ = val; }

    bool play_articulation() const { return play_articulation_; }

    void set_play_articulation(bool val) { play_articulation_ = val; }

    String channel_name() const { return channel_name_; }

    void set_channel_name(const String &s) { channel_name_ = s; }

    bool is_double() const { return categories_ & ArticulationCategory::DOUBLE; }

    bool is_tenuto() const { return categories_ & ArticulationCategory::TENUTO; }

    bool is_staccato() const { return categories_ & ArticulationCategory::STACCATO; }

    bool is_accent() const { return categories_ & ArticulationCategory::ACCENT; }

    bool is_marcato() const { return categories_ & ArticulationCategory::MARCATO; }

    bool is_lute_fingering() const { return categories_ & ArticulationCategory::LUTE_FINGERING; }

    bool is_laissez_vib() const { return categories_ & ArticulationCategory::LAISSEZ_VIB; }

    bool is_handbells_articulation() const { return categories_ & ArticulationCategory::HANDBELLS; }

    bool is_basic_articulation() const;

    void style_changed() override;

    bool is_on_cross_beam_side() const;

    staff_idx_t v_staff_idx() const override;

    struct LayoutData : public EngravingItem::LayoutData {
        ld_field<bool> up = {"[Articulation] up", true};
        ld_field<SymId> sym_id = {"[Articulation] symId", SymId::NO_SYM};

        void reset() override {
            EngravingItem::LayoutData::reset();
            up.reset();
            sym_id.reset();
        }

        double optical_center() const;
    };
    DECLARE_LAYOUTDATA_METHODS(Articulation)

    void set_up(bool val);

    //! --- DEPRECATED ---
    bool up() const { return ldata()->up; }

    //! ------------------

protected:
    friend struct mu::engraving::Factory;
    Articulation(DummyParentOr<ChordRest> parent, ElementType type = ElementType::ARTICULATION);

private:

    void setup_show_on_tab_styles();

    enum class AnchorGroup : unsigned char { ARTICULATION, LUTE_FINGERING, OTHER };
    static AnchorGroup anchor_group(SymId);

    void compute_categories();

    ArticulationCategories categories_ = ArticulationCategory::NONE;

    SymId sym_id_ = SymId::NO_SYM;
    DirectionV direction_ = DirectionV::AUTO;
    String channel_name_;

    ArticulationTextType text_type_ = ArticulationTextType::NO_TEXT;
    Text *text_ = nullptr;

    ArticulationAnchor anchor_ = ArticulationAnchor::AUTO;

    OrnamentStyle ornament_style_ = OrnamentStyle::DEFAULT; // for use in ornaments such as trill
    bool play_articulation_ = true;

    std::pair<Sid, Sid> show_on_tab_styles_ = {Sid::NOSTYLE, Sid::NOSTYLE};
};
} // namespace mu::engraving

#endif
