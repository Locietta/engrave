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

#include "lyrics.h"

#include "../editing/navigation.h"

#include "chord.h"
#include "chordrest.h"
#include "factory.h"
#include "measure.h"
#include "note.h"
#include "score.h"
#include "segment.h"
#include "system.h"
#include "utils.h"

using namespace mu;

namespace mu::engraving {
//---------------------------------------------------------
//   LyricsLine
//---------------------------------------------------------

static const ElementStyle lyrics_line_element_style{{Sid::LYRICS_DASH_LINE_THICKNESS, Pid::LINE_WIDTH}};

LyricsLine::LyricsLine(DummyParentOr<EngravingItem> parent) : SLine(ElementType::LYRICSLINE, parent) {
    set_diagonal(false);
    init_element_style(&::mu::engraving::lyrics_line_element_style);
    next_lyrics_ = 0;
}

LyricsLine::LyricsLine(const ElementType &type, DummyParentOr<EngravingItem> parent, ElementFlags f) : SLine(type, parent, f) {
    set_diagonal(false);
    init_element_style(&::mu::engraving::lyrics_line_element_style);
    next_lyrics_ = 0;
}

LyricsLine::LyricsLine(const LyricsLine &g) : SLine(g) { next_lyrics_ = 0; }

//---------------------------------------------------------
//   createLineSegment
//---------------------------------------------------------

LineSegment *LyricsLine::create_line_segment() {
    LyricsLineSegment *seg = new LyricsLineSegment(this);
    seg->set_track(track());
    seg->set_color(color());
    return seg;
}

//---------------------------------------------------------
//   removeUnmanaged
//    same as Spanner::removeUnmanaged(), but in addition, remove from hosting Lyrics
//---------------------------------------------------------

void LyricsLine::remove_unmanaged() {
    Spanner::remove_unmanaged();
    if (lyrics()) {
        lyrics()->remove(this);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool LyricsLine::set_property(Pid property_id, const engraving::PropertyValue &v) {
    switch (property_id) {
        case Pid::SPANNER_TICKS: {
            // if parent lyrics has a melisma, change its length too
            if (ownership_parent() && ownership_parent()->is_lyrics() && is_end_melisma()) {
                Fraction new_ticks = to_lyrics(ownership_parent())->ticks() + v.value<Fraction>() - ticks();
                ownership_parent()->undo_change_property(Pid::LYRIC_TICKS, new_ticks);
            }
            set_ticks(v.value<Fraction>());
        } break;
        default:
            if (!SLine::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

PropertyValue LyricsLine::property_default(Pid id) const {
    switch (id) {
        case Pid::LINE_WIDTH: return style_value(Pid::LINE_WIDTH, get_property_style(Pid::LINE_WIDTH));
        default: return SLine::property_default(id);
    }
}

Sid LyricsLine::get_property_style(Pid property_id) const {
    if (property_id == Pid::LINE_WIDTH) {
        return is_end_melisma() ? Sid::LYRICS_LINE_THICKNESS : Sid::LYRICS_DASH_LINE_THICKNESS;
    }

    return SLine::get_property_style(property_id);
}

void LyricsLine::do_compute_end_element() {
    if (!is_end_melisma()) {
        Spanner::do_compute_end_element();
        return;
    }

    set_end_element(score()->find_chord_rest_ending_before_tick_in_track(tick2(), track()));

    if (!end_element()) {
        set_end_element(score()->find_chord_rest_ending_before_tick_in_staff(tick2(), track2staff(track())));
    }
}

//=========================================================
//   LyricsLineSegment
//=========================================================

LyricsLineSegment::LyricsLineSegment(LyricsLine *sp) : LineSegment(ElementType::LYRICSLINE_SEGMENT, sp, ElementFlag::ON_STAFF) {}

LyricsLineSegment::LyricsLineSegment(const ElementType &type, LyricsLine *sp, ElementFlags f) : LineSegment(type, sp, f) {}

double LyricsLineSegment::base_line_shift() const {
    if (lyrics_line()->is_end_melisma()) {
        return -0.5 * absolute_from_spatium(line_width());
    }

    Lyrics *seg_lyrics = lyrics();
    return -style().style_d(Sid::LYRICS_DASH_YPOS_RATIO) * seg_lyrics->font_metrics().x_height();
}

PropertyValue LyricsLineSegment::get_property(Pid property_id) const {
    if (EngravingObject *delegate = property_delegate(property_id)) {
        return delegate->get_property(property_id);
    }

    return LineSegment::get_property(property_id);
}

bool LyricsLineSegment::set_property(Pid property_id, const PropertyValue &val) {
    if (EngravingObject *delegate = property_delegate(property_id)) {
        return delegate->set_property(property_id, val);
    }

    return LineSegment::set_property(property_id, val);
}

PropertyValue LyricsLineSegment::property_default(Pid property_id) const {
    if (EngravingObject *delegate = property_delegate(property_id)) {
        return delegate->property_default(property_id);
    }

    return LineSegment::property_default(property_id);
}

EngravingObject *LyricsLineSegment::property_delegate(Pid property_id) const {
    if (property_id == Pid::GENERATED) {
        return lyrics_line();
    }

    return LineSegment::property_delegate(property_id);
}

//=========================================================
//   PartialLyricsLine
//=========================================================

static const ElementStyle partial_lyrics_line_element_style{
    {Sid::LYRICS_DASH_LINE_THICKNESS, Pid::LINE_WIDTH},
    {Sid::LYRICS_PLACEMENT, Pid::PLACEMENT},
};

PartialLyricsLine::PartialLyricsLine(DummyParentOr<EngravingItem> parent) : LyricsLine(ElementType::PARTIAL_LYRICSLINE, parent) {
    set_generated(false);
    init_element_style(&::mu::engraving::partial_lyrics_line_element_style);
}

PartialLyricsLine::PartialLyricsLine(const PartialLyricsLine &other) : LyricsLine(other) {
    is_end_melisma_ = other.is_end_melisma_;
    center_between_staves_ = other.center_between_staves_;
}

LineSegment *PartialLyricsLine::create_line_segment() {
    PartialLyricsLineSegment *seg = new PartialLyricsLineSegment(this);
    seg->set_track(track());
    seg->set_color(color());
    return seg;
}

PropertyValue PartialLyricsLine::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::VERSE: return verse_;
        case Pid::CENTER_BETWEEN_STAVES: return center_between_staves();
        default: return LyricsLine::get_property(property_id);
    }
}

bool PartialLyricsLine::set_property(Pid property_id, const PropertyValue &val) {
    switch (property_id) {
        case Pid::VERSE: set_verse(val.to_int()); break;
        case Pid::CENTER_BETWEEN_STAVES: set_center_between_staves(val.value<AutoOnOff>()); break;
        default: return LyricsLine::set_property(property_id, val);
    }

    trigger_layout();

    return true;
}

PropertyValue PartialLyricsLine::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::VERSE: return 0;
        case Pid::CENTER_BETWEEN_STAVES: return AutoOnOff::AUTO;
        default: return LyricsLine::property_default(property_id);
    }
}

void PartialLyricsLine::reset() {
    undo_reset_property(Pid::CENTER_BETWEEN_STAVES);
    LyricsLine::reset();
}

Sid PartialLyricsLine::get_property_style(Pid property_id) const {
    switch (property_id) {
        case Pid::PLACEMENT: return Sid::LYRICS_PLACEMENT;
        default: return LyricsLine::get_property_style(property_id);
    }
}

void PartialLyricsLine::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::VERSE && verse() != v.to_int()) {
        ChordRest *end_cr = end_element() && end_element()->is_chord_rest() ? to_chord_rest(end_element()) : nullptr;
        Lyrics *end_lyrics = nullptr;
        if (end_cr) {
            for (Lyrics *lyr : end_cr->lyrics()) {
                if (lyr->verse() == verse()) {
                    end_lyrics = lyr;
                    break;
                }
            }
        }

        LyricsLine::undo_change_property(id, v, ps);
        if (end_lyrics && end_lyrics->verse() != v.to_int()) {
            end_lyrics->undo_change_property(id, v, ps);
        }
        return;
    }

    LyricsLine::undo_change_property(id, v, ps);
}

void PartialLyricsLine::do_compute_end_element() {
    LyricsLine::do_compute_end_element();

    if (start_element() && end_element() && start_element()->tick() > end_element()->tick()) {
        set_end_element(start_element());
    }
}

//=========================================================
//   PartialLyricsLineSegment
//=========================================================

static const ElementStyle partial_lyrics_line_segment_element_style{
    {Sid::LYRICS_MIN_TOP_DISTANCE, Pid::MIN_DISTANCE},
};

PartialLyricsLineSegment::PartialLyricsLineSegment(PartialLyricsLine *line)
    : LyricsLineSegment(ElementType::PARTIAL_LYRICSLINE_SEGMENT, line, ElementFlag::ON_STAFF) {
    set_generated(false);
    init_element_style(&::mu::engraving::partial_lyrics_line_segment_element_style);
}

double PartialLyricsLineSegment::line_spacing() const {
    const Lyrics *lyrics = lyrics_line()->find_adjacent_lyrics_or_default();

    if (!lyrics) {
        return 0.0;
    }

    return lyrics->line_spacing();
}

double PartialLyricsLineSegment::base_line_shift() const {
    if (lyrics_line()->is_end_melisma()) {
        return -0.5 * absolute_from_spatium(line_width());
    }

    const Lyrics *lyrics = lyrics_line()->find_adjacent_lyrics_or_default();
    if (!lyrics) {
        return 0.0;
    }

    return -style().style_d(Sid::LYRICS_DASH_YPOS_RATIO) * lyrics->font_metrics().x_height();
}

EngravingObject *PartialLyricsLineSegment::property_delegate(Pid pid) const {
    switch (pid) {
        case Pid::VERSE:
        case Pid::CENTER_BETWEEN_STAVES: return lyrics_line();
        default: return LyricsLineSegment::property_delegate(pid);
    }
}

Lyrics *PartialLyricsLine::find_lyrics_in_previous_repeat_seg() const {
    const std::vector<Measure *> measures = find_previous_repeat_measures(find_start_measure());

    for (const Measure *measure : measures) {
        Lyrics *prev = Navigation::last_lyrics_in_measure(measure->last(SegmentType::CHORD_REST), staff_idx(), verse(), placement());

        if (!prev) {
            continue;
        }

        return prev;
    }

    return nullptr;
}

Lyrics *PartialLyricsLine::find_adjacent_lyrics_or_default() const {
    Lyrics *prev = find_lyrics_in_previous_repeat_seg();
    if (prev) {
        return prev;
    }

    Lyrics *next = next_lyrics();
    if (next) {
        return next;
    }

    // If there are no adjacent lyrics, create dummy lyrics using the odd lyrics text style to get font information
    Lyrics *dummy_lyr = Factory::create_lyrics(score()->dummy());
    dummy_lyr->set_text_style_type(TextStyleType::LYRICS_ODD);
    return dummy_lyr;
}
} // namespace mu::engraving
