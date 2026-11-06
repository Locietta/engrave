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

#include "../types/types.h"
#include "line.h"
#include "textbase.h"

namespace mu::engraving {
struct Transaction;

//---------------------------------------------------------
//   Lyrics
//---------------------------------------------------------

struct LyricsLine;

struct Lyrics final : TextBase {
private:
    OBJECT_ALLOCATOR(engraving, Lyrics)
    DECLARE_CLASSOF(ElementType::LYRICS)

public:

    // MELISMA FIRST UNDERSCORE:
    // used as_ticks value to mark a melisma for which only the first chord has been spanned so far
    // and to give the user a visible feedback that the underscore has been actually entered;
    // it should be cleared to 0 at some point, so that it will not be carried over
    // if the melisma is not extended beyond a single chord, but no suitable place to do this
    // has been identified yet.
    static constexpr Fraction temp_melisma_ticks =
        Fraction::eps(); // THIS WAS A HORRIBLE HACK. At some point we must remove it and replace it with a proper solution. (M.S.)

    ~Lyrics();

    Lyrics *clone() const override { return new Lyrics(*this); }

    bool accept_drop(EditData &) const override;
    EngravingItem *drop(Transaction &tx, EditData &) override;

    Segment *segment() const { return to_segment(ownership_parent()->ownership_parent()); }

    Measure *measure() const { return to_measure(ownership_parent()->ownership_parent()->ownership_parent()); }

    ChordRest *chord_rest() const { return to_chord_rest(ownership_parent()); }

    int subtype() const override { return verse_; }

    TranslatableString subtype_user_name() const override;

    void set_verse(int n) { verse_ = n; }

    int verse() const { return verse_; }

    bool is_even() const { return verse_ % 2; }

    void set_syllabic(LyricsSyllabic s) { syllabic_ = s; }

    LyricsSyllabic syllabic() const { return syllabic_; }

    void add(EngravingItem *) override;
    void remove(EngravingItem *) override;

    bool position_relative_to_notehead_rest() const override { return true; }

    const Fraction &ticks() const { return ticks_; }

    void set_ticks(const Fraction &tick) { ticks_ = tick; }

    Fraction end_tick() const;
    void remove_from_score();

    void adjust_previous();

    bool need_remove_invalid_segments() const { return need_remove_invalid_segments_; }

    void set_need_remove_invalid_segments();
    void remove_invalid_segments();

    LyricsLine *separator() const { return separator_; }

    void set_separator(LyricsLine *s) { separator_ = s; }

    bool is_melisma() const;

    bool allow_time_anchor() const override { return false; }

    using EngravingObject::undo_change_property;
    void paste(const String &txt) override;

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid id) const override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;
    void reset() override;
    void trigger_layout() const override;

    double y_relative_to_staff() const;
    void set_y_relative_to_staff(double y);

    bool avoid_barlines() const { return avoid_barlines_; }

    void set_avoid_barlines(bool v) { avoid_barlines_ = v; }

private:

    friend struct Factory;
    Lyrics(DummyParentOr<ChordRest> parent);
    Lyrics(const Lyrics &);

    int verse_ = 0;  // row index
    Fraction ticks_; // if > 0 then draw an underline to tick() + _ticks (melisma)
    LyricsSyllabic syllabic_ = LyricsSyllabic::SINGLE;
    LyricsLine *separator_ = nullptr;
    bool need_remove_invalid_segments_ = false;
    bool avoid_barlines_ = true;
};

//---------------------------------------------------------
//   LyricsLine
///   \cond PLUGIN_API \private \endcond
//---------------------------------------------------------

struct LyricsLine : SLine {
private:
    OBJECT_ALLOCATOR(engraving, LyricsLine)
    DECLARE_CLASSOF(ElementType::LYRICSLINE)

public:
    LyricsLine(DummyParentOr<EngravingItem> parent);
    LyricsLine(const LyricsLine &);

    LyricsLine *clone() const override { return new LyricsLine(*this); }

    Anchor anchor() const override { return Anchor::SEGMENT; }

    LineSegment *create_line_segment() override;
    void remove_unmanaged() override;

    virtual Lyrics *lyrics() const { return to_lyrics(ownership_parent()); }

    Lyrics *next_lyrics() const { return next_lyrics_; }

    void set_next_lyrics(Lyrics *l) { next_lyrics_ = l; }

    virtual bool is_end_melisma() const { return lyrics() && lyrics()->ticks().is_not_zero(); }

    bool is_dash() const { return !is_end_melisma(); }

    bool set_property(Pid property_id, const PropertyValue &v) override;
    PropertyValue property_default(Pid id) const override;
    Sid get_property_style(Pid) const override;

protected:
    LyricsLine(const ElementType &type, DummyParentOr<EngravingItem> parent, ElementFlags = ElementFlag::NOTHING);

    bool is_in_spanner_map() const override { return false; }

    Lyrics *next_lyrics_ = nullptr;

    void do_compute_end_element() override;
};

//---------------------------------------------------------
//   LyricsLineSegment
///   \cond PLUGIN_API \private \endcond
//---------------------------------------------------------

struct LyricsLineSegment : LineSegment {
private:
    OBJECT_ALLOCATOR(engraving, LyricsLineSegment)
    DECLARE_CLASSOF(ElementType::LYRICSLINE_SEGMENT)

public:
    LyricsLineSegment(LyricsLine *);

    LyricsLineSegment *clone() const override { return new LyricsLineSegment(*this); }

    LyricsLine *lyrics_line() const { return to_lyrics_line(spanner()); }

    virtual Lyrics *lyrics() const { return lyrics_line()->lyrics(); }

    virtual double base_line_shift() const;

    virtual int verse() const { return lyrics()->verse(); }

    virtual bool lyrics_place_above() const { return lyrics()->place_above(); }

    virtual bool lyrics_add_to_skyline() const { return lyrics()->add_to_skyline(); }

    virtual double line_spacing() const { return lyrics()->line_spacing(); }

    Color color() const override { return lyrics()->color(); }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;
    EngravingObject *property_delegate(Pid property_id) const override;

    bool allow_time_anchor() const override { return false; }

    struct LayoutData : public LineSegment::LayoutData {
    public:
        const std::vector<LineF> &dashes() const { return dashes_; }

        void clear_dashes() { dashes_.clear(); }

        void add_dash(const LineF &dash) { dashes_.push_back(dash); }
    private:
        std::vector<LineF> dashes_;
    };
    DECLARE_LAYOUTDATA_METHODS(LyricsLineSegment)

protected:
    LyricsLineSegment(const ElementType &type, LyricsLine *sp, ElementFlags f = ElementFlag::NOTHING);
};

struct PartialLyricsLine final : LyricsLine {
private:
    OBJECT_ALLOCATOR(engraving, PartialLyricsLine)
    DECLARE_CLASSOF(ElementType::PARTIAL_LYRICSLINE)

public:
    PartialLyricsLine(DummyParentOr<EngravingItem> parent);
    PartialLyricsLine(const PartialLyricsLine &);

    PartialLyricsLine *clone() const override { return new PartialLyricsLine(*this); }

    LineSegment *create_line_segment() override;

    Lyrics *lyrics() const override { return nullptr; }

    void set_is_end_melisma(bool val) { is_end_melisma_ = val; }

    bool is_end_melisma() const override { return is_end_melisma_; }

    void set_verse(int val) { verse_ = val; }

    int verse() const { return verse_; }

    void set_center_between_staves(AutoOnOff v) { center_between_staves_ = v; }

    AutoOnOff center_between_staves() const { return center_between_staves_; }

    PropertyValue get_property(Pid property_id) const override;
    bool set_property(Pid property_id, const PropertyValue &) override;
    PropertyValue property_default(Pid property_id) const override;
    Sid get_property_style(Pid property_id) const override;
    void undo_change_property(Pid id, const PropertyValue &, PropertyFlags ps) override;
    void reset() override;

    Lyrics *find_lyrics_in_previous_repeat_seg() const;
    Lyrics *find_adjacent_lyrics_or_default() const;

protected:
    void do_compute_end_element() override;

    bool is_in_spanner_map() const override { return true; }

private:
    bool is_end_melisma_ = false;
    int verse_ = 0;
    AutoOnOff center_between_staves_ = AutoOnOff::AUTO;
};

struct PartialLyricsLineSegment final : LyricsLineSegment {
private:
    OBJECT_ALLOCATOR(engraving, PartialLyricsLineSegment)
    DECLARE_CLASSOF(ElementType::PARTIAL_LYRICSLINE_SEGMENT)

public:
    PartialLyricsLineSegment(PartialLyricsLine *);

    LyricsLineSegment *clone() const override { return new PartialLyricsLineSegment(*this); }

    PartialLyricsLine *lyrics_line() const { return to_partial_lyrics_line(spanner()); }

    Lyrics *lyrics() const override { return nullptr; }

    int verse() const override { return lyrics_line()->verse(); }

    double line_spacing() const override;

    bool lyrics_place_above() const override { return lyrics_line()->place_above(); } // Delegate?

    bool lyrics_add_to_skyline() const override { return lyrics_line()->add_to_skyline(); }

    Color color() const override { return lyrics_line()->color(); }

    double base_line_shift() const override;

    EngravingObject *property_delegate(Pid) const override;
};
} // namespace mu::engraving
