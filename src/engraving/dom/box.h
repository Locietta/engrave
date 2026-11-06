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

#pragma once

#include "measurebase.h"
#include "property.h"

namespace mu::engraving {
struct Transaction;

//---------------------------------------------------------
//   Box
//   virtual base class for frames "boxes"
//---------------------------------------------------------

struct Box : MeasureBase {
private:
    OBJECT_ALLOCATOR(engraving, Box)

public:
    Box(const ElementType &type, Score *parent);

    virtual bool accept_drop(EditData &) const override;
    virtual EngravingItem *drop(Transaction &tx, EditData &) override;
    virtual void add(EngravingItem *e) override;
    virtual double absolute_from_spatium(const Spatium &val) const override;

    RectF content_rect() const;

    Spatium box_width() const { return box_width_; }

    void set_box_width(Spatium val) { box_width_ = val; }

    Spatium box_height() const { return box_height_; }

    void set_box_height(Spatium val) { box_height_ = val; }

    double left_margin() const { return left_margin_; }

    double right_margin() const { return right_margin_; }

    double top_margin() const { return top_margin_; }

    double bottom_margin() const { return bottom_margin_; }

    void set_left_margin(double val) { left_margin_ = val; }

    void set_right_margin(double val) { right_margin_ = val; }

    void set_top_margin(double val) { top_margin_ = val; }

    void set_bottom_margin(double val) { bottom_margin_ = val; }

    Spatium top_gap() const { return top_gap_; }

    void set_top_gap(Spatium val) { top_gap_ = val; }

    Spatium bottom_gap() const { return bottom_gap_; }

    void set_bottom_gap(Spatium val) { bottom_gap_ = val; }

    bool is_auto_size_enabled() const { return is_auto_size_enabled_; }

    void set_auto_size_enabled(const bool val) { is_auto_size_enabled_ = val; }

    void copy_values(Box *origin);
    bool is_title_frame() const;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    // TODO: add a grip for moving the entire box
    // overridden in descendants

    bool can_be_excluded_from_other_parts() const override { return true; }

    void manage_exclusion_from_parts(bool exclude) override;

private:
    Spatium box_width_;  // only valid for HBox
    Spatium box_height_; // only valid for VBox
    Spatium top_gap_;    // distance from previous system (left border for hbox)
                         // initialized with Sid::systemFrameDistance
    Spatium bottom_gap_; // distance to next system (right border for hbox)
                         // initialized with Sid::frameSystemDistance
    double left_margin_ = 0.0;
    double right_margin_ = 0.0; // inner margins in metric mm
    double top_margin_ = 0.0;
    double bottom_margin_ = 0.0;
    bool is_auto_size_enabled_ = true;
};

//---------------------------------------------------------
//   HBox
//   horizontal frame
//---------------------------------------------------------

struct HBox final : Box {
private:
    OBJECT_ALLOCATOR(engraving, HBox)
    DECLARE_CLASSOF(ElementType::HBOX)

public:
    HBox(Score *parent);

    HBox *clone() const override { return new HBox(*this); }

    bool is_movable() const override;
    void compute_min_width() override;

    bool create_system_header() const { return create_system_header_; }

    void set_create_system_header(bool val) { create_system_header_ = val; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

private:
    bool create_system_header_ = true;
};

//---------------------------------------------------------
//   VBox
//   vertical frame
//---------------------------------------------------------

struct VBox : Box {
private:
    OBJECT_ALLOCATOR(engraving, VBox)
    DECLARE_CLASSOF(ElementType::VBOX)

public:
    VBox(Score *parent);

    VBox *clone() const override { return new VBox(*this); }

    void add(EngravingItem *e) override;

    double min_height() const;
    double max_height() const;

    PropertyValue get_property(Pid property_id) const override;
    PropertyValue property_default(Pid) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;

    bool accept_drop(EditData &) const override;

    Spatium padding_to_notation_above() const { return padding_to_notation_above_; }

    Spatium padding_to_notation_below() const { return padding_to_notation_below_; }

protected:
    VBox(const ElementType &type, Score *parent);

private:
    Spatium padding_to_notation_above_;
    Spatium padding_to_notation_below_;
};

//---------------------------------------------------------
//   FBox
//   frame containing fret diagrams
//---------------------------------------------------------

struct FBox : VBox {
private:
    OBJECT_ALLOCATOR(engraving, FBox)
    DECLARE_CLASSOF(ElementType::FBOX)

public:
    FBox(Score *parent);

    FBox *clone() const override { return new FBox(*this); }

    void init();

    void add(EngravingItem *) override;
    void add_at_idx(FretDiagram *fret_diagram, size_t idx);

    double text_scale() const { return text_scale_; }

    double diagram_scale() const { return diagram_scale_; }

    Spatium column_gap() const { return column_gap_; }

    Spatium row_gap() const { return row_gap_; }

    int chords_per_row() const { return chords_per_row_; }

    AlignH content_horizontal_alignment() const { return content_alignment_h_; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &val) override;
    PropertyValue property_default(Pid property_id) const override;

    void undo_reorder_elements(const StringList &new_order);
    void reorder_elements(const StringList &new_order);
    StringList diagrams_order() const;

    bool needs_rebuild() const { return needs_rebuild_; }

    void set_needs_rebuild(bool v) { needs_rebuild_ = v; }

private:

    void update_invisible_diagrams(const StringList &current_diagrams);
    size_t compute_insertion_idx(const String &name_of_diagram_before_this);

    double text_scale_ = 0.0;
    double diagram_scale_ = 0.0;
    Spatium column_gap_;
    Spatium row_gap_;
    int chords_per_row_ = 0;

    bool needs_rebuild_ = false;

    AlignH content_alignment_h_ = AlignH::HCENTER;

    StringList diagrams_order_in_score_;
};

//---------------------------------------------------------
//   TBox
//   Text frame.
//---------------------------------------------------------

struct Text;

struct TBox : VBox {
private:
    OBJECT_ALLOCATOR(engraving, TBox)
    DECLARE_CLASSOF(ElementType::TBOX)

public:
    TBox(Score *parent);
    TBox(const TBox &);
    ~TBox() override;

    Text *text() const { return text_; }

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    TBox *clone() const override { return new TBox(*this); }

    EngravingItem *drop(Transaction &tx, EditData &) override;
    void add(EngravingItem *e) override;
    void remove(EngravingItem *el) override;

    PropertyValue property_default(Pid) const override;


private:
    Text *text_ = nullptr;
};
} // namespace mu::engraving
