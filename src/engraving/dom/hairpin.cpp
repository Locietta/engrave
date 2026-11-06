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

#include "hairpin.h"

#include <cmath>

#include "draw/types/transform.h"

#include "../editing/elementeditdata.h"
#include "types/typesconv.h"

#include "dynamic.h"
#include "measure.h"
#include "score.h"
#include "segment.h"
#include "system.h"
#include "text.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   hairpinStyle
//---------------------------------------------------------

static const ElementStyle hairpin_style{
    {Sid::HAIRPIN_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::HAIRPIN_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::HAIRPIN_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::HAIRPIN_TEXT, Pid::BEGIN_TEXT},
    {Sid::HAIRPIN_TEXT_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::HAIRPIN_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::HAIRPIN_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::HAIRPIN_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::HAIRPIN_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::HAIRPIN_TEXT, Pid::CONTINUE_TEXT},
    {Sid::HAIRPIN_TEXT_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::HAIRPIN_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::HAIRPIN_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::HAIRPIN_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::HAIRPIN_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::HAIRPIN_TEXT_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::HAIRPIN_POSITION, Pid::END_TEXT_POSITION},
    {Sid::HAIRPIN_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::HAIRPIN_HEIGHT, Pid::HAIRPIN_HEIGHT},
    {Sid::HAIRPIN_CONT_HEIGHT, Pid::HAIRPIN_CONT_HEIGHT},
    {Sid::HAIRPIN_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::HAIRPIN_LINE_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::HAIRPIN_LINE_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {
        Sid::HAIRPIN_FONT_SPATIUM_DEPENDENT,
        Pid::TEXT_SIZE_SPATIUM_DEPENDENT,
    },
    {Sid::HAIRPIN_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::HAIRPIN_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::HAIRPIN_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::HAIRPIN_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::HAIRPIN_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::HAIRPIN_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::HAIRPIN_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::HAIRPIN_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},
    {Sid::HAIRPIN_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::HAIRPIN_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::HAIRPIN_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

//---------------------------------------------------------
//   HairpinSegment
//---------------------------------------------------------

HairpinSegment::HairpinSegment(Hairpin *sp)
    : TextLineBaseSegment(ElementType::HAIRPIN_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

bool HairpinSegment::accept_drop(EditData &data) const {
    EngravingItem *e = data.drop_element;
    if (e->is_dynamic()) {
        return true;
    }
    return false;
}

EngravingItem *HairpinSegment::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.drop_element;
    if (!e->is_dynamic()) {
        return nullptr;
    }

    if (EngravingItem *item = ldata()->item_snapped_after()) {
        if (item->is_dynamic()) {
            return item->drop(tx, data);
        }
    }

    Fraction end_tick = hairpin()->tick2();
    Measure *measure = score()->tick2measure(end_tick);
    Segment *segment = measure->get_chord_rest_or_time_tick_segment(end_tick);

    Dynamic *d = to_dynamic(e->clone());
    d->set_track(hairpin()->track());
    d->set_ownership_parent(segment);
    d->set_voice_assignment(hairpin()->voice_assignment());
    score()->undo_add_element(d);

    return d;
}

void HairpinSegment::set_property_flags(Pid id, PropertyFlags f) {
    int i = get_property_flags_idx(id);
    if (i == -1) {
        // Can happen (for example when edit-dragging the aperture) that we're editing
        // the HairpinSegment but the relevant property belongs to the Hairpin.
        hairpin()->set_property_flags(id, f);
        return;
    }
    property_flags_list_[i] = f;
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *HairpinSegment::property_delegate(Pid pid) const {
    if (pid == Pid::HAIRPIN_TYPE || pid == Pid::VELO_CHANGE || pid == Pid::VELO_CHANGE_METHOD || pid == Pid::SINGLE_NOTE_DYNAMICS ||
        pid == Pid::HAIRPIN_CIRCLEDTIP || pid == Pid::HAIRPIN_HEIGHT || pid == Pid::HAIRPIN_CONT_HEIGHT || pid == Pid::LINE_STYLE ||
        pid == Pid::VOICE_ASSIGNMENT || pid == Pid::DIRECTION || pid == Pid::CENTER_BETWEEN_STAVES) {
        return spanner();
    }
    return TextLineBaseSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid HairpinSegment::get_property_style(Pid pid) const {
    switch (pid) {
        case Pid::BEGIN_TEXT:
            switch (hairpin()->hairpin_type()) {
                default: return Sid::HAIRPIN_TEXT;
                case HairpinType::CRESC_LINE: return Sid::HAIRPIN_CRESC_TEXT;
                case HairpinType::DIM_LINE: return Sid::HAIRPIN_DECRESC_TEXT;
            }
            break;
        case Pid::CONTINUE_TEXT:
            switch (hairpin()->hairpin_type()) {
                default: return Sid::HAIRPIN_TEXT;
                case HairpinType::CRESC_LINE: return Sid::HAIRPIN_CRESC_CONT_TEXT;
                case HairpinType::DIM_LINE: return Sid::HAIRPIN_DECRESC_CONT_TEXT;
            }
            break;
        case Pid::LINE_STYLE: return hairpin()->is_line_type() ? Sid::HAIRPIN_LINE_LINE_STYLE : Sid::HAIRPIN_LINE_STYLE;
        case Pid::DASH_LINE_LEN: return hairpin()->is_line_type() ? Sid::HAIRPIN_LINE_DASH_LINE_LEN : Sid::HAIRPIN_DASH_LINE_LEN;
        case Pid::DASH_GAP_LEN: return hairpin()->is_line_type() ? Sid::HAIRPIN_LINE_DASH_GAP_LEN : Sid::HAIRPIN_DASH_GAP_LEN;
        default: break;
    }
    return TextLineBaseSegment::get_property_style(pid);
}

EngravingItem *HairpinSegment::find_element_to_snap_before(bool ignore_invisible) const {
    TextBase *start_dyn_or_expr = find_start_dynamic_or_expression(ignore_invisible);
    if (start_dyn_or_expr) {
        return start_dyn_or_expr;
    }

    Hairpin *this_hairpin = hairpin();
    Fraction start_tick = hairpin()->tick();

    auto intervals = score()->spanner_map().find_overlapping(start_tick.ticks(), start_tick.ticks());
    for (auto interval : intervals) {
        Spanner *spanner = interval.value;
        bool is_valid_hairpin = spanner->is_hairpin() && !spanner->segments_empty() && spanner != this_hairpin &&
                                (spanner->add_to_skyline() || !ignore_invisible);
        if (!is_valid_hairpin) {
            continue;
        }
        Hairpin *preceding_hairpin = to_hairpin(spanner);
        bool ends_match = preceding_hairpin->track() == this_hairpin->track() && preceding_hairpin->tick2() == start_tick &&
                          preceding_hairpin->place_above() == this_hairpin->place_above() &&
                          to_hairpin(spanner)->voice_assignment() == this_hairpin->voice_assignment();
        if (ends_match && preceding_hairpin->snap_to_item_after()) {
            return preceding_hairpin->back_segment();
        }
    }

    return nullptr;
}

EngravingItem *HairpinSegment::find_element_to_snap_after(bool ignore_invisible, bool require_playable) const {
    // Note: we don't need to look for a hairpin after.
    // It is the next hairpin which looks for a hairpin before.
    return find_end_dynamic_or_expression(ignore_invisible, require_playable);
}

TextBase *HairpinSegment::find_start_dynamic_or_expression(bool ignore_invisible) const {
    Fraction ref_tick = hairpin()->tick();
    Measure *measure = score()->tick2measure(ref_tick);
    if (!measure) {
        return nullptr;
    }

    std::vector<TextBase *> dynamics_and_expr;
    dynamics_and_expr.reserve(2);

    for (Segment *segment = measure->last(); segment; segment = segment->prev1()) {
        Fraction segment_tick = segment->tick();
        if (segment_tick > ref_tick) {
            continue;
        }
        if (segment_tick < ref_tick) {
            break;
        }
        for (EngravingItem *item : segment->annotations()) {
            if (!item->is_dynamic() && !item->is_expression()) {
                continue;
            }
            if (ignore_invisible && !item->add_to_skyline()) {
                continue;
            }
            bool ends_match = item->track() == hairpin()->track() && item->placement() == placement() &&
                              item->get_property(Pid::VOICE_ASSIGNMENT) == hairpin()->get_property(Pid::VOICE_ASSIGNMENT);
            if (ends_match) {
                dynamics_and_expr.push_back(to_text_base(item));
            }
        }
        if (dynamics_and_expr.size() > 0) {
            break;
        }
    }

    if (dynamics_and_expr.size() == 0) {
        return nullptr;
    }

    if (dynamics_and_expr.size() > 1) {
        std::sort(dynamics_and_expr.begin(), dynamics_and_expr.end(), [](TextBase *item1, TextBase *item2) {
            bool dynamic_before_expression = item1->is_dynamic() && item2->is_expression();
            bool one_is_anchor_to_previous = item1->is_dynamic() && to_dynamic(item1)->anchor_to_end_of_previous() && item2->is_dynamic() &&
                                             !to_dynamic(item2)->anchor_to_end_of_previous();
            return dynamic_before_expression || one_is_anchor_to_previous;
        });
    }

    return dynamics_and_expr.back();
}

TextBase *HairpinSegment::find_end_dynamic_or_expression(bool ignore_invisible, bool require_playable) const {
    Fraction ref_tick = hairpin()->tick2();
    Measure *measure = score()->tick2measure(ref_tick - Fraction::eps());
    if (!measure) {
        return nullptr;
    }

    std::vector<TextBase *> dynamics_and_expr;
    dynamics_and_expr.reserve(2);

    for (Segment *segment = measure->first(); segment; segment = segment->next1()) {
        Fraction segment_tick = segment->tick();
        if (segment_tick < ref_tick) {
            continue;
        }
        if (segment_tick > ref_tick) {
            break;
        }
        for (EngravingItem *item : segment->annotations()) {
            if (!item->is_dynamic() && !item->is_expression()) {
                continue;
            }
            if (ignore_invisible && !item->add_to_skyline()) {
                continue;
            }
            if (require_playable && (!item->is_dynamic() || !to_dynamic(item)->play_dynamic())) {
                continue;
            }
            bool ends_match = item->track() == hairpin()->track() && item->placement() == placement() &&
                              item->get_property(Pid::VOICE_ASSIGNMENT) == hairpin()->get_property(Pid::VOICE_ASSIGNMENT);
            if (ends_match) {
                dynamics_and_expr.push_back(to_text_base(item));
            }
        }
        if (dynamics_and_expr.size() > 0) {
            break;
        }
    }

    if (dynamics_and_expr.size() == 0) {
        return nullptr;
    }

    if (dynamics_and_expr.size() > 1) {
        std::sort(dynamics_and_expr.begin(), dynamics_and_expr.end(), [](TextBase *item1, TextBase *item2) {
            bool dynamic_before_expression = item1->is_dynamic() && item2->is_expression();
            bool one_is_anchor_to_previous = item1->is_dynamic() && to_dynamic(item1)->anchor_to_end_of_previous() && item2->is_dynamic() &&
                                             !to_dynamic(item2)->anchor_to_end_of_previous();
            return dynamic_before_expression || one_is_anchor_to_previous;
        });
    }

    return dynamics_and_expr.front();
}

Sid Hairpin::get_property_style(Pid pid) const {
    switch (pid) {
        case Pid::BEGIN_TEXT:
            switch (hairpin_type()) {
                default: return Sid::HAIRPIN_TEXT;
                case HairpinType::CRESC_LINE: return Sid::HAIRPIN_CRESC_TEXT;
                case HairpinType::DIM_LINE: return Sid::HAIRPIN_DECRESC_TEXT;
            }
            break;
        case Pid::CONTINUE_TEXT:
            switch (hairpin_type()) {
                default: return Sid::HAIRPIN_TEXT;
                case HairpinType::CRESC_LINE: return Sid::HAIRPIN_CRESC_CONT_TEXT;
                case HairpinType::DIM_LINE: return Sid::HAIRPIN_DECRESC_CONT_TEXT;
            }
            break;
        case Pid::LINE_STYLE: return is_line_type() ? Sid::HAIRPIN_LINE_LINE_STYLE : Sid::HAIRPIN_LINE_STYLE;
        case Pid::DASH_LINE_LEN: return is_line_type() ? Sid::HAIRPIN_LINE_DASH_LINE_LEN : Sid::HAIRPIN_DASH_LINE_LEN;
        case Pid::DASH_GAP_LEN: return is_line_type() ? Sid::HAIRPIN_LINE_DASH_GAP_LEN : Sid::HAIRPIN_DASH_GAP_LEN;
        case Pid::PLACEMENT: return Sid::HAIRPIN_PLACEMENT;
        default: break;
    }
    return TextLineBase::get_property_style(pid);
}

//---------------------------------------------------------
//   Hairpin
//---------------------------------------------------------

Hairpin::Hairpin(DummyParentOr<EngravingItem> parent) : TextLineBase(ElementType::HAIRPIN, parent) {
    init_element_style(&::mu::engraving::hairpin_style);

    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::BEGIN_HOOK_HEIGHT);
    reset_property(Pid::END_HOOK_HEIGHT);
    reset_property(Pid::HAIRPIN_TYPE);
    reset_property(Pid::LINE_VISIBLE);

    hairpin_circled_tip_ = false;
    velo_change_ = 0;
    single_note_dynamics_ = true;
    velo_change_method_ = ChangeMethod::NORMAL;
}

DynamicType Hairpin::dynamic_type_from() const {
    if (hairpin_type_ == HairpinType::CRESC_HAIRPIN && hairpin_circled_tip()) {
        return DynamicType::N;
    }

    muse::ByteArray ba = begin_text().to_ascii();
    return TConv::dynamic_type(ba.const_char());
}

DynamicType Hairpin::dynamic_type_to() const {
    if (hairpin_type_ == HairpinType::DIM_HAIRPIN && hairpin_circled_tip()) {
        return DynamicType::N;
    }

    muse::ByteArray ba = end_text().to_ascii();
    return TConv::dynamic_type(ba.const_char());
}

const Dynamic *Hairpin::dynamic_snapped_before() const {
    const LineSegment *seg = front_segment();
    if (!seg) {
        return nullptr;
    }

    const EngravingItem *item = seg->ldata()->item_snapped_before();
    if (!item || !item->is_dynamic()) {
        return nullptr;
    }

    return to_dynamic(item);
}

const Dynamic *Hairpin::dynamic_snapped_after() const {
    const LineSegment *seg = back_segment();
    if (!seg) {
        return nullptr;
    }

    const EngravingItem *item = seg->ldata()->item_snapped_after();
    if (!item || !item->is_dynamic()) {
        return nullptr;
    }

    return to_dynamic(item);
}

//---------------------------------------------------------
//   setHairpinType
//---------------------------------------------------------

void Hairpin::set_hairpin_type(HairpinType val) {
    if (hairpin_type_ == val) {
        return;
    }
    hairpin_type_ = val;
    style_changed();
}

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle hairpin_segment_style{
    {Sid::HAIRPIN_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

LineSegment *Hairpin::create_line_segment() {
    HairpinSegment *h = new HairpinSegment(this);
    h->set_track(track());
    h->init_element_style(&::mu::engraving::hairpin_segment_style);
    return h;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Hairpin::get_property(Pid id) const {
    switch (id) {
        case Pid::HAIRPIN_CIRCLEDTIP: return hairpin_circled_tip_;
        case Pid::HAIRPIN_TYPE: return int(hairpin_type_);
        case Pid::VELO_CHANGE: return velo_change_;
        case Pid::HAIRPIN_HEIGHT: return hairpin_height_;
        case Pid::HAIRPIN_CONT_HEIGHT: return hairpin_cont_height_;
        case Pid::SINGLE_NOTE_DYNAMICS: return single_note_dynamics_;
        case Pid::VELO_CHANGE_METHOD: return velo_change_method_;
        case Pid::VOICE_ASSIGNMENT: return voice_assignment();
        case Pid::CENTER_BETWEEN_STAVES: return center_between_staves();
        case Pid::DIRECTION: return direction();
        case Pid::SNAP_BEFORE: return snap_to_item_before();
        case Pid::SNAP_AFTER: return snap_to_item_after();

        default: return TextLineBase::get_property(id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Hairpin::set_property(Pid id, const PropertyValue &v) {
    switch (id) {
        case Pid::HAIRPIN_CIRCLEDTIP: hairpin_circled_tip_ = v.to_bool(); break;
        case Pid::HAIRPIN_TYPE: set_hairpin_type(HairpinType(v.to_int())); break;
        case Pid::VELO_CHANGE: velo_change_ = v.to_int(); break;
        case Pid::HAIRPIN_HEIGHT: hairpin_height_ = v.value<Spatium>(); break;
        case Pid::HAIRPIN_CONT_HEIGHT: hairpin_cont_height_ = v.value<Spatium>(); break;
        case Pid::SINGLE_NOTE_DYNAMICS: single_note_dynamics_ = v.to_bool(); break;
        case Pid::VELO_CHANGE_METHOD: velo_change_method_ = v.value<ChangeMethod>(); break;
        case Pid::VOICE_ASSIGNMENT: set_voice_assignment(v.value<VoiceAssignment>()); break;
        case Pid::CENTER_BETWEEN_STAVES: set_center_between_staves(v.value<AutoOnOff>()); break;
        case Pid::DIRECTION: set_direction(v.value<DirectionV>()); break;
        case Pid::SNAP_BEFORE: set_snap_to_item_before(v.to_bool()); break;
        case Pid::SNAP_AFTER: set_snap_to_item_after(v.to_bool()); break;
        default: return TextLineBase::set_property(id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Hairpin::property_default(Pid id) const {
    switch (id) {
        case Pid::HAIRPIN_CIRCLEDTIP: return false;

        case Pid::VELO_CHANGE: return 0;

        case Pid::BEGIN_TEXT:
            if (hairpin_type_ == HairpinType::CRESC_LINE) {
                return style().style_v(Sid::HAIRPIN_CRESC_TEXT);
            }
            if (hairpin_type_ == HairpinType::DIM_LINE) {
                return style().style_v(Sid::HAIRPIN_DECRESC_TEXT);
            }
            return String();

        case Pid::CONTINUE_TEXT:
            if (hairpin_type_ == HairpinType::CRESC_LINE) {
                return style().style_v(Sid::HAIRPIN_CRESC_CONT_TEXT);
            }
            if (hairpin_type_ == HairpinType::DIM_LINE) {
                return style().style_v(Sid::HAIRPIN_DECRESC_CONT_TEXT);
            }
            return String();

        case Pid::END_TEXT: return String();

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::LEFT;

        case Pid::BEGIN_TEXT_OFFSET:
        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF());

        case Pid::BEGIN_HOOK_TYPE:
        case Pid::END_HOOK_TYPE: return HookType::NONE;

        case Pid::BEGIN_HOOK_HEIGHT:
        case Pid::END_HOOK_HEIGHT: return 1.9_sp;

        case Pid::LINE_VISIBLE: return true;

        case Pid::HAIRPIN_TYPE: return int(HairpinType::CRESC_HAIRPIN);

        case Pid::SINGLE_NOTE_DYNAMICS: return true;

        case Pid::VELO_CHANGE_METHOD: return ChangeMethod::NORMAL;

        case Pid::PLACEMENT: return style().style_v(Sid::HAIRPIN_PLACEMENT);

        case Pid::VOICE_ASSIGNMENT: return VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;

        case Pid::CENTER_BETWEEN_STAVES: return AutoOnOff::AUTO;

        case Pid::DIRECTION: return DirectionV::AUTO;

        case Pid::SNAP_BEFORE: return true;
        case Pid::SNAP_AFTER: return true;

        case Pid::TEXT_STYLE: return TextStyleType::HAIRPIN;

        default: return TextLineBase::property_default(id);
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

PointF Hairpin::line_pos(Grip grip, System **system) const {
    bool start = grip == Grip::START;

    Segment *segment = start ? start_segment() : end_segment();
    if (!segment) {
        return PointF();
    }

    if (!start) {
        Fraction cur_tick = segment->tick();
        Segment *prev_seg = segment->prev1(SegmentType::END_BAR_LINE);
        if (prev_seg && prev_seg->tick() == cur_tick) {
            segment = prev_seg;
        }
    }

    *system = segment->measure()->system();
    double x = segment->x() + segment->measure()->x();
    if (!start) {
        x -= spatium();
    }

    return PointF(x, 0.0);
}

void Hairpin::reset() {
    undo_reset_property(Pid::DIRECTION);
    undo_reset_property(Pid::CENTER_BETWEEN_STAVES);
    TextLineBase::reset();
}

muse::TranslatableString Hairpin::subtype_user_name() const {
    switch (hairpin_type()) {
        case HairpinType::CRESC_HAIRPIN: return TranslatableString("engraving/hairpintype", "Crescendo hairpin");
        case HairpinType::DIM_HAIRPIN: return TranslatableString("engraving/hairpintype", "Diminuendo hairpin");
        case HairpinType::CRESC_LINE: return TranslatableString("engraving/hairpintype", "Crescendo line");
        case HairpinType::DIM_LINE: return TranslatableString("engraving/hairpintype", "Diminuendo line");
        default: return TranslatableString("engraving/hairpintype", "Custom");
    }
}

Sid Hairpin::default_pos_sid() const {
    if (is_line_type()) {
        return place_above() ? Sid::HAIRPIN_LINE_POS_ABOVE : Sid::HAIRPIN_LINE_POS_BELOW;
    }
    return place_above() ? Sid::HAIRPIN_POS_ABOVE : Sid::HAIRPIN_POS_BELOW;
}
} // namespace mu::engraving
