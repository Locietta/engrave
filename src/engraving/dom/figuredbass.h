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

#ifndef MU_ENGRAVING_FIGUREDBASS_H
#define MU_ENGRAVING_FIGUREDBASS_H

#include "textbase.h"

namespace mu::engraving {
class Factory;
class Segment;

/*---------------------------------------------------------
NOTE ON ARCHITECTURE

FiguredBass elements are stored in the annotations of a Segment (like for instance Harmony)

FiguredBass is rather simple: it contains only _ticks, telling the duration of the element,
and a list of FiguredBassItem elements which do most of the job. It also maintains a text with the
normalized (made uniform) version of the text, which is used during editing.

Normally, a FiguredBass element is assumed to be styled with an internally maintained text style
(based on the parameters of the general style "Figured Bass") FIGURED_BASS style and it is set
in this way upon creation and upon layout().
- - - -
FiguredBassItem contains the actually f.b. info; it is made of 4 parts (in this order):
1) prefix: one of [nothing, doubleflat, flat, natural, sharp, doublesharp, cross]
2) digit: one digit from 1 to 9
3) suffix: one of [nothing, doubleflat, flat, natural, sharp, doublesharp, cross, backslash, slash]
4) contLine: true if the item has a continuation line (whose length is determined by parent's _ticks)
and 5 parenthesis flags, one for each position before, between and after the four parts above:
each of them may contain one of [nothing, roundOpen, roundClosed, squaredOpen, squaredClosed].

There is a number of restrictions, implemented at the end of FiguredBassItem::parse().
Currently, no attempt is made to ensure that, if multiple parentheses are present, they are consistent
(matching open and closed parentheses is left to the user).

If an item cannot be parsed, the whole FiguredBass element is kept as entered, possibly un-styled.
If all items can be parsed, each item generates a display text from its properties,
lays it out so that it properly aligns under the chord, draws it at its proper location
and provides its FiguredBass parent with a normalized text for future editing.

FiguredBassItem has not use for formats (italics, bold, ...) and it is never edited directly;
more generally, it is never accessed directly, only via its FiguredBass parent;
so it is directly derived from EngravingItem and returns INVALID as type.

FiguredBass might require formatting (discouraged, but might be necessary for very uncommon cases)
and it is edited (via the normalized text); so it is derived from Text.
---------------------------------------------------------*/

constexpr int fbi_digit_none = -1;

//---------------------------------------------------------
//   @@ FiguredBassItem
///   One line of a figured bass indication
//
//   @P continuationLine   enum (FiguredBassItem.NONE, .SIMPLE, .EXTENDED)  whether item has continuation line or not, and of which type
//   @P digit              int                              main digit(s) (0 - 9)
//   @P displayText        string                           text displayed (depends on configured fonts) (read only)
//   @P normalizedText     string                           conventional textual representation of item properties (= text used during input) (read only)
//   @P parenthesis1       enum (FiguredBassItem.NONE, .ROUNDOPEN, .ROUNDCLOSED, .SQUAREDOPEN, .SQUAREDCLOSED)  parenthesis before the prefix
//   @P parenthesis2       enum (FiguredBassItem.NONE, .ROUNDOPEN, .ROUNDCLOSED, .SQUAREDOPEN, .SQUAREDCLOSED)  parenthesis after the prefix / before the digit
//   @P parenthesis3       enum (FiguredBassItem.NONE, .ROUNDOPEN, .ROUNDCLOSED, .SQUAREDOPEN, .SQUAREDCLOSED)  parenthesis after the digit / before the suffix
//   @P parenthesis4       enum (FiguredBassItem.NONE, .ROUNDOPEN, .ROUNDCLOSED, .SQUAREDOPEN, .SQUAREDCLOSED)  parenthesis after the suffix / before the cont. line
//   @P parenthesis5       enum (FiguredBassItem.NONE, .ROUNDOPEN, .ROUNDCLOSED, .SQUAREDOPEN, .SQUAREDCLOSED)  parenthesis after the cont. line
//   @P prefix             enum (FiguredBassItem.NONE, .DOUBLEFLAT, .FLAT, .NATURAL, .SHARP, .DOUBLESHARP, .PLUS, .BACKSLASH, .SLASH)  accidental before the digit
//   @P suffix             enum (FiguredBassItem.NONE, .DOUBLEFLAT, .FLAT, .NATURAL, .SHARP, .DOUBLESHARP, .PLUS, .BACKSLASH, .SLASH)  accidental/diacritic after the digit
//---------------------------------------------------------

class FiguredBass;

class FiguredBassItem final : public EngravingItem {
    OBJECT_ALLOCATOR(engraving, FiguredBassItem)
    DECLARE_CLASSOF(ElementType::FIGURED_BASS_ITEM)

public:
    enum class Modifier : char { NONE = 0, DOUBLEFLAT, FLAT, NATURAL, SHARP, DOUBLESHARP, CROSS, BACKSLASH, SLASH, NUMOF };
    enum class Parenthesis : char { NONE = 0, ROUNDOPEN, ROUNDCLOSED, SQUAREDOPEN, SQUAREDCLOSED, NUMOF };
    enum class ContLine : char {
        NONE = 0,
        SIMPLE,  // cont. line stops at f.b. element end
        EXTENDED // cont. line joins with next element, if possible
    };

    enum class Style : char { MODERN = 0, HISTORIC, NUMOF };
    enum class Combination : char { SIMPLE = 0, CROSSED, BACKSLASHED, SLASHED, NUMOF };

    ~FiguredBassItem();

    FiguredBassItem &operator=(const FiguredBassItem &) = delete;

    // standard re-implemented virtual functions
    FiguredBassItem *clone() const override { return new FiguredBassItem(*this); }

    bool starts_with_parenthesis() const;

    // specific API
    const FiguredBass *figured_bass() const { return (FiguredBass *) (ownership_parent()); }

    bool parse(String &text);

    // getters / setters
    int ord() const { return ord_; }

    Modifier prefix() const { return prefix_; }

    void set_prefix(const Modifier &v) { prefix_ = v; }

    void undo_set_prefix(Modifier pref);

    int digit() const { return digit_; }

    void set_digit(int val) { digit_ = val; }

    void undo_set_digit(int digit);

    Modifier suffix() const { return suffix_; }

    void set_suffix(const Modifier &v) { suffix_ = v; }

    void undo_set_suffix(Modifier suff);

    ContLine cont_line() const { return cont_line_; }

    void set_cont_line(const ContLine &v) { cont_line_ = v; }

    void undo_set_cont_line(ContLine val);

    Parenthesis parenth1() const { return parenth_[0]; }

    Parenthesis parenth2() const { return parenth_[1]; }

    Parenthesis parenth3() const { return parenth_[2]; }

    Parenthesis parenth4() const { return parenth_[3]; }

    Parenthesis parenth5() const { return parenth_[4]; }

    void set_parenth1(Parenthesis v) { parenth_[0] = v; }

    void set_parenth2(Parenthesis v) { parenth_[1] = v; }

    void set_parenth3(Parenthesis v) { parenth_[2] = v; }

    void set_parenth4(Parenthesis v) { parenth_[3] = v; }

    void set_parenth5(Parenthesis v) { parenth_[4] = v; }

    void undo_set_parenth1(Parenthesis par);
    void undo_set_parenth2(Parenthesis par);
    void undo_set_parenth3(Parenthesis par);
    void undo_set_parenth4(Parenthesis par);
    void undo_set_parenth5(Parenthesis par);
    String normalized_text() const;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    struct LayoutData : public EngravingItem::LayoutData {
        String display_text;     // the constructed display text (read-only)
        double text_width = 0.0; // the text width (in raster units)
    };
    DECLARE_LAYOUTDATA_METHODS(FiguredBassItem)

private:

    friend class FiguredBass;

    FiguredBassItem(FiguredBass *parent, int line = 0);
    FiguredBassItem(const FiguredBassItem &);

    // part parsing
    int parse_digit(String &str);
    int parse_parenthesis(String &str, int parenth_idx);
    int parse_prefix_suffix(String &str, bool b_prefix);

    void regenerate_display_text();

    static const Char norm_parenth_to_char[int(Parenthesis::NUMOF)];

    int ord_ = 0;                         // the line ordinal of this element in the FB stack
                                          // the parts making a FiguredBassItem up
    Modifier prefix_ = Modifier::NONE;    // the accidental coming before the body
    int digit_ = 0;                       // the main digit (if present)
    Modifier suffix_ = Modifier::NONE;    // the accidental coming after the body
    ContLine cont_line_ = ContLine::NONE; // whether the item has continuation line or not
    Parenthesis parenth_[5];              // each of the parenthesis: before, between and after parts
};

//---------------------------------------------------------
//   FiguredBassFont
//---------------------------------------------------------

struct FiguredBassFont {
    String family;
    String display_name;
    double def_pitch = 0.0;
    double def_line_height = 0.0;
    Char display_accidental[int(FiguredBassItem::Modifier::NUMOF)];
    Char display_parenthesis[int(FiguredBassItem::Parenthesis::NUMOF)];
    Char display_digit[int(FiguredBassItem::Style::NUMOF)][10][int(FiguredBassItem::Combination::NUMOF)];

    bool read(XmlReader &);
};

//---------------------------------------------------------
//   @@ FiguredBass
///    A complete figured bass indication
//
//   @P onNote  bool  whether it is placed on a note beginning or between notes (read only)
//   @P ticks   int   duration in ticks
//---------------------------------------------------------

class FiguredBass final : public TextBase {
    OBJECT_ALLOCATOR(engraving, FiguredBass)
    DECLARE_CLASSOF(ElementType::FIGURED_BASS)

public:

    static constexpr double fb_contline_height = 0.875;            // the % of font EM to raise the cont. line at
                                                                   // (0 = top of font; 1 = bottom of font)
    static constexpr Spatium fb_contline_left_padding = 0.1875_sp; // (3/16sp) the blank space at the left of a cont. line (in sp)
    static constexpr Spatium fb_contline_overlap = 0.125_sp;       // (1/8sp)  the overlap of an extended cont. line (in sp)
    static constexpr Spatium fb_contline_thickness = 0.09375_sp;   // (3/32sp) the thickness of a cont. line (in sp)

    ~FiguredBass();

    // a convenience static function to create/retrieve a new FiguredBass into/from its intended parent
    static FiguredBass *add_figured_bass_to_segment(Segment *seg, track_idx_t track, const Fraction &ext_ticks, bool *p_new);

    // static functions for font config files
    static bool read_config_file(const String &file_name);
    static std::vector<String> font_names();
    static bool font_data(int n_idx, String *p_family, String *p_display_name, double *p_size, double *p_line_height);

    // standard re-implemented virtual functions
    FiguredBass *clone() const override { return new FiguredBass(*this); }

    FiguredBassItem *create_item(int line) { return new FiguredBassItem(this, line); }

    void set_selected(bool f) override;
    void set_visible(bool f) override;
    void regenerate_text();

    bool on_note() const { return on_note_; }

    void set_on_note(bool val) { on_note_ = val; }

    Segment *segment() const { return (Segment *) (ownership_parent()); }

    const Fraction &ticks() const { return ticks_; }

    void set_ticks(const Fraction &v) { ticks_ = v; }

    double additional_cont_line_x(double page_pos_y) const; // returns the X coord (in page coord) of cont. line at pagePosY, if any
    FiguredBass *next_figured_bass() const;                 // returns next *adjacent* f.b. item, if any

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid) const override;

    bool position_relative_to_notehead_rest() const override { return true; }

    size_t items_count() const { return items_.size(); }

    void append_item(FiguredBassItem *item) { items_.push_back(item); }

    const std::vector<FiguredBassItem *> &items() const { return items_; }

    void clear_items();
    void add_item_to_linked(FiguredBassItem *item);

    // the array of configured fonts
    static const std::vector<FiguredBassFont> &fb_fonts();

    bool parentheses_mode() const; // read / write MusicXML support

    struct LayoutData : public TextBase::LayoutData {
        std::vector<double> line_lengths; // lengths of duration indicator lines (in raster units)
        double printed_line_length = 0.0; // the length of lines actually printed (i.e. continuation lines)

        double line_length(size_t idx) const {
            if (idx < line_lengths.size()) {
                return line_lengths.at(idx);
            }
            return 0.0;
        }
    };
    DECLARE_LAYOUTDATA_METHODS(FiguredBass)

private:

    friend class Factory;
    FiguredBass(DummyParentOr<Segment> parent);
    FiguredBass(const FiguredBass &);

    Sid get_property_style(Pid) const override;

    std::vector<FiguredBassItem *> items_; // the individual lines of the F.B.

    bool on_note_ = true; // true if this element is on a staff note | false if it is between notes
    Fraction ticks_;      // the duration (used for cont. lines and for multiple F.B.
                          // under the same note)
};
} // namespace mu::engraving

#endif
