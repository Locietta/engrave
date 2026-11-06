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

#include "volta.h"

#include <algorithm>
#include <vector>

#include "barline.h"
#include "keysig.h"
#include "measure.h"
#include "score.h"
#include "staff.h"
#include "system.h"
#include "text.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
static const ElementStyle volta_style{
    {Sid::VOLTA_FONT_FACE, Pid::BEGIN_FONT_FACE},
    {Sid::VOLTA_FONT_FACE, Pid::CONTINUE_FONT_FACE},
    {Sid::VOLTA_FONT_FACE, Pid::END_FONT_FACE},
    {Sid::VOLTA_FONT_SIZE, Pid::BEGIN_FONT_SIZE},
    {Sid::VOLTA_FONT_SIZE, Pid::CONTINUE_FONT_SIZE},
    {Sid::VOLTA_FONT_SIZE, Pid::END_FONT_SIZE},
    {Sid::VOLTA_FONT_STYLE, Pid::BEGIN_FONT_STYLE},
    {Sid::VOLTA_FONT_STYLE, Pid::CONTINUE_FONT_STYLE},
    {Sid::VOLTA_FONT_STYLE, Pid::END_FONT_STYLE},
    {Sid::VOLTA_ALIGN, Pid::BEGIN_TEXT_ALIGN},
    {Sid::VOLTA_ALIGN, Pid::CONTINUE_TEXT_ALIGN},
    {Sid::VOLTA_ALIGN, Pid::END_TEXT_ALIGN},
    {Sid::VOLTA_POSITION, Pid::BEGIN_TEXT_POSITION},
    {Sid::VOLTA_POSITION, Pid::CONTINUE_TEXT_POSITION},
    {Sid::VOLTA_POSITION, Pid::END_TEXT_POSITION},
    {Sid::VOLTA_OFFSET, Pid::BEGIN_TEXT_OFFSET},
    {Sid::VOLTA_OFFSET, Pid::CONTINUE_TEXT_OFFSET},
    {Sid::VOLTA_OFFSET, Pid::END_TEXT_OFFSET},
    {Sid::VOLTA_MUSICAL_SYMBOL_SIZE, Pid::BEGIN_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::VOLTA_MUSICAL_SYMBOL_SIZE, Pid::CONTINUE_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::VOLTA_MUSICAL_SYMBOL_SIZE, Pid::END_TEXT_MUSIC_SYMBOLS_SIZE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::BEGIN_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::CONTINUE_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::DUMMY_MUSICAL_SYMBOLS_SCALE, Pid::END_TEXT_MUSICAL_SYMBOLS_SCALE},
    {Sid::VOLTA_LINE_WIDTH, Pid::LINE_WIDTH},
    {Sid::VOLTA_LINE_STYLE, Pid::LINE_STYLE},
    {Sid::VOLTA_DASH_LINE_LEN, Pid::DASH_LINE_LEN},
    {Sid::VOLTA_DASH_GAP_LEN, Pid::DASH_GAP_LEN},
    {Sid::VOLTA_HOOK, Pid::BEGIN_HOOK_HEIGHT},
    {Sid::VOLTA_HOOK, Pid::END_HOOK_HEIGHT},
    {Sid::VOLTA_FONT_SPATIUM_DEPENDENT, Pid::TEXT_SIZE_SPATIUM_DEPENDENT},
};

//---------------------------------------------------------
//   VoltaSegment
//---------------------------------------------------------

VoltaSegment::VoltaSegment(Volta *sp)
    : TextLineBaseSegment(ElementType::VOLTA_SEGMENT, sp, ElementFlag::MOVABLE | ElementFlag::ON_STAFF | ElementFlag::SYSTEM) {
    text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
    end_text_->set_text_style_type(property_default(Pid::TEXT_STYLE).value<TextStyleType>());
}

//---------------------------------------------------------
//   propertyDelegate
//---------------------------------------------------------

EngravingObject *VoltaSegment::property_delegate(Pid pid) const {
    if (pid == Pid::BEGIN_HOOK_TYPE || pid == Pid::END_HOOK_TYPE || pid == Pid::VOLTA_ENDING) {
        return spanner();
    }
    return TextLineBaseSegment::property_delegate(pid);
}

//---------------------------------------------------------
//   Volta
//---------------------------------------------------------

Volta::Volta(DummyParentOr<EngravingItem> parent) : TextLineBase(ElementType::VOLTA, parent, ElementFlag::SYSTEM) {
    set_placement(PlacementV::ABOVE);
    init_element_style(&::mu::engraving::volta_style);

    set_begin_text_place(TextPlace::BELOW);
    set_continue_text_place(TextPlace::BELOW);
    set_line_visible(true);
    reset_property(Pid::BEGIN_TEXT);
    reset_property(Pid::CONTINUE_TEXT);
    reset_property(Pid::END_TEXT);
    reset_property(Pid::BEGIN_TEXT_PLACE);
    reset_property(Pid::CONTINUE_TEXT_PLACE);
    reset_property(Pid::END_TEXT_PLACE);
    reset_property(Pid::BEGIN_HOOK_TYPE);
    reset_property(Pid::END_HOOK_TYPE);
}

///
/// \brief sorts the provided list in ascending order
///
void Volta::set_endings(const std::vector<int> &l) {
    endings_ = l;
    std::sort(endings_.begin(), endings_.end());
}

//---------------------------------------------------------
//   setText
//---------------------------------------------------------

void Volta::set_text(const String &s) { set_begin_text(s); }

//---------------------------------------------------------
//   text
//---------------------------------------------------------

String Volta::text() const { return begin_text(); }

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

static const ElementStyle volta_segment_style{
    {Sid::VOLTA_MIN_DISTANCE, Pid::MIN_DISTANCE},
};

LineSegment *Volta::create_line_segment() {
    VoltaSegment *vs = new VoltaSegment(this);
    vs->set_track(track());
    vs->init_element_style(&::mu::engraving::volta_segment_style);
    return vs;
}

//---------------------------------------------------------
//   hasEnding
//---------------------------------------------------------

bool Volta::has_ending(int repeat) const {
    for (int ending : endings()) {
        if (ending == repeat) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------
//   firstEnding
//---------------------------------------------------------

int Volta::first_ending() const {
    if (endings_.empty()) {
        return 0;
    }
    return endings_.front();
}

//---------------------------------------------------------
//   lastEnding
//---------------------------------------------------------

int Volta::last_ending() const {
    if (endings_.empty()) {
        return 0;
    }
    return endings_.back();
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Volta::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::VOLTA_ENDING: return PropertyValue::from_value(endings());
        default: break;
    }
    return TextLineBase::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Volta::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::VOLTA_ENDING: {
            set_endings(val.value<std::vector<int>>());
        } break;
        default:
            if (!TextLineBase::set_property(property_id, val)) {
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

PropertyValue Volta::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::VOLTA_ENDING: return PropertyValue::from_value(std::vector<int>());
        case Pid::BEGIN_HOOK_TYPE: return HookType::HOOK_90;
        case Pid::END_HOOK_TYPE: return HookType::NONE;
        case Pid::BEGIN_TEXT:
        case Pid::CONTINUE_TEXT:
        case Pid::END_TEXT: return "";
        case Pid::LINE_VISIBLE: return true;
        case Pid::BEGIN_TEXT_PLACE:
        case Pid::CONTINUE_TEXT_PLACE:
        case Pid::END_TEXT_PLACE: return TextPlace::ABOVE;

        case Pid::PLACEMENT: return PlacementV::ABOVE;

        case Pid::TEXT_STYLE: return TextStyleType::VOLTA;

        case Pid::BEGIN_FILLED_ARROW_HEIGHT: // No arrow endings for voltas
        case Pid::BEGIN_FILLED_ARROW_WIDTH:
        case Pid::END_FILLED_ARROW_HEIGHT:
        case Pid::END_FILLED_ARROW_WIDTH:
        case Pid::BEGIN_LINE_ARROW_HEIGHT:
        case Pid::BEGIN_LINE_ARROW_WIDTH:
        case Pid::END_LINE_ARROW_HEIGHT:
        case Pid::END_LINE_ARROW_WIDTH: return 0.0;

        default: return TextLineBase::property_default(property_id);
    }
}

//---------------------------------------------------------
//   setChannel
//---------------------------------------------------------

void Volta::set_channel() const {
    Measure *start_measure = Spanner::start_measure();
    Measure *end_measure = Spanner::end_measure();

    if (start_measure && end_measure) {
        if (!end_measure->repeat_end()) {
            return;
        }

        Fraction start_tick = start_measure->tick() - Fraction::eps();
        Fraction end_tick = end_measure->end_tick() - Fraction::eps();
        Staff *st = staff();
        for (voice_idx_t voice = 0; voice < ::mu::engraving::voices; ++voice) {
            int channel = st->channel(start_tick, voice);
            st->insert_into_channel_list(voice, end_tick, channel);
        }
    }
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

PointF Volta::line_pos(Grip grip, System **system) const {
    bool start = grip == Grip::START;

    Segment *segment = score()->tick2left_segment(start ? tick() : tick2(), true,
                                                  SegmentType::CHORD_REST | SegmentType::START_REPEAT_BAR_LINE | SegmentType::END_BAR_LINE);
    if (!segment) {
        return PointF();
    }

    const Measure *measure = segment->measure();
    bool is_at_system_start = segment->rtick().is_zero() && measure && measure->system() && measure->is_first_in_system();
    bool search_for_prev_barline = start ? segment->rtick().is_zero() && (measure->repeat_start() || !is_at_system_start) : true;

    SegmentType barline_type = start ? (SegmentType::START_REPEAT_BAR_LINE | SegmentType::END_BAR_LINE) : SegmentType::END_BAR_LINE;

    if (search_for_prev_barline) {
        Segment *prev = segment;
        while (prev && !prev->is_type(barline_type) && prev->tick() == segment->tick()) {
            prev = prev->prev1_m_menabled();
        }

        if (prev && prev->is_type(barline_type)) {
            segment = prev;
        }
    }

    if (start && !segment->is_type(SegmentType::BAR_LINE_TYPES) && style().style_b(Sid::VOLTA_ALIGN_START_BEFORE_KEY_SIG)) {
        Segment *prev = segment;
        while (prev && !prev->is_type(SegmentType::KEY_SIG) && prev->tick() == segment->tick()) {
            prev = prev->prev1_m_menabled();
        }
        if (prev && prev->is_type(SegmentType::KEY_SIG)) {
            segment = prev;
        }
    }

    *system = segment->measure()->system();
    double x = segment->x() + segment->measure()->x();

    if (start) {
        bool align_left_of_repeat_bar_line = false;
        if (segment->is_chord_rest_type()) {
            x -= style().style_absolute(Sid::BAR_NOTE_DISTANCE);
        } else if (segment->is_key_sig_type()) {
            KeySig *sig = to_key_sig(segment->element(track()));
            if (sig && !sig->ldata()->key_symbols.empty()) {
                KeySym key_sym = sig->ldata()->key_symbols.front();
                PointF cutout_nw = score()->engraving_font()->smufl_anchor(key_sym.sym, SmuflAnchorId::CUT_OUT_NW, 1.0);
                x += cutout_nw.x();
            }
        } else if (segment->segment_type() & SegmentType::BAR_LINE_TYPES && !is_at_system_start) {
            x += segment->width();
            const BarLine *barline = to_bar_line(segment->element(track()));
            align_left_of_repeat_bar_line =
                barline && barline->bar_line_type() == BarLineType::END_REPEAT && style().style_b(Sid::VOLTA_ALIGN_END_LEFT_OF_BARLINE);
            if (align_left_of_repeat_bar_line) {
                x -= style().style_absolute(Sid::END_BAR_WIDTH);
            }
        }
        x += (is_at_system_start || align_left_of_repeat_bar_line ? 0.5 : -0.5) * absolute_from_spatium(line_width());
    } else {
        if ((*system) && segment->tick() == (*system)->end_tick()) {
            staff_idx_t si = back_segment()->effective_staff_idx();
            if (si == muse::nidx) {
                return PointF(x, 0.0);
            }
            x += segment->staff_shape(si).right();
            x -= 0.5 * absolute_from_spatium(line_width());
        } else if (segment->segment_type() & SegmentType::BAR_LINE_TYPES) {
            BarLine *bar_line = to_bar_line(segment->element(track()));
            if (bar_line->bar_line_type() == BarLineType::END_REPEAT || bar_line->bar_line_type() == BarLineType::END_START_REPEAT) {
                x += sym_width(SymId::REPEAT_DOT) + style().style_absolute(Sid::REPEAT_BARLINE_DOT_SEPARATION);
            }
            x += 0.5 * absolute_from_spatium(line_width());
        }
    }

    return PointF(x, 0.0);
}

//---------------------------------------------------------
//   setVoltaType
//    deprecated
//---------------------------------------------------------

void Volta::set_volta_type(Type val) { set_end_hook_type(Type::CLOSED == val ? HookType::HOOK_90 : HookType::NONE); }

//---------------------------------------------------------
//   voltaType
//    deprecated
//---------------------------------------------------------

Volta::Type Volta::volta_type() const { return end_hook_type() != HookType::NONE ? Type::CLOSED : Type::OPEN; }

Sid Volta::default_pos_sid() const { return Sid::VOLTA_POS_ABOVE; }
} // namespace mu::engraving
