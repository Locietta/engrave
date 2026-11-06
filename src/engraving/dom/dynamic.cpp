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
#include "dynamic.h"

#include "../editing/edithairpin.h"
#include "../editing/textedit.h"
#include "../editing/transaction/transaction.h"
#include "../types/typesconv.h"
#include "expression.h"
#include "hairpin.h"
#include "log.h"
#include "measure.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"

using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//-----------------------------------------------------------------------------
//   Dyn
//    see: http://en.wikipedia.org/wiki/File:Dynamic's_Note_Velocity.svg
//-----------------------------------------------------------------------------

// variant with ligatures, works for both emmentaler and bravura:
const std::vector<Dyn> Dynamic::dyn_list = {
    // dynamic:
    {DynamicType::OTHER, -1, 0, true, ""},
    {DynamicType::PPPPPP, 1, 0, false,
     "<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</"
     "sym><sym>dynamicPiano</sym>"},
    {DynamicType::PPPPP, 5, 0, false,
     "<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>"},
    {DynamicType::PPPP, 10, 0, false, "<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>"},
    {DynamicType::PPP, 16, 0, false, "<sym>dynamicPiano</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>"},
    {DynamicType::PP, 33, 0, false, "<sym>dynamicPiano</sym><sym>dynamicPiano</sym>"},
    {DynamicType::P, 49, 0, false, "<sym>dynamicPiano</sym>"},

    {DynamicType::MP, 64, 0, false, "<sym>dynamicMezzo</sym><sym>dynamicPiano</sym>"},
    {DynamicType::MF, 80, 0, false, "<sym>dynamicMezzo</sym><sym>dynamicForte</sym>"},

    {DynamicType::F, 96, 0, false, "<sym>dynamicForte</sym>"},
    {DynamicType::FF, 112, 0, false, "<sym>dynamicForte</sym><sym>dynamicForte</sym>"},
    {DynamicType::FFF, 126, 0, false, "<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>"},
    {DynamicType::FFFF, 127, 0, false, "<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>"},
    {DynamicType::FFFFF, 127, 0, false,
     "<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>"},
    {DynamicType::FFFFFF, 127, 0, false,
     "<sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</"
     "sym><sym>dynamicForte</sym>"},

    {DynamicType::FP, 96, -47, true, "<sym>dynamicForte</sym><sym>dynamicPiano</sym>"},
    {DynamicType::PF, 49, 47, true, "<sym>dynamicPiano</sym><sym>dynamicForte</sym>"},

    {DynamicType::SF, 112, -18, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym>"},
    {DynamicType::SFZ, 112, -18, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>"},
    {DynamicType::SFF, 126, -18, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>"},
    {DynamicType::SFFZ, 126, -18, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>"},
    {DynamicType::SFFF, 127, -18, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym>"},
    {DynamicType::SFFFZ, 127, -18, true,
     "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>"},
    {DynamicType::SFP, 112, -47, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicPiano</sym>"},
    {DynamicType::SFPP, 112, -79, true, "<sym>dynamicSforzando</sym><sym>dynamicForte</sym><sym>dynamicPiano</sym><sym>dynamicPiano</sym>"},

    {DynamicType::RFZ, 112, -18, true, "<sym>dynamicRinforzando</sym><sym>dynamicForte</sym><sym>dynamicZ</sym>"},
    {DynamicType::RF, 112, -18, true, "<sym>dynamicRinforzando</sym><sym>dynamicForte</sym>"},
    {DynamicType::FZ, 112, -18, true, "<sym>dynamicForte</sym><sym>dynamicZ</sym>"},

    {DynamicType::M, 96, -16, true, "<sym>dynamicMezzo</sym>"},
    {DynamicType::R, 112, -18, true, "<sym>dynamicRinforzando</sym>"},
    {DynamicType::S, 112, -18, true, "<sym>dynamicSforzando</sym>"},
    {DynamicType::Z, 80, 0, true, "<sym>dynamicZ</sym>"},
    {DynamicType::N, 49, -48, true, "<sym>dynamicNiente</sym>"}};

//---------------------------------------------------------
//   dynamicsStyle
//---------------------------------------------------------

static const ElementStyle dynamics_style{
    {Sid::DYNAMICS_MIN_DISTANCE, Pid::MIN_DISTANCE},
    {Sid::AVOID_BAR_LINES, Pid::AVOID_BARLINES},
    {Sid::CENTER_ON_NOTEHEAD, Pid::CENTER_ON_NOTEHEAD},
};

//---------------------------------------------------------
//   Dynamic
//---------------------------------------------------------

Dynamic::Dynamic(DummyParentOr<Segment> parent)
    : TextBase(ElementType::DYNAMIC, parent, TextStyleType::DYNAMICS, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    velocity_ = -1;
    dynamic_type_ = DynamicType::OTHER;
    change_in_velocity_ = 128;
    vel_change_speed_ = DynamicSpeed::NORMAL;
    init_element_style(&::mu::engraving::dynamics_style);
}

Dynamic::Dynamic(const Dynamic &d) : TextBase(d) {
    dynamic_type_ = d.dynamic_type_;
    velocity_ = d.velocity_;
    change_in_velocity_ = d.change_in_velocity_;
    vel_change_speed_ = d.vel_change_speed_;
    avoid_bar_lines_value = d.avoid_bar_lines_value;
    center_on_notehead_value = d.center_on_notehead_value;
}

//---------------------------------------------------------
//   velocity
//---------------------------------------------------------

int Dynamic::velocity() const { return velocity_ <= 0 ? ::mu::engraving::Dynamic::dyn_list[int(dynamic_type())].velocity : velocity_; }

//---------------------------------------------------------
//   changeInVelocity
//---------------------------------------------------------

int Dynamic::change_in_velocity() const {
    return change_in_velocity_ >= 128 ? ::mu::engraving::Dynamic::dyn_list[int(dynamic_type())].change_in_velocity : change_in_velocity_;
}

//---------------------------------------------------------
//   setChangeInVelocity
//---------------------------------------------------------

void Dynamic::set_change_in_velocity(int val) {
    if (::mu::engraving::Dynamic::dyn_list[int(dynamic_type())].change_in_velocity == val) {
        change_in_velocity_ = 128;
    } else {
        change_in_velocity_ = val;
    }
}

//---------------------------------------------------------
//   velocityChangeLength
//    the time over which the velocity change occurs
//---------------------------------------------------------

Fraction Dynamic::velocity_change_length(BeatsPerSecond tempo) const {
    if (change_in_velocity() == 0) {
        return Fraction::from_ticks(0);
    }

    double ratio = tempo.val / Constants::default_tempo.val;
    double speed_mult;
    switch (vel_change_speed()) {
        case DynamicSpeed::SLOW: speed_mult = 1.3; break;
        case DynamicSpeed::FAST: speed_mult = 0.5; break;
        case DynamicSpeed::NORMAL:
        default: speed_mult = 0.8; break;
    }

    return Fraction::from_ticks(int(ratio * (speed_mult * double(Constants::division))));
}

//---------------------------------------------------------
//   isVelocityChangeAvailable
//---------------------------------------------------------

bool Dynamic::is_velocity_change_available() const {
    switch (dynamic_type()) {
        case DynamicType::FP:
        case DynamicType::SF:
        case DynamicType::SFZ:
        case DynamicType::SFF:
        case DynamicType::SFFZ:
        case DynamicType::SFFF:
        case DynamicType::SFFFZ:
        case DynamicType::SFP:
        case DynamicType::SFPP:
        case DynamicType::RFZ:
        case DynamicType::RF:
        case DynamicType::FZ:
        case DynamicType::M:
        case DynamicType::R:
        case DynamicType::S: return true;
        default: return false;
    }
}

//---------------------------------------------------------
//   setDynamicType
//---------------------------------------------------------

void Dynamic::set_dynamic_type(const String &tag) {
    const auto dynamic_info = parse_dynamic_text(tag);

    if (dynamic_info.first == DynamicType::OTHER) {
        LOGD("setDynamicType: other <%s>", muPrintable(tag));
    }

    set_dynamic_type(dynamic_info.first);
    set_xml_text(dynamic_info.second);
}

std::pair<DynamicType, String> Dynamic::parse_dynamic_text(const String &tag) const {
    std::string utf8_tag = tag.to_std_string();
    const std::regex dynamic_regex(R"((?:<sym>.*?</sym>)+|(?:\b)[fmnprsz]+(?:\b(?=[^>]|$)))");
    auto begin = std::sregex_iterator(utf8_tag.begin(), utf8_tag.end(), dynamic_regex);
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        const std::smatch match = *it;
        const std::string match_str = match.str();
        size_t n = ::mu::engraving::Dynamic::dyn_list.size();
        for (size_t i = 0; i < n; ++i) {
            if (TConv::to_xml(DynamicType(i)).ascii() == match_str || ::mu::engraving::Dynamic::dyn_list[i].text == match_str) {
                utf8_tag.replace(match.position(0), match.length(0), ::mu::engraving::Dynamic::dyn_list[i].text);
                return {DynamicType(i), String::from_std_string(utf8_tag)};
            }
        }
    }
    return {DynamicType::OTHER, tag};
}

String Dynamic::dynamic_text(DynamicType t) { return String::from_utf8(::mu::engraving::Dynamic::dyn_list[int(t)].text); }

Expression *Dynamic::snapped_expression() const {
    EngravingItem *item = ldata()->item_snapped_after();
    return item && item->is_expression() ? to_expression(item) : nullptr;
}

HairpinSegment *Dynamic::find_snap_before_hairpin_across_system_break() const {
    /* Normally it is the hairpin which looks for a snappable dynamic. Except if this dynamic
     * is on the first beat of next system, in which case it needs to look back for a hairpin. */
    Segment *seg = segment();
    Measure *measure = seg ? seg->measure() : nullptr;
    System *system = measure ? measure->system() : nullptr;
    bool is_on_first_beat_of_system = system && system->first_measure() == measure && seg->rtick().is_zero();
    if (!is_on_first_beat_of_system) {
        return nullptr;
    }

    Measure *prev_measure = measure->prev_measure();
    System *prev_system = prev_measure ? prev_measure->system() : nullptr;
    if (!prev_system) {
        return nullptr;
    }

    for (SpannerSegment *spanner_seg : prev_system->spanner_segments()) {
        if (!spanner_seg->is_hairpin_segment() || spanner_seg->track() != track() || spanner_seg->spanner()->tick2() != tick()) {
            continue;
        }
        HairpinSegment *hairpin_seg = to_hairpin_segment(spanner_seg);
        if (hairpin_seg->find_element_to_snap_after() == this) {
            return hairpin_seg;
        }
    }

    return nullptr;
}

bool Dynamic::accept_drop(EditData &ed) const {
    ElementType dropped_type = ed.drop_element->type();
    return dropped_type == ElementType::DYNAMIC || dropped_type == ElementType::EXPRESSION || dropped_type == ElementType::HAIRPIN;
}

EngravingItem *Dynamic::drop(Transaction &tx, EditData &ed) {
    EngravingItem *item = ed.drop_element;

    if (item->is_hairpin()) {
        EditHairpin::add_hairpin_to_dynamic(tx, score(), to_hairpin(item), this);
        return item;
    }

    if (item->is_dynamic()) {
        Dynamic *dynamic = to_dynamic(item);
        undo_change_property(Pid::DYNAMIC_TYPE, dynamic->dynamic_type());
        undo_change_property(Pid::TEXT, dynamic->xml_text());
        delete dynamic;
        ed.drop_element = this;
        return this;
    }

    if (item->is_expression()) {
        item->set_track(track());
        item->set_ownership_parent(segment());
        to_expression(item)->set_voice_assignment(voice_assignment());
        score()->undo_add_element(item);
        return item;
    }

    return nullptr;
}

int Dynamic::dynamic_velocity(DynamicType t) { return ::mu::engraving::Dynamic::dyn_list[int(t)].velocity; }

TranslatableString Dynamic::subtype_user_name() const {
    if (dynamic_type() == DynamicType::OTHER) {
        String s = plain_text().simplified();
        if (s.size() > 20) {
            s.truncate(20);
            s += u"…";
        }
        return TranslatableString::untranslatable(s);
    } else {
        return TConv::user_name(dynamic_type());
    }
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Dynamic::reset() {
    undo_reset_property(Pid::DIRECTION);
    undo_reset_property(Pid::CENTER_BETWEEN_STAVES);
    TextBase::reset();
    Expression *snapped_exp = snapped_expression();
    if (snapped_exp && !snapped_exp->offset().is_null()) {
        snapped_exp->reset();
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Dynamic::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::DYNAMIC_TYPE: return dynamic_type_;
        case Pid::VELOCITY: return velocity();
        case Pid::VELO_CHANGE:
            if (is_velocity_change_available()) {
                return change_in_velocity();
            } else {
                return PropertyValue();
            }
        case Pid::VELO_CHANGE_SPEED: return vel_change_speed_;
        case Pid::AVOID_BARLINES: return avoid_bar_lines();
        case Pid::CENTER_ON_NOTEHEAD: return center_on_notehead_value;
        case Pid::PLAY: return play_dynamic();
        case Pid::ANCHOR_TO_END_OF_PREVIOUS: return anchor_to_end_of_previous();
        default: return TextBase::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Dynamic::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::DYNAMIC_TYPE:
            if (v.type() == PType::DYNAMIC_TYPE) {
                set_dynamic_type(v.value<DynamicType>());
                break;
            }
            set_dynamic_type(v.value<String>());
            break;
        case Pid::VELOCITY: velocity_ = v.to_int(); break;
        case Pid::VELO_CHANGE:
            if (is_velocity_change_available()) {
                set_change_in_velocity(v.to_int());
            }
            break;
        case Pid::VELO_CHANGE_SPEED: vel_change_speed_ = v.value<DynamicSpeed>(); break;
        case Pid::AVOID_BARLINES: set_avoid_bar_lines(v.to_bool()); break;
        case Pid::CENTER_ON_NOTEHEAD: center_on_notehead_value = v.to_bool(); break;
        case Pid::PLAY: set_play_dynamic(v.to_bool()); break;
        case Pid::ANCHOR_TO_END_OF_PREVIOUS: set_anchor_to_end_of_previous(v.to_bool()); break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Dynamic::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::DYNAMICS;
        case Pid::VELOCITY: return -1;
        case Pid::VELO_CHANGE:
            if (is_velocity_change_available()) {
                return ::mu::engraving::Dynamic::dyn_list[int(dynamic_type())].change_in_velocity;
            } else {
                return PropertyValue();
            }
        case Pid::VELO_CHANGE_SPEED: return DynamicSpeed::NORMAL;
        case Pid::PLAY: return true;
        case Pid::ANCHOR_TO_END_OF_PREVIOUS: return false;
        default: return TextBase::property_default(id);
    }
}

Sid Dynamic::get_property_style(Pid pid) const {
    switch (pid) {
        case Pid::PLACEMENT: return Sid::DYNAMICS_PLACEMENT;
        default: return TextBase::get_property_style(pid);
    }
}

//---------------------------------------------------------
//   screenReaderInfo
//---------------------------------------------------------

} // namespace mu::engraving

//---------------------------------------------------------
//   hasLeftHairpin
//---------------------------------------------------------

bool Dynamic::has_left_grip() const {
    if (segment()->tick().is_zero()) {
        return false; // Don't show the left grip for the leftmost dynamic with tick zero
    }
    return left_hairpin_ == nullptr;
}

//---------------------------------------------------------
//   hasRightHairpin
//---------------------------------------------------------

bool Dynamic::has_right_grip() const { return right_hairpin_ == nullptr; }

//---------------------------------------------------------
//   findAdjacentHairpins
//---------------------------------------------------------

void Dynamic::find_adjacent_hairpins() {
    left_hairpin_ = nullptr;
    right_hairpin_ = nullptr;

    const Fraction tick = segment()->tick();
    const int int_tick = tick.ticks();

    const auto &spanners = score()->spanner_map().find_overlapping(int_tick - 1, int_tick + 1);
    for (auto i : spanners) {
        Spanner *sp = i.value;
        if (sp->track() == track() && sp->is_hairpin()) {
            Hairpin *hp = to_hairpin(sp);
            if (hp->tick() == tick) {
                right_hairpin_ = hp;
            } else if (hp->tick2() == tick) {
                left_hairpin_ = hp;
            }
        }
    }
}

Shape Dynamic::sym_shape_with_cutouts(SymId id) const {
    Staff *stf = staff();
    double staff_mag = stf ? stf->staff_mag(tick()) : 1.0;
    Shape shape = score()->engraving_font()->shape_with_cutouts(id, mag_s() * staff_mag * symbol_scale());
    for (ShapeElement &element : shape.elements()) {
        element.set_item(this);
    }

    return shape;
}

Dyn Dynamic::dyn_info(DynamicType type) { return ::mu::engraving::Dynamic::dyn_list[static_cast<int>(type)]; }
