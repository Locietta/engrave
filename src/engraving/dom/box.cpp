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

#include "../editing/editfretboarddiagram.h"
#include "factory.h"
#include "fret.h"
#include "harmony.h"
#include "layoutbreak.h"
#include "log.h"
#include "masterscore.h"
#include "measure.h"
#include "mscore.h"
#include "score.h"
#include "stafftext.h"
#include "system.h"
#include "text.h"

using namespace mu;
using namespace mu::engraving;
using namespace muse::draw;

namespace mu::engraving {
static const ElementStyle box_style{
    {Sid::SYSTEM_FRAME_DISTANCE, Pid::TOP_GAP},
    {Sid::FRAME_SYSTEM_DISTANCE, Pid::BOTTOM_GAP},
    {Sid::PADDING_TO_NOTATION_ABOVE, Pid::PADDING_TO_NOTATION_ABOVE},
    {Sid::PADDING_TO_NOTATION_BELOW, Pid::PADDING_TO_NOTATION_BELOW},
};

static const ElementStyle h_box_style{};

static const String fret_box_diagrams_separator = u",";

Box::Box(const ElementType &type, Score *parent) : MeasureBase(type, parent) {}

//---------------------------------------------------------
//   computeMinWidth
//---------------------------------------------------------

void HBox::compute_min_width() {
    set_width(absolute_from_spatium(box_width() + top_gap() + bottom_gap())); // top/bottom is really left/right
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to Box
//---------------------------------------------------------

void Box::add(EngravingItem *e) {
    if (e->is_text()) {
        to_text(e)->set_layout_to_parent_width(true);
    }
    MeasureBase::add(e);
}

double Box::absolute_from_spatium(const Spatium &val) const {
    const double sp = size_is_spatium_dependent() ? spatium() : style().default_spatium();
    return val.val() * sp;
}

RectF Box::content_rect() const {
    RectF result;

    for (const EngravingItem *element : el()) {
        result = result.united(element->ldata()->bbox());
    }

    return result;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Box::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::BOX_HEIGHT: return box_height_;
        case Pid::BOX_WIDTH: return box_width_;
        case Pid::TOP_GAP: return top_gap_;
        case Pid::BOTTOM_GAP: return bottom_gap_;
        case Pid::LEFT_MARGIN: return left_margin_;
        case Pid::RIGHT_MARGIN: return right_margin_;
        case Pid::TOP_MARGIN: return top_margin_;
        case Pid::BOTTOM_MARGIN: return bottom_margin_;
        case Pid::BOX_AUTOSIZE: return (score()->msc_version() >= 302) ? is_auto_size_enabled_ : false;
        default: return MeasureBase::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Box::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::BOX_HEIGHT: box_height_ = v.value<Spatium>(); break;
        case Pid::BOX_WIDTH: box_width_ = v.value<Spatium>(); break;
        case Pid::TOP_GAP: top_gap_ = v.value<Spatium>(); break;
        case Pid::BOTTOM_GAP: bottom_gap_ = v.value<Spatium>(); break;
        case Pid::LEFT_MARGIN: left_margin_ = v.to_double(); break;
        case Pid::RIGHT_MARGIN: right_margin_ = v.to_double(); break;
        case Pid::TOP_MARGIN: top_margin_ = v.to_double(); break;
        case Pid::BOTTOM_MARGIN: bottom_margin_ = v.to_double(); break;
        case Pid::BOX_AUTOSIZE: is_auto_size_enabled_ = v.to_bool(); break;
        default: return MeasureBase::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Box::property_default(Pid id) const {
    switch (id) {
        case Pid::BOX_HEIGHT:
        case Pid::BOX_WIDTH: return 0.0_sp;

        case Pid::TOP_GAP: return is_h_box() ? 0.0_sp : style().style_s(Sid::SYSTEM_FRAME_DISTANCE);
        case Pid::BOTTOM_GAP: return is_h_box() ? 0.0_sp : style().style_s(Sid::FRAME_SYSTEM_DISTANCE);

        case Pid::LEFT_MARGIN:
        case Pid::RIGHT_MARGIN:
        case Pid::TOP_MARGIN:
        case Pid::BOTTOM_MARGIN: return 0.0;
        case Pid::BOX_AUTOSIZE: return true;
        case Pid::SIZE_SPATIUM_DEPENDENT: return !is_title_frame();
        default: return MeasureBase::property_default(id);
    }
}

bool Box::is_title_frame() const { return this == score()->first() && is_v_box(); }

//---------------------------------------------------------
//   copyValues
//---------------------------------------------------------

void Box::copy_values(Box *origin) {
    box_height_ = origin->box_height();
    box_width_ = origin->box_width();

    double factor = mag_s() / origin->mag_s();
    bottom_gap_ = origin->bottom_gap() * factor;
    top_gap_ = origin->top_gap() * factor;
    bottom_margin_ = origin->bottom_margin() * factor;
    top_margin_ = origin->top_margin() * factor;
    left_margin_ = origin->left_margin() * factor;
    right_margin_ = origin->right_margin() * factor;

    set_size_is_spatium_dependent(origin->size_is_spatium_dependent());
}

//---------------------------------------------------------
//   HBox
//---------------------------------------------------------

HBox::HBox(Score *parent) : Box(ElementType::HBOX, parent) {
    init_element_style(&::mu::engraving::h_box_style);
    reset_property(Pid::BOX_WIDTH);
}

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

void Box::manage_exclusion_from_parts(bool exclude) {
    // manage Layout Breaks - remove old ones first
    LayoutBreak *section_break = section_break_element();
    if (section_break) {
        to_engraving_item(section_break)->manage_exclusion_from_parts(true);
    }

    bool title_frame = is_title_frame();
    if (exclude) {
        const std::list<EngravingObject *> links = link_list();
        for (EngravingObject *linked_object : links) {
            // Only remove title frame from score
            if (linked_object->score() == score() || (!this->score()->is_master() && title_frame && !linked_object->score()->is_master())) {
                continue;
            }
            EngravingItem *linked_item = to_engraving_item(linked_object);
            if (linked_item->selected()) {
                linked_item->score()->deselect(linked_item);
            }
            linked_item->score()->undo_remove_element(linked_item, false);
            linked_item->undo_unlink();
        }

        // manage Layout Breaks - there are no linked boxes, so add linked Line Breaks to previous measure
        if (section_break && !title_frame) {
            if (MeasureBase *prev_measure = this->prev_measure()) {
                for (Score *score : master_score()->score_list()) {
                    if (score == this->score()) {
                        continue;
                    }
                    if (MeasureBase *local_prev_measure = score->tick2measure(prev_measure->tick())) {
                        EngravingItem *new_section_break = section_break->linked_clone();
                        new_section_break->set_score(score);
                        new_section_break->set_ownership_parent(local_prev_measure);
                        score->do_undo_add_element(new_section_break);
                    }
                }
            }
        }
    } else {
        for (Score *score : master_score()->score_list()) {
            if (score == this->score() || (title_frame && !this->score()->is_master() && !score->is_master())) {
                continue;
            }

            MeasureBase *new_mb = next() ? next()->get_in_score(score, true) : nullptr;
            Score::InsertMeasureOptions options;
            options.clone_box_to_all_parts = false;
            MeasureBase *new_frame = score->insert_box(type(), new_mb, options);
            new_frame->set_exclude_from_other_parts(false);
            // newFrame->setSizeIsSpatiumDependent(!titleFrame);

            if (!is_f_box()) {
                for (EngravingItem *item : el()) {
                    // Don't add instrument name from current part
                    if (item->is_text() && to_text(item)->text_style_type() == TextStyleType::INSTRUMENT_EXCERPT) {
                        continue;
                    }
                    // add frame items (Layout Break, Title, ...)
                    new_frame->add(item->linked_clone());
                }
            }

            if (is_t_box()) {
                Text *this_text = to_t_box(this)->text();
                to_t_box(new_frame)->add(this_text->linked_clone());
            }

            if (!score->is_master() && title_frame) {
                // Title frame - add part name
                String part_label = score->name();
                if (!part_label.empty()) {
                    Text *txt = Factory::create_text(new_frame, TextStyleType::INSTRUMENT_EXCERPT);
                    txt->set_plain_text(part_label);
                    new_frame->add(txt);

                    score->set_meta_tag(u"partName", part_label);
                }
            }

            new_frame->link_to(this);
        }
    }
}

//---------------------------------------------------------
//   isMovable
//---------------------------------------------------------

bool HBox::is_movable() const { return ownership_parent() && (ownership_parent()->is_h_box() || ownership_parent()->is_v_box()); }

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue HBox::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::CREATE_SYSTEM_HEADER: return create_system_header();
        default: return Box::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool HBox::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::CREATE_SYSTEM_HEADER:
            set_create_system_header(v.to_bool());
            trigger_layout();
            break;
        default: return Box::set_property(property_id, v);
    }
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue HBox::property_default(Pid id) const {
    switch (id) {
        case Pid::CREATE_SYSTEM_HEADER: return true;
        case Pid::BOX_WIDTH: return 5.0_sp;
        default: return Box::property_default(id);
    }
}

//---------------------------------------------------------
//   VBox
//---------------------------------------------------------

VBox::VBox(const ElementType &type, Score *parent) : Box(type, parent) {
    init_element_style(&::mu::engraving::box_style);

    reset_property(Pid::BOX_HEIGHT);

    set_line_break(true);
}

VBox::VBox(Score *parent) : VBox(ElementType::VBOX, parent) {}

void VBox::add(EngravingItem *e) {
    IF_ASSERT_FAILED(e) { return; }
    if (e->is_layout_break()) {
        const LayoutBreakType type = to_layout_break(e)->layout_break_type();
        if (type == LayoutBreakType::LINE) {
            LOGW("Cannot add %s", e->type_name());
            return;
        }
    }

    Box::add(e);
}

double VBox::min_height() const { return absolute_from_spatium(10_sp); }

double VBox::max_height() const { return absolute_from_spatium(30_sp); }

PropertyValue VBox::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::BOX_AUTOSIZE: return is_auto_size_enabled();
        case Pid::PADDING_TO_NOTATION_ABOVE: return padding_to_notation_above_;
        case Pid::PADDING_TO_NOTATION_BELOW: return padding_to_notation_below_;
        default: return Box::get_property(property_id);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue VBox::property_default(Pid id) const {
    switch (id) {
        case Pid::BOX_HEIGHT: return 10.0_sp;
        default: return Box::property_default(id);
    }
}

bool VBox::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::PADDING_TO_NOTATION_ABOVE: padding_to_notation_above_ = v.value<Spatium>(); break;
        case Pid::PADDING_TO_NOTATION_BELOW: padding_to_notation_below_ = v.value<Spatium>(); break;
        default: return Box::set_property(property_id, v);
    }

    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   startDragGrip
//---------------------------------------------------------

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a e to fret diagram box
//---------------------------------------------------------

FBox::FBox(Score *parent) : VBox(ElementType::FBOX, parent) {
    reset_property(Pid::FRET_FRAME_TEXT_SCALE);
    reset_property(Pid::FRET_FRAME_DIAGRAM_SCALE);
    reset_property(Pid::FRET_FRAME_COLUMN_GAP);
    reset_property(Pid::FRET_FRAME_ROW_GAP);
    reset_property(Pid::FRET_FRAME_CHORDS_PER_ROW);
    reset_property(Pid::FRET_FRAME_H_ALIGN);

    reset_property(Pid::LEFT_MARGIN);
    reset_property(Pid::RIGHT_MARGIN);
    reset_property(Pid::TOP_MARGIN);
    reset_property(Pid::BOTTOM_MARGIN);
    reset_property(Pid::TOP_GAP);
    reset_property(Pid::BOTTOM_GAP);
    reset_property(Pid::EXCLUDE_FROM_OTHER_PARTS);
    reset_property(Pid::APPEARANCE_LINKED_TO_MASTER);
    reset_property(Pid::POSITION_LINKED_TO_MASTER);
}

void FBox::init() {
    StringList old_diagrams_names;
    std::vector<FretDiagram *> old_diagrams;
    for (EngravingItem *element : el()) {
        FretDiagram *diagram = to_fret_diagram(element);
        old_diagrams.push_back(diagram);
        old_diagrams_names.push_back(diagram->harmony_display_text());
    }

    StringList diagrams_names_in_score;
    std::vector<EngravingItem *> harmony_or_diagrams_in_score;
    for (mu::engraving::Segment *segment = score()->first_segment(mu::engraving::SegmentType::CHORD_REST); segment;
         segment = segment->next1(mu::engraving::SegmentType::CHORD_REST)) {
        for (EngravingItem *item : segment->annotations()) {
            if (!item || !item->part()) {
                continue;
            }

            if (item->is_harmony() && to_harmony(item)->harmony_type() != HarmonyType::STANDARD) {
                continue;
            }

            if (!(item->is_harmony() || item->is_fret_diagram())) {
                continue;
            }

            String harmony_name = item->is_harmony()      ? to_harmony(item)->display_text() :
                                  item->is_fret_diagram() ? to_fret_diagram(item)->harmony_display_text() :
                                                            String();
            if (harmony_name.empty() || muse::contains(diagrams_names_in_score, harmony_name)) {
                continue;
            }

            harmony_or_diagrams_in_score.push_back(item);
            diagrams_names_in_score.push_back(harmony_name);
        }
    }

    diagrams_order_in_score_ = diagrams_names_in_score;

    for (size_t i = 0; i < old_diagrams_names.size(); ++i) {
        String old_name = old_diagrams_names[i];
        if (!muse::contains(diagrams_names_in_score, old_name)) {
            score()->undo(new RemoveFretDiagramFromFretBox(old_diagrams[i]));
        }
    }

    for (size_t i = 0; i < diagrams_names_in_score.size(); ++i) {
        String new_name = diagrams_names_in_score[i];
        if (!muse::contains(old_diagrams_names, new_name)) {
            FretDiagram *new_diagram = FretDiagram::make_from_harmony_or_fret_diagram(harmony_or_diagrams_in_score[i]);
            new_diagram->set_ownership_parent(this);
            String name_of_diagram_before_this = i > 0 ? diagrams_names_in_score[i - 1] : String();
            size_t idx = compute_insertion_idx(name_of_diagram_before_this);
            score()->undo(new AddFretDiagramToFretBox(new_diagram, idx));
        }
    }
}

void FBox::add(EngravingItem *e) {
    e->set_ownership_parent(this);
    if (e->is_fret_diagram()) {
        add_at_idx(to_fret_diagram(e), muse::nidx);
    } else {
        LOGD("FBox::add: element not allowed");
        return;
    }
}

void FBox::add_at_idx(FretDiagram *fret_diagram, size_t idx) {
    fret_diagram->set_track(muse::nidx);
    fret_diagram->set_flag(ElementFlag::MOVABLE, false);
    fret_diagram->set_flag(ElementFlag::ON_STAFF, false);

    Harmony *harmony = fret_diagram->harmony();
    harmony->set_track(muse::nidx);
    harmony->set_flag(ElementFlag::MOVABLE, false);
    harmony->set_flag(ElementFlag::ON_STAFF, false);

    if (idx < el_.size()) {
        el_.insert(el_.begin() + idx, fret_diagram);
    } else {
        el_.push_back(fret_diagram);
    }

    fret_diagram->added();
}

size_t FBox::compute_insertion_idx(const String &name_of_diagram_before_this) {
    if (name_of_diagram_before_this.empty()) {
        return 0;
    }

    for (size_t i = 0; i < el_.size(); ++i) {
        FretDiagram *fret_diagram = to_fret_diagram(el_[i]);
        if (fret_diagram->harmony_display_text() == name_of_diagram_before_this) {
            return i + 1;
        }
    }

    return muse::nidx;
}

PropertyValue FBox::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::FRET_FRAME_TEXT_SCALE: return text_scale_;
        case Pid::FRET_FRAME_DIAGRAM_SCALE: return diagram_scale_;
        case Pid::FRET_FRAME_COLUMN_GAP: return column_gap_;
        case Pid::FRET_FRAME_ROW_GAP: return row_gap_;
        case Pid::FRET_FRAME_CHORDS_PER_ROW: return chords_per_row_;
        case Pid::FRET_FRAME_H_ALIGN: return static_cast<int>(content_alignment_h_);
        case Pid::LEFT_MARGIN: return content_alignment_h_ == AlignH::LEFT ? VBox::get_property(property_id) : PropertyValue();
        case Pid::RIGHT_MARGIN: return content_alignment_h_ == AlignH::RIGHT ? VBox::get_property(property_id) : PropertyValue();
        case Pid::FRET_FRAME_DIAGRAMS_ORDER: return diagrams_order().join(::mu::engraving::fret_box_diagrams_separator);
        default: return VBox::get_property(property_id);
    }
}

bool FBox::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::FRET_FRAME_TEXT_SCALE: text_scale_ = val.to_double(); break;
        case Pid::FRET_FRAME_DIAGRAM_SCALE: diagram_scale_ = val.to_double(); break;
        case Pid::FRET_FRAME_COLUMN_GAP: column_gap_ = val.value<Spatium>(); break;
        case Pid::FRET_FRAME_ROW_GAP: row_gap_ = val.value<Spatium>(); break;
        case Pid::FRET_FRAME_CHORDS_PER_ROW: chords_per_row_ = val.to_int(); break;
        case Pid::FRET_FRAME_H_ALIGN:
            content_alignment_h_ = static_cast<AlignH>(val.to_int());
            reset_property(Pid::LEFT_MARGIN);
            reset_property(Pid::RIGHT_MARGIN);
            break;
        case Pid::FRET_FRAME_DIAGRAMS_ORDER:
            reorder_elements(val.value<String>().split(::mu::engraving::fret_box_diagrams_separator));
            break;
        default: return VBox::set_property(property_id, val);
    }

    trigger_layout();
    return true;
}

PropertyValue FBox::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::FRET_FRAME_TEXT_SCALE:
        case Pid::FRET_FRAME_DIAGRAM_SCALE: return 1.0;
        case Pid::FRET_FRAME_COLUMN_GAP:
        case Pid::FRET_FRAME_ROW_GAP: return 3.0_sp;
        case Pid::FRET_FRAME_CHORDS_PER_ROW: return 8;
        case Pid::FRET_FRAME_H_ALIGN: return static_cast<int>(AlignH::HCENTER);
        case Pid::FRET_FRAME_DIAGRAMS_ORDER: return diagrams_order_in_score_.join(::mu::engraving::fret_box_diagrams_separator);
        case Pid::EXCLUDE_FROM_OTHER_PARTS: return true;
        case Pid::APPEARANCE_LINKED_TO_MASTER:
        case Pid::POSITION_LINKED_TO_MASTER: return false;
        default: return VBox::property_default(property_id);
    }
}

void FBox::undo_reorder_elements(const StringList &new_order) {
    undo_change_property(Pid::FRET_FRAME_DIAGRAMS_ORDER, new_order.join(::mu::engraving::fret_box_diagrams_separator));
    trigger_layout();
}

void FBox::reorder_elements(const StringList &new_order) {
    std::sort(el_.begin(), el_.end(), [&](EngravingItem *a, EngravingItem *b) {
        String name_a = to_fret_diagram(a)->harmony_display_text();
        String name_b = to_fret_diagram(b)->harmony_display_text();
        auto iter_a = std::find(new_order.begin(), new_order.end(), name_a);
        auto iter_b = std::find(new_order.begin(), new_order.end(), name_b);
        return iter_a < iter_b;
    });
}

StringList FBox::diagrams_order() const {
    StringList result;
    for (EngravingItem *item : el_) {
        result.push_back(to_fret_diagram(item)->harmony_display_text());
    }

    return result;
}

//---------------------------------------------------------
//   TBox
//---------------------------------------------------------

TBox::TBox(Score *parent) : VBox(ElementType::TBOX, parent) {
    reset_property(Pid::BOX_HEIGHT);
    text_ = Factory::create_text(this, TextStyleType::FRAME);
    text_->set_layout_to_parent_width(true);
}

TBox::TBox(const TBox &tbox) : VBox(tbox) {
    if (tbox.text_) {
        add(Factory::copy_text(*(tbox.text_)));
    }
}

TBox::~TBox() { delete text_; }

void TBox::scan_elements(std::function<void(EngravingItem *)> func) {
    text_->scan_elements(func);
    Box::scan_elements(func);
}

//---------------------------------------------------------
//   add
///   Add new EngravingItem \a el to TBox
//---------------------------------------------------------

void TBox::add(EngravingItem *e) {
    if (e->is_text()) {
        if (text_) {
            delete text_;
        }
        text_ = to_text(e);
        text_->set_layout_to_parent_width(true);
        e->set_ownership_parent(this);
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
        text_ = Factory::create_text(this, TextStyleType::FRAME);
        text_->set_layout_to_parent_width(true);
        el->removed();
    } else {
        VBox::remove(el);
    }
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue TBox::property_default(Pid id) const {
    switch (id) {
        case Pid::BOX_HEIGHT: return 1_sp;
        default: return VBox::property_default(id);
    }
}

//---------------------------------------------------------
//   accessibleExtraInfo
//---------------------------------------------------------

} // namespace mu::engraving
