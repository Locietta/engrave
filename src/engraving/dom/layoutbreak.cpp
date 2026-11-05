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

#include "log.h"

#include "iengravingconfiguration.h" // IWYU pragma: keep

#include "types/typesconv.h"

#include "layoutbreak.h"
#include "measurebase.h"
#include "score.h"

using namespace mu;
using namespace muse::draw;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   sectionBreakStyle
//---------------------------------------------------------

static const ElementStyle sectionBreakStyle{{Sid::sectionPause, Pid::PAUSE}};

//---------------------------------------------------------
//   LayoutBreak
//---------------------------------------------------------

LayoutBreak::LayoutBreak(DummyParentOr<MeasureBase> parent) : EngravingItem(ElementType::LAYOUT_BREAK, parent, ElementFlag::SYSTEM) {
    pause_ = 0.;
    startWithLongNames_ = false;
    startWithMeasureOne_ = false;
    firstSystemIndentation_ = false;
    showCourtesy_ = false;
    layoutBreakType_ = LayoutBreakType::PAGE;

    initElementStyle(&sectionBreakStyle);

    resetProperty(Pid::PAUSE);
    resetProperty(Pid::START_WITH_LONG_NAMES);
    resetProperty(Pid::START_WITH_MEASURE_ONE);
    resetProperty(Pid::FIRST_SYSTEM_INDENTATION);
    resetProperty(Pid::SHOW_COURTESY);
}

LayoutBreak::LayoutBreak(const LayoutBreak &lb) : EngravingItem(lb) {
    layoutBreakType_ = lb.layoutBreakType_;
    pause_ = lb.pause_;
    startWithLongNames_ = lb.startWithLongNames_;
    startWithMeasureOne_ = lb.startWithMeasureOne_;
    firstSystemIndentation_ = lb.firstSystemIndentation_;
    showCourtesy_ = lb.showCourtesy_;
}

void LayoutBreak::setOwnershipParent(DummyParentOr<MeasureBase> parent) { EngravingItem::setOwnershipParent(parent); }

char16_t LayoutBreak::iconCode() const {
    switch (layoutBreakType_) {
        case LayoutBreakType::LINE: return 0xF483;
        case LayoutBreakType::PAGE: return 0xF484;
        case LayoutBreakType::SECTION: return 0xF485;
        case LayoutBreakType::NOBREAK: return 0xF486;
        default: return 0x000;
    }
}

//---------------------------------------------------------
//   setLayoutBreakType
//---------------------------------------------------------

void LayoutBreak::setLayoutBreakType(LayoutBreakType val) { layoutBreakType_ = val; }

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool LayoutBreak::acceptDrop(EditData &data) const {
    return data.dropElement->isLayoutBreak() && toLayoutBreak(data.dropElement)->layoutBreakType() != layoutBreakType();
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *LayoutBreak::drop(Transaction &, EditData &data) {
    EngravingItem *e = data.dropElement;
    score()->undoChangeElement(this, e);
    return e;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue LayoutBreak::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::LAYOUT_BREAK: return layoutBreakType_;
        case Pid::PAUSE: return pause_;
        case Pid::START_WITH_LONG_NAMES: return startWithLongNames_;
        case Pid::START_WITH_MEASURE_ONE: return startWithMeasureOne_;
        case Pid::FIRST_SYSTEM_INDENTATION: return firstSystemIndentation_;
        case Pid::SHOW_COURTESY: return showCourtesy_;
        default: return EngravingItem::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool LayoutBreak::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::LAYOUT_BREAK: setLayoutBreakType(v.value<LayoutBreakType>()); break;
        case Pid::PAUSE: setPause(v.toDouble()); break;
        case Pid::START_WITH_LONG_NAMES: setStartWithLongNames(v.toBool()); break;
        case Pid::START_WITH_MEASURE_ONE: setStartWithMeasureOne(v.toBool()); break;
        case Pid::FIRST_SYSTEM_INDENTATION: setFirstSystemIndentation(v.toBool()); break;
        case Pid::SHOW_COURTESY: setShowCourtesy(v.toBool()); break;
        default:
            if (!EngravingItem::setProperty(propertyId, v)) {
                return false;
            }
            break;
    }

    if (propertyId == Pid::START_WITH_MEASURE_ONE) {
        triggerLayoutToEnd();
    } else {
        triggerLayout();
        if (ownershipParent() && measure()->next()) {
            measure()->next()->triggerLayout();
        }
    }

    setGenerated(false);
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue LayoutBreak::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::LAYOUT_BREAK: return PropertyValue(); // LAYOUT_BREAK_LINE;
        case Pid::PAUSE: return style().styleD(Sid::sectionPause);
        case Pid::START_WITH_LONG_NAMES: return true;
        case Pid::START_WITH_MEASURE_ONE: return true;
        case Pid::FIRST_SYSTEM_INDENTATION: return true;
        case Pid::SHOW_COURTESY: return false;
        default: return EngravingItem::propertyDefault(id);
    }
}

//---------------------------------------------------------
//   subtypeUserName
//---------------------------------------------------------

muse::TranslatableString LayoutBreak::subtypeUserName() const { return TConv::userName(layoutBreakType()); }

//---------------------------------------------------------
//   accessibleInfo
//---------------------------------------------------------

Font LayoutBreak::font() const {
    Font font(configuration()->iconsFontFamily(), Font::Type::Icon);
    font.setPointSizeF(UI_ICONS_DEFAULT_FONT_SIZE * magS());
    return font;
}
} // namespace mu::engraving
