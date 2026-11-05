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

#include "engravingobject.h"

#include <algorithm>

#include "global/containers.h"

#include "../editing/addremoveelement.h"
#include "../editing/editproperty.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transaction/undostack.h"
#include "style/textstyle.h"
#include "types/typesconv.h"

#include "bracketitem.h"
#include "dummyparent.h"
#include "linkedobjects.h"
#include "masterscore.h"
#include "score.h"
#include "staff.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
ElementStyle const EngravingObject::EMPTY_STYLE;

EngravingObject::EngravingObject(const ElementType &type, EngravingObject *parent) : type_(type) {
    IF_ASSERT_FAILED(this != parent) { return; }

    if (type != ElementType::SCORE) {
        assert(parent);
    }

    // The parent an object is constructed with is its parent: it is attached to it
    // right away. Passing the dummy means it is not attached to anything yet.
    doSetParent(parent);
    if (parent_) {
        doSetScore(parent_->score());
    }

    if (type == ElementType::SCORE) {
        score_ = static_cast<Score *>(this);
    }
}

EngravingObject::EngravingObject(const EngravingObject &se) {
    type_ = se.type_;
    doSetParent(se.parent_);
    score_ = se.score_;
    elementStyle_ = se.elementStyle_;
    if (elementStyle_) {
        size_t n = elementStyle_->size();
        propertyFlagsList_ = new PropertyFlags[n];
        for (size_t i = 0; i < n; ++i) {
            propertyFlagsList_[i] = se.propertyFlagsList_[i];
        }
    }
    links_ = 0;
}

//---------------------------------------------------------
//   ~ScoreElement
//---------------------------------------------------------

EngravingObject::~EngravingObject() {
    if (parent_) {
        parent_->removeChild(this);
    }

    {
        bool canMoveToDummy = !this->isType(ElementType::ROOT_ITEM) && !this->isType(ElementType::DUMMY) &&
                              !this->isType(ElementType::SCORE) && score()->dummy();

        // copy because moveToDummy might modify children
        EngravingObjectList children = children_;
        for (EngravingObject *c : children) {
            if (canMoveToDummy) {
                c->moveToDummy();
            } else {
                delete c;
            }
        }

        children_.clear();
    }

    if (links_) {
        links_->remove(this);
        if (links_->empty()) {
            delete links_;
            links_ = 0;
        }
    }
    delete[] propertyFlagsList_;
}

void EngravingObject::doSetParent(EngravingObject *p) {
    if (parent_ == p) {
        return;
    }

    IF_ASSERT_FAILED(p != this) { return; }

    if (parent_) {
        parent_->removeChild(this);
    }

    parent_ = p;

    if (parent_) {
        parent_->addChild(this);
    }
}

void EngravingObject::doSetScore(Score *sc) {
    if (score_ == sc) {
        return;
    }

    score_ = sc;

    for (EngravingObject *ch : children_) {
        ch->doSetScore(sc);
    }
}

void EngravingObject::moveToDummy() {
    Score *sc = score();
    if (sc) {
        if (sc->dummy() && sc->dummy() != this) {
            setOwnershipParent(sc->dummy());
        }
    }
}

static void collectChildrenItems(const EngravingObject *item, EngravingItemList &list, bool all) {
    for (EngravingObject *ch : item->children()) {
        if (ch->isEngravingItem()) {
            list.push_back(toEngravingItem(ch));

            if (all) {
                collectChildrenItems(ch, list, all);
            }
        }
    }
}

EngravingItemList EngravingObject::childrenItems(bool all) const {
    EngravingItemList list;
    collectChildrenItems(this, list, all);
    return list;
}

EngravingItemList EngravingObject::getChildren(bool includeInvisible) const {
    EngravingItemList childrenList;
    auto collectChildren = [&](EngravingItem *item) {
        if (item != this && (includeInvisible || item->visible())) {
            childrenList.push_back(item);
        }
    };

    const_cast<EngravingObject *>(this)->scanElements(collectChildren);

    return childrenList;
}

void EngravingObject::setScore(Score *s) {
    doSetScore(s);

    if (!parent_) {
        return;
    }

    if (parent_->score() == s) {
        return;
    }

    if (parent_->isType(ElementType::DUMMY)) {
        moveToDummy();
    } else if (parent_->isType(ElementType::SCORE)) {
        setOwnershipParent(s);
    }
}

void EngravingObject::addChild(EngravingObject *o) {
    IF_ASSERT_FAILED(o != this) { return; }

    children_.push_back(o);
}

void EngravingObject::removeChild(EngravingObject *o) {
    IF_ASSERT_FAILED(o->parent_ == this) { return; }
    o->parent_ = nullptr;

    // Search from the back: a child is often unparented shortly after it was added, so it
    // still sits near the end of the list. That matters for the dummy, whose child list is
    // long-lived and large. Empirical evidence shows that searching from the back walks on
    // average orders-of-magnitude less list entries than searching from the front.
    auto it = std::find(children_.rbegin(), children_.rend(), o);
    if (it != children_.rend()) {
        children_.erase(std::next(it).base());
    }
}

EngravingObject *EngravingObject::parent() const { return parent_; }

EngravingObject *EngravingObject::ownershipParent() const {
    if (!parent_ || parent_->isType(ElementType::DUMMY)) {
        return nullptr;
    }
    return parent_;
}

void EngravingObject::setOwnershipParent(EngravingObject *p) {
    IF_ASSERT_FAILED(this != p) { return; }

    if (!p) {
        moveToDummy();
        return;
    }

    doSetParent(p);
    if (parent_) {
        doSetScore(parent_->score());
    }
}

Score *EngravingObject::score() const { return score_; }

MasterScore *EngravingObject::masterScore() const { return score()->masterScore(); }

bool EngravingObject::onSameScore(const EngravingObject *other) const { return this->score() == other->score(); }

const MStyle &EngravingObject::style() const { return score()->style(); }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue EngravingObject::propertyDefault(Pid pid) const {
    Sid sid = getPropertyStyle(pid);
    if (sid != Sid::NOSTYLE) {
        return styleValue(pid, sid);
    }
    //      LOGD("<%s>(%d) not found in <%s>", propertyQmlName(pid), int(pid), typeName());
    return PropertyValue();
}

//---------------------------------------------------------
//   initElementStyle
//---------------------------------------------------------

void EngravingObject::initElementStyle(const ElementStyle *ss) {
    elementStyle_ = ss;
    size_t n = elementStyle_->size();
    delete[] propertyFlagsList_;
    propertyFlagsList_ = new PropertyFlags[n];
    for (size_t i = 0; i < n; ++i) {
        propertyFlagsList_[i] = PropertyFlags::STYLED;
    }
    for (const StyledProperty &spp : *elementStyle_) {
        //            setProperty(spp.pid, styleValue(spp.pid, spp.sid));
        setProperty(spp.pid, styleValue(spp.pid, getPropertyStyle(spp.pid)));
    }
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void EngravingObject::resetProperty(Pid pid) {
    PropertyValue v = propertyDefault(pid);
    if (v.isValid()) {
        setProperty(pid, v);
        PropertyFlags p = propertyFlags(pid);
        if (p == PropertyFlags::UNSTYLED) {
            setPropertyFlags(pid, PropertyFlags::STYLED);
        }
    }
}

//---------------------------------------------------------
//   undoResetProperty
//---------------------------------------------------------

static void changeProperties(EngravingObject *, Pid, const PropertyValue &, PropertyFlags, bool resetPath = false);

void EngravingObject::undoResetProperty(Pid id) {
    PropertyFlags f = propertyFlags(id);
    if (f == PropertyFlags::UNSTYLED) {
        f = PropertyFlags::STYLED;
    }
    if (isChordLine() && id == Pid::PATH) {
        if (toChordLine(this)->hasDefaultPath() && propertyFlags(id) == f) {
            return;
        }
        // A reset must run even when the layout cache is already empty.
        changeProperties(this, id, propertyDefault(id), f, true);
        changeProperties(this, Pid::GENERATED, false, PropertyFlags::NOSTYLE);
        return;
    }
    undoChangeProperty(id, propertyDefault(id), f);
}

//---------------------------------------------------------
//   isStyled
//---------------------------------------------------------

bool EngravingObject::isStyled(Pid pid) const {
    PropertyFlags f = propertyFlags(pid);
    return f == PropertyFlags::STYLED;
}

//---------------------------------------------------------
//   changeProperty
//---------------------------------------------------------

static void changeProperty(EngravingObject *e, Pid t, const PropertyValue &st, PropertyFlags ps, bool resetPath = false) {
    if (resetPath || (e->isChordLine() && t == Pid::PATH && toChordLine(e)->hasSourceOffsets()) || e->getProperty(t) != st ||
        e->propertyFlags(t) != ps) {
        if (e->isBracketItem()) {
            BracketItem *bi = toBracketItem(e);
            e->score()->undo(new ChangeBracketProperty(bi->startStaff(), bi->column(), t, st, ps));
        } else {
            e->score()->undo(new ChangeProperty(e, t, st, ps, resetPath));
        }
    }
}

//---------------------------------------------------------
//   changeProperties
//---------------------------------------------------------

static void changeProperties(EngravingObject *object, Pid propertyId, const PropertyValue &propertyValue, PropertyFlags propertyFlag,
                             bool resetPath) {
    const std::list<EngravingObject *> linkList = object->linkListForPropertyPropagation();
    for (EngravingObject *linkedObject : linkList) {
        if (linkedObject == object) {
            changeProperty(object, propertyId, propertyValue, propertyFlag, resetPath);
            continue;
        }

        if (!object->isEngravingItem() || !linkedObject->isEngravingItem()) {
            continue;
        }

        EngravingItem *item = toEngravingItem(object);
        EngravingItem *linkedItem = toEngravingItem(linkedObject);
        PropertyPropagation propertyPropagate = item->propertyPropagation(linkedItem, propertyId);
        switch (propertyPropagate) {
            case PropertyPropagation::NONE: break;
            case PropertyPropagation::PROPAGATE: changeProperty(linkedObject, propertyId, propertyValue, propertyFlag, resetPath); break;
            case PropertyPropagation::UNLINK: item->unlinkPropertyFromMaster(propertyId); break;
            default: break;
        }
    }

    if (!object->isEngravingItem() || propertyGroup(propertyId) == PropertyGroup::POSITION) {
        return;
    }

    EngravingItem *item = toEngravingItem(object);
    for (EngravingItem *originItem : item->originItems()) {
        // This is a shared item: propagate to all origin items
        changeProperty(originItem, propertyId, propertyValue, propertyFlag, resetPath);
    }
    if (EngravingItem *sharedItem = item->sharedItem(); sharedItem && sharedItem->originItems().front() == item) {
        // This is the first origin item of the shared item: propagate to shared item
        changeProperty(sharedItem, propertyId, propertyValue, propertyFlag, resetPath);
    }
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void EngravingObject::undoChangeProperty(Pid id, const PropertyValue &v) { undoChangeProperty(id, v, propertyFlags(id)); }

void EngravingObject::undoChangeProperty(Pid id, const PropertyValue &v, PropertyFlags ps) {
    const bool changesChordLinePathSource = isChordLine() && id == Pid::PATH && toChordLine(this)->hasSourceOffsets();
    if ((getProperty(id) == v) && (propertyFlags(id) == ps) && !changesChordLinePathSource) {
        return;
    }
    if (id == Pid::TEXT_STYLE) {
        //
        // change a list of properties
        //
        auto l = textStyle(v.value<TextStyleType>());
        // Change to ElementStyle defaults
        for (const auto &p : *l) {
            if (p.sid == Sid::NOSTYLE) {
                break;
            }
            changeProperties(this, p.pid, style().styleV(p.sid), PropertyFlags::STYLED);
        }
    } else if (id == Pid::EXCLUDE_FROM_OTHER_PARTS) {
        if (isEngravingItem() && getProperty(Pid::EXCLUDE_FROM_OTHER_PARTS) != v) {
            EngravingItem *delegate = toEngravingItem(propertyDelegate(id));
            if (delegate) {
                delegate->manageExclusionFromParts(v.toBool());
            } else {
                toEngravingItem(this)->manageExclusionFromParts(v.toBool());
            }
        }
    } else if (id == Pid::VOICE_ASSIGNMENT) {
        if (v.value<VoiceAssignment>() != VoiceAssignment::CURRENT_VOICE_ONLY) {
            changeProperties(this, Pid::VOICE, voice_idx_t(0), ps);
        }
    }
    changeProperties(this, id, v, ps);
    if (id != Pid::GENERATED) {
        changeProperties(this, Pid::GENERATED, false, PropertyFlags::NOSTYLE);
    }
}

//---------------------------------------------------------
//   undoPushProperty
//---------------------------------------------------------

void EngravingObject::undoPushProperty(Pid id) {
    PropertyValue val = getProperty(id);

    Transaction &tx = masterScore()->transactionManager()->currentOrDummyTransaction();
    tx.pushWithoutPerforming(new ChangeProperty(this, id, val));
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void EngravingObject::reset() {
    for (const StyledProperty &spp : *styledProperties()) {
        undoResetProperty(spp.pid);
    }
}

//---------------------------------------------------------
//   linkTo
//    link this to element
//---------------------------------------------------------
void EngravingObject::linkTo(EngravingObject *element) {
    assert(element != this);
    assert(!links_);

    if (element->links()) {
        setLinks(element->links_);
        assert(links_->contains(element));
    } else {
        setLinks(new LinkedObjects());
        links_->push_back(element);
        element->setLinks(links_);
    }
    assert(!links_->contains(this));
    links_->push_back(this);
}

//---------------------------------------------------------
//   unlink
//---------------------------------------------------------

void EngravingObject::unlink() {
    if (!links_) {
        return;
    }

    assert(links_->contains(this));
    links_->remove(this);

    // if link list is empty, remove list
    if (links_->size() <= 1) {
        if (!links_->empty()) {
            links_->front()->links_ = nullptr;
        }
        delete links_;
    }
    links_ = 0; // this element is not linked anymore
}

//---------------------------------------------------------
//   isLinked
///  return true if se is different and
///  linked to this element
//---------------------------------------------------------

bool EngravingObject::isLinked(EngravingObject *se) const {
    if (se == this || !links_) {
        return false;
    }

    if (se == nullptr) {
        return !links_->empty() && links_->mainElement() != this;
    }

    return links_->contains(se);
}

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void EngravingObject::undoUnlink() {
    if (links_) {
        score()->undo(new Unlink(this));
    }
}

void EngravingObject::setLinks(LinkedObjects *le) { links_ = le; }

//---------------------------------------------------------
//   linkList
//---------------------------------------------------------

std::list<EngravingObject *> EngravingObject::linkList() const {
    std::list<EngravingObject *> el;
    if (links_) {
        el = *links_;
    } else {
        el.push_back(const_cast<EngravingObject *>(this));
    }
    return el;
}

//---------------------------------------------------------
//   getPropertyFlagsIdx
//---------------------------------------------------------

int EngravingObject::getPropertyFlagsIdx(Pid id) const {
    int i = 0;
    for (const StyledProperty &p : *elementStyle_) {
        if (p.pid == id) {
            return i;
        }
        ++i;
    }
    return -1;
}

//---------------------------------------------------------
//   propertyFlags
//---------------------------------------------------------

PropertyFlags EngravingObject::propertyFlags(Pid id) const {
    if (EngravingObject *e = propertyDelegate(id)) {
        return e->propertyFlags(id);
    }

    static PropertyFlags f = PropertyFlags::NOSTYLE;

    int i = getPropertyFlagsIdx(id);
    if (i == -1) {
        return f;
    }
    return propertyFlagsList_[i];
}

//---------------------------------------------------------
//   setPropertyFlags
//---------------------------------------------------------

void EngravingObject::setPropertyFlags(Pid id, PropertyFlags f) {
    if (EngravingObject *e = propertyDelegate(id)) {
        e->setPropertyFlags(id, f);
        return;
    }

    int i = getPropertyFlagsIdx(id);
    if (i == -1) {
        return;
    }
    propertyFlagsList_[i] = f;
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid EngravingObject::getPropertyStyle(Pid id) const {
    for (const StyledProperty &p : *elementStyle_) {
        if (p.pid == id) {
            return p.sid;
        }
    }
    return Sid::NOSTYLE;
}

//---------------------------------------------------------
//   styleChanged
//---------------------------------------------------------

void EngravingObject::styleChanged() {
    for (const StyledProperty &spp : *elementStyle_) {
        PropertyFlags f = propertyFlags(spp.pid);
        if (f == PropertyFlags::STYLED) {
            setProperty(spp.pid, styleValue(spp.pid, getPropertyStyle(spp.pid)));
        }
    }
}

const char *EngravingObject::typeName() const { return TConv::toXml(type()).ascii(); }

TranslatableString EngravingObject::typeUserName() const { return TConv::capitalizedUserName(type()); }

String EngravingObject::translatedTypeUserName() const { return typeUserName().translated(); }

EID EngravingObject::eid() const { return masterScore()->eidRegister()->EIDFromItem(this); }

bool EngravingObject::setEID(EID id) const { return masterScore()->eidRegister()->registerItemEID(id, this); }

EID EngravingObject::assignNewEID() const { return masterScore()->eidRegister()->newEIDForItem(this); }

//---------------------------------------------------------
//   isSLineSegment
//---------------------------------------------------------

bool EngravingObject::isSLineSegment() const {
    return isHairpinSegment() || isOttavaSegment() || isPedalSegment() || isTrillSegment() || isVoltaSegment() || isTextLineSegment() ||
           isGlissandoSegment() || isLetRingSegment() || isVibratoSegment() || isPalmMuteSegment() || isGradualTempoChangeSegment() ||
           isWhammyBarSegment();
}

//---------------------------------------------------------
//   isText
//---------------------------------------------------------

bool EngravingObject::isTextBase() const { return muse::contains(TEXTBASE_TYPES, type()); }

//---------------------------------------------------------
//   styleValue
//---------------------------------------------------------

PropertyValue EngravingObject::styleValue(Pid pid, Sid sid) const {
    switch (propertyType(pid)) {
        case P_TYPE::ABSOLUTE: return style().styleAbsolute(sid);
        case P_TYPE::POINT: {
            PointF val = style().styleV(sid).value<PointF>();
            if (offsetIsSpatiumDependent()) {
                val *= style().spatium();
                if (isEngravingItem()) {
                    const EngravingItem *e = toEngravingItem(this);
                    if (e->staff() && !e->systemFlag()) {
                        val *= e->staff()->staffMag(e->tick());
                    }
                }
            } else {
                val *= DPMM;
            }
            return val;
        }
        default: return style().styleV(sid);
    }
}
} // namespace mu::engraving
