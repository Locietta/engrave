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

#include "box.h"

#include <cmath>

#include "../editing/elementeditdata.h"
#include "../editing/editfretboarddiagram.h"

#include "actionicon.h"
#include "factory.h"
#include "fret.h"
#include "harmony.h"
#include "layoutbreak.h"
#include "masterscore.h"
#include "measure.h"
#include "mscore.h"
#include "score.h"
#include "stafftext.h"
#include "system.h"
#include "text.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::draw;

namespace mu::engraving {
static const ElementStyle boxStyle{
    {Sid::systemFrameDistance, Pid::TOP_GAP},
    {Sid::frameSystemDistance, Pid::BOTTOM_GAP},
    {Sid::paddingToNotationAbove, Pid::PADDING_TO_NOTATION_ABOVE},
    {Sid::paddingToNotationBelow, Pid::PADDING_TO_NOTATION_BELOW},
};

static const ElementStyle hBoxStyle{};

static const String FRET_BOX_DIAGRAMS_SEPARATOR = u",";

Box::Box(const ElementType &type, Score *parent) : MeasureBase(type, parent) {}

//---------------------------------------------------------
//   computeMinWidth
//---------------------------------------------------------

void HBox::computeMinWidth() {
    setWidth(absoluteFromSpatium(boxWidth() + topGap() + bottomGap())); // top/bottom is really left/right
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to Box
//---------------------------------------------------------

void Box::add(EngravingItem *e) {
    if (e->isText()) {
        toText(e)->setLayoutToParentWidth(true);
    }
    MeasureBase::add(e);
}

double Box::absoluteFromSpatium(const Spatium &val) const {
    const double sp = sizeIsSpatiumDependent() ? spatium() : style().defaultSpatium();
    return val.val() * sp;
}

RectF Box::contentRect() const {
    RectF result;

    for (const EngravingItem *element : el()) {
        result = result.united(element->ldata()->bbox());
    }

    return result;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Box::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::BOX_HEIGHT: return boxHeight_;
        case Pid::BOX_WIDTH: return boxWidth_;
        case Pid::TOP_GAP: return topGap_;
        case Pid::BOTTOM_GAP: return bottomGap_;
        case Pid::LEFT_MARGIN: return leftMargin_;
        case Pid::RIGHT_MARGIN: return rightMargin_;
        case Pid::TOP_MARGIN: return topMargin_;
        case Pid::BOTTOM_MARGIN: return bottomMargin_;
        case Pid::BOX_AUTOSIZE: return (score()->mscVersion() >= 302) ? isAutoSizeEnabled_ : false;
        default: return MeasureBase::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Box::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::BOX_HEIGHT: boxHeight_ = v.value<Spatium>(); break;
        case Pid::BOX_WIDTH: boxWidth_ = v.value<Spatium>(); break;
        case Pid::TOP_GAP: topGap_ = v.value<Spatium>(); break;
        case Pid::BOTTOM_GAP: bottomGap_ = v.value<Spatium>(); break;
        case Pid::LEFT_MARGIN: leftMargin_ = v.toDouble(); break;
        case Pid::RIGHT_MARGIN: rightMargin_ = v.toDouble(); break;
        case Pid::TOP_MARGIN: topMargin_ = v.toDouble(); break;
        case Pid::BOTTOM_MARGIN: bottomMargin_ = v.toDouble(); break;
        case Pid::BOX_AUTOSIZE: isAutoSizeEnabled_ = v.toBool(); break;
        default: return MeasureBase::setProperty(propertyId, v);
    }
    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Box::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::BOX_HEIGHT:
        case Pid::BOX_WIDTH: return 0.0_sp;

        case Pid::TOP_GAP: return isHBox() ? 0.0_sp : style().styleS(Sid::systemFrameDistance);
        case Pid::BOTTOM_GAP: return isHBox() ? 0.0_sp : style().styleS(Sid::frameSystemDistance);

        case Pid::LEFT_MARGIN:
        case Pid::RIGHT_MARGIN:
        case Pid::TOP_MARGIN:
        case Pid::BOTTOM_MARGIN: return 0.0;
        case Pid::BOX_AUTOSIZE: return true;
        case Pid::SIZE_SPATIUM_DEPENDENT: return !isTitleFrame();
        default: return MeasureBase::propertyDefault(id);
    }
}

bool Box::isTitleFrame() const { return this == score()->first() && isVBox(); }

//---------------------------------------------------------
//   copyValues
//---------------------------------------------------------

void Box::copyValues(Box *origin) {
    boxHeight_ = origin->boxHeight();
    boxWidth_ = origin->boxWidth();

    double factor = magS() / origin->magS();
    bottomGap_ = origin->bottomGap() * factor;
    topGap_ = origin->topGap() * factor;
    bottomMargin_ = origin->bottomMargin() * factor;
    topMargin_ = origin->topMargin() * factor;
    leftMargin_ = origin->leftMargin() * factor;
    rightMargin_ = origin->rightMargin() * factor;

    setSizeIsSpatiumDependent(origin->sizeIsSpatiumDependent());
}

//---------------------------------------------------------
//   HBox
//---------------------------------------------------------

HBox::HBox(Score *parent) : Box(ElementType::HBOX, parent) {
    initElementStyle(&hBoxStyle);
    resetProperty(Pid::BOX_WIDTH);
}

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Box::acceptDrop(EditData &data) const {
    if (data.dropElement->flag(ElementFlag::ON_STAFF)) {
        return false;
    }
    if (MScore::debugMode) {
        LOGD("<%s>", data.dropElement->typeName());
    }
    ElementType t = data.dropElement->type();
    switch (t) {
        case ElementType::TEXT:
        case ElementType::STAFF_TEXT:
        case ElementType::IMAGE:
        case ElementType::SYMBOL: return true;
        case ElementType::ACTION_ICON:
            switch (toActionIcon(data.dropElement)->actionType()) {
                case ActionIconType::VFRAME:
                case ActionIconType::TFRAME:
                case ActionIconType::FFRAME:
                case ActionIconType::HFRAME:
                case ActionIconType::MEASURE: return true;
                default: break;
            }
            break;
        case ElementType::BAR_LINE: return isHBox();
        default: break;
    }
    return MeasureBase::acceptDrop(data);
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Box::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.dropElement;
    if (e->flag(ElementFlag::ON_STAFF)) {
        return 0;
    }
    if (MScore::debugMode) {
        LOGD("<%s>", e->typeName());
    }
    switch (e->type()) {
        case ElementType::STAFF_TEXT: {
            Text *text = Factory::createText(this, TextStyleType::FRAME);
            text->setXmlText(toStaffText(e)->xmlText());
            score()->undoAddElement(text);
            delete e;
            return text;
        }

        case ElementType::ACTION_ICON:
            switch (toActionIcon(e)->actionType()) {
                case ActionIconType::VFRAME: score()->insertBox(ElementType::VBOX, this); break;
                case ActionIconType::TFRAME: score()->insertBox(ElementType::TBOX, this); break;
                case ActionIconType::FFRAME: score()->insertBox(ElementType::FBOX, this); break;
                case ActionIconType::HFRAME: score()->insertBox(ElementType::HBOX, this); break;
                case ActionIconType::MEASURE: score()->insertMeasure(ElementType::MEASURE, this); break;
                default: break;
            }
            break;

        case ElementType::TEXT:
        case ElementType::IMAGE:
        case ElementType::SYMBOL:
            e->setOwnershipParent(this);
            score()->undoAddElement(e);
            return e;
        default: break;
    }

    return MeasureBase::drop(tx, data);
}

void Box::manageExclusionFromParts(bool exclude) {
    // manage Layout Breaks - remove old ones first
    LayoutBreak *sectionBreak = sectionBreakElement();
    if (sectionBreak) {
        toEngravingItem(sectionBreak)->manageExclusionFromParts(true);
    }

    bool titleFrame = isTitleFrame();
    if (exclude) {
        const std::list<EngravingObject *> links = linkList();
        for (EngravingObject *linkedObject : links) {
            // Only remove title frame from score
            if (linkedObject->score() == score() || (!this->score()->isMaster() && titleFrame && !linkedObject->score()->isMaster())) {
                continue;
            }
            EngravingItem *linkedItem = toEngravingItem(linkedObject);
            if (linkedItem->selected()) {
                linkedItem->score()->deselect(linkedItem);
            }
            linkedItem->score()->undoRemoveElement(linkedItem, false);
            linkedItem->undoUnlink();
        }

        // manage Layout Breaks - there are no linked boxes, so add linked Line Breaks to previous measure
        if (sectionBreak && !titleFrame) {
            if (MeasureBase *prevMeasure = this->prevMeasure()) {
                for (Score *score : masterScore()->scoreList()) {
                    if (score == this->score()) {
                        continue;
                    }
                    if (MeasureBase *localPrevMeasure = score->tick2measure(prevMeasure->tick())) {
                        EngravingItem *newSectionBreak = sectionBreak->linkedClone();
                        newSectionBreak->setScore(score);
                        newSectionBreak->setOwnershipParent(localPrevMeasure);
                        score->doUndoAddElement(newSectionBreak);
                    }
                }
            }
        }
    } else {
        for (Score *score : masterScore()->scoreList()) {
            if (score == this->score() || (titleFrame && !this->score()->isMaster() && !score->isMaster())) {
                continue;
            }

            MeasureBase *newMB = next() ? next()->getInScore(score, true) : nullptr;
            Score::InsertMeasureOptions options;
            options.cloneBoxToAllParts = false;
            MeasureBase *newFrame = score->insertBox(type(), newMB, options);
            newFrame->setExcludeFromOtherParts(false);
            // newFrame->setSizeIsSpatiumDependent(!titleFrame);

            if (!isFBox()) {
                for (EngravingItem *item : el()) {
                    // Don't add instrument name from current part
                    if (item->isText() && toText(item)->textStyleType() == TextStyleType::INSTRUMENT_EXCERPT) {
                        continue;
                    }
                    // add frame items (Layout Break, Title, ...)
                    newFrame->add(item->linkedClone());
                }
            }

            if (isTBox()) {
                Text *thisText = toTBox(this)->text();
                toTBox(newFrame)->add(thisText->linkedClone());
            }

            if (!score->isMaster() && titleFrame) {
                // Title frame - add part name
                String partLabel = score->name();
                if (!partLabel.empty()) {
                    Text *txt = Factory::createText(newFrame, TextStyleType::INSTRUMENT_EXCERPT);
                    txt->setPlainText(partLabel);
                    newFrame->add(txt);

                    score->setMetaTag(u"partName", partLabel);
                }
            }

            newFrame->linkTo(this);
        }
    }
}

//---------------------------------------------------------
//   isMovable
//---------------------------------------------------------

bool HBox::isMovable() const { return ownershipParent() && (ownershipParent()->isHBox() || ownershipParent()->isVBox()); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue HBox::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::CREATE_SYSTEM_HEADER: return createSystemHeader();
        default: return Box::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool HBox::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::CREATE_SYSTEM_HEADER:
            setCreateSystemHeader(v.toBool());
            triggerLayout();
            break;
        default: return Box::setProperty(propertyId, v);
    }
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue HBox::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::CREATE_SYSTEM_HEADER: return true;
        case Pid::BOX_WIDTH: return 5.0_sp;
        default: return Box::propertyDefault(id);
    }
}

//---------------------------------------------------------
//   VBox
//---------------------------------------------------------

VBox::VBox(const ElementType &type, Score *parent) : Box(type, parent) {
    initElementStyle(&boxStyle);

    resetProperty(Pid::BOX_HEIGHT);

    setLineBreak(true);
}

VBox::VBox(Score *parent) : VBox(ElementType::VBOX, parent) {}

void VBox::add(EngravingItem *e) {
    IF_ASSERT_FAILED(e) { return; }
    if (e->isLayoutBreak()) {
        const LayoutBreakType type = toLayoutBreak(e)->layoutBreakType();
        if (type == LayoutBreakType::LINE) {
            LOGW("Cannot add %s", e->typeName());
            return;
        }
    }
    if (e->isActionIcon()) {
        const ActionIconType type = toActionIcon(e)->actionType();
        if (type == ActionIconType::SYSTEM_LOCK) {
            LOGW("Cannot add %s", e->typeName());
            return;
        }
    }
    Box::add(e);
}

double VBox::minHeight() const { return absoluteFromSpatium(10_sp); }

double VBox::maxHeight() const { return absoluteFromSpatium(30_sp); }

PropertyValue VBox::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::BOX_AUTOSIZE: return isAutoSizeEnabled();
        case Pid::PADDING_TO_NOTATION_ABOVE: return paddingToNotationAbove_;
        case Pid::PADDING_TO_NOTATION_BELOW: return paddingToNotationBelow_;
        default: return Box::getProperty(propertyId);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue VBox::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::BOX_HEIGHT: return 10.0_sp;
        default: return Box::propertyDefault(id);
    }
}

bool VBox::setProperty(Pid propertyId, const PropertyValue &v) {
    switch (propertyId) {
        case Pid::PADDING_TO_NOTATION_ABOVE: paddingToNotationAbove_ = v.value<Spatium>(); break;
        case Pid::PADDING_TO_NOTATION_BELOW: paddingToNotationBelow_ = v.value<Spatium>(); break;
        default: return Box::setProperty(propertyId, v);
    }

    triggerLayout();
    return true;
}

//---------------------------------------------------------
//   startDragGrip
//---------------------------------------------------------

bool VBox::acceptDrop(EditData &data) const {
    const EngravingItem *e = data.dropElement;
    IF_ASSERT_FAILED(e) { return false; }
    if (e->isLayoutBreak()) {
        const LayoutBreakType type = toLayoutBreak(e)->layoutBreakType();
        if (type == LayoutBreakType::LINE) {
            return false;
        }
    }
    if (e->isActionIcon()) {
        const ActionIconType type = toActionIcon(e)->actionType();
        if (type == ActionIconType::SYSTEM_LOCK) {
            return false;
        }
    }
    return Box::acceptDrop(data);
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a e to fret diagram box
//---------------------------------------------------------

FBox::FBox(Score *parent) : VBox(ElementType::FBOX, parent) {
    resetProperty(Pid::FRET_FRAME_TEXT_SCALE);
    resetProperty(Pid::FRET_FRAME_DIAGRAM_SCALE);
    resetProperty(Pid::FRET_FRAME_COLUMN_GAP);
    resetProperty(Pid::FRET_FRAME_ROW_GAP);
    resetProperty(Pid::FRET_FRAME_CHORDS_PER_ROW);
    resetProperty(Pid::FRET_FRAME_H_ALIGN);

    resetProperty(Pid::LEFT_MARGIN);
    resetProperty(Pid::RIGHT_MARGIN);
    resetProperty(Pid::TOP_MARGIN);
    resetProperty(Pid::BOTTOM_MARGIN);
    resetProperty(Pid::TOP_GAP);
    resetProperty(Pid::BOTTOM_GAP);
    resetProperty(Pid::EXCLUDE_FROM_OTHER_PARTS);
    resetProperty(Pid::APPEARANCE_LINKED_TO_MASTER);
    resetProperty(Pid::POSITION_LINKED_TO_MASTER);
}

void FBox::init() {
    StringList oldDiagramsNames;
    std::vector<FretDiagram *> oldDiagrams;
    for (EngravingItem *element : el()) {
        FretDiagram *diagram = toFretDiagram(element);
        oldDiagrams.push_back(diagram);
        oldDiagramsNames.push_back(diagram->harmonyDisplayText());
    }

    StringList diagramsNamesInScore;
    std::vector<EngravingItem *> harmonyOrDiagramsInScore;
    for (mu::engraving::Segment *segment = score()->firstSegment(mu::engraving::SegmentType::ChordRest); segment;
         segment = segment->next1(mu::engraving::SegmentType::ChordRest)) {
        for (EngravingItem *item : segment->annotations()) {
            if (!item || !item->part()) {
                continue;
            }

            if (item->isHarmony() && toHarmony(item)->harmonyType() != HarmonyType::STANDARD) {
                continue;
            }

            if (!(item->isHarmony() || item->isFretDiagram())) {
                continue;
            }

            String harmonyName = item->isHarmony()     ? toHarmony(item)->displayText() :
                                 item->isFretDiagram() ? toFretDiagram(item)->harmonyDisplayText() :
                                                         String();
            if (harmonyName.empty() || muse::contains(diagramsNamesInScore, harmonyName)) {
                continue;
            }

            harmonyOrDiagramsInScore.push_back(item);
            diagramsNamesInScore.push_back(harmonyName);
        }
    }

    diagramsOrderInScore_ = diagramsNamesInScore;

    for (size_t i = 0; i < oldDiagramsNames.size(); ++i) {
        String oldName = oldDiagramsNames[i];
        if (!muse::contains(diagramsNamesInScore, oldName)) {
            score()->undo(new RemoveFretDiagramFromFretBox(oldDiagrams[i]));
        }
    }

    for (size_t i = 0; i < diagramsNamesInScore.size(); ++i) {
        String newName = diagramsNamesInScore[i];
        if (!muse::contains(oldDiagramsNames, newName)) {
            FretDiagram *newDiagram = FretDiagram::makeFromHarmonyOrFretDiagram(harmonyOrDiagramsInScore[i]);
            newDiagram->setOwnershipParent(this);
            String nameOfDiagramBeforeThis = i > 0 ? diagramsNamesInScore[i - 1] : String();
            size_t idx = computeInsertionIdx(nameOfDiagramBeforeThis);
            score()->undo(new AddFretDiagramToFretBox(newDiagram, idx));
        }
    }
}

void FBox::add(EngravingItem *e) {
    e->setOwnershipParent(this);
    if (e->isFretDiagram()) {
        addAtIdx(toFretDiagram(e), muse::nidx);
    } else {
        LOGD("FBox::add: element not allowed");
        return;
    }
}

void FBox::addAtIdx(FretDiagram *fretDiagram, size_t idx) {
    fretDiagram->setTrack(muse::nidx);
    fretDiagram->setFlag(ElementFlag::MOVABLE, false);
    fretDiagram->setFlag(ElementFlag::ON_STAFF, false);

    Harmony *harmony = fretDiagram->harmony();
    harmony->setTrack(muse::nidx);
    harmony->setFlag(ElementFlag::MOVABLE, false);
    harmony->setFlag(ElementFlag::ON_STAFF, false);

    if (idx < el_.size()) {
        el_.insert(el_.begin() + idx, fretDiagram);
    } else {
        el_.push_back(fretDiagram);
    }

    fretDiagram->added();
}

size_t FBox::computeInsertionIdx(const String &nameOfDiagramBeforeThis) {
    if (nameOfDiagramBeforeThis.empty()) {
        return 0;
    }

    for (size_t i = 0; i < el_.size(); ++i) {
        FretDiagram *fretDiagram = toFretDiagram(el_[i]);
        if (fretDiagram->harmonyDisplayText() == nameOfDiagramBeforeThis) {
            return i + 1;
        }
    }

    return muse::nidx;
}

PropertyValue FBox::getProperty(Pid propertyId) const {
    switch (propertyId) {
        case Pid::FRET_FRAME_TEXT_SCALE: return textScale_;
        case Pid::FRET_FRAME_DIAGRAM_SCALE: return diagramScale_;
        case Pid::FRET_FRAME_COLUMN_GAP: return columnGap_;
        case Pid::FRET_FRAME_ROW_GAP: return rowGap_;
        case Pid::FRET_FRAME_CHORDS_PER_ROW: return chordsPerRow_;
        case Pid::FRET_FRAME_H_ALIGN: return static_cast<int>(contentAlignmentH_);
        case Pid::LEFT_MARGIN: return contentAlignmentH_ == AlignH::LEFT ? VBox::getProperty(propertyId) : PropertyValue();
        case Pid::RIGHT_MARGIN: return contentAlignmentH_ == AlignH::RIGHT ? VBox::getProperty(propertyId) : PropertyValue();
        case Pid::FRET_FRAME_DIAGRAMS_ORDER: return diagramsOrder().join(FRET_BOX_DIAGRAMS_SEPARATOR);
        default: return VBox::getProperty(propertyId);
    }
}

bool FBox::setProperty(Pid propertyId, const PropertyValue &val) {
    switch (propertyId) {
        case Pid::FRET_FRAME_TEXT_SCALE: textScale_ = val.toDouble(); break;
        case Pid::FRET_FRAME_DIAGRAM_SCALE: diagramScale_ = val.toDouble(); break;
        case Pid::FRET_FRAME_COLUMN_GAP: columnGap_ = val.value<Spatium>(); break;
        case Pid::FRET_FRAME_ROW_GAP: rowGap_ = val.value<Spatium>(); break;
        case Pid::FRET_FRAME_CHORDS_PER_ROW: chordsPerRow_ = val.toInt(); break;
        case Pid::FRET_FRAME_H_ALIGN:
            contentAlignmentH_ = static_cast<AlignH>(val.toInt());
            resetProperty(Pid::LEFT_MARGIN);
            resetProperty(Pid::RIGHT_MARGIN);
            break;
        case Pid::FRET_FRAME_DIAGRAMS_ORDER: reorderElements(val.value<String>().split(FRET_BOX_DIAGRAMS_SEPARATOR)); break;
        default: return VBox::setProperty(propertyId, val);
    }

    triggerLayout();
    return true;
}

PropertyValue FBox::propertyDefault(Pid propertyId) const {
    switch (propertyId) {
        case Pid::FRET_FRAME_TEXT_SCALE:
        case Pid::FRET_FRAME_DIAGRAM_SCALE: return 1.0;
        case Pid::FRET_FRAME_COLUMN_GAP:
        case Pid::FRET_FRAME_ROW_GAP: return 3.0_sp;
        case Pid::FRET_FRAME_CHORDS_PER_ROW: return 8;
        case Pid::FRET_FRAME_H_ALIGN: return static_cast<int>(AlignH::HCENTER);
        case Pid::FRET_FRAME_DIAGRAMS_ORDER: return diagramsOrderInScore_.join(FRET_BOX_DIAGRAMS_SEPARATOR);
        case Pid::EXCLUDE_FROM_OTHER_PARTS: return true;
        case Pid::APPEARANCE_LINKED_TO_MASTER:
        case Pid::POSITION_LINKED_TO_MASTER: return false;
        default: return VBox::propertyDefault(propertyId);
    }
}

void FBox::undoReorderElements(const StringList &newOrder) {
    undoChangeProperty(Pid::FRET_FRAME_DIAGRAMS_ORDER, newOrder.join(FRET_BOX_DIAGRAMS_SEPARATOR));
    triggerLayout();
}

void FBox::reorderElements(const StringList &newOrder) {
    std::sort(el_.begin(), el_.end(), [&](EngravingItem *a, EngravingItem *b) {
        String nameA = toFretDiagram(a)->harmonyDisplayText();
        String nameB = toFretDiagram(b)->harmonyDisplayText();
        auto iterA = std::find(newOrder.begin(), newOrder.end(), nameA);
        auto iterB = std::find(newOrder.begin(), newOrder.end(), nameB);
        return iterA < iterB;
    });
}

StringList FBox::diagramsOrder() const {
    StringList result;
    for (EngravingItem *item : el_) {
        result.push_back(toFretDiagram(item)->harmonyDisplayText());
    }

    return result;
}

//---------------------------------------------------------
//   TBox
//---------------------------------------------------------

TBox::TBox(Score *parent) : VBox(ElementType::TBOX, parent) {
    resetProperty(Pid::BOX_HEIGHT);
    text_ = Factory::createText(this, TextStyleType::FRAME);
    text_->setLayoutToParentWidth(true);
}

TBox::TBox(const TBox &tbox) : VBox(tbox) {
    if (tbox.text_) {
        add(Factory::copyText(*(tbox.text_)));
    }
}

TBox::~TBox() { delete text_; }

void TBox::scanElements(std::function<void(EngravingItem *)> func) {
    text_->scanElements(func);
    Box::scanElements(func);
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *TBox::drop(Transaction &tx, EditData &data) {
    EngravingItem *e = data.dropElement;
    switch (e->type()) {
        case ElementType::TEXT:
            text_->undoChangeProperty(Pid::TEXT, toText(e)->xmlText());
            delete e;
            return text_;
        default: return VBox::drop(tx, data);
    }
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to TBox
//---------------------------------------------------------

void TBox::add(EngravingItem *e) {
    if (e->isText()) {
        if (text_) {
            delete text_;
        }
        text_ = toText(e);
        text_->setLayoutToParentWidth(true);
        e->setOwnershipParent(this);
        e->added();
    } else {
        VBox::add(e);
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void TBox::remove(EngravingItem *el) {
    if (el == text_) {
        // does not normally happen, since Score::deleteItem() handles this directly
        // but if it does:
        // replace with new empty text element
        // this keeps undo/redo happier than just clearing the text
        LOGD("TBox::remove() - replacing _text");
        text_ = Factory::createText(this, TextStyleType::FRAME);
        text_->setLayoutToParentWidth(true);
        el->removed();
    } else {
        VBox::remove(el);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TBox::propertyDefault(Pid id) const {
    switch (id) {
        case Pid::BOX_HEIGHT: return 1_sp;
        default: return VBox::propertyDefault(id);
    }
}

//---------------------------------------------------------
//   accessibleExtraInfo
//---------------------------------------------------------

} // namespace mu::engraving
