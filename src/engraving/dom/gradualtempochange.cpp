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

#include "gradualtempochange.h"

#include "log.h"
#include "measure.h"
#include "rehearsalmark.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "system.h"
#include "tempotext.h"
#include "text.h"
#include "types/typesconv.h"

using namespace mu;
using namespace mu::engraving;

static const ElementStyle tempo_style{
    {Sid::TEMPO_CHANGE_SYSTEM_FLAG, Pid::SYSTEM_FLAG},
    {Sid::TEMPO_CHANGE_PLACEMENT, Pid::PLACEMENT},
    {Sid::TEMPO_CHANGE_MIN_DISTANCE, Pid::MIN_DISTANCE},

    {Sid::TEMPO_CHANGE_COLOR, Pid::COLOR},

    {Sid::TEMPO_CHANGE_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::TEMPO_CHANGE_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::TEMPO_CHANGE_FONT_FACE, Pid::END_FONT_FACE},

    {Sid::TEMPO_CHANGE_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::TEMPO_CHANGE_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::TEMPO_CHANGE_FONT_SIZE, Pid::END_FONT_SIZE},

    {Sid::TEMPO_CHANGE_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::TEMPO_CHANGE_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::TEMPO_CHANGE_FONT_STYLE, Pid::END_FONT_STYLE},

    {Sid::TEMPO_CHANGE_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::TEMPO_CHANGE_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::TEMPO_CHANGE_ALIGN, Pid::END_TEXT_ALIGN},

    {Sid::TEMPO_CHANGE_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::TEMPO_CHANGE_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::TEMPO_CHANGE_POSITION, Pid::END_TEXT_POSITION},

    {Sid::TEMPO_CHANGE_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
    {Sid::TEMPO_CHANGE_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::TEMPO_CHANGE_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::TEMPO_CHANGE_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::TEMPO_CHANGE_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::TEMPO_CHANGE_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},

    {Sid::GRADUAL_TEMPO_CHANGE_END_LINE_ARROW_HEIGHT, Pid::END_LINE_ARROW_HEIGHT},
    {Sid::GRADUAL_TEMPO_CHANGE_END_LINE_ARROW_WIDTH, Pid::END_LINE_ARROW_WIDTH},
    {Sid::GRADUAL_TEMPO_CHANGE_BEGIN_LINE_ARROW_HEIGHT, Pid::BEGIN_LINE_ARROW_HEIGHT},
    {Sid::GRADUAL_TEMPO_CHANGE_BEGIN_LINE_ARROW_WIDTH, Pid::BEGIN_LINE_ARROW_WIDTH},
    {Sid::GRADUAL_TEMPO_CHANGE_END_FILLED_ARROW_HEIGHT, Pid::END_FILLED_ARROW_HEIGHT},
    {Sid::GRADUAL_TEMPO_CHANGE_END_FILLED_ARROW_WIDTH, Pid::END_FILLED_ARROW_WIDTH},
    {Sid::GRADUAL_TEMPO_CHANGE_BEGIN_FILLED_ARROW_HEIGHT, Pid::BEGIN_FILLED_ARROW_HEIGHT},
    {Sid::GRADUAL_TEMPO_CHANGE_BEGIN_FILLED_ARROW_WIDTH, Pid::BEGIN_FILLED_ARROW_WIDTH},

    {Sid::TEMPO_CHANGE_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::TEMPO_CHANGE_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::TEMPO_CHANGE_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
};

static const ElementStyle tempo_segment_style{{Sid::TEMPO_CHANGE_MIN_DISTANCE, Pid::MIN_DISTANCE}};

static const std::unordered_map<GradualTempoChangeType, double> default_factors_map{
    {GradualTempoChangeType::ACCELERANDO, 1.33}, {GradualTempoChangeType::ALLARGANDO, 0.75}, {GradualTempoChangeType::CALANDO, 0.5},
    {GradualTempoChangeType::LENTANDO, 0.75},    {GradualTempoChangeType::MORENDO, 0.5},     {GradualTempoChangeType::PRECIPITANDO, 1.15},
    {GradualTempoChangeType::RALLENTANDO, 0.75}, {GradualTempoChangeType::RITARDANDO, 0.75}, {GradualTempoChangeType::SMORZANDO, 0.5},
    {GradualTempoChangeType::SOSTENUTO, 0.95},   {GradualTempoChangeType::STRINGENDO, 1.5}};

GradualTempoChange::GradualTempoChange(DummyParentOr<EngravingItem> parent)
    : TextLineBase(ElementType::GRADUAL_TEMPO_CHANGE, parent, ElementFlag::SYSTEM) {
    init_element_style(&::tempo_style);

    reset_property(Pid::LINE_VISIBLE);
    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::BEGIN_TEXT);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::END_TEXT);
}

GradualTempoChange *GradualTempoChange::clone() const { return new GradualTempoChange(*this); }

LineSegment *GradualTempoChange::create_line_segment() {
    GradualTempoChangeSegment *line_segment = new GradualTempoChangeSegment(this);
    line_segment->set_track(track());
    return line_segment;
}

GradualTempoChangeType GradualTempoChange::tempo_change_type() const { return tempo_change_type_; }

ChangeMethod GradualTempoChange::easing_method() const { return tempo_easing_method_; }

void GradualTempoChange::set_tempo_change_type(const GradualTempoChangeType type) { tempo_change_type_ = type; }

double GradualTempoChange::tempo_change_factor() const {
    if (tempo_change_factor_.has_value()) {
        return tempo_change_factor_.value();
    }

    return muse::value(::default_factors_map, tempo_change_type_, 1.0);
}

PropertyValue GradualTempoChange::get_property(Pid id) const {
    switch (id) {
        case Pid::TEMPO_CHANGE_TYPE: return tempo_change_type_;
        case Pid::TEMPO_EASING_METHOD: return tempo_easing_method_;
        case Pid::TEMPO_CHANGE_FACTOR: return tempo_change_factor();
        case Pid::SNAP_AFTER: return snap_to_item_after();
        case Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK: return align_right_of_rehearsal_mark_;
        default: return TextLineBase::get_property(id);
    }
}

bool GradualTempoChange::set_property(Pid id, const PropertyValue &val) {
    switch (id) {
        case Pid::TEMPO_CHANGE_TYPE: tempo_change_type_ = val.value<GradualTempoChangeType>(); break;
        case Pid::TEMPO_EASING_METHOD: tempo_easing_method_ = val.value<ChangeMethod>(); break;
        case Pid::TEMPO_CHANGE_FACTOR: tempo_change_factor_ = val.to_real(); break;
        case Pid::SNAP_AFTER: set_snap_to_item_after(val.to_bool()); break;
        case Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK: align_right_of_rehearsal_mark_ = val.to_bool(); break;
        default:
            if (!TextLineBase::set_property(id, val)) {
                return false;
            }
            break;
    }

    trigger_layout();
    return true;
}

PropertyValue GradualTempoChange::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::ALIGN: return style().style_v(Sid::TEMPO_CHANGE_ALIGN);

        case Pid::LINE_WIDTH: return style().style_v(Sid::TEMPO_CHANGE_LINE_WIDTH);
        case Pid::LINE_STYLE: return style().style_v(Sid::TEMPO_CHANGE_LINE_STYLE);
        case Pid::LINE_VISIBLE: return true;

        case Pid::CONTINUE_TEXT_OFFSET:
        case Pid::END_TEXT_OFFSET: return PropertyValue::from_value(PointF(0, 0));

        case Pid::BEGIN_FONT_STYLE: return style().style_v(Sid::TEMPO_CHANGE_FONT_STYLE);

        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT:
        case Pid::END_TEXT: return "";

        case Pid::BEGIN_HOOK_TYPE: return HookType::NONE;

        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::AUTO;

        case Pid::TEMPO_CHANGE_TYPE: return GradualTempoChangeType::UNDEFINED;
        case Pid::TEMPO_EASING_METHOD: return ChangeMethod::NORMAL;
        case Pid::TEMPO_CHANGE_FACTOR: return muse::value(::default_factors_map, tempo_change_type_, 1.0);

        case Pid::SNAP_AFTER: return true;

        case Pid::TEMPO_ALIGN_RIGHT_OF_REHEARSAL_MARK: return true;

        case Pid::TEXT_STYLE: return TextStyleType::TEMPO_CHANGE;

        default: return TextLineBase::property_default(property_id);
    }
}

Sid GradualTempoChange::get_property_style(Pid id) const {
    switch (id) {
        case Pid::PLACEMENT: return Sid::TEMPO_CHANGE_PLACEMENT;
        case Pid::BEGIN_FONT_FACE:
        case Pid::CONTINUE_FONT_FACE:
        case Pid::END_FONT_FACE: return Sid::TEMPO_CHANGE_FONT_FACE;
        case Pid::BEGIN_FONT_SIZE:
        case Pid::CONTINUE_FONT_SIZE:
        case Pid::END_FONT_SIZE: return Sid::TEMPO_CHANGE_FONT_SIZE;
        case Pid::BEGIN_FONT_STYLE:
        case Pid::CONTINUE_FONT_STYLE:
        case Pid::END_FONT_STYLE: return Sid::TEMPO_CHANGE_FONT_STYLE;
        case Pid::BEGIN_TEXT_ALIGN:
        case Pid::CONTINUE_TEXT_ALIGN:
        case Pid::END_TEXT_ALIGN: return Sid::TEMPO_CHANGE_ALIGN;
        case Pid::BEGIN_TEXT: return Sid::LET_RING_TEXT;
        default: break;
    }
    return TextLineBase::get_property_style(id);
}

bool GradualTempoChange::adjust_for_rehearsal_mark(bool start) const {
    const Segment *segment = start ? start_segment() : end_segment();
    if (!align_right_of_rehearsal_mark_ || !segment) {
        return false;
    }

    const RehearsalMark *rehearsal_mark = to_rehearsal_mark(segment->find_annotation(ElementType::REHEARSAL_MARK, track(), track()));
    if (!rehearsal_mark) {
        return false;
    }

    double staff_height = staff() && place_below() ? staff()->staff_height(tick()) : 0.0;
    double tempo_change_pos = staff_height + default_pos().y() + (autoplace() ? absolute_from_spatium(min_distance()) : 0.0);
    RectF rehearsal_mark_bbox = rehearsal_mark ? rehearsal_mark->ldata()->bbox().translated(rehearsal_mark->pos()) : RectF();

    const bool same_side = place_above() == rehearsal_mark->place_above();
    const bool collision = muse::real_is_equal_or_more(rehearsal_mark_bbox.bottom(), tempo_change_pos) &&
                           muse::real_is_equal_or_less(rehearsal_mark_bbox.top(), tempo_change_pos);

    return same_side && collision;
}

PointF GradualTempoChange::line_pos(Grip grip, System **system) const {
    bool start = grip == Grip::START;
    PointF default_pos = TextLineBase::line_pos(grip, system);
    if (!adjust_for_rehearsal_mark(start)) {
        return default_pos;
    }

    const Segment *segment = start ? start_segment() : end_segment();
    const RehearsalMark *rehearsal_mark = to_rehearsal_mark(segment->find_annotation(ElementType::REHEARSAL_MARK, track(), track()));
    RectF rehearsal_mark_bbox = rehearsal_mark ? rehearsal_mark->ldata()->bbox().translated(rehearsal_mark->pos()) : RectF();

    PointF rehearsal_mark_pos = segment->pos() + segment->measure()->pos();
    rehearsal_mark_bbox.translate(rehearsal_mark_pos);

    Text *text =
        start ? to_gradual_tempo_change_segment(front_segment())->text() : to_gradual_tempo_change_segment(back_segment())->end_text();

    const double sp = spatium();

    double padding = sp;
    if (text) {
        const double font_size_scale_factor = text->size() / 10.0;
        padding = 0.5 * sp * font_size_scale_factor;
    }

    padding *= start ? 1.0 : -1.0;
    double x = (start ? rehearsal_mark_bbox.right() : rehearsal_mark_bbox.left()) + padding;

    *system = segment->measure()->system();

    x = start ? std::max(x, default_pos.x()) : x;

    return PointF(x, 0.0);
}

TranslatableString GradualTempoChange::subtype_user_name() const { return TConv::user_name(tempo_change_type_); }

Sid GradualTempoChange::default_pos_sid() const { return place_above() ? Sid::TEMPO_CHANGE_POS_ABOVE : Sid::TEMPO_CHANGE_POS_BELOW; }

GradualTempoChangeSegment::GradualTempoChangeSegment(GradualTempoChange *annotation)
    : TextLineBaseSegment(ElementType::GRADUAL_TEMPO_CHANGE_SEGMENT, annotation,
                          ElementFlag::MOVABLE | ElementFlag::ON_STAFF | ElementFlag::SYSTEM) {
    init_element_style(&::tempo_segment_style);
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

GradualTempoChangeSegment *GradualTempoChangeSegment::clone() const { return new GradualTempoChangeSegment(*this); }

GradualTempoChange *GradualTempoChangeSegment::tempo_change() const { return static_cast<GradualTempoChange *>(spanner()); }

GradualTempoChangeSegment *GradualTempoChangeSegment::find_element_to_snap_before() const {
    const System *sys = system();
    IF_ASSERT_FAILED(sys) { return nullptr; }

    GradualTempoChange *this_tempo_change = tempo_change();
    Fraction start_tick = this_tempo_change->tick();
    if (!sys->measures().empty() && start_tick == sys->measures().front()->tick()) {
        return nullptr;
    }

    auto intervals = score()->spanner_map().find_overlapping(start_tick.ticks(), start_tick.ticks());
    for (auto interval : intervals) {
        Spanner *spanner = interval.value;
        bool is_valid_tempo_change =
            spanner->is_gradual_tempo_change() && !spanner->segments_empty() && spanner->visible() && spanner != this_tempo_change;
        if (!is_valid_tempo_change) {
            continue;
        }

        GradualTempoChange *preceding_tempo_change = to_gradual_tempo_change(spanner);
        bool ends_match = preceding_tempo_change->track() == this_tempo_change->track() && preceding_tempo_change->tick2() == start_tick &&
                          preceding_tempo_change->place_above() == this_tempo_change->place_above();

        if (ends_match && preceding_tempo_change->snap_to_item_after()) {
            return to_gradual_tempo_change_segment(preceding_tempo_change->back_segment());
        }
    }

    return nullptr;
}

TempoText *GradualTempoChangeSegment::find_element_to_snap_after() const {
    if (!tempo_change()->snap_to_item_after()) {
        return nullptr;
    }

    System *sys = system();
    IF_ASSERT_FAILED(sys) { return nullptr; }

    // Note: we don't need to look for a tempoChange after.
    // It is the next tempoChange which looks for a tempoChange before.
    Fraction ref_tick = tempo_change()->tick2();
    Measure *measure = score()->tick2measure_mm(ref_tick);
    if (!measure) {
        return nullptr;
    }

    for (Segment *segment = measure->last(); segment; segment = segment->prev1()) {
        if (segment->system() != sys) {
            continue;
        }
        Fraction segment_tick = segment->tick();
        if (segment_tick > ref_tick) {
            continue;
        }
        if (segment_tick < ref_tick) {
            break;
        }
        EngravingItem *tempo_text = segment->find_annotation(ElementType::TEMPO_TEXT, track(), track());
        if (tempo_text && tempo_text->place_above() == place_above() && tempo_text->visible()) {
            return to_tempo_text(tempo_text);
        }
    }

    return nullptr;
}
