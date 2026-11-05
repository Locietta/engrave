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

#include <functional>
#include <cmath>
#include <stdexcept>
#include "../style/style.h"
#include "../types/constants.h"

#include "types/translatablestring.h"

#include "../editing/elementeditdata.h"
#include "../types/typesconv.h"

#include "chord.h"
#include "score.h"
#include "note.h"

#include "log.h"

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
    hasSourceOffsets_ = cl.hasSourceOffsets_;
    sourceOffsets_ = cl.sourceOffsets_;
    chordLineType_ = cl.chordLineType_;
    straight_ = cl.straight_;
    wavy_ = cl.wavy_;
    lengthX_ = cl.lengthX_;
    lengthY_ = cl.lengthY_;
    playChordLine_ = cl.playChordLine_;
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

void ChordLine::setChordLineType(ChordLineType st) { chordLineType_ = st; }

const TranslatableString &ChordLine::chordLineTypeName() const { return TConv::userName(chordLineType_, straight_, wavy_); }

//---------------------------------------------------------
//   slideType
//---------------------------------------------------------

static Note::SlideType slideType(ChordLineType type) {
    static const std::unordered_map<ChordLineType, Note::SlideType> chordLineToSlideTypes{
        {ChordLineType::FALL, Note::SlideType::DownFromNote},
        {ChordLineType::DOIT, Note::SlideType::UpFromNote},
        {ChordLineType::SCOOP, Note::SlideType::UpToNote},
        {ChordLineType::PLOP, Note::SlideType::DownToNote}};

    auto it = chordLineToSlideTypes.find(type);
    if (it != chordLineToSlideTypes.end()) {
        return it->second;
    }

    return Note::SlideType::Undefined;
}

//---------------------------------------------------------
//   setNote
//---------------------------------------------------------

void ChordLine::setNote(Note *note) {
    note_ = note;

    if (note) {
        note->attachSlide(slideType(chordLineType_));
    }
}

namespace {
constexpr double chordLineCoordinateLimit = 1000000.0;

void requireChordLineCoordinate(double value) {
    if (!std::isfinite(value) || std::abs(value) > chordLineCoordinateLimit) {
        throw std::runtime_error("ChordLine coordinate exceeds supported geometry range (+/-1000000 drawing units)");
    }
}

double chordLineProduct(double value, double factor) {
    const double result = value * factor;
    requireChordLineCoordinate(result);
    if (value != 0.0 && result == 0.0) {
        throw std::runtime_error("ChordLine coordinate underflows");
    }
    return result;
}

void requireChordLineScale(double scale) {
    if (!std::isfinite(scale) || scale <= 0.0) {
        throw std::runtime_error("ChordLine requires positive finite scale");
    }
}

} // namespace

void ChordLine::readSourceOffsets(const std::array<PointF, 4> &xml, double unit) {
    // No chord tick or local staff lookup here: it may still have a dummy parent.
    requireChordLineScale(unit);
    auto physical = xml;
    for (PointF &point : physical) {
        point = PointF(chordLineProduct(point.x(), unit), chordLineProduct(point.y(), unit));
    }
    sourceOffsets_ = physical;
    hasSourceOffsets_ = true;
}

std::array<PointF, 4> ChordLine::xmlSourceOffsets() const {
    const double unit = offsetIsSpatiumDependent() ? style().spatium() : DPMM;
    requireChordLineScale(unit);
    auto xml = sourceOffsets_;
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

std::array<PointF, 4> ChordLine::offsetPoints(const std::array<PointF, 4> &offsets, double sp, double mag) const {
    requireChordLineScale(sp);
    requireChordLineScale(mag);
    const double x = isToTheLeft() ? -1.2 : 1.2;
    const double y = isBelow() ? 1.0 : -1.0;
    std::array<PointF, 4> points = isToTheLeft() ? std::array<PointF, 4>{PointF(), PointF(0.0, y / 2), PointF(x / 2, y), PointF(x, y)} :
                                                   std::array<PointF, 4>{PointF(), PointF(x / 2, 0.0), PointF(x, y / 2), PointF(x, y)};
    for (size_t i = 0; i < points.size(); ++i) {
        requireChordLineCoordinate(offsets[i].x());
        requireChordLineCoordinate(offsets[i].y());
        points[i] = (points[i] * sp + offsets[i]) * mag;
        requireChordLineCoordinate(points[i].x());
        requireChordLineCoordinate(points[i].y());
    }
    return points;
}

void ChordLine::scaleOffsets(double oldValue, double newValue) {
    if (!hasSourceOffsets_) {
        return;
    }
    for (PointF &point : sourceOffsets_) {
        point *= newValue / oldValue;
    }
}

void ChordLine::spatiumChanged(double oldValue, double newValue) {
    EngravingItem::spatiumChanged(oldValue, newValue);
    scaleOffsets(oldValue, newValue); // upstream also scales mm-mode controls
}

void ChordLine::localSpatiumChanged(double oldValue, double newValue) {
    EngravingItem::localSpatiumChanged(oldValue, newValue);
    scaleOffsets(oldValue, newValue);
}

PainterPath ChordLine::sourceOffsetPath(double sp, double mag) const {
    const auto points = offsetPoints(sourceOffsets_, sp, mag);
    PainterPath path;
    if (chordLineType_ == ChordLineType::NOTYPE || wavy_) {
        return path;
    }
    path.moveTo(points[0]);
    if (straight_) {
        path.lineTo(points[3]);
    } else {
        path.cubicTo(points[1], points[2], points[3]);
    }
    return path;
}

SymId ChordLine::waveSym() const {
    if (chordLineType_ == ChordLineType::FALL || chordLineType_ == ChordLineType::PLOP) {
        return SymId::brassFallRoughShort;
    }

    return SymId::brassLiftShort;
}

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

int ChordLine::subtype() const {
    size_t h1 = std::hash<ChordLineType>{}(chordLineType_);
    size_t h2 = std::hash<bool>{}(straight_);
    size_t h3 = std::hash<bool>{}(wavy_);

    return static_cast<int>(h1 ^ (h2 << 1) ^ (h3 << 2));
}

muse::TranslatableString ChordLine::subtypeUserName() const { return chordLineTypeName(); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue ChordLine::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::PATH: return PropertyValue::fromValue(ldata()->path);
        case Pid::CHORD_LINE_TYPE: return int(chordLineType_);
        case Pid::CHORD_LINE_STRAIGHT: return straight_;
        case Pid::CHORD_LINE_WAVY: return wavy_;
        case Pid::PLAY: return playChordLine_;
        default: break;
    }
    return EngravingItem::getProperty(propertyId);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

ChordLine::AppearanceState ChordLine::appearanceState() const {
    AppearanceState state;
    for (size_t i = 0; i < appearanceProperties.size(); ++i) {
        state.values[i] = getProperty(appearanceProperties[i]);
        state.flags[i] = propertyFlags(appearanceProperties[i]);
    }
    state.linked = isAppearanceLinkedToMaster();
    state.offsets = hasSourceOffsets_;
    state.controls = sourceOffsets_;
    state.modified = modified_;
    state.lengthX = lengthX_;
    state.lengthY = lengthY_;
    return state;
}

void ChordLine::restoreAppearance(const AppearanceState &state) {
    for (size_t i = 0; i < appearanceProperties.size(); ++i) {
        setProperty(appearanceProperties[i], state.values[i]);
        setPropertyFlags(appearanceProperties[i], state.flags[i]);
    }
    setAppearanceLinkedToMaster(state.linked);
    hasSourceOffsets_ = state.offsets;
    sourceOffsets_ = state.controls;
    modified_ = state.modified;
    lengthX_ = state.lengthX;
    lengthY_ = state.lengthY;
    triggerLayout();
}

bool ChordLine::setProperty(Pid propertyId, const PropertyValue &val) {
    switch (propertyId) {
        case Pid::APPEARANCE_LINKED_TO_MASTER:
            if (val.toBool() && !isAppearanceLinkedToMaster()) {
                for (EngravingObject *object : linkListForPropertyPropagation()) {
                    if (object->isChordLine() && object->score()->isMaster()) {
                        restoreAppearance(toChordLine(object)->appearanceState());
                        break;
                    }
                }
            }
            setAppearanceLinkedToMaster(val.toBool());
            triggerLayout();
            return true;
        case Pid::PATH:
            mutldata()->path = val.value<PainterPath>();
            hasSourceOffsets_ = false;
            sourceOffsets_ = {};
            setModified(true);
            break;
        case Pid::CHORD_LINE_TYPE: setChordLineType(ChordLineType(val.toInt())); break;
        case Pid::CHORD_LINE_STRAIGHT: setStraight(val.toBool()); break;
        case Pid::CHORD_LINE_WAVY: setWavy(val.toBool()); break;
        case Pid::PLAY: setPlayChordLine(val.toBool()); break;
        default: return EngravingItem::setProperty(propertyId, val);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

void ChordLine::resetProperty(Pid pid) {
    if (pid != Pid::PATH) {
        EngravingItem::resetProperty(pid);
        return;
    }
    // Keep the source representation; both kinds return to their default shape.
    sourceOffsets_ = {};
    modified_ = false;
    mutldata()->path = PainterPath();
    triggerLayout();
}

PropertyValue ChordLine::propertyDefault(Pid pid) const {
    switch (pid) {
        case Pid::PATH: return PropertyValue::fromValue(PainterPath());
        case Pid::CHORD_LINE_STRAIGHT: return false;
        case Pid::CHORD_LINE_WAVY: return false;
        case Pid::PLAY: return true;
        default: break;
    }
    return EngravingItem::propertyDefault(pid);
}
} // namespace mu::engraving
