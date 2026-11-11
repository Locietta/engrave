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

#include "../editing/editdata.h"
#include "../infrastructure/ld_access.h"
#include "../infrastructure/shape.h"
#include "../infrastructure/skyline.h"
#include "../rendering/paintoptions.h"
#include "../types/fraction.h"
#include "../types/symid.h"
#include "../types/types.h"
#include "draw/types/color.h"
#include "draw/types/geometry.h"
#include "engravingobject.h"

namespace muse::draw {
struct Painter;
} // namespace muse::draw

#define DECLARE_LAYOUTDATA_METHODS(Class)                                                                      \
    const LayoutData *ldata() const { return static_cast<const Class::LayoutData *>(EngravingItem::ldata()); } \
    LayoutData *mutldata() { return static_cast<Class::LayoutData *>(EngravingItem::mutldata()); }             \
    LayoutData *create_layout_data() const override { return new Class::LayoutData(); }

namespace mu::engraving::rendering::score {
struct ScoreRenderer;
} // namespace mu::engraving::rendering::score

namespace mu::engraving {
struct EngravingConfiguration;
struct EngravingContextConfiguration;

struct Transaction;

template <typename T>
inline void dump(const ld_field<T> &f, std::stringstream &ss) {
    if (f.has_value()) {
        dump(f.value(), ss);
    } else {
        ss << "no value";
    }
}

struct Factory;
struct XmlReader;

enum class Pid : short;
struct StaffType;

//---------------------------------------------------------
//   ElementFlag
//---------------------------------------------------------

enum class ElementFlag {
    NOTHING = 0x00000000,
    DROP_TARGET = 0x00000001,
    NOT_SELECTABLE = 0x00000002,
    MOVABLE = 0x00000004,
    COMPOSITION = 0x00000008, // true if element is part of another element
    ON_STAFF = 0x00000010,
    SELECTED = 0x00000020,
    GENERATED = 0x00000040,
    INVISIBLE = 0x00000080,
    NO_AUTOPLACE = 0x00000100,
    SYSTEM = 0x00000200,
    PLACE_ABOVE = 0x00000400,
    SIZE_SPATIUM_DEPENDENT = 0x00000800,

    // measure flags
    REPEAT_END = 0x00001000,
    REPEAT_START = 0x00002000,
    REPEAT_JUMP = 0x00004000,
    EXCLUDE_FROM_NUMBERING = 0x00008000,
    LINE_BREAK = 0x00010000,
    PAGE_BREAK = 0x00020000,
    SECTION_BREAK = 0x00040000,
    NO_BREAK = 0x00080000,
    HEADER = 0x00100000,
    TRAILER = 0x00200000, // also used in segment
    COURTESY_KEYSIG = 0x00400000,
    COURTESY_TIMESIG = 0x00800000,
    COURTESY_CLEF = 0x01000000,

    // segment flags
    ENABLED = 0x02000000, // used for segments
    EMPTY = 0x04000000,
    WRITTEN = 0x08000000,
    END_OF_MEASURE_CHANGE = 0x10000000,
};

typedef muse::Flags<ElementFlag> ElementFlags;
DECLARE_OPERATORS_FOR_FLAGS(ElementFlags)

enum class KerningType : unsigned char {
    KERNING,
    NON_KERNING,
    KERN_UNTIL_LEFT_EDGE,
    KERN_UNTIL_CENTER,
    KERN_UNTIL_RIGHT_EDGE,
    SAME_VOICE_LIMIT,
    ALLOW_COLLISION,
};

// The default stacking order of an element type. Saved scores store z only when it
// differs from this default, so the value is pinned to the ElementType ordinals of
// ea2b00ee48 (times 100) and must not move when enumerators are added or deleted.
int default_z(ElementType type);

//-------------------------------------------------------------------
//    @@ EngravingItem
///     \brief Base class of score layout elements
///
///     The EngravingItem class is the virtual base class of all
///     score layout elements.
//-------------------------------------------------------------------

struct EngravingItem : EngravingObject {
private:
    M_PROPERTY2(bool, is_position_linked_to_master, set_position_linked_to_master, true)
    M_PROPERTY2(bool, is_appearance_linked_to_master, set_appearance_linked_to_master, true)
    M_PROPERTY2(bool, exclude_from_other_parts, set_exclude_from_other_parts, false)

public:

    virtual ~EngravingItem();

    EngravingItem &operator=(const EngravingItem &) = delete;

    virtual EngravingItem *clone() const = 0;
    virtual EngravingItem *linked_clone();

    void delete_later();

    //! Item-typed variant of ownershipParent(); additionally null when the
    //! owner is not an item.
    EngravingItem *ownership_parent_item() const;

    //! The parent in the visual hierarchy, which positions are relative to.
    //! Defaults to the owner; overridden where placement differs from ownership
    //! (e.g. measures are placed on systems, but not owned by them).
    virtual EngravingItem *layout_parent() const;

    EngravingConfiguration *configuration() const;
    EngravingContextConfiguration *context_configuration() const;
    rendering::score::ScoreRenderer *renderer() const;

    EngravingItem *find_ancestor(ElementType t);
    const EngravingItem *find_ancestor(ElementType t) const;

    Measure *find_measure();
    const Measure *find_measure() const;
    MeasureBase *find_measure_base();
    const MeasureBase *find_measure_base() const;

    //!Note muse::Returns basic representative for the current element.
    //!     For example: notes->chord, chords->beam, etc.
    virtual EngravingItem *element_base() const { return const_cast<EngravingItem *>(this); }

    virtual bool is_engraving_item() const override { return true; }

    double spatium() const;
    double default_spatium() const;

    inline void set_flag(ElementFlag f, bool v) {
        if (v) {
            flags_ |= f;
        } else {
            flags_ &= ~ElementFlags(f);
        }
    }

    inline void set_flag(ElementFlag f, bool v) const {
        if (v) {
            flags_ |= f;
        } else {
            flags_ &= ~ElementFlags(f);
        }
    }

    inline bool flag(ElementFlag f) const { return flags_ & f; }

    bool selected() const;
    virtual void set_selected(bool f);

    bool visible() const { return !flag(ElementFlag::INVISIBLE); }

    virtual void set_visible(bool f);

    virtual bool is_interaction_available() const;

    bool size_is_spatium_dependent() const override { return !flag(ElementFlag::SIZE_SPATIUM_DEPENDENT); }

    void set_size_is_spatium_dependent(bool v) { set_flag(ElementFlag::SIZE_SPATIUM_DEPENDENT, !v); }

    bool offset_is_spatium_dependent() const override;

    virtual PointF default_pos() const;
    virtual Sid default_pos_sid() const;

    PlacementV placement() const;

    void set_placement(PlacementV val) { set_flag(ElementFlag::PLACE_ABOVE, !bool(val)); }

    bool place_above() const { return placement() == PlacementV::ABOVE; }

    bool place_below() const { return placement() == PlacementV::BELOW; }

    virtual bool place_multiple() const { return true; }

    bool generated() const { return flag(ElementFlag::GENERATED); }

    void set_generated(bool val) { set_flag(ElementFlag::GENERATED, val); }

    Spatium min_distance() const { return min_distance_; }

    void set_min_distance(Spatium v) { min_distance_ = v; }

    PointF system_pos() const;
    virtual PointF page_pos() const;   ///< position in page coordinates
    virtual PointF canvas_pos() const; ///< position in canvas coordinates
    double page_x() const;
    double canvas_x() const;

    PointF map_from_canvas(const PointF &p) const { return p - canvas_pos(); }

    PointF map_to_canvas(const PointF &p) const { return p + canvas_pos(); }

    const PointF &offset() const { return offset_; }

    virtual void set_offset(const PointF &o) { offset_ = o; }

    void set_offset(double x, double y) { offset_.set_x(x), offset_.set_y(y); }

    PointF &roffset() { return offset_; }

    real_t &rxoffset() { return offset_.rx(); }

    real_t &ryoffset() { return offset_.ry(); }

    virtual Fraction tick() const;
    virtual Fraction rtick() const;

    Fraction beat() const;

    bool is_nudged() const { return !offset_.is_null(); }

    bool contains(const PointF &p) const;
    bool intersects(const RectF &r) const;

    virtual int subtype() const { return -1; } // for select gui

    track_idx_t track() const;
    virtual void set_track(track_idx_t val);

    int z() const;
    virtual void set_z(int val);

    staff_idx_t staff_idx() const;
    void set_staff_idx(staff_idx_t val);
    virtual staff_idx_t effective_staff_idx() const; // for system objects migrating
    bool is_top_system_object() const;
    virtual staff_idx_t v_staff_idx() const;
    voice_idx_t voice() const;
    void set_voice(voice_idx_t v);
    Staff *staff() const;
    bool has_staff() const;
    const StaffType *staff_type() const;
    bool on_tab_staff() const;
    Part *part() const;
    Part *origin_part() const; // Delegates to an origin part for items in shared parts

    virtual void add(EngravingItem *);
    virtual void remove(EngravingItem *);

    virtual void added() {}

    virtual void removed() {}

    virtual void change(EngravingItem *o, EngravingItem *n);

    virtual void spatium_changed(double /*oldValue*/, double /*newValue*/);
    virtual void local_spatium_changed(double /*oldValue*/, double /*newValue*/);

    // debug functions
    virtual void dump() const;
    virtual TranslatableString subtype_user_name() const;
    virtual String translated_subtype_user_name() const;

    virtual void set_color(const Color &c);
    virtual Color color() const;
    virtual Color cur_color(const rendering::PaintOptions &opt) const;
    Color cur_color(bool is_visible, const rendering::PaintOptions &opt) const;
    Color cur_color(bool is_visible, Color normal_color, const rendering::PaintOptions &opt) const;

    void undo_set_visible(bool v);
    void undo_add_element(EngravingItem *element, bool add_to_linked_staves = true);

    mutable bool item_discovered = false; // helper flag for bsp

    void scan_elements(std::function<void(EngravingItem *)> func) override;
    virtual bool collect_for_drawing() const;

    virtual void reset() override; // reset all properties & position to default

    double mag_s() const;

    bool is_printable() const;
    virtual bool is_playable() const;

    virtual double absolute_from_spatium(const Spatium &sp) const { return sp.val() * spatium(); }

    bool system_flag() const { return flag(ElementFlag::SYSTEM); }

    void set_system_flag(bool v) const { set_flag(ElementFlag::SYSTEM, v); }

    virtual bool is_system_object_below_bottom_staff() const;

    bool header() const { return flag(ElementFlag::HEADER); }

    void set_header(bool v) { set_flag(ElementFlag::HEADER, v); }

    bool trailer() const { return flag(ElementFlag::TRAILER); }

    void set_trailer(bool val) { set_flag(ElementFlag::TRAILER, val); }

    bool selectable() const { return !flag(ElementFlag::NOT_SELECTABLE); }

    void set_selectable(bool val) { set_flag(ElementFlag::NOT_SELECTABLE, !val); }

    bool drop_target() const { return flag(ElementFlag::DROP_TARGET); }

    void set_drop_target(bool v) const { set_flag(ElementFlag::DROP_TARGET, v); }

    bool composition() const { return flag(ElementFlag::COMPOSITION); }

    void set_composition(bool v) const { set_flag(ElementFlag::COMPOSITION, v); }

    virtual bool is_movable() const { return flag(ElementFlag::MOVABLE); }

    bool enabled() const { return flag(ElementFlag::ENABLED); }

    void set_enabled(bool val) { set_flag(ElementFlag::ENABLED, val); }

    bool autoplace() const;

    virtual void set_autoplace(bool v) { set_flag(ElementFlag::NO_AUTOPLACE, !v); }

    bool add_to_skyline() const { return !(flags_ & (ElementFlag::INVISIBLE | ElementFlag::NO_AUTOPLACE)) && !ldata()->is_skip_draw(); }

    bool exclude_vertical_align() const { return exclude_vertical_align_; }

    void set_exclude_vertical_align(bool v) { exclude_vertical_align_ = v; }

    PropertyValue get_property(Pid) const override;
    bool set_property(Pid, const PropertyValue &) override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;
    using EngravingObject::undo_change_property;
    PropertyValue property_default(Pid) const override;

    bool custom(Pid) const;
    virtual bool is_user_modified() const;

    void draw_symbol(SymId id, muse::draw::Painter *p, const PointF &o = PointF(), double scale = 1.0) const;
    void draw_symbols(const SymIdList &, muse::draw::Painter *p, const PointF &o = PointF(), double scale = 1.0) const;
    void draw_symbols(const SymIdList &, muse::draw::Painter *p, const PointF &o, const SizeF &scale) const;
    double sym_height(SymId id) const;
    double sym_width(SymId id) const;
    double sym_width(const SymIdList &) const;
    RectF sym_bbox(SymId id) const;
    RectF sym_bbox(const SymIdList &) const;
    virtual Shape sym_shape_with_cutouts(SymId id) const;

    PointF sym_smufl_anchor(SymId sym_id, SmuflAnchorId anchor_id) const;

    double sym_advance(SymId id) const;
    bool sym_is_valid(SymId id) const;

    bool concert_pitch() const;
    virtual EngravingItem *next_element();         // selects the next score element, (notes, rests etc. as well as articulation etc.)
    virtual EngravingItem *prev_element();         // selects the next score element, (notes, rests etc. as well as articulation etc.)
    virtual EngravingItem *next_segment_element(); //< Used for navigation
    virtual EngravingItem *prev_segment_element(); //< next-element and prev-element command

    virtual String format_bars_and_beats() const;

    virtual void trigger_layout() const;
    virtual void trigger_layout_all() const;
    virtual void trigger_layout_to_end() const;

    double style_p(Sid idx) const;

    virtual void set_parentheses_mode(const ParenthesesMode &v, bool add_to_linked = true, bool generated = false);
    ParenthesesMode parentheses_mode() const;

    inline Parenthesis *paren(const DirectionH &dir) const { return dir == DirectionH::LEFT ? left_parenthesis_ : right_parenthesis_; }

    Parenthesis *left_paren() const { return left_parenthesis_; }

    Parenthesis *right_paren() const { return right_parenthesis_; }

    void set_left_paren(Parenthesis *paren) { left_parenthesis_ = paren; }

    void set_right_paren(Parenthesis *paren) { right_parenthesis_ = paren; }

    struct BarBeat {
        int bar = 0;
        int displayed_bar = 0;
        double beat = 0.0;
    };

    BarBeat barbeat() const;

    virtual EngravingItem *find_linked_in_score(const Score *score) const;
    EngravingItem *find_linked_in_staff(const Staff *staff) const;

    struct LayoutData {
        virtual ~LayoutData() = default;

        virtual void reset() {
            shape_.reset();
            mask_.reset();
            //! NOTE Temporary removed, have problems, need investigation
            //pos_.reset();
        }

        virtual bool is_valid() const { return shape_.has_value() && shape_.value().bbox().is_valid(); }

        bool is_skip_draw() const { return is_skip_draw_; }

        void set_is_skip_draw(bool val) { is_skip_draw_ = val; }

        double mag() const { return mag_; }

        void set_mag(double val) { mag_ = val; }

        bool is_set_pos() const { return pos_.has_value(); }

        const PointF &pos(LdAccess mode = LdAccess::CHECK) const {
            //! NOTE Temporarily to mute a lot of messages
            mode = (LdAccess::CHECK == mode) ? LdAccess::MAYBE_NOTINITED : mode;
            return pos_.value(mode);
        }

        void set_pos(const PointF &p) { do_set_pos(p.x(), p.y()); }

        void set_pos(double x, double y) { do_set_pos(x, y); }

        void set_pos_x(double x) { do_set_pos(x, pos(LdAccess::MAYBE_NOTINITED).y()); }

        void set_pos_y(double y) { do_set_pos(pos(LdAccess::MAYBE_NOTINITED).x(), y); }

        void move(const PointF &p) { do_set_pos(pos(LdAccess::MAYBE_NOTINITED).x() + p.x(), pos(LdAccess::MAYBE_NOTINITED).y() + p.y()); }

        void move_x(double x) { do_set_pos(pos(LdAccess::MAYBE_NOTINITED).x() + x, pos(LdAccess::MAYBE_NOTINITED).y()); }

        void move_y(double y) { do_set_pos(pos(LdAccess::MAYBE_NOTINITED).x(), pos(LdAccess::MAYBE_NOTINITED).y() + y); }

        bool is_set_bbox() const { return shape_.has_value(); }

        void clear_bbox() { shape_.reset(); }

        const RectF &bbox(LdAccess mode = LdAccess::CHECK) const;

        bool is_set_shape() const { return shape_.has_value(); }

        void clear_shape() { shape_.reset(); }

        Shape shape(LdAccess mode = LdAccess::CHECK) const;

        void set_shape(const Shape &sh) { shape_.set_value(sh); }

        void set_bbox(const RectF &r);

        void set_bbox(double x, double y, double w, double h) { set_bbox(RectF(x, y, w, h)); }

        void add_bbox(const RectF &r) {
            DO_ASSERT(!std::isnan(r.x()) && !std::isinf(r.x()));
            DO_ASSERT(!std::isnan(r.y()) && !std::isinf(r.y()));
            DO_ASSERT(!std::isnan(r.width()) && !std::isinf(r.width()));
            DO_ASSERT(!std::isnan(r.height()) && !std::isinf(r.height()));

            //DO_ASSERT(!isShapeComposite());
            mut_shape().add_b_box(r);
        }

        void set_height(double v) {
            RectF r = bbox();
            r.set_height(v);
            set_bbox(r);
        }

        void set_width(double v) {
#ifndef NDEBUG
            set_width_debug_hook(v);
#endif
            RectF r = bbox();
            r.set_width(v);
            set_bbox(r);
        }

        void set_mask(const Shape &m) { mask_.set_value(m); }

        const Shape &mask() const { return mask_.value(); }

        void connect_item_snapped_before(EngravingItem *item_before);
        void disconnect_item_snapped_before();
        void connect_item_snapped_after(EngravingItem *item_after);
        void disconnect_item_snapped_after();

        void disconnect_snapped_items() {
            disconnect_item_snapped_before();
            disconnect_item_snapped_after();
        }

        EngravingItem *item_snapped_before() const { return item_snapped_before_; }

        EngravingItem *item_snapped_after() const { return item_snapped_after_; }

        void scan_connected_items_in_snapping_chain(const System *system, const std::function<void(EngravingItem *)> &func) const;

        void dump(std::stringstream &ss) const;

    protected:

        virtual void sup_dump([[maybe_unused]] std::stringstream &ss) const {}

#ifndef NDEBUG
        void do_set_pos_debug_hook(double x, double y);
        void set_width_debug_hook(double w);
#endif

        inline void do_set_pos(double x, double y) {
#ifndef NDEBUG
            do_set_pos_debug_hook(x, y);
#endif
            pos_.mut_value().set_x(x), pos_.mut_value().set_y(y);
        }

        Shape &mut_shape() { return shape_.mut_value(); }

        bool is_shape_composite() const { return shape_.has_value() && shape_.value().is_composite(); }

        friend struct EngravingItem;

        const EngravingItem *item_ = nullptr;
        bool is_skip_draw_ = false;
        double mag_ = 1.0;             // standard magnification (derived value)
        ld_field<PointF> pos_ = "pos"; // Reference position, relative to _parent, set by autoplace
        ld_field<Shape> shape_ = "shape";
        ld_field<Shape> mask_ = "mask";

        EngravingItem *item_snapped_before_ = nullptr;
        EngravingItem *item_snapped_after_ = nullptr;

        // STAVE SHARING
        EngravingItem *shared_item_ = nullptr;
        std::vector<EngravingItem *> origin_items_;
    };

    const LayoutData *ldata() const;
    LayoutData *mutldata();

    virtual double mag() const;

    Shape shape(LdAccess mode = LdAccess::CHECK) const { return ldata()->shape(mode); }

    virtual double base_line() const { return -height(); }

    RectF page_bounding_rect(LdAccess mode = LdAccess::CHECK) const { return ldata()->bbox(mode).translated(page_pos()); }

    RectF canvas_bounding_rect(LdAccess mode = LdAccess::CHECK) const { return ldata()->bbox(mode).translated(canvas_pos()); }

    virtual PointF staff_offset() const;

    double staff_offset_y() const { return staff_offset().y(); }

    virtual bool is_property_linked_to_master(Pid id) const;
    virtual bool is_unlinked_from_master() const;
    void unlink_property_from_master(Pid id);
    void relink_properties_to_master(PropertyGroup prop_group);
    void relink_property_to_master(Pid property_id);
    PropertyPropagation property_propagation(const EngravingItem *destination_item, Pid property_id) const;

    virtual bool can_be_excluded_from_other_parts() const { return false; }

    virtual void manage_exclusion_from_parts(bool exclude);

    EngravingItem *shared_item() const;
    const std::vector<EngravingItem *> &origin_items() const;
    static void connect_shared_item(EngravingItem *shared_item, EngravingItem *origin_item);
    static void disconnect_shared_item(EngravingItem *shared_item, EngravingItem *origin_item);
    static void disconnect_all_origin_items(EngravingItem *shared_item);
    static void disconnect_shared_tree(EngravingItem *item);

    virtual bool is_before(const EngravingItem *item) const;

    /** The staff this item would be centered against if it were centered between staves, or
     * nullptr if there is none. `system` limits the search to the staves visible on that
     * system; if null, the system this item is laid out on is used.
     */
    const Staff *staff_to_center_against(bool above, const System *system = nullptr) const;

    //! --- Old Interface ---
    void setbbox(const RectF &r) { mutldata()->set_bbox(r); }

    double height() const { return ldata()->bbox().height(); }

    void set_height(double v) { mutldata()->set_height(v); }

    double width(LdAccess mode = LdAccess::CHECK) const { return ldata()->bbox(mode).width(); }

    void set_width(double v) { mutldata()->set_width(v); }

    virtual const PointF pos() const { return ldata()->pos() + offset_; }

    virtual double x() const { return ldata()->pos().x() + offset_.x(); }

    virtual double y() const { return ldata()->pos().y() + offset_.y(); }

    virtual void set_pos(double x, double y) { mutldata()->set_pos(x, y); }

    virtual void set_pos(const PointF &p) { mutldata()->set_pos(p.x(), p.y()); }

    virtual void move(const PointF &s) { mutldata()->move(s); }

    virtual bool allow_time_anchor() const { return false; }

    virtual bool has_voice_assignment_properties() const { return false; }

    bool applies_to_all_voices_in_instrument() const;
    void set_initial_track_and_voice_assignment(track_idx_t track, bool cur_voice_only_override);
    void check_voice_assignment_compatible_with_track();
    virtual bool element_applies_to_track(const track_idx_t ref_track) const;
    void set_placement_based_on_voice_assignment(DirectionV styled_direction);
    //! ---------------------

protected:
    EngravingItem(const ElementType &type, EngravingObject *parent = nullptr, ElementFlags = ElementFlag::NOTHING);
    EngravingItem(const EngravingItem &, bool link = false);

    virtual LayoutData *create_layout_data() const;
    virtual const LayoutData *ldata_internal() const;
    virtual LayoutData *mutldata_internal();

    mutable int z_ = 0;
    Color color_; // element color attribute

    track_idx_t track_ = muse::nidx; // staffIdx * VOICES + voice

    static bool element_applies_to_track(const track_idx_t element_track, const track_idx_t ref_track,
                                         const VoiceAssignment voice_assignment, const Part *part);

private:

    friend struct Factory;

    PointF offset_; // offset from reference position, set by autoplace or user

    Spatium min_distance_; // autoplace min distance
    mutable ElementFlags flags_;

    bool exclude_vertical_align_ = false;

    mutable LayoutData *layout_data_ = nullptr;

    Parenthesis *left_parenthesis_ = nullptr;
    Parenthesis *right_parenthesis_ = nullptr;
    void set_has_left_parenthesis(bool v, bool add_to_linked = true, bool generated = false);
    void set_has_right_parenthesis(bool v, bool add_to_linked = true, bool generated = false);
};

using ElementPtr = std::shared_ptr<EngravingItem>;

//---------------------------------------------------------
//   ElementList
//---------------------------------------------------------

struct ElementList : std::vector<EngravingItem *> {
public:
    ElementList() {}

    bool remove(EngravingItem *);
    void replace(EngravingItem *old, EngravingItem *n);
};

extern bool element_less_than(const EngravingItem *const, const EngravingItem *const);
} // namespace mu::engraving
