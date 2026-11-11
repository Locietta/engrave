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

#include <variant>

#include "../iengravingfontsprovider.h"
#include "../types/types.h"
#include "draw/fontmetrics.h"
#include "engravingitem.h"
#include "property.h"

namespace mu::engraving {
struct TextBase;
struct TextBlock;
struct Transaction;

static constexpr double subscript_size = 0.6;
static constexpr double subscript_offset = 0.5;    // of x-height
static constexpr double superscript_offset = -0.9; // of x-height

//---------------------------------------------------------
//   FrameType
//---------------------------------------------------------

enum class FrameType : char { NO_FRAME, RECTANGLE, CIRCLE };

//---------------------------------------------------------
//   VerticalAlignment
//---------------------------------------------------------

enum class VerticalAlignment : signed char { ALIGN_UNDEFINED = -1, ALIGN_NORMAL, ALIGN_SUPER_SCRIPT, ALIGN_SUB_SCRIPT };

//---------------------------------------------------------
//   FormatId
//---------------------------------------------------------

enum class FormatId : char { BOLD, ITALIC, UNDERLINE, STRIKE, VALIGN, FONT_SIZE, FONT_FAMILY };

using FormatValue = std::variant<std::monostate, bool, int, double, String>;

//---------------------------------------------------------
//   MultiClick
//---------------------------------------------------------

enum class SelectTextType : char { WORD, ALL };

//---------------------------------------------------------
//   CharFormat
//---------------------------------------------------------

struct CharFormat {
    CharFormat() {}

    CharFormat(const CharFormat &cf) { *this = cf; }

    bool operator==(const CharFormat &cf) const;

    bool operator!=(const CharFormat &cf) const { return !(*this == cf); }

    CharFormat &operator=(const CharFormat &cf);

    FontStyle style() const { return style_; }

    void set_style(FontStyle s) { style_ = s; }

    bool bold() const { return style_ & FontStyle::BOLD; }

    bool italic() const { return style_ & FontStyle::ITALIC; }

    bool underline() const { return style_ & FontStyle::UNDERLINE; }

    bool strike() const { return style_ & FontStyle::STRIKE; }

    void set_bold(bool val) { style_ = val ? style_ + FontStyle::BOLD : style_ - FontStyle::BOLD; }

    void set_italic(bool val) { style_ = val ? style_ + FontStyle::ITALIC : style_ - FontStyle::ITALIC; }

    void set_underline(bool val) { style_ = val ? style_ + FontStyle::UNDERLINE : style_ - FontStyle::UNDERLINE; }

    void set_strike(bool val) { style_ = val ? style_ + FontStyle::STRIKE : style_ - FontStyle::STRIKE; }

    VerticalAlignment valign() const { return valign_; }

    double font_size() const { return font_size_; }

    const String &font_family() const { return font_family_; }

    void set_valign(VerticalAlignment val) { valign_ = val; }

    void set_font_size(double val) { font_size_ = val; }

    void set_font_family(const String &val) { font_family_ = val; }

    FormatValue format_value(FormatId) const;
    void set_format_value(FormatId, const FormatValue &val);

private:

    FontStyle style_ = FontStyle::NORMAL;
    VerticalAlignment valign_ = VerticalAlignment::ALIGN_NORMAL;
    double font_size_ = 12.0;
    String font_family_;
};

//---------------------------------------------------------
//   TextCursor
//    Contains current position and start of selection
//    during editing.
//---------------------------------------------------------

struct TextCursor {
    enum class MoveOperation : unsigned char { START, UP, START_OF_LINE, LEFT, WORD_LEFT, END, DOWN, END_OF_LINE, NEXT_WORD, RIGHT };

    enum class MoveMode : unsigned char { MOVE_ANCHOR, KEEP_ANCHOR };

    TextCursor(TextBase *t) : text_(t) {}

    TextBase *text() const { return text_; }

    bool has_selection() const { return (select_line_ != row_) || (select_column_ != column_); }

    void clear_selection();
    void end_edit();
    void start_edit();

    bool editing() const { return editing_; }

    bool visible() const { return visible_; }

    void set_visible(bool val) { visible_ = val; }

    void toggle_visible() { set_visible(!visible_); }

    CharFormat *format() { return &format_; }

    const CharFormat *format() const { return &format_; }

    void set_format(const CharFormat &f) { format_ = f; }

    size_t row() const { return row_; }

    size_t column() const { return column_; }

    size_t select_line() const { return select_line_; }

    size_t select_column() const { return select_column_; }

    void set_row(size_t val) { row_ = val; }

    void set_column(size_t val) { column_ = val; }

    void set_select_line(size_t val) { select_line_ = val; }

    void set_select_column(size_t val) { select_column_ = val; }

    size_t columns() const;
    void init();

    struct Range {
        int start_position = 0;
        int end_position = 0;
        String text;
    };

    std::pair<size_t, size_t> position_to_local_coord(int position) const;

    int current_position() const;
    Range selection_range() const;

    const TextBlock &cur_line() const;
    TextBlock &cur_line();

    RectF cursor_rect() const;
    RectF cursor_canvas_rect() const;
    bool move_position(TextCursor::MoveOperation op, TextCursor::MoveMode mode = TextCursor::MoveMode::MOVE_ANCHOR, int count = 1);
    void select_word();

    void move_cursor_to_end() { move_position(TextCursor::MoveOperation::END); }

    void move_cursor_to_start() { move_position(TextCursor::MoveOperation::START); }

    Char current_character() const;
    bool set(const PointF &p, TextCursor::MoveMode mode = TextCursor::MoveMode::MOVE_ANCHOR);
    String selected_text(bool with_format = false) const;
    String extract_text(int r1, int c1, int r2, int c2, bool with_format = false) const;
    void update_cursor_format();
    void set_format(FormatId, FormatValue val);
    void change_selection_format(FormatId id, const FormatValue &val);
    const CharFormat selected_fragments_format() const;

private:
    Range range(int start, int end) const;
    int position(int row, int column) const;

    TextBase *text_ = nullptr;
    CharFormat format_;
    size_t row_ = 0;
    size_t column_ = 0;
    size_t select_line_ = 0; // start of selection
    size_t select_column_ = 0;
    bool editing_ = false;
    bool visible_ = false;
};

//---------------------------------------------------------
//   TextFragment
//    contains a styled text
//---------------------------------------------------------

struct TextFragment {

public:
    mutable CharFormat format;
    PointF pos; // y is relative to TextBlock->y()
    mutable String text;

    TextFragment() = default;
    TextFragment(const String &s);
    TextFragment(TextCursor *, const String &);
    TextFragment(const TextFragment &f);

    TextFragment &operator=(const TextFragment &f);

    bool operator==(const TextFragment &f) const;

    TextFragment split(int column);
    muse::draw::Font font(const TextBase *) const;
    double calculated_font_size(const TextBase *) const;
    int columns() const;
    void change_format(FormatId id, const FormatValue &data);

private:
    void resolve_fallback(muse::draw::Font::Type font_type, const muse::draw::FontMetrics &fm, String &family) const;
};

//---------------------------------------------------------
//   TextBlock
//    represents a block of formatted text
//---------------------------------------------------------

struct TextBlock {
    TextBlock() = default;

    bool operator==(const TextBlock &x) const { return fragments_ == x.fragments_; }

    bool operator!=(const TextBlock &x) const { return fragments_ != x.fragments_; }

    const std::list<TextFragment> &fragments() const { return fragments_; }

    std::list<TextFragment> &fragments() { return fragments_; }

    std::list<TextFragment> fragments_without_empty();

    const Shape &shape() const { return shape_; }

    Shape &shape() { return shape_; }

    const RectF &bounding_rect() const { return shape_.bbox(); }

    RectF bounding_rect(int col1, int col2, const TextBase *) const;
    size_t columns() const;
    void insert(TextCursor *, const String &);
    void insert_empty_fragment_if_needed(TextCursor *);
    void remove_empty_fragment();
    String remove(int column, TextCursor *);
    String remove(int start, int n, TextCursor *);
    int column(double x, TextBase *) const;
    TextBlock split(int column, TextCursor *cursor);
    double xpos(size_t col, const TextBase *) const;
    const CharFormat *format_at(int) const;
    const TextFragment *fragment(int col) const;
    std::list<TextFragment>::iterator fragment(int column, int *rcol, int *ridx);

    double y() const { return y_; }

    void set_y(double val) { y_ = val; }

    double line_spacing() const { return line_spacing_; }

    void set_line_spacing(double val) { line_spacing_ = val; }

    String text(int, int, const TextBase *, bool = false) const;

    bool eol() const { return eol_; }

    void set_eol(bool val) { eol_ = val; }

    void change_format(FormatId, const FormatValue &val, int start, int n);

private:
    void simplify();

    std::list<TextFragment> fragments_;
    double y_ = 0.0;
    double line_spacing_ = 0.0;
    Shape shape_;
    bool eol_ = false;
};

//---------------------------------------------------------
//   TextBase
//---------------------------------------------------------

struct TextBase : EngravingItem {
private:

    M_PROPERTY2(bool, is_text_linked_to_master, set_text_linked_to_master, true)

public:

    ~TextBase();

    Text &operator=(const Text &) = delete;

    Align align() const { return align_; }

    void set_align(Align a) { align_ = a; }

    AlignH position() const { return position_; }

    void set_position(AlignH val) { position_ = val; }

    static String plain_to_xml_text(const String &s) { return s.to_xml_escaped(); }

    void set_plain_text(const String &t) { set_xml_text(plain_to_xml_text(t)); }

    virtual void set_xml_text(const String &);

    void set_xml_text(const char *str) { set_xml_text(String::from_utf8(str)); }

    void check_custom_formatting(const String &);
    String xml_text() const;
    String plain_text() const;
    void reset_formatting();

    void insert_text(const String &);

    double line_spacing() const;
    double line_height() const;
    virtual double base_line() const override;

    bool empty() const { return xml_text().is_empty(); }

    void clear() { set_xml_text(String()); }

    virtual FontStyle font_style() const;
    virtual String family() const;
    virtual double size() const;

    virtual void set_font_style(const FontStyle &val);
    virtual void set_family(const String &val);
    virtual void set_size(const double &val);

    bool anchor_to_end_of_previous() const { return anchor_to_end_of_previous_; }

    void set_anchor_to_end_of_previous(bool v) { anchor_to_end_of_previous_ = v; }

    bool has_parent_segment() const { return ownership_parent() && parent()->is_segment(); }

    virtual bool allow_time_anchor() const override { return has_parent_segment(); }

    void move_position(TextCursor::MoveOperation);

    bool delete_selected_text();

    void select_all();
    void select(SelectTextType);

    bool is_primed() const { return primed_; }

    void set_primed(bool primed) { primed_ = primed; }

    virtual void paste(const String &txt);

    RectF page_rectangle() const;

    const Shape &high_res_shape() const { return ldata()->high_res_shape.value(); }

    virtual bool accept_drop(EditData &) const override;
    virtual EngravingItem *drop(Transaction &tx, EditData &) override;

    friend struct TextBlock;
    friend struct TextFragment;

    static String un_escape(String s);
    static String escape(String s);

    int subtype() const override;
    TranslatableString subtype_user_name() const override;

    std::list<TextFragment> fragment_list() const; // for MusicXML formatted export

    static bool validate_text(String &s);

    bool in_hex_state() const { return hex_state_ >= 0; }

    void end_hex_state();

    muse::draw::Font font() const;
    muse::draw::FontMetrics font_metrics() const;

    bool is_property_linked_to_master(Pid id) const override;
    bool is_unlinked_from_master() const override;
    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;
    void undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) override;
    Sid get_property_style(Pid) const override;
    void style_changed() override;
    void edit_insert_text(TextCursor *, const String &);

    TextCursor *cursor() const { return cursor_; }

    void set_text_invalid() { text_invalid_ = true; }

    bool is_text_invalid() const { return text_invalid_; }

    // helper functions
    bool has_frame() const { return frame_type_ != FrameType::NO_FRAME; }

    bool circle() const { return frame_type_ == FrameType::CIRCLE; }

    bool rectangle() const { return frame_type_ == FrameType::RECTANGLE; }

    TextStyleType text_style_type() const { return text_style_type_; }

    void set_text_style_type(TextStyleType id) { text_style_type_ = id; }

    void init_text_style_type(TextStyleType id);
    void init_text_style_type(TextStyleType id, bool preserve_different);
    virtual void init_element_style(const ElementStyle *) override;

    static const String undefined_font_family;
    static const double undefined_font_size;

    static bool is_sorted(size_t r1, size_t c1, size_t r2, size_t c2);
    static void sort(size_t &r1, size_t &c1, size_t &r2, size_t &c2);

    bool bold() const { return font_style() & FontStyle::BOLD; }

    bool italic() const { return font_style() & FontStyle::ITALIC; }

    bool underline() const { return font_style() & FontStyle::UNDERLINE; }

    bool strike() const { return font_style() & FontStyle::STRIKE; }

    void set_bold(bool val) { set_font_style(val ? font_style() + FontStyle::BOLD : font_style() - FontStyle::BOLD); }

    void set_italic(bool val) { set_font_style(val ? font_style() + FontStyle::ITALIC : font_style() - FontStyle::ITALIC); }

    void set_underline(bool val) { set_font_style(val ? font_style() + FontStyle::UNDERLINE : font_style() - FontStyle::UNDERLINE); }

    void set_strike(bool val) { set_font_style(val ? font_style() + FontStyle::STRIKE : font_style() - FontStyle::STRIKE); }

    bool has_custom_formatting() const;

    friend struct TextCursor;
    using EngravingObject::undo_change_property;

    Color text_color(const rendering::PaintOptions &opt) const;

    FrameType frame_type() const { return frame_type_; }

    void set_frame_type(FrameType val) { frame_type_ = val; }

    double text_line_spacing() const { return text_line_spacing_; }

    void set_text_line_spacing(double val) { text_line_spacing_ = val; }

    Color bg_color() const { return bg_color_; }

    void set_bg_color(const Color &val) { bg_color_ = val; }

    Color frame_color() const { return frame_color_; }

    void set_frame_color(const Color &val) { frame_color_ = val; }

    Spatium frame_width() const { return frame_width_; }

    void set_frame_width(Spatium val) { frame_width_ = val; }

    Spatium padding_width() const { return padding_width_; }

    void set_padding_width(Spatium val) { padding_width_ = val; }

    Spatium frame_round() const { return frame_round_; }

    void set_frame_round(Spatium val) { frame_round_ = val; }

    struct LayoutData : public EngravingItem::LayoutData {
        std::vector<TextBlock> blocks;
        bool layout_invalid = true;

        RectF frame;

        size_t rows() const { return blocks.size(); }

        const TextBlock &text_block(size_t i) const { return blocks.at(i); }

        TextBlock &text_block(size_t i) { return blocks[i]; }

        ld_field<Shape> high_res_shape = {"[TextBase] highResShape", Shape()};
    };
    DECLARE_LAYOUTDATA_METHODS(TextBase)

    void create_blocks();
    void create_blocks(LayoutData *ldata) const;
    void layout_frame();
    void layout_frame(LayoutData *ldata) const;

    //! NOTE It can only be set for some types of text, see who has the setter.
    //! At the moment it's: Text, Jump, Marker
    bool layout_to_parent_width() const { return layout_to_parent_width_; }

    virtual bool position_relative_to_notehead_rest() const = 0;

    void set_voice_assignment(VoiceAssignment v) { voice_assignment_ = v; }

    VoiceAssignment voice_assignment() const { return voice_assignment_; }

    void set_direction(DirectionV v) { direction_ = v; }

    DirectionV direction() const { return direction_; }

    void set_center_between_staves(AutoOnOff v) { center_between_staves_ = v; }

    AutoOnOff center_between_staves() const { return center_between_staves_; }

    void gen_text();

    double symbol_size() const { return symbol_size_; }

    void set_symbol_size(double v) { symbol_size_ = v; }

    double symbol_scale() const { return symbol_scale_; }

    void set_symbol_scale(double v) { symbol_scale_ = v; }

    bool has_symbol_scale() const;

    bool has_symbol_size() const { return !has_symbol_scale(); }

    double spatium_scaling() const;

    double text_angle() const { return text_angle_; }

    void set_text_angle(double v) { text_angle_ = v; }

    PointF default_pos() const override;

protected:
    TextBase(const ElementType &type, EngravingObject *parent = 0, TextStyleType tid = TextStyleType::DEFAULT,
             ElementFlags = ElementFlag::NOTHING);
    TextBase(const ElementType &type, EngravingObject *parent, ElementFlags);
    TextBase(const TextBase &);

    void insert_sym(SymId id);
    void prepare_format(const String &token, TextCursor &cursor);
    bool prepare_format(const String &token, CharFormat &format);

    virtual void commit_text();

    bool layout_to_parent_width_ = false;

private:
    void insert(TextCursor *, char32_t code, LayoutData *ldata) const;
    String gen_text(const LayoutData *ldata) const;

    virtual int get_property_flags_idx(Pid id) const override;
    String strip_text(bool, bool, bool) const;

    static String get_html_start_tag(double, double &, const String &, String &, FontStyle, VerticalAlignment);
    static String get_html_end_tag(FontStyle, VerticalAlignment);

    static void swap(size_t &r1, size_t &c1, size_t &r2, size_t &c2);

    Sid default_pos_sid() const override;

    void notify_about_text_cursor_changed();
    void notify_about_text_inserted(int start_position, int end_position, const String &text);
    void notify_about_text_removed(int start_position, int end_position, const String &text);

    Align align_;

    AlignH position_ = AlignH::LEFT;

    FrameType frame_type_ = FrameType::NO_FRAME;
    double text_line_spacing_ = 1.0;
    Color bg_color_;
    Color frame_color_;
    Spatium frame_width_;
    Spatium padding_width_;
    Spatium frame_round_;

    // there are two representations of text; only one
    // might be valid and the other can be constructed from it

    String text_; // cached
    bool text_invalid_ = true;

    TextStyleType text_style_type_ = TextStyleType::DEFAULT; // text style id

    int hex_state_ = -1;
    bool primed_ = 0;

    double symbol_size_ = 18.0;
    double symbol_scale_ = 1.0;

    TextCursor *cursor_ = nullptr;

    VoiceAssignment voice_assignment_ = VoiceAssignment::ALL_VOICE_IN_INSTRUMENT;
    DirectionV direction_ = DirectionV::AUTO;
    AutoOnOff center_between_staves_ = AutoOnOff::AUTO;
    bool anchor_to_end_of_previous_ = false;

    double text_angle_ = 0.0;
};

} // namespace mu::engraving
