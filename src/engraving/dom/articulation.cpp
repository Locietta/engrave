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

#include "articulation.h"

#include "beam.h"
#include "chord.h"
#include "chordrest.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "page.h"
#include "staff.h"
#include "stafftype.h"
#include "style/style.h"
#include "system.h"
#include "types/symnames.h"
#include "types/translatablestring.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::draw;

//---------------------------------------------------------
//   articulationStyle
//---------------------------------------------------------

static const ElementStyle articulation_style{
    {Sid::ARTICULATION_MIN_DISTANCE, Pid::MIN_DISTANCE},
    {Sid::ARTICULATION_ANCHOR_DEFAULT, Pid::ARTICULATION_ANCHOR},
};

//---------------------------------------------------------
//   Articulation
//---------------------------------------------------------

Articulation::Articulation(DummyParentOr<ChordRest> parent, ElementType type) : EngravingItem(type, parent, ElementFlag::MOVABLE) {
    sym_id_ = SymId::NO_SYM;
    anchor_ = ArticulationAnchor::AUTO;
    direction_ = DirectionV::AUTO;
    ornament_style_ = OrnamentStyle::DEFAULT;
    play_articulation_ = true;

    init_element_style(&::articulation_style);
    setup_show_on_tab_styles();
}

//---------------------------------------------------------
//   setSymId
//---------------------------------------------------------

void Articulation::set_sym_id(SymId id) {
    sym_id_ = id;
    compute_categories();
    setup_show_on_tab_styles();
    anchor_ = ArticulationAnchor(property_default(Pid::ARTICULATION_ANCHOR).to_int());
    text_type_ = ArticulationTextType::NO_TEXT;
}

//---------------------------------------------------------
//   setTextType
//---------------------------------------------------------

void Articulation::set_text_type(ArticulationTextType text_type) { text_type_ = text_type; }

void Articulation::set_selected(bool f) {
    if (text_) {
        text_->set_selected(f);
    }

    EngravingItem::set_selected(f);
}

void Articulation::set_visible(bool f) {
    if (text_) {
        text_->set_visible(f);
    }

    EngravingItem::set_visible(f);
}

//---------------------------------------------------------
//   subtype
//---------------------------------------------------------

int Articulation::subtype() const {
    if (text_type_ != ArticulationTextType::NO_TEXT) {
        return int(text_type_);
    }
    String s = String::from_ascii(SymNames::name_for_sym_id(sym_id_).ascii());
    if (s.ends_with(u"Below")) {
        return int(SymNames::sym_id_by_name(s.left(s.size() - 5) + u"Above"));
    } else if (s.ends_with(u"Turned")) {
        return int(SymNames::sym_id_by_name(s.left(s.size() - 6)));
    }

    return int(sym_id_);
}

//---------------------------------------------------------
//   setUp
//---------------------------------------------------------

void Articulation::set_up(bool val) {
    Articulation::LayoutData *ldata = mutldata();
    ldata->up = val;

    //! NOTE member of Articulation symId_ - this is `given` data
    //! member of LayoutData symId_ - this is layout data
    //! I would not like to change the `given` data here, but they are changing for backward compatibility
    //! Even better, I wouldn’t want symId to be `given` data,
    //! it would be better if there was some type,
    //! and from it we would already figure out how (with what symbol) to display it

    bool dup = direction_ == DirectionV::AUTO ? val : direction_ == DirectionV::UP;
    String s = String::from_ascii(SymNames::name_for_sym_id(sym_id_).ascii());
    if (s.ends_with(!dup ? u"Above" : u"Below")) {
        String s2 = s.left(s.size() - 5) + (dup ? u"Above" : u"Below");
        sym_id_ = SymNames::sym_id_by_name(s2);
    } else if (s.ends_with(u"Turned")) {
        String s2 = dup ? s.left(s.size() - 6) : s;
        sym_id_ = SymNames::sym_id_by_name(s2);
    } else if (!dup) {
        String s2 = s + u"Turned";
        SymId sym = SymNames::sym_id_by_name(s2);
        if (sym != SymId::NO_SYM) {
            sym_id_ = sym;
        }
    }

    ldata->sym_id = sym_id_;
}

//---------------------------------------------------------
//   typeUserName
//---------------------------------------------------------

muse::TranslatableString Articulation::type_user_name() const {
    if (text_type_ != ArticulationTextType::NO_TEXT) {
        return TranslatableString("engraving", "Articulation text");
    }

    return TranslatableString("engraving", "Articulation");
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString Articulation::subtype_user_name() const {
    if (text_type_ != ArticulationTextType::NO_TEXT) {
        return TConv::user_name(text_type_);
    }

    return SymNames::user_name_for_sym_id(sym_id());
}

//---------------------------------------------------------
//   chordRest
//---------------------------------------------------------

ChordRest *Articulation::chord_rest() const {
    if (ownership_parent() && ownership_parent()->is_chord_rest()) {
        return to_chord_rest(ownership_parent());
    }
    return 0;
}

Segment *Articulation::segment() const {
    ChordRest *cr = chord_rest();
    if (!cr) {
        return 0;
    }

    Segment *s = 0;
    if (cr->is_grace()) {
        if (cr->ownership_parent()) {
            s = to_segment(cr->ownership_parent()->ownership_parent());
        }
    } else {
        s = to_segment(cr->ownership_parent());
    }

    return s;
}

Measure *Articulation::measure() const {
    Segment *s = segment();
    return to_measure(s ? s->ownership_parent() : 0);
}

System *Articulation::system() const {
    Measure *m = measure();
    return m ? m->system() : nullptr;
}

Page *Articulation::page() const {
    System *s = system();
    return s ? s->page() : nullptr;
}

bool Articulation::is_hidden_on_tab_staff() const {
    if (show_on_tab_styles_.first == Sid::NOSTYLE || show_on_tab_styles_.second == Sid::NOSTYLE) {
        return false;
    }

    const StaffType *st_type = staff_type();

    if (!st_type || !st_type->is_tab_staff()) {
        return false;
    }

    return st_type->is_hidden_element_on_tab(show_on_tab_styles_.first, show_on_tab_styles_.second);
}

//---------------------------------------------------------
//   layoutCloseToNote
//    Needed to figure out the layout policy regarding
//    distance to the note and placement in relation to
//    slur.
//---------------------------------------------------------

bool Articulation::layout_close_to_note() const {
    Staff *s = staff();
    if (s && s->staff_type()->is_tab_staff() && is_staccato()) {
        return false;
    }

    return (is_staccato() || is_tenuto()) && !is_double();
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Articulation::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SYMBOL: return PropertyValue::from_value(sym_id_);
        case Pid::DIRECTION: return PropertyValue::from_value<DirectionV>(direction());
        case Pid::ARTICULATION_ANCHOR: return int(anchor());
        case Pid::ORNAMENT_STYLE: return ornament_style();
        case Pid::PLAY: return play_articulation();
        default: return EngravingItem::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Articulation::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SYMBOL: set_sym_id(v.value<SymId>()); break;
        case Pid::DIRECTION: set_direction(v.value<DirectionV>()); break;
        case Pid::ARTICULATION_ANCHOR: set_anchor(ArticulationAnchor(v.to_int())); break;
        case Pid::PLAY: set_play_articulation(v.to_bool()); break;
        case Pid::ORNAMENT_STYLE: set_ornament_style(v.value<OrnamentStyle>()); break;
        default: return EngravingItem::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Articulation::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::DIRECTION: return DirectionV::AUTO;

        case Pid::ORNAMENT_STYLE: return OrnamentStyle::DEFAULT;

        case Pid::PLAY: return true;

        default: break;
    }
    return EngravingItem::property_default(property_id);
}

//---------------------------------------------------------
//   anchorGroup
//---------------------------------------------------------

Articulation::AnchorGroup Articulation::anchor_group(SymId sym_id) {
    switch (sym_id) {
        case SymId::ARTIC_ACCENT_ABOVE:
        case SymId::ARTIC_ACCENT_BELOW:
        case SymId::ARTIC_STACCATO_ABOVE:
        case SymId::ARTIC_STACCATO_BELOW:
        case SymId::ARTIC_STACCATISSIMO_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_BELOW:
        case SymId::ARTIC_TENUTO_ABOVE:
        case SymId::ARTIC_TENUTO_BELOW:
        case SymId::ARTIC_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_TENUTO_STACCATO_BELOW:
        case SymId::ARTIC_MARCATO_ABOVE:
        case SymId::ARTIC_MARCATO_BELOW:

        case SymId::ARTIC_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_ACCENT_STACCATO_BELOW:
        case SymId::ARTIC_LAISSEZ_VIBRER_ABOVE:
        case SymId::ARTIC_LAISSEZ_VIBRER_BELOW:
        case SymId::ARTIC_MARCATO_STACCATO_ABOVE:
        case SymId::ARTIC_MARCATO_STACCATO_BELOW:
        case SymId::ARTIC_MARCATO_TENUTO_ABOVE:
        case SymId::ARTIC_MARCATO_TENUTO_BELOW:
        case SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_STROKE_BELOW:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_BELOW:
        case SymId::ARTIC_STRESS_ABOVE:
        case SymId::ARTIC_STRESS_BELOW:
        case SymId::ARTIC_TENUTO_ACCENT_ABOVE:
        case SymId::ARTIC_TENUTO_ACCENT_BELOW:
        case SymId::ARTIC_UNSTRESS_ABOVE:
        case SymId::ARTIC_UNSTRESS_BELOW:

        case SymId::ARTIC_SOFT_ACCENT_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_BELOW:
        case SymId::ARTIC_SOFT_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_STACCATO_BELOW:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_BELOW:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_SOFT_ACCENT_TENUTO_STACCATO_BELOW:

        case SymId::WIGGLE_SAWTOOTH:
        case SymId::WIGGLE_SAWTOOTH_WIDE:
        case SymId::WIGGLE_VIBRATO_LARGE_FASTER:
        case SymId::WIGGLE_VIBRATO_LARGE_SLOWEST: return AnchorGroup::ARTICULATION;

        case SymId::LUTE_FINGERING_RH_THUMB:
        case SymId::LUTE_FINGERING_RH_FIRST:
        case SymId::LUTE_FINGERING_RH_SECOND:
        case SymId::LUTE_FINGERING_RH_THIRD: return AnchorGroup::LUTE_FINGERING;

        case SymId::TREMOLO_DIVISI_DOTS2:
        case SymId::TREMOLO_DIVISI_DOTS3:
        case SymId::TREMOLO_DIVISI_DOTS4:
        case SymId::TREMOLO_DIVISI_DOTS6: return AnchorGroup::ARTICULATION;

        default: break;
    }
    return AnchorGroup::OTHER;
}

//---------------------------------------------------------
//   symId2ArticulationName
//---------------------------------------------------------

String Articulation::sym_id2_articulation_name(SymId sym_id) {
    switch (sym_id) {
        case SymId::ARTIC_STACCATISSIMO_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_BELOW:
        case SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_STROKE_BELOW:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_ABOVE:
        case SymId::ARTIC_STACCATISSIMO_WEDGE_BELOW: return u"staccatissimo";

        case SymId::ARTIC_STACCATO_ABOVE:
        case SymId::ARTIC_STACCATO_BELOW:
        case SymId::TREMOLO_DIVISI_DOTS2:
        case SymId::TREMOLO_DIVISI_DOTS3:
        case SymId::TREMOLO_DIVISI_DOTS4:
        case SymId::TREMOLO_DIVISI_DOTS6: return u"staccato";

        case SymId::ARTIC_ACCENT_STACCATO_ABOVE:
        case SymId::ARTIC_ACCENT_STACCATO_BELOW: return u"sforzatoStaccato";

        case SymId::ARTIC_MARCATO_STACCATO_ABOVE:
        case SymId::ARTIC_MARCATO_STACCATO_BELOW: return u"marcatoStaccato";

        case SymId::ARTIC_TENUTO_STACCATO_ABOVE:
        case SymId::ARTIC_TENUTO_STACCATO_BELOW: return u"portato";

        case SymId::ARTIC_TENUTO_ACCENT_ABOVE:
        case SymId::ARTIC_TENUTO_ACCENT_BELOW: return u"sforzatoTenuto";

        case SymId::ARTIC_MARCATO_TENUTO_ABOVE:
        case SymId::ARTIC_MARCATO_TENUTO_BELOW: return u"marcatoTenuto";

        case SymId::ARTIC_TENUTO_ABOVE:
        case SymId::ARTIC_TENUTO_BELOW: return u"tenuto";

        case SymId::ARTIC_MARCATO_ABOVE:
        case SymId::ARTIC_MARCATO_BELOW: return u"marcato";

        case SymId::ARTIC_ACCENT_ABOVE:
        case SymId::ARTIC_ACCENT_BELOW: return u"accent";

        case SymId::DYNAMIC_SFORZATO:
        case SymId::DYNAMIC_SFORZANDO: return u"sforzato";

        case SymId::BRASS_MUTE_OPEN: return u"open";

        case SymId::BRASS_MUTE_CLOSED: return u"closed";

        case SymId::STRINGS_HARMONIC: return u"harmonic";

        case SymId::ORNAMENT_MORDENT: return u"mordent";

        default: return u"---";
    }
}

//---------------------------------------------------------
//   articulationName
//---------------------------------------------------------

String Articulation::articulation_name() const { return sym_id2_articulation_name(sym_id_); }

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid Articulation::get_property_style(Pid id) const {
    switch (id) {
        case Pid::MIN_DISTANCE: return EngravingItem::get_property_style(id);

        case Pid::ARTICULATION_ANCHOR: {
            if (is_handbells_articulation()) {
                return Sid::ARTICULATION_ANCHOR_DEFAULT;
            }

            switch (anchor_group(sym_id_)) {
                case AnchorGroup::ARTICULATION: return Sid::ARTICULATION_ANCHOR_DEFAULT;
                case AnchorGroup::LUTE_FINGERING: return Sid::ARTICULATION_ANCHOR_LUTE_FINGERING;
                case AnchorGroup::OTHER: return Sid::ARTICULATION_ANCHOR_OTHER;
            }
        }
            assert(false); // should never be reached
        // fallthrough
        default: return Sid::NOSTYLE;
    }
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void Articulation::reset_property(Pid id) {
    switch (id) {
        case Pid::DIRECTION:
        case Pid::ORNAMENT_STYLE: set_property(id, property_default(id)); return;
        case Pid::ARTICULATION_ANCHOR: set_property(id, property_default(id)); return;

        default: break;
    }
    EngravingItem::reset_property(id);
}

//---------------------------------------------------------
//   mag
//---------------------------------------------------------

double Articulation::mag() const { return layout_parent() ? layout_parent()->mag() * style().style_d(Sid::ARTICULATION_MAG) : 1.0; }

void Articulation::compute_categories() {
    categories_.set_flag(ArticulationCategory::DOUBLE,
                         sym_id_ == SymId::ARTIC_MARCATO_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_TENUTO_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_TENUTO_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_ACCENT_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_ACCENT_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_MARCATO_TENUTO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_TENUTO_BELOW ||
                             sym_id_ == SymId::ARTIC_TENUTO_ACCENT_ABOVE || sym_id_ == SymId::ARTIC_TENUTO_ACCENT_BELOW);

    categories_.set_flag(ArticulationCategory::TENUTO,
                         sym_id_ == SymId::ARTIC_TENUTO_ABOVE || sym_id_ == SymId::ARTIC_TENUTO_BELOW ||
                             sym_id_ == SymId::ARTIC_MARCATO_TENUTO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_TENUTO_BELOW ||
                             sym_id_ == SymId::ARTIC_TENUTO_ACCENT_ABOVE || sym_id_ == SymId::ARTIC_TENUTO_ACCENT_BELOW);

    categories_.set_flag(ArticulationCategory::STACCATO,
                         sym_id_ == SymId::ARTIC_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_MARCATO_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_TENUTO_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_TENUTO_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_ACCENT_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_ACCENT_STACCATO_BELOW ||
                             sym_id_ == SymId::TREMOLO_DIVISI_DOTS2 || sym_id_ == SymId::TREMOLO_DIVISI_DOTS3 ||
                             sym_id_ == SymId::TREMOLO_DIVISI_DOTS4 || sym_id_ == SymId::TREMOLO_DIVISI_DOTS6);

    categories_.set_flag(ArticulationCategory::ACCENT, sym_id_ == SymId::ARTIC_ACCENT_ABOVE || sym_id_ == SymId::ARTIC_ACCENT_BELOW ||
                                                           sym_id_ == SymId::ARTIC_ACCENT_STACCATO_ABOVE ||
                                                           sym_id_ == SymId::ARTIC_ACCENT_STACCATO_BELOW);

    categories_.set_flag(ArticulationCategory::MARCATO,
                         sym_id_ == SymId::ARTIC_MARCATO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_MARCATO_STACCATO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_STACCATO_BELOW ||
                             sym_id_ == SymId::ARTIC_MARCATO_TENUTO_ABOVE || sym_id_ == SymId::ARTIC_MARCATO_TENUTO_BELOW);

    categories_.set_flag(ArticulationCategory::LUTE_FINGERING,
                         sym_id_ == SymId::STRINGS_THUMB_POSITION || sym_id_ == SymId::LUTE_FINGERING_RH_THUMB ||
                             sym_id_ == SymId::LUTE_FINGERING_RH_FIRST || sym_id_ == SymId::LUTE_FINGERING_RH_SECOND ||
                             sym_id_ == SymId::LUTE_FINGERING_RH_THIRD);
    categories_.set_flag(ArticulationCategory::LAISSEZ_VIB,
                         sym_id_ == SymId::ARTIC_LAISSEZ_VIBRER_ABOVE || sym_id_ == SymId::ARTIC_LAISSEZ_VIBRER_BELOW);

    categories_.set_flag(ArticulationCategory::HANDBELLS,
                         (static_cast<int>(sym_id_) >= static_cast<int>(SymId::HANDBELLS_BELLTREE) &&
                          static_cast<int>(sym_id_) <= static_cast<int>(SymId::HANDBELLS_TABLE_SINGLE_BELL)) ||
                             (static_cast<int>(text_type_) >= static_cast<int>(ArticulationTextType::TD) &&
                              static_cast<int>(text_type_) <= static_cast<int>(ArticulationTextType::VIB)));
}

bool Articulation::is_basic_articulation() const {
    static const std::set<SymId> articulations{SymId::ARTIC_ACCENT_ABOVE,
                                               SymId::ARTIC_ACCENT_BELOW,
                                               SymId::ARTIC_STACCATO_ABOVE,
                                               SymId::ARTIC_STACCATO_BELOW,
                                               SymId::ARTIC_TENUTO_ABOVE,
                                               SymId::ARTIC_TENUTO_BELOW,
                                               SymId::ARTIC_MARCATO_ABOVE,
                                               SymId::ARTIC_MARCATO_BELOW,

                                               SymId::ARTIC_ACCENT_STACCATO_ABOVE,
                                               SymId::ARTIC_ACCENT_STACCATO_BELOW,
                                               SymId::ARTIC_MARCATO_STACCATO_ABOVE,
                                               SymId::ARTIC_MARCATO_STACCATO_BELOW,
                                               SymId::ARTIC_MARCATO_TENUTO_ABOVE,
                                               SymId::ARTIC_MARCATO_TENUTO_BELOW,
                                               SymId::ARTIC_TENUTO_STACCATO_ABOVE,
                                               SymId::ARTIC_TENUTO_STACCATO_BELOW,
                                               SymId::ARTIC_TENUTO_ACCENT_ABOVE,
                                               SymId::ARTIC_TENUTO_ACCENT_BELOW,
                                               SymId::ARTIC_STACCATISSIMO_ABOVE,
                                               SymId::ARTIC_STACCATISSIMO_BELOW,
                                               SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE,
                                               SymId::ARTIC_STACCATISSIMO_STROKE_BELOW,
                                               SymId::ARTIC_STACCATISSIMO_WEDGE_ABOVE,
                                               SymId::ARTIC_STACCATISSIMO_WEDGE_BELOW,
                                               SymId::ARTIC_STRESS_ABOVE,
                                               SymId::ARTIC_STRESS_BELOW};
    SymId sym_id = static_cast<SymId>(subtype());
    return articulations.find(sym_id) != articulations.end();
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

void Articulation::setup_show_on_tab_styles() {
    /// staccato
    if (is_staccato()) {
        show_on_tab_styles_ = {Sid::STACCATO_SHOW_TAB_COMMON, Sid::STACCATO_SHOW_TAB_SIMPLE};
    }

    /// accent
    if (is_accent() || is_marcato()) {
        show_on_tab_styles_ = {Sid::ACCENT_SHOW_TAB_COMMON, Sid::ACCENT_SHOW_TAB_SIMPLE};
    }

    /// turn
    if (sym_id_ == SymId::ORNAMENT_TURN || sym_id_ == SymId::ORNAMENT_TURN_INVERTED) {
        show_on_tab_styles_ = {Sid::TURN_SHOW_TAB_COMMON, Sid::TURN_SHOW_TAB_SIMPLE};
    }

    /// mordent
    if (sym_id_ == SymId::ORNAMENT_MORDENT || sym_id_ == SymId::ORNAMENT_SHORT_TRILL) {
        show_on_tab_styles_ = {Sid::MORDENT_SHOW_TAB_COMMON, Sid::MORDENT_SHOW_TAB_SIMPLE};
    }

    /// wah
    if (sym_id_ == SymId::BRASS_MUTE_OPEN || sym_id_ == SymId::BRASS_MUTE_CLOSED) {
        show_on_tab_styles_ = {Sid::WAH_SHOW_TAB_COMMON, Sid::WAH_SHOW_TAB_SIMPLE};
    }

    /// golpe
    if (sym_id_ == SymId::GUITAR_GOLPE) {
        show_on_tab_styles_ = {Sid::GOLPE_SHOW_TAB_COMMON, Sid::GOLPE_SHOW_TAB_SIMPLE};
    }
}

void Articulation::style_changed() {
    if (text_) {
        text_->style_changed();
    }

    bool is_golpe_thumb = sym_id_ == SymId::GUITAR_GOLPE && anchor_ == ArticulationAnchor::BOTTOM;
    EngravingItem::style_changed();
    if (is_golpe_thumb) {
        set_anchor(ArticulationAnchor::BOTTOM);
    }
}

bool Articulation::is_on_cross_beam_side() const {
    ChordRest *cr = chord_rest();
    if (!cr || !cr->is_chord()) {
        return false;
    }
    Chord *chord = to_chord(cr);
    return chord->beam() && (chord->beam()->cross() || chord->staff_move() != 0) && (up() == chord->up());
}

staff_idx_t Articulation::v_staff_idx() const {
    ChordRest *cr = chord_rest();
    if (!cr) {
        return staff_idx();
    }
    return cr->v_staff_idx();
}

struct ArticulationGroup {
    SymId first;
    SymId second;

    bool operator<(const ArticulationGroup &other) const {
        if ((first == other.first && second == other.second) || (first == other.second && second == other.first)) {
            return false;
        }

        if (first != other.first) {
            return first < other.first;
        }

        return second < other.second;
    }
};

namespace mu::engraving {
static const std::map<SymId, ArticulationGroup> articulation_above_split_groups = {
    {SymId::ARTIC_ACCENT_STACCATO_ABOVE, {SymId::ARTIC_STACCATO_ABOVE, SymId::ARTIC_ACCENT_ABOVE}},
    {SymId::ARTIC_TENUTO_ACCENT_ABOVE, {SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_ACCENT_ABOVE}},
    {SymId::ARTIC_TENUTO_STACCATO_ABOVE, {SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_STACCATO_ABOVE}},
    {SymId::ARTIC_MARCATO_STACCATO_ABOVE, {SymId::ARTIC_STACCATO_ABOVE, SymId::ARTIC_MARCATO_ABOVE}},
    {SymId::ARTIC_MARCATO_TENUTO_ABOVE, {SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_MARCATO_ABOVE}},
};

static const std::map<SymId, ArticulationGroup> articulation_below_split_group = {
    {SymId::ARTIC_ACCENT_STACCATO_BELOW, {SymId::ARTIC_STACCATO_BELOW, SymId::ARTIC_ACCENT_BELOW}},
    {SymId::ARTIC_TENUTO_ACCENT_BELOW, {SymId::ARTIC_TENUTO_BELOW, SymId::ARTIC_ACCENT_BELOW}},
    {SymId::ARTIC_TENUTO_STACCATO_BELOW, {SymId::ARTIC_TENUTO_BELOW, SymId::ARTIC_STACCATO_BELOW}},
    {SymId::ARTIC_MARCATO_STACCATO_BELOW, {SymId::ARTIC_STACCATO_BELOW, SymId::ARTIC_MARCATO_BELOW}},
    {SymId::ARTIC_MARCATO_TENUTO_BELOW, {SymId::ARTIC_TENUTO_BELOW, SymId::ARTIC_MARCATO_BELOW}},
};

static const std::map<ArticulationGroup, SymId> articulation_above_join_groups = {
    {{SymId::ARTIC_STACCATO_ABOVE, SymId::ARTIC_ACCENT_ABOVE}, SymId::ARTIC_ACCENT_STACCATO_ABOVE},
    {{SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_ACCENT_ABOVE}, SymId::ARTIC_TENUTO_ACCENT_ABOVE},
    {{SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_STACCATO_ABOVE}, SymId::ARTIC_TENUTO_STACCATO_ABOVE},
    {{SymId::ARTIC_STACCATO_ABOVE, SymId::ARTIC_MARCATO_ABOVE}, SymId::ARTIC_MARCATO_STACCATO_ABOVE},
    {{SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_MARCATO_ABOVE}, SymId::ARTIC_MARCATO_TENUTO_ABOVE},
};

static const std::map<SymId, SymId> articulation_placements = {
    {SymId::ARTIC_STACCATISSIMO_ABOVE, SymId::ARTIC_STACCATISSIMO_BELOW},
    {SymId::ARTIC_STACCATISSIMO_STROKE_ABOVE, SymId::ARTIC_STACCATISSIMO_STROKE_BELOW},
    {SymId::ARTIC_STACCATISSIMO_WEDGE_ABOVE, SymId::ARTIC_STACCATISSIMO_WEDGE_BELOW},
    {SymId::ARTIC_STACCATO_ABOVE, SymId::ARTIC_STACCATO_BELOW},
    {SymId::ARTIC_ACCENT_STACCATO_ABOVE, SymId::ARTIC_ACCENT_STACCATO_BELOW},
    {SymId::ARTIC_MARCATO_STACCATO_ABOVE, SymId::ARTIC_MARCATO_STACCATO_BELOW},
    {SymId::ARTIC_TENUTO_STACCATO_ABOVE, SymId::ARTIC_TENUTO_STACCATO_BELOW},
    {SymId::ARTIC_TENUTO_ACCENT_ABOVE, SymId::ARTIC_TENUTO_ACCENT_BELOW},
    {SymId::ARTIC_MARCATO_TENUTO_ABOVE, SymId::ARTIC_MARCATO_TENUTO_BELOW},
    {SymId::ARTIC_TENUTO_ABOVE, SymId::ARTIC_TENUTO_BELOW},
    {SymId::ARTIC_MARCATO_ABOVE, SymId::ARTIC_MARCATO_BELOW},
    {SymId::ARTIC_ACCENT_ABOVE, SymId::ARTIC_ACCENT_BELOW}};

std::set<SymId> split_articulations(const std::set<SymId> &articulation_symbol_ids) {
    std::set<SymId> result;
    for (const SymId &articulation_symbol_id : articulation_symbol_ids) {
        auto artic = ::mu::engraving::articulation_above_split_groups.find(articulation_symbol_id);
        if (artic != ::mu::engraving::articulation_above_split_groups.end()) {
            // check above
            const ArticulationGroup &group = artic->second;
            result.insert(group.first);
            result.insert(group.second);
        } else {
            // check below
            artic = ::mu::engraving::articulation_below_split_group.find(articulation_symbol_id);
            if (artic != ::mu::engraving::articulation_below_split_group.end()) {
                const ArticulationGroup &group = artic->second;
                result.insert(group.first);
                result.insert(group.second);
            } else {
                result.insert(articulation_symbol_id);
            }
        }
    }

    return result;
}

std::set<SymId> join_articulations(const std::set<SymId> &articulation_symbol_ids) {
    SymIdList result;

    SymIdList vsymbol_ids(articulation_symbol_ids.begin(), articulation_symbol_ids.end());

    std::sort(vsymbol_ids.begin(), vsymbol_ids.end(), [](SymId l, SymId r) { return l > r; });

    std::set<SymId> splitted_symbols;

    auto symbol_selected = [&splitted_symbols](const SymId &symbol_id) -> bool {
        return splitted_symbols.find(symbol_id) != splitted_symbols.end();
    };

    for (size_t i = 0; i < vsymbol_ids.size(); i++) {
        if (symbol_selected(vsymbol_ids[i])) {
            continue;
        }

        bool found = false;
        for (size_t j = i + 1; j < vsymbol_ids.size(); j++) {
            if (symbol_selected(vsymbol_ids[i]) || symbol_selected(vsymbol_ids[j])) {
                continue;
            }

            ArticulationGroup group = {vsymbol_ids[i], vsymbol_ids[j]};
            auto join_articulation = ::mu::engraving::articulation_above_join_groups.find(group);
            if (join_articulation != ::mu::engraving::articulation_above_join_groups.end()) {
                result.push_back(join_articulation->second);
                splitted_symbols.insert(vsymbol_ids[i]);
                splitted_symbols.insert(vsymbol_ids[j]);
                found = true;
            }
        }

        if (!found) {
            result.push_back(vsymbol_ids[i]);
            splitted_symbols.insert(vsymbol_ids[i]);
        }
    }

    std::sort(result.begin(), result.end(), [](SymId l, SymId r) { return l < r; });

    return std::set<SymId>(result.begin(), result.end());
}

std::set<SymId> update_articulations(const std::set<SymId> &articulation_symbol_ids, SymId articulation_symbol_id,
                                     ArticulationsUpdateMode update_mode) {
    std::set<SymId> splitted_articulations = split_articulations(articulation_symbol_ids);

    switch (articulation_symbol_id) {
        case SymId::ARTIC_MARCATO_ABOVE: {
            if (splitted_articulations.find(SymId::ARTIC_ACCENT_ABOVE) != splitted_articulations.end()) {
                splitted_articulations.erase(SymId::ARTIC_ACCENT_ABOVE);
                splitted_articulations.insert(SymId::ARTIC_MARCATO_ABOVE);
            } else if (splitted_articulations.find(SymId::ARTIC_MARCATO_ABOVE) != splitted_articulations.end()) {
                if (update_mode == ArticulationsUpdateMode::REMOVE) {
                    splitted_articulations.erase(SymId::ARTIC_MARCATO_ABOVE);
                }
            } else {
                splitted_articulations.insert(SymId::ARTIC_MARCATO_ABOVE);
            }
            break;
        }
        case SymId::ARTIC_ACCENT_ABOVE: {
            if (splitted_articulations.find(SymId::ARTIC_MARCATO_ABOVE) != splitted_articulations.end()) {
                splitted_articulations.erase(SymId::ARTIC_MARCATO_ABOVE);
                splitted_articulations.insert(SymId::ARTIC_ACCENT_ABOVE);
            } else if (splitted_articulations.find(SymId::ARTIC_ACCENT_ABOVE) != splitted_articulations.end()) {
                if (update_mode == ArticulationsUpdateMode::REMOVE) {
                    splitted_articulations.erase(SymId::ARTIC_ACCENT_ABOVE);
                }
            } else {
                splitted_articulations.insert(SymId::ARTIC_ACCENT_ABOVE);
            }
            break;
        }
        case SymId::ARTIC_TENUTO_ABOVE: {
            if (splitted_articulations.find(SymId::ARTIC_TENUTO_ABOVE) != splitted_articulations.end()) {
                if (update_mode == ArticulationsUpdateMode::REMOVE) {
                    splitted_articulations.erase(SymId::ARTIC_TENUTO_ABOVE);
                }
            } else {
                splitted_articulations.insert(SymId::ARTIC_TENUTO_ABOVE);
            }
            break;
        }
        case SymId::ARTIC_STACCATO_ABOVE: {
            if (splitted_articulations.find(SymId::ARTIC_STACCATO_ABOVE) != splitted_articulations.end()) {
                if (update_mode == ArticulationsUpdateMode::REMOVE) {
                    splitted_articulations.erase(SymId::ARTIC_STACCATO_ABOVE);
                }
            } else {
                splitted_articulations.insert(SymId::ARTIC_STACCATO_ABOVE);
            }
            break;
        }
        default: break;
    }
    // we no longer want to join articulations when adding or removing individual ones. combined articulations can still
    // be added by the palette (which doesn't perform any of this splitting)
    return splitted_articulations;
}

std::set<SymId> flip_articulations(const std::set<SymId> &articulation_symbol_ids, PlacementV placement) {
    std::set<SymId> result;
    switch (placement) {
        case PlacementV::ABOVE:
            for (const SymId &articulation_symbol_id : articulation_symbol_ids) {
                bool found = false;
                for (auto it = ::mu::engraving::articulation_placements.begin(); it != ::mu::engraving::articulation_placements.end();
                     ++it) {
                    if (it->second == articulation_symbol_id) {
                        result.insert(it->first);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    result.insert(articulation_symbol_id);
                }
            }
            break;
        case PlacementV::BELOW:
            for (const SymId &articulation_symbol_id : articulation_symbol_ids) {
                bool found = false;
                for (auto it = ::mu::engraving::articulation_placements.begin(); it != ::mu::engraving::articulation_placements.end();
                     ++it) {
                    if (it->first == articulation_symbol_id) {
                        result.insert(it->second);
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    result.insert(articulation_symbol_id);
                }
            }
    }

    return result;
}

double Articulation::LayoutData::optical_center() const {
    switch (sym_id.value()) {
        case SymId::HANDBELLS_MARTELLATO_LIFT: return 0.5 * item_->sym_width(SymId::HANDBELLS_MARTELLATO);
        case SymId::HANDBELLS_MALLET_LFT: return 0.5 * item_->sym_width(SymId::HANDBELLS_MALLET_BELL_ON_TABLE);
        case SymId::HANDBELLS_PLUCK_LIFT: return 0.5 * item_->sym_width(SymId::ARTIC_STACCATO_ABOVE);
        default: return 0.5 * bbox().width();
    }
}
} // namespace mu::engraving
