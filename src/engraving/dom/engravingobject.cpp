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
ElementStyle const EngravingObject::empty_style;

EngravingObject::EngravingObject(const ElementType &type, EngravingObject *parent) : type_(type) {
    IF_ASSERT_FAILED(this != parent) { return; }

    if (type != ElementType::SCORE) {
        assert(parent);
    }

    // The parent an object is constructed with is its parent: it is attached to it
    // right away. Passing the dummy means it is not attached to anything yet.
    do_set_parent(parent);
    if (parent_) {
        do_set_score(parent_->score());
    }

    if (type == ElementType::SCORE) {
        score_ = static_cast<Score *>(this);
    }
}

EngravingObject::EngravingObject(const EngravingObject &se) {
    type_ = se.type_;
    do_set_parent(se.parent_);
    score_ = se.score_;
    element_style_ = se.element_style_;
    if (element_style_) {
        size_t n = element_style_->size();
        property_flags_list_ = new PropertyFlags[n];
        for (size_t i = 0; i < n; ++i) {
            property_flags_list_[i] = se.property_flags_list_[i];
        }
    }
    links_ = 0;
}

//---------------------------------------------------------
//   ~ScoreElement
//---------------------------------------------------------

EngravingObject::~EngravingObject() {
    if (parent_) {
        parent_->remove_child(this);
    }

    {
        bool can_move_to_dummy = !this->is_type(ElementType::ROOT_ITEM) && !this->is_type(ElementType::DUMMY) &&
                                 !this->is_type(ElementType::SCORE) && score()->dummy();

        // copy because moveToDummy might modify children
        EngravingObjectList children = children_;
        for (EngravingObject *c : children) {
            if (can_move_to_dummy) {
                c->move_to_dummy();
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
    delete[] property_flags_list_;
}

void EngravingObject::do_set_parent(EngravingObject *p) {
    if (parent_ == p) {
        return;
    }

    IF_ASSERT_FAILED(p != this) { return; }

    if (parent_) {
        parent_->remove_child(this);
    }

    parent_ = p;

    if (parent_) {
        parent_->add_child(this);
    }
}

void EngravingObject::do_set_score(Score *sc) {
    if (score_ == sc) {
        return;
    }

    score_ = sc;

    for (EngravingObject *ch : children_) {
        ch->do_set_score(sc);
    }
}

void EngravingObject::move_to_dummy() {
    Score *sc = score();
    if (sc) {
        if (sc->dummy() && sc->dummy() != this) {
            set_ownership_parent(sc->dummy());
        }
    }
}

static void collect_children_items(const EngravingObject *item, EngravingItemList &list, bool all) {
    for (EngravingObject *ch : item->children()) {
        if (ch->is_engraving_item()) {
            list.push_back(to_engraving_item(ch));

            if (all) {
                collect_children_items(ch, list, all);
            }
        }
    }
}

EngravingItemList EngravingObject::children_items(bool all) const {
    EngravingItemList list;
    collect_children_items(this, list, all);
    return list;
}

EngravingItemList EngravingObject::get_children(bool include_invisible) const {
    EngravingItemList children_list;
    auto collect_children = [&](EngravingItem *item) {
        if (item != this && (include_invisible || item->visible())) {
            children_list.push_back(item);
        }
    };

    const_cast<EngravingObject *>(this)->scan_elements(collect_children);

    return children_list;
}

void EngravingObject::set_score(Score *s) {
    do_set_score(s);

    if (!parent_) {
        return;
    }

    if (parent_->score() == s) {
        return;
    }

    if (parent_->is_type(ElementType::DUMMY)) {
        move_to_dummy();
    } else if (parent_->is_type(ElementType::SCORE)) {
        set_ownership_parent(s);
    }
}

void EngravingObject::add_child(EngravingObject *o) {
    IF_ASSERT_FAILED(o != this) { return; }

    children_.push_back(o);
}

void EngravingObject::remove_child(EngravingObject *o) {
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

EngravingObject *EngravingObject::ownership_parent() const {
    if (!parent_ || parent_->is_type(ElementType::DUMMY)) {
        return nullptr;
    }
    return parent_;
}

void EngravingObject::set_ownership_parent(EngravingObject *p) {
    IF_ASSERT_FAILED(this != p) { return; }

    if (!p) {
        move_to_dummy();
        return;
    }

    do_set_parent(p);
    if (parent_) {
        do_set_score(parent_->score());
    }
}

Score *EngravingObject::score() const { return score_; }

MasterScore *EngravingObject::master_score() const { return score()->master_score(); }

bool EngravingObject::on_same_score(const EngravingObject *other) const { return this->score() == other->score(); }

const MStyle &EngravingObject::style() const { return score()->style(); }

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue EngravingObject::property_default(Pid pid) const {
    Sid sid = get_property_style(pid);
    if (sid != Sid::NOSTYLE) {
        return style_value(pid, sid);
    }
    //      LOGD("<%s>(%d) not found in <%s>", propertyQmlName(pid), int(pid), typeName());
    return PropertyValue();
}

//---------------------------------------------------------
//   initElementStyle
//---------------------------------------------------------

void EngravingObject::init_element_style(const ElementStyle *ss) {
    element_style_ = ss;
    size_t n = element_style_->size();
    delete[] property_flags_list_;
    property_flags_list_ = new PropertyFlags[n];
    for (size_t i = 0; i < n; ++i) {
        property_flags_list_[i] = PropertyFlags::STYLED;
    }
    for (const StyledProperty &spp : *element_style_) {
        //            setProperty(spp.pid, styleValue(spp.pid, spp.sid));
        set_property(spp.pid, style_value(spp.pid, get_property_style(spp.pid)));
    }
}

//---------------------------------------------------------
//   resetProperty
//---------------------------------------------------------

void EngravingObject::reset_property(Pid pid) {
    PropertyValue v = property_default(pid);
    if (v.is_valid()) {
        set_property(pid, v);
        PropertyFlags p = property_flags(pid);
        if (p == PropertyFlags::UNSTYLED) {
            set_property_flags(pid, PropertyFlags::STYLED);
        }
    }
}

//---------------------------------------------------------
//   undoResetProperty
//---------------------------------------------------------

static void change_properties(EngravingObject *, Pid, const PropertyValue &, PropertyFlags, bool reset_path = false);

void EngravingObject::undo_reset_property(Pid id) {
    PropertyFlags f = property_flags(id);
    if (f == PropertyFlags::UNSTYLED) {
        f = PropertyFlags::STYLED;
    }
    if (is_chord_line() && id == Pid::PATH) {
        if (to_chord_line(this)->has_default_path() && property_flags(id) == f) {
            return;
        }
        // A reset must run even when the layout cache is already empty.
        change_properties(this, id, property_default(id), f, true);
        change_properties(this, Pid::GENERATED, false, PropertyFlags::NOSTYLE);
        return;
    }
    undo_change_property(id, property_default(id), f);
}

//---------------------------------------------------------
//   isStyled
//---------------------------------------------------------

bool EngravingObject::is_styled(Pid pid) const {
    PropertyFlags f = property_flags(pid);
    return f == PropertyFlags::STYLED;
}

//---------------------------------------------------------
//   changeProperty
//---------------------------------------------------------

static void change_property(EngravingObject *e, Pid t, const PropertyValue &st, PropertyFlags ps, bool reset_path = false) {
    if (reset_path || (e->is_chord_line() && t == Pid::PATH && to_chord_line(e)->has_source_offsets()) || e->get_property(t) != st ||
        e->property_flags(t) != ps) {
        if (e->is_bracket_item()) {
            BracketItem *bi = to_bracket_item(e);
            e->score()->undo(new ChangeBracketProperty(bi->start_staff(), bi->column(), t, st, ps));
        } else {
            e->score()->undo(new ChangeProperty(e, t, st, ps, reset_path));
        }
    }
}

//---------------------------------------------------------
//   changeProperties
//---------------------------------------------------------

static void change_properties(EngravingObject *object, Pid property_id, const PropertyValue &property_value, PropertyFlags property_flag,
                              bool reset_path) {
    const std::list<EngravingObject *> link_list = object->link_list_for_property_propagation();
    for (EngravingObject *linked_object : link_list) {
        if (linked_object == object) {
            change_property(object, property_id, property_value, property_flag, reset_path);
            continue;
        }

        if (!object->is_engraving_item() || !linked_object->is_engraving_item()) {
            continue;
        }

        EngravingItem *item = to_engraving_item(object);
        EngravingItem *linked_item = to_engraving_item(linked_object);
        PropertyPropagation property_propagate = item->property_propagation(linked_item, property_id);
        switch (property_propagate) {
            case PropertyPropagation::NONE: break;
            case PropertyPropagation::PROPAGATE:
                change_property(linked_object, property_id, property_value, property_flag, reset_path);
                break;
            case PropertyPropagation::UNLINK: item->unlink_property_from_master(property_id); break;
            default: break;
        }
    }

    if (!object->is_engraving_item() || property_group(property_id) == PropertyGroup::POSITION) {
        return;
    }

    EngravingItem *item = to_engraving_item(object);
    for (EngravingItem *origin_item : item->origin_items()) {
        // This is a shared item: propagate to all origin items
        change_property(origin_item, property_id, property_value, property_flag, reset_path);
    }
    if (EngravingItem *shared_item = item->shared_item(); shared_item && shared_item->origin_items().front() == item) {
        // This is the first origin item of the shared item: propagate to shared item
        change_property(shared_item, property_id, property_value, property_flag, reset_path);
    }
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void EngravingObject::undo_change_property(Pid id, const PropertyValue &v) { undo_change_property(id, v, property_flags(id)); }

void EngravingObject::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    const bool changes_chord_line_path_source = is_chord_line() && id == Pid::PATH && to_chord_line(this)->has_source_offsets();
    if ((get_property(id) == v) && (property_flags(id) == ps) && !changes_chord_line_path_source) {
        return;
    }
    if (id == Pid::TEXT_STYLE) {
        //
        // change a list of properties
        //
        auto l = text_style(v.value<TextStyleType>());
        // Change to ElementStyle defaults
        for (const auto &p : *l) {
            if (p.sid == Sid::NOSTYLE) {
                break;
            }
            change_properties(this, p.pid, style().style_v(p.sid), PropertyFlags::STYLED);
        }
    } else if (id == Pid::EXCLUDE_FROM_OTHER_PARTS) {
        if (is_engraving_item() && get_property(Pid::EXCLUDE_FROM_OTHER_PARTS) != v) {
            EngravingItem *delegate = to_engraving_item(property_delegate(id));
            if (delegate) {
                delegate->manage_exclusion_from_parts(v.to_bool());
            } else {
                to_engraving_item(this)->manage_exclusion_from_parts(v.to_bool());
            }
        }
    } else if (id == Pid::VOICE_ASSIGNMENT) {
        if (v.value<VoiceAssignment>() != VoiceAssignment::CURRENT_VOICE_ONLY) {
            change_properties(this, Pid::VOICE, voice_idx_t(0), ps);
        }
    }
    change_properties(this, id, v, ps);
    if (id != Pid::GENERATED) {
        change_properties(this, Pid::GENERATED, false, PropertyFlags::NOSTYLE);
    }
}

//---------------------------------------------------------
//   undoPushProperty
//---------------------------------------------------------

void EngravingObject::undo_push_property(Pid id) {
    PropertyValue val = get_property(id);

    Transaction &tx = master_score()->transaction_manager()->current_or_dummy_transaction();
    tx.push_without_performing(new ChangeProperty(this, id, val));
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void EngravingObject::reset() {
    for (const StyledProperty &spp : *styled_properties()) {
        undo_reset_property(spp.pid);
    }
}

//---------------------------------------------------------
//   linkTo
//    link this to element
//---------------------------------------------------------
void EngravingObject::link_to(EngravingObject *element) {
    assert(element != this);
    assert(!links_);

    if (element->links()) {
        set_links(element->links_);
        assert(links_->contains(element));
    } else {
        set_links(new LinkedObjects());
        links_->push_back(element);
        element->set_links(links_);
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

bool EngravingObject::is_linked(EngravingObject *se) const {
    if (se == this || !links_) {
        return false;
    }

    if (se == nullptr) {
        return !links_->empty() && links_->main_element() != this;
    }

    return links_->contains(se);
}

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void EngravingObject::undo_unlink() {
    if (links_) {
        score()->undo(new Unlink(this));
    }
}

void EngravingObject::set_links(LinkedObjects *le) { links_ = le; }

//---------------------------------------------------------
//   linkList
//---------------------------------------------------------

std::list<EngravingObject *> EngravingObject::link_list() const {
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

int EngravingObject::get_property_flags_idx(Pid id) const {
    int i = 0;
    for (const StyledProperty &p : *element_style_) {
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

PropertyFlags EngravingObject::property_flags(Pid id) const {
    if (EngravingObject *e = property_delegate(id)) {
        return e->property_flags(id);
    }

    static PropertyFlags f = PropertyFlags::NOSTYLE;

    int i = get_property_flags_idx(id);
    if (i == -1) {
        return f;
    }
    return property_flags_list_[i];
}

//---------------------------------------------------------
//   setPropertyFlags
//---------------------------------------------------------

void EngravingObject::set_property_flags(Pid id, PropertyFlags f) {
    if (EngravingObject *e = property_delegate(id)) {
        e->set_property_flags(id, f);
        return;
    }

    int i = get_property_flags_idx(id);
    if (i == -1) {
        return;
    }
    property_flags_list_[i] = f;
}

//---------------------------------------------------------
//   getPropertyStyle
//---------------------------------------------------------

Sid EngravingObject::get_property_style(Pid id) const {
    for (const StyledProperty &p : *element_style_) {
        if (p.pid == id) {
            return p.sid;
        }
    }
    return Sid::NOSTYLE;
}

//---------------------------------------------------------
//   styleChanged
//---------------------------------------------------------

void EngravingObject::style_changed() {
    for (const StyledProperty &spp : *element_style_) {
        PropertyFlags f = property_flags(spp.pid);
        if (f == PropertyFlags::STYLED) {
            set_property(spp.pid, style_value(spp.pid, get_property_style(spp.pid)));
        }
    }
}

const char *EngravingObject::type_name() const { return TConv::to_xml(type()).ascii(); }

TranslatableString EngravingObject::type_user_name() const { return TConv::capitalized_user_name(type()); }

String EngravingObject::translated_type_user_name() const { return type_user_name().translated(); }

EID EngravingObject::eid() const { return master_score()->eid_register()->eid_from_item(this); }

bool EngravingObject::set_eid(EID id) const { return master_score()->eid_register()->register_item_eid(id, this); }

EID EngravingObject::assign_new_eid() const { return master_score()->eid_register()->new_eid_for_item(this); }

//---------------------------------------------------------
//   isSLineSegment
//---------------------------------------------------------

bool EngravingObject::is_s_line_segment() const {
    return is_hairpin_segment() || is_ottava_segment() || is_pedal_segment() || is_trill_segment() || is_volta_segment() ||
           is_text_line_segment() || is_glissando_segment() || is_let_ring_segment() || is_vibrato_segment() || is_palm_mute_segment() ||
           is_gradual_tempo_change_segment() || is_whammy_bar_segment();
}

//---------------------------------------------------------
//   isText
//---------------------------------------------------------

bool EngravingObject::is_text_base() const { return muse::contains(::mu::engraving::textbase_types, type()); }

//---------------------------------------------------------
//   styleValue
//---------------------------------------------------------

PropertyValue EngravingObject::style_value(Pid pid, Sid sid) const {
    switch (property_type(pid)) {
        case PType::ABSOLUTE: return style().style_absolute(sid);
        case PType::POINT: {
            PointF val = style().style_v(sid).value<PointF>();
            if (offset_is_spatium_dependent()) {
                val *= style().spatium();
                if (is_engraving_item()) {
                    const EngravingItem *e = to_engraving_item(this);
                    if (e->staff() && !e->system_flag()) {
                        val *= e->staff()->staff_mag(e->tick());
                    }
                }
            } else {
                val *= ::mu::engraving::dpmm;
            }
            return val;
        }
        default: return style().style_v(sid);
    }
}
} // namespace mu::engraving
