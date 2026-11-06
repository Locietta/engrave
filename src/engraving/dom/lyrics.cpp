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

#include "types/translatablestring.h"

#include "../editing/navigation.h"
#include "../editing/textedit.h"

#include "measure.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "text.h"
#include "utils.h"

#include "log.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   lyricsElementStyle
//---------------------------------------------------------

static const ElementStyle lyrics_element_style{
    {Sid::LYRICS_PLACEMENT, Pid::PLACEMENT},
    {Sid::LYRICS_AVOID_BARLINES, Pid::AVOID_BARLINES},
};

//---------------------------------------------------------
//   Lyrics
//---------------------------------------------------------

Lyrics::Lyrics(DummyParentOr<ChordRest> parent) : TextBase(ElementType::LYRICS, parent, TextStyleType::LYRICS_ODD, ElementFlag::ON_STAFF) {
    separator_ = 0;
    init_element_style(&::mu::engraving::lyrics_element_style);
    verse_ = 0;
    ticks_ = Fraction(0, 1);
    syllabic_ = LyricsSyllabic::SINGLE;
}

Lyrics::Lyrics(const Lyrics &l) : TextBase(l) {
    verse_ = l.verse_;
    ticks_ = l.ticks_;
    syllabic_ = l.syllabic_;
    separator_ = 0;
}

Lyrics::~Lyrics() {
    if (separator_) {
        remove(separator_);
    }
}

TranslatableString Lyrics::subtype_user_name() const { return TranslatableString("engraving", "Verse %1").arg(verse_ + 1); }

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Lyrics::add(EngravingItem *el) {
    if (el->is_lyrics_line()) {
        LyricsLine *separator = to_lyrics_line(el);
        separator_ = separator;
        score()->add_unmanaged_spanner(separator);
    } else {
        LOGD("Lyrics::add: unknown element %s", el->type_name());
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Lyrics::remove(EngravingItem *el) {
    if (el->is_lyrics_line()) {
        // only if separator still exists and is the right one
        if (separator_ && el == separator_) {
            // Lyrics::remove() and LyricsLine::removeUnmanaged() call each other;
            // be sure each finds a clean context
            LyricsLine *separ = separator_;
            separator_ = 0;
            separ->remove_unmanaged();
        }
    } else {
        LOGD("Lyrics::remove: unknown element %s", el->type_name());
    }
}

//---------------------------------------------------------
//   isMelisma
//---------------------------------------------------------

bool Lyrics::is_melisma() const {
    // entered as melisma using underscore?
    if (ticks_ > Fraction(0, 1)) {
        return true;
    }

    // hyphenated?
    // if so, it is a melisma only if there is no lyric in same verse on next CR
    if (separator_ && (syllabic_ == LyricsSyllabic::BEGIN || syllabic_ == LyricsSyllabic::MIDDLE)) {
        // find next CR and check for existence of lyric in same verse and placement (in any voice)
        const ChordRest *cr = chord_rest();
        if (cr) {
            const Segment *s = cr->segment()->next1();
            const track_idx_t strack = staff_idx() * ::mu::engraving::voices;
            const track_idx_t etrack = strack + ::mu::engraving::voices;
            const track_idx_t lyr_track = track();
            const ChordRest *lyr_voice_next_cr = s ? s->next_chord_rest(lyr_track) : nullptr;
            for (track_idx_t track = strack; track < etrack; ++track) {
                const ChordRest *track_next_cr = s ? s->next_chord_rest(track) : nullptr;
                if (track_next_cr) {
                    if (lyr_track != track && lyr_voice_next_cr && !lyr_voice_next_cr->lyrics(verse_, placement()) &&
                        lyr_voice_next_cr->tick() < track_next_cr->tick()) {
                        // There is an intermediary note in a different voice, this is a melisma
                        return true;
                    }
                    if (track_next_cr->lyrics(verse_, placement())) {
                        // Next note has lyrics, not a melisma just a dash
                        return false;
                    }
                }
            }
            return true;
        }
    }

    // default - not a melisma
    return false;
}

//---------------------------------------------------------
//   paste
//---------------------------------------------------------

void Lyrics::paste(const String &txt) {
    if (txt.starts_with('<') && txt.contains('>')) {
        TextBase::paste(txt);
        return;
    }

    String regex = String(u"[^\\S") + Char(0xa0) + Char(0x202F) + u"]+";
    StringList sl = txt.split(std::regex(regex.to_std_string()), muse::SKIP_EMPTY_PARTS);
    if (sl.empty()) {
        return;
    }

    StringList hyph = sl.at(0).split(u'-');
    score()->start_cmd(TranslatableString("undoableAction", "Paste lyrics"));

    delete_selected_text();

    if (hyph.size() > 1) {
        score()->undo(new InsertText(cursor(), hyph[0]));
        hyph.remove_at(0);
        sl[0] = hyph.join(u"-");
    } else if (sl.size() > 1 && sl[1] == u"-") {
        score()->undo(new InsertText(cursor(), sl[0]));
        sl.remove_at(0);
        sl.remove_at(0);
    } else if (sl[0].starts_with(u"_")) {
        sl[0].remove(0, 1);
        if (sl[0].is_empty()) {
            sl.remove_at(0);
        }
    } else if (sl[0].contains(u"_")) {
        size_t p = sl[0].index_of(u'_');
        score()->undo(new InsertText(cursor(), sl[0]));
        sl[0] = sl[0].mid(p + 1);
        if (sl[0].is_empty()) {
            sl.remove_at(0);
        }
    } else if (sl.size() > 1 && sl[1] == "_") {
        score()->undo(new InsertText(cursor(), sl[0]));
        sl.remove_at(0);
        sl.remove_at(0);
    } else {
        score()->undo(new InsertText(cursor(), sl[0]));
        sl.remove_at(0);
    }

    score()->end_cmd();
}

//---------------------------------------------------------
//   endTick
//---------------------------------------------------------

Fraction Lyrics::end_tick() const { return segment()->tick() + ticks(); }

//---------------------------------------------------------
//   acceptDrop
//---------------------------------------------------------

bool Lyrics::accept_drop(EditData &data) const { return data.drop_element->is_text() || TextBase::accept_drop(data); }

//---------------------------------------------------------
//   drop
//---------------------------------------------------------

EngravingItem *Lyrics::drop(Transaction &tx, EditData &data) {
    ElementType type = data.drop_element->type();
    if (type == ElementType::SYMBOL || type == ElementType::FSYMBOL) {
        TextBase::drop(tx, data);
        return 0;
    }
    if (!data.drop_element->is_text()) {
        delete data.drop_element;
        data.drop_element = 0;
        return 0;
    }
    Text *e = to_text(data.drop_element);
    e->set_ownership_parent(this);
    score()->undo_add_element(e);
    return e;
}

void Lyrics::adjust_previous() {
    Lyrics *prev = Navigation::prev_lyrics(to_lyrics(this));
    if (prev && prev->ticks().is_not_zero()) {
        // search for lyric spanners to split at this point if necessary
        if (prev->tick() + prev->ticks() >= tick()) {
            // the previous lyric has a spanner attached that goes through this one
            // we need to shorten it
            Segment *s = score()->tick2segment(tick());
            if (s) {
                s = s->prev1(SegmentType::CHORD_REST);
                if (s->tick() > prev->tick()) {
                    prev->undo_change_property(Pid::LYRIC_TICKS, s->tick() - prev->tick());
                } else {
                    prev->undo_change_property(Pid::LYRIC_TICKS, Fraction::eps());
                }
            }
        }
        prev->set_need_remove_invalid_segments();
        prev->trigger_layout();
    }
}

void Lyrics::set_need_remove_invalid_segments() {
    // Allow "invalid" segments when there is a following repeat item

    const Measure *meas = measure();
    const ChordRest *separator_end_chord = separator_ ? to_chord_rest(separator_->end_element()) : nullptr;
    const ChordRest *last_chord_rest = meas ? meas->last_chord_rest(track()) : nullptr;
    const bool end_chord_is_last_in_measure = separator_end_chord == last_chord_rest;
    const bool has_following_jump = last_chord_rest ? last_chord_rest->has_following_jump_item() : false;

    if (end_chord_is_last_in_measure && has_following_jump) {
        return;
    }
    need_remove_invalid_segments_ = true;
}

//---------------------------------------------------------
//   removeFromScore
//---------------------------------------------------------

void Lyrics::remove_from_score() {
    if (ticks_.is_not_zero()) {
        // clear melismaEnd flag from end cr
        ChordRest *ecr = score()->find_cr(end_tick(), track());
        if (ecr) {
            ecr->set_melisma_end(false);
        }
    }

    if (!plain_text().is_empty()) {
        PartialLyricsLine *partial_dash = find_prev_partial_lyrics_line_dash(this);
        if (partial_dash) {
            score()->undo_remove_element(partial_dash);
        }
    }

    if (separator_) {
        separator_->remove_unmanaged();
        delete separator_;
        separator_ = 0;
    }
    Lyrics *prev = Navigation::prev_lyrics(this);
    if (prev) {
        // check to make sure we haven't created an invalid segment by deleting this lyric
        prev->set_need_remove_invalid_segments();
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Lyrics::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SYLLABIC: return int(syllabic_);
        case Pid::LYRIC_TICKS: return ticks_;
        case Pid::VERSE: return verse_;
        case Pid::AVOID_BARLINES: return avoid_barlines_;
        default: return TextBase::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Lyrics::set_property(Pid property_id, const PropertyValue &v) {
    ChordRest *scr = nullptr;
    ChordRest *ecr = nullptr;

    switch (property_id) {
        case Pid::PLACEMENT: {
            PlacementV new_val = v.value<PlacementV>();
            if (new_val != placement()) {
                if (Lyrics *l = Navigation::prev_lyrics(this)) {
                    l->set_need_remove_invalid_segments();
                }
                if (Navigation::next_lyrics(this)) {
                    set_need_remove_invalid_segments();
                }
                set_placement(new_val);
            }
        } break;
        case Pid::SYLLABIC: syllabic_ = LyricsSyllabic(v.to_int()); break;
        case Pid::LYRIC_TICKS:
            if (ticks_.is_not_zero()) {
                // clear melismaEnd flag from previous end cr
                // this might be premature, as there may be other melismas ending there
                // but flag will be generated correctly on layout
                // TODO: after inserting a measure,
                // endTick info is wrong.
                // Somehow we need to fix this.
                // See https://musescore.org/en/node/285304 and https://musescore.org/en/node/311289
                ecr = score()->find_cr(end_tick(), track());
                if (ecr) {
                    ecr->set_melisma_end(false);
                }
            }
            scr = score()->find_cr(tick(), track());
            ticks_ = v.value<Fraction>();
            if (scr && ticks_ <= scr->ticks()) {
                // if no ticks, we have to relayout in order to remove invalid melisma segments
                set_need_remove_invalid_segments();
            }
            break;
        case Pid::VERSE: {
            if (Lyrics *l = Navigation::prev_lyrics(this)) {
                l->set_need_remove_invalid_segments();
            }
            bool follow_text_style = get_property(Pid::TEXT_STYLE) == property_default(Pid::TEXT_STYLE);
            verse_ = v.to_int();
            if (follow_text_style) {
                set_property(Pid::TEXT_STYLE, property_default(Pid::TEXT_STYLE));
            }
            break;
        }
        case Pid::AVOID_BARLINES: avoid_barlines_ = v.to_bool(); break;
        case Pid::VISIBLE: set_visible(v.to_bool()); break;
        default:
            if (!TextBase::set_property(property_id, v)) {
                return false;
            }
            break;
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Lyrics::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return is_even() ? TextStyleType::LYRICS_EVEN : TextStyleType::LYRICS_ODD;
        case Pid::PLACEMENT: return style().style_v(Sid::LYRICS_PLACEMENT);
        case Pid::SYLLABIC: return int(LyricsSyllabic::SINGLE);
        case Pid::LYRIC_TICKS: return Fraction(0, 1);
        case Pid::VERSE: return 0;
        case Pid::AVOID_BARLINES: return style().style_b(Sid::LYRICS_AVOID_BARLINES);
        case Pid::POSITION:
            if (is_melisma()) {
                return style().style_v(Sid::LYRICS_MELISMA_ALIGN).value<Align>().horizontal;
            }
        // fall through
        default: return TextBase::property_default(id);
    }
}

void Lyrics::trigger_layout() const {
    if (separator_) {
        // The separator may extend to next system(s), so we must use Spanner::triggerLayout()
        separator_->trigger_layout();
    } else {
        // In this case is ok to use EngravingItem::triggerLayout()
        EngravingItem::trigger_layout();
    }
}

double Lyrics::y_relative_to_staff() const {
    const double y_off = staff_offset_y();
    return pos().y() + chord_rest()->pos().y() + y_off;
}

void Lyrics::set_y_relative_to_staff(double y) {
    const double y_off = staff_offset_y();
    mutldata()->set_pos_y(y - chord_rest()->pos().y() - y_off);
}

//---------------------------------------------------------
//   forAllLyrics
//---------------------------------------------------------

void Score::for_all_lyrics(std::function<void(Lyrics *)> f) {
    for (Segment *s = first_segment(SegmentType::CHORD_REST); s; s = s->next1(SegmentType::CHORD_REST)) {
        for (EngravingItem *e : s->elist()) {
            if (e) {
                for (Lyrics *l : to_chord_rest(e)->lyrics()) {
                    f(l);
                }
            }
        }
    }
}

//---------------------------------------------------------
//   undoChangeProperty
//---------------------------------------------------------

void Lyrics::undo_change_property(Pid id, const PropertyValue &v, PropertyFlags ps) {
    if (id == Pid::VERSE && verse() != v.to_int()) {
        PartialLyricsLine *prev_partial = find_prev_partial_lyrics_line_dash(this);

        for (Lyrics *l : chord_rest()->lyrics()) {
            if (l->verse() == v.to_int()) {
                // verse already exists, swap
                l->TextBase::undo_change_property(id, verse(), ps);
                const PlacementV p = l->placement();
                l->TextBase::undo_change_property(Pid::PLACEMENT, placement(), ps);
                TextBase::undo_change_property(Pid::PLACEMENT, p, ps);
                break;
            }
        }
        TextBase::undo_change_property(id, v, ps);
        if (prev_partial && prev_partial->verse() != v.to_int()) {
            // Skip logic to update Lyrics by calling parent class
            prev_partial->LyricsLine::undo_change_property(id, v, ps);
        }
        return;
    } else if (id == Pid::VISIBLE && separator()) {
        separator()->undo_change_property(Pid::VISIBLE, v.to_bool(), ps);
    }

    TextBase::undo_change_property(id, v, ps);
}

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Lyrics::reset() {
    undo_reset_property(Pid::CENTER_BETWEEN_STAVES);
    TextBase::reset();
}

//---------------------------------------------------------
//   removeInvalidSegments
//
// Remove lyric-final melisma lines and reset the alignment of the lyric
//---------------------------------------------------------

void Lyrics::remove_invalid_segments() {
    need_remove_invalid_segments_ = false;
    if (separator_ && is_melisma() && ticks_ < separator_->start_cr()->ticks()) {
        set_ticks(Fraction(0, 1));
        separator_->set_ticks(Fraction(0, 1));
        separator_->remove_unmanaged();
        separator_ = nullptr;
        set_position(property_default(Pid::POSITION).value<AlignH>());
        if (syllabic_ == LyricsSyllabic::BEGIN || syllabic_ == LyricsSyllabic::SINGLE) {
            undo_change_property(Pid::SYLLABIC, int(LyricsSyllabic::SINGLE));
        } else {
            undo_change_property(Pid::SYLLABIC, int(LyricsSyllabic::END));
        }
    }
}
} // namespace mu::engraving
