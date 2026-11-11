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

#include "chordline.h"

#include <cmath>
#include <functional>
#include <stdexcept>

#include "../style/style.h"
#include "../types/constants.h"
#include "../types/typesconv.h"
#include "chord.h"
#include "log.h"
#include "note.h"
#include "score.h"
#include "types/translatablestring.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   ChordLine
//---------------------------------------------------------

ChordLine::ChordLine(DummyParentOr<Chord> parent) : EngravingItem(ElementType::CHORDLINE, parent, ElementFlag::MOVABLE) {}

ChordLine::ChordLine(const ChordLine &cl) : EngravingItem(cl) {
    modified_ = cl.modified_;
    has_source_offsets_ = cl.has_source_offsets_;
    source_offsets_ = cl.source_offsets_;
    chord_line_type_ = cl.chord_line_type_;
    straight_ = cl.straight_;
    wavy_ = cl.wavy_;
    length_x_ = cl.length_x_;
    length_y_ = cl.length_y_;
    play_chord_line_ = cl.play_chord_line_;
    note_ = cl.note_;

    /* The manually edited shape is "given" data which happens to live in the layout
     * data, and EngravingItem's copy constructor doesn't copy that. Without this, an edited
     * chordline is invisible in newly created parts: modified_ is true, so the layout won't
     * regenerate the path, but the path itself is empty. */
    mutldata()->path = cl.ldata()->path;
}

//---------------------------------------------------------
//   setChordLineType
//---------------------------------------------------------

void ChordLine::set_chord_line_type(ChordLineType st) { chord_line_type_ = st; }

const TranslatableString &ChordLine::chord_line_type_name() const { return TConv::user_name(chord_line_type_, straight_, wavy_); }

//---------------------------------------------------------
//   slideType
//---------------------------------------------------------

static Note::SlideType slide_type(ChordLineType type) {
    static const std::unordered_map<ChordLineType, Note::SlideType> chord_line_to_slide_types{
        {ChordLineType::FALL, Note::SlideType::DOWN_FROM_NOTE},
        {ChordLineType::DOIT, Note::SlideType::UP_FROM_NOTE},
        {ChordLineType::SCOOP, Note::SlideType::UP_TO_NOTE},
        {ChordLineType::PLOP, Note::SlideType::DOWN_TO_NOTE}};

    auto it = chord_line_to_slide_types.find(type);
    if (it != chord_line_to_slide_types.end()) {
        return it->second;
    }

    return Note::SlideType::UNDEFINED;
}

//---------------------------------------------------------
//   setNote
//---------------------------------------------------------

void ChordLine::set_note(Note *note) {
    note_ = note;

    if (note) {
        note->attach_slide(slide_type(chord_line_type_));
    }
}

namespace {
constexpr double chord_line_coordinate_limit = 1000000.0;

void require_chord_line_coordinate(double value) {
    if (!std::isfinite(value) || std::abs(value) > ::mu::engraving::chord_line_coordinate_limit) {
        throw std::runtime_error("ChordLine coordinate exceeds supported geometry range (+/-1000000 drawing units)");
    }
}

double chord_line_product(double value, double factor) {
    const double result = value * factor;
    require_chord_line_coordinate(result);
    if (value != 0.0 && result == 0.0) {
        throw std::runtime_error("ChordLine coordinate underflows");
    }
    return result;
}

void require_chord_line_scale(double scale) {
    if (!std::isfinite(scale) || scale <= 0.0) {
        throw std::runtime_error("ChordLine requires positive finite scale");
    }
}

} // namespace

void ChordLine::read_source_offsets(const std::array<PointF, 4> &xml, double unit) {
    // No chord tick or local staff lookup here: it may still have a dummy parent.
    require_chord_line_scale(unit);
    auto physical = xml;
    for (PointF &point : physical) {
        point = PointF(chord_line_product(point.x(), unit), chord_line_product(point.y(), unit));
    }
    source_offsets_ = physical;
    has_source_offsets_ = true;
}

std::array<PointF, 4> ChordLine::xml_source_offsets() const {
    const double unit = offset_is_spatium_dependent() ? style().spatium() : ::mu::engraving::dpmm;
    require_chord_line_scale(unit);
    auto xml = source_offsets_;
    for (PointF &point : xml) {
        const PointF converted = point / unit;
        if (!std::isfinite(converted.x()) || !std::isfinite(converted.y()) || (point.x() != 0.0 && converted.x() == 0.0) ||
            (point.y() != 0.0 && converted.y() == 0.0)) {
            throw std::runtime_error("ChordLine cannot encode its coordinates in the selected file unit");
        }
        point = converted;
    }
    return xml;
}

std::array<PointF, 4> ChordLine::offset_points(const std::array<PointF, 4> &offsets, double sp, double mag) const {
    require_chord_line_scale(sp);
    require_chord_line_scale(mag);
    const double x = is_to_the_left() ? -1.2 : 1.2;
    const double y = is_below() ? 1.0 : -1.0;
    std::array<PointF, 4> points = is_to_the_left() ? std::array<PointF, 4>{PointF(), PointF(0.0, y / 2), PointF(x / 2, y), PointF(x, y)} :
                                                      std::array<PointF, 4>{PointF(), PointF(x / 2, 0.0), PointF(x, y / 2), PointF(x, y)};
    for (size_t i = 0; i < points.size(); ++i) {
        require_chord_line_coordinate(offsets[i].x());
        require_chord_line_coordinate(offsets[i].y());
        points[i] = (points[i] * sp + offsets[i]) * mag;
        require_chord_line_coordinate(points[i].x());
        require_chord_line_coordinate(points[i].y());
    }
    return points;
}

void ChordLine::scale_offsets(double old_value, double new_value) {
    if (!has_source_offsets_) {
        return;
    }
    for (PointF &point : source_offsets_) {
        point *= new_value / old_value;
    }
}

void ChordLine::spatium_changed(double old_value, double new_value) {
    EngravingItem::spatium_changed(old_value, new_value);
    scale_offsets(old_value, new_value); // upstream also scales mm-mode controls
}

void ChordLine::local_spatium_changed(double old_value, double new_value) {
    EngravingItem::local_spatium_changed(old_value, new_value);
    scale_offsets(old_value, new_value);
}

PainterPath ChordLine::source_offset_path(double sp, double mag) const {
    const auto points = offset_points(source_offsets_, sp, mag);
    PainterPath path;
    if (chord_line_type_ == ChordLineType::NOTYPE || wavy_) {
        return path;
    }
    path.move_to(points[0]);
    if (straight_) {
        path.line_to(points[3]);
    } else {
        path.cubic_to(points[1], points[2], points[3]);
    }
    return path;
}

SymId ChordLine::wave_sym() const {
    if (chord_line_type_ == ChordLineType::FALL || chord_line_type_ == ChordLineType::PLOP) {
        return SymId::BRASS_FALL_ROUGH_SHORT;
    }

    return SymId::BRASS_LIFT_SHORT;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

int ChordLine::subtype() const {
    size_t h1 = std::hash<ChordLineType>{}(chord_line_type_);
    size_t h2 = std::hash<bool>{}(straight_);
    size_t h3 = std::hash<bool>{}(wavy_);

    return static_cast<int>(h1 ^ (h2 << 1) ^ (h3 << 2));
}

muse::TranslatableString ChordLine::subtype_user_name() const { return chord_line_type_name(); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue ChordLine::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::PATH: return PropertyValue::from_value(ldata()->path);
        case Pid::CHORD_LINE_TYPE: return int(chord_line_type_);
        case Pid::CHORD_LINE_STRAIGHT: return straight_;
        case Pid::CHORD_LINE_WAVY: return wavy_;
        case Pid::PLAY: return play_chord_line_;
        default: break;
    }
    return EngravingItem::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

ChordLine::AppearanceState ChordLine::appearance_state() const {
    AppearanceState state;
    for (size_t i = 0; i < ::mu::engraving::ChordLine::appearance_properties.size(); ++i) {
        state.values[i] = get_property(::mu::engraving::ChordLine::appearance_properties[i]);
        state.flags[i] = property_flags(::mu::engraving::ChordLine::appearance_properties[i]);
    }
    state.linked = is_appearance_linked_to_master();
    state.offsets = has_source_offsets_;
    state.controls = source_offsets_;
    state.modified = modified_;
    state.length_x = length_x_;
    state.length_y = length_y_;
    return state;
}

void ChordLine::restore_appearance(const AppearanceState &state) {
    for (size_t i = 0; i < ::mu::engraving::ChordLine::appearance_properties.size(); ++i) {
        set_property(::mu::engraving::ChordLine::appearance_properties[i], state.values[i]);
        set_property_flags(::mu::engraving::ChordLine::appearance_properties[i], state.flags[i]);
    }
    set_appearance_linked_to_master(state.linked);
    has_source_offsets_ = state.offsets;
    source_offsets_ = state.controls;
    modified_ = state.modified;
    length_x_ = state.length_x;
    length_y_ = state.length_y;
    trigger_layout();
}

bool ChordLine::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (val.to_bool() && !is_appearance_linked_to_master()) {
                for (EngravingObject *object : link_list_for_property_propagation()) {
                    if (object->is_chord_line() && object->score()->is_master()) {
                        restore_appearance(to_chord_line(object)->appearance_state());
                        break;
                    }
                }
            }
            set_appearance_linked_to_master(val.to_bool());
            trigger_layout();
            return true;
        case Pid::PATH:
            mutldata()->path = val.value<PainterPath>();
            has_source_offsets_ = false;
            source_offsets_ = {};
            set_modified(true);
            break;
        case Pid::CHORD_LINE_TYPE: set_chord_line_type(ChordLineType(val.to_int())); break;
        case Pid::CHORD_LINE_STRAIGHT: set_straight(val.to_bool()); break;
        case Pid::CHORD_LINE_WAVY: set_wavy(val.to_bool()); break;
        case Pid::PLAY: set_play_chord_line(val.to_bool()); break;
        default: return EngravingItem::set_property(property_id, val);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

void ChordLine::reset_property(Pid pid) {
    if (pid != Pid::PATH) {
        EngravingItem::reset_property(pid);
        return;
    }
    // Keep the source representation; both kinds return to their default shape.
    source_offsets_ = {};
    modified_ = false;
    mutldata()->path = PainterPath();
    trigger_layout();
}

PropertyValue ChordLine::property_default(Pid pid) const {
    switch (pid) {
        case Pid::PATH: return PropertyValue::from_value(PainterPath());
        case Pid::CHORD_LINE_STRAIGHT: return false;
        case Pid::CHORD_LINE_WAVY: return false;
        case Pid::PLAY: return true;
        default: break;
    }
    return EngravingItem::property_default(pid);
}
} // namespace mu::engraving
