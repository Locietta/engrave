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

#include "chordrest.h"

#include "barline.h"
#include "beam.h"
#include "breath.h"
#include "chord.h"
#include "clef.h"
#include "editing/editclef.h"
#include "editing/editkeysig.h"
#include "editing/editrehearsalmark.h"
#include "editing/navigation.h"
#include "editing/splitjoinmeasure.h"
#include "editing/transaction/transaction.h"
#include "editing/transpose.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "factory.h"
#include "figuredbass.h"
#include "harmony.h"
#include "harppedaldiagram.h"
#include "hook.h"
#include "instrchange.h"
#include "keysig.h"
#include "log.h"
#include "lyrics.h"
#include "marker.h"
#include "measure.h"
#include "note.h"
#include "page.h"
#include "part.h"
#include "rehearsalmark.h"
#include "score.h"
#include "segment.h"
#include "slur.h"
#include "staff.h"
#include "stafftype.h"
#include "system.h"
#include "tabdurationsymbol.h"
#include "translation.h"
#include "tuplet.h"
#include "types/typesconv.h"
#include "utils.h"
#include "volta.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   ChordRest
//---------------------------------------------------------

ChordRest::ChordRest(const ElementType &type, DummyParentOr<Segment> parent) : DurationElement(type, parent) {
    staff_move_ = 0;
    beam_ = 0;
    tab_dur_ = 0;
    beam_mode_ = BeamMode::AUTO;
    is_small_ = false;
    melisma_end_ = false;
    cross_measure_ = CrossMeasure::UNKNOWN;
}

ChordRest::ChordRest(const ChordRest &cr, bool link) : DurationElement(cr, link) {
    duration_type_ = cr.duration_type_;
    staff_move_ = cr.staff_move_;
    beam_ = 0;
    tab_dur_ = 0; // tab sur. symb. depends upon context: can't be
                  // simply copied from another CR

    beam_mode_ = cr.beam_mode_;
    is_small_ = cr.is_small_;
    melisma_end_ = cr.melisma_end_;
    cross_measure_ = cr.cross_measure_;

    for (Lyrics *l : cr.lyrics_) { // make deep copy
        Lyrics *nl = Factory::copy_lyrics(*l);
        if (link) {
            nl->link_to(l);
        }
        nl->set_ownership_parent(this);
        nl->set_track(track());
        lyrics_.push_back(nl);
    }
}

//---------------------------------------------------------
//   undoUnlink
//---------------------------------------------------------

void ChordRest::undo_unlink() {
    DurationElement::undo_unlink();
    for (Lyrics *l : lyrics_) {
        l->undo_unlink();
    }
}

//---------------------------------------------------------
//   ChordRest
//---------------------------------------------------------

ChordRest::~ChordRest() {
    muse::delete_all(lyrics_);
    muse::delete_all(el_);
    delete tab_dur_;

    if (beam_) {
        muse::remove(beam_->elements(), this);
        beam_ = nullptr;
    }

    if (beamlet_) {
        beamlet_ = nullptr;
    }
}

//---------------------------------------------------------
//   beam
//---------------------------------------------------------

Beam *ChordRest::beam() const { return !(measure() && measure()->stemless(staff_idx())) ? beam_ : nullptr; }

//---------------------------------------------------------
//   setBeam
//---------------------------------------------------------

void ChordRest::set_beam(Beam *b) { beam_ = b; }

void ChordRest::set_beamlet(BeamSegment *b) { beamlet_ = b; }

//---------------------------------------------------------
//   setDurationType
//---------------------------------------------------------

void ChordRest::set_duration_type(DurationType t) {
    duration_type_.set_type(t);
    cross_measure_ = CrossMeasure::UNKNOWN;
}

void ChordRest::set_duration_type(const Fraction &ticks) {
    duration_type_.set_val(ticks.ticks());
    cross_measure_ = CrossMeasure::UNKNOWN;
}

void ChordRest::set_duration_type(TDuration v) {
    duration_type_ = v;
    cross_measure_ = CrossMeasure::UNKNOWN;
}

//---------------------------------------------------------
//   durationUserName
//---------------------------------------------------------

String ChordRest::duration_user_name() const {
    String tuplet_type;
    if (tuplet()) {
        switch (tuplet()->ratio().numerator()) {
            case 2: tuplet_type = muse::mtrc("engraving", "Duplet"); break;
            case 3: tuplet_type = muse::mtrc("engraving", "Triplet"); break;
            case 4: tuplet_type = muse::mtrc("engraving", "Quadruplet"); break;
            case 5: tuplet_type = muse::mtrc("engraving", "Quintuplet"); break;
            case 6: tuplet_type = muse::mtrc("engraving", "Sextuplet"); break;
            case 7: tuplet_type = muse::mtrc("engraving", "Septuplet"); break;
            case 8: tuplet_type = muse::mtrc("engraving", "Octuplet"); break;
            case 9: tuplet_type = muse::mtrc("engraving", "Nonuplet"); break;
            default:
                //: %1 is tuplet ratio numerator (i.e. the number of notes in the tuplet)
                tuplet_type = muse::mtrc("engraving", "%1 note tuplet").arg(tuplet()->ratio().numerator());
        }
    }
    String dot_string;
    if (!tuplet_type.is_empty()) {
        dot_string += u' ';
    }

    switch (dots()) {
        case 1: dot_string += muse::mtrc("engraving", "Dotted %1").arg(TConv::translated_user_name(duration_type().type())); break;
        case 2: dot_string += muse::mtrc("engraving", "Double dotted %1").arg(TConv::translated_user_name(duration_type().type())); break;
        case 3: dot_string += muse::mtrc("engraving", "Triple dotted %1").arg(TConv::translated_user_name(duration_type().type())); break;
        case 4:
            dot_string += muse::mtrc("engraving", "Quadruple dotted %1").arg(TConv::translated_user_name(duration_type().type()));
            break;
        default: dot_string += TConv::translated_user_name(duration_type().type());
    }
    return String(u"%1%2").arg(tuplet_type, dot_string);
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void ChordRest::add(EngravingItem *e) {
    e->set_ownership_parent(this);
    e->set_track(track());
    switch (e->type()) {
        case ElementType::ARTICULATION: // for backward compatibility
            LOGD("ChordRest::add: unknown element %s", e->type_name());
            break;
        case ElementType::LYRICS:
            lyrics_.push_back(to_lyrics(e));
            e->added();
            break;
        default: EngravingItem::add(e); break;
    }
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void ChordRest::remove(EngravingItem *e) {
    switch (e->type()) {
        case ElementType::LYRICS: {
            to_lyrics(e)->remove_from_score();
            auto i = std::find(lyrics_.begin(), lyrics_.end(), to_lyrics(e));
            if (i != lyrics_.end()) {
                lyrics_.erase(i);
                e->removed();
            } else {
                LOGD("ChordRest::remove: %s %p not found", e->type_name(), e);
            }
        } break;
        default: EngravingItem::remove(e);
    }
}

//---------------------------------------------------------
//   removeDeleteBeam
///   Remove ChordRest from beam, delete beam if empty.
///   \param beamed - if the chordrest is beamed (will get
///                   a (new) beam)
//---------------------------------------------------------

void ChordRest::remove_delete_beam(bool beamed) {
    set_beamlet(nullptr);
    if (beam_) {
        Beam *b = beam_;
        beam_->remove(this);
        if (b->empty()) {
            score()->do_undo_remove_element(b);
        } else {
            renderer()->layout_beam1(b);
        }
    }
    if (!beamed && is_chord()) {
        Chord *c = to_chord(this);
        if (c->should_have_hook()) {
            if (!c->hook()) {
                c->create_hook();
            }
        } else if (c->hook()) {
            score()->do_undo_remove_element(c->hook());
        }
        renderer()->layout_stem(c);
    }
}

void ChordRest::compute_up() { UNREACHABLE; }

//---------------------------------------------------------
//   replaceBeam
//---------------------------------------------------------

void ChordRest::replace_beam(Beam *new_beam) {
    if (beam_ == new_beam) {
        return;
    }
    remove_delete_beam(true);
    new_beam->add(this);
}

Slur *ChordRest::slur(const ChordRest *second_chord_rest) const {
    if (second_chord_rest == nullptr) {
        ChordRestNavigateOptions options;
        options.disable_over_repeats = true;
        second_chord_rest = Navigation::next_chord_rest(const_cast<ChordRest *>(this), options);
    }
    int current_tick = tick().ticks();
    Slur *result = nullptr;
    for (auto it : score()->spanner_map().find_overlapping(current_tick, current_tick + 1)) {
        Spanner *spanner = it.value;
        if (!spanner->is_slur()) {
            continue;
        }
        Slur *slur = to_slur(spanner);
        if (slur->start_element() == this && slur->end_element() == second_chord_rest) {
            if (slur->slur_direction() == DirectionV::AUTO) {
                return slur;
            }
            result = slur;
        }
    }
    return result;
}

void ChordRest::undo_change_property(Pid id, const PropertyValue &new_value, PropertyFlags ps) {
    if (id == Pid::BEAM_MODE) {
        if (is_chord() && duration_type_.hooks() == 0) {
            const StaffType *tab = staff_type();
            if (!tab || !tab->is_tab_staff() || !tab->gen_durations()) {
                return;
            }
        }
        BeamMode new_beam_mode = new_value.value<BeamMode>();
        if ((new_beam_mode == BeamMode::BEGIN16 && duration_type_.hooks() < 2) ||
            (new_beam_mode == BeamMode::BEGIN32 && duration_type_.hooks() < 3)) {
            return;
        }
    }

    EngravingItem::undo_change_property(id, new_value, ps);
}

//---------------------------------------------------------
//   localSpatiumChanged
//---------------------------------------------------------

void ChordRest::local_spatium_changed(double old_value, double new_value) {
    DurationElement::local_spatium_changed(old_value, new_value);
    for (EngravingItem *e : lyrics()) {
        e->local_spatium_changed(old_value, new_value);
    }
    for (EngravingItem *e : el()) {
        e->local_spatium_changed(old_value, new_value);
    }
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue ChordRest::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::SMALL: return PropertyValue::from_value(is_small());
        case Pid::BEAM_MODE: return beam_mode();
        case Pid::STAFF_MOVE: return staff_move();
        case Pid::DURATION_TYPE_WITH_DOTS: return actual_duration_type().type_with_dots();
        default: return DurationElement::get_property(property_id);
    }
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool ChordRest::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::SMALL: set_small(v.to_bool()); break;
        case Pid::BEAM_MODE: set_beam_mode(v.value<BeamMode>()); break;
        case Pid::STAFF_MOVE: set_staff_move(v.to_int()); break;
        case Pid::VISIBLE:
            set_visible(v.to_bool());
            measure()->check_multi_voices(staff_idx());
            break;
        case Pid::DURATION_TYPE_WITH_DOTS: set_duration_type(v.value<DurationTypeWithDots>()); break;
        default: return DurationElement::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue ChordRest::property_default(Pid property_id) const {
    switch (property_id) {
        case Pid::SMALL: return false;
        case Pid::BEAM_MODE: return BeamMode::AUTO;
        case Pid::STAFF_MOVE: return 0;
        default: return DurationElement::property_default(property_id);
    }
    // Prevent unreachable code warning
    // triggerLayout();
}

//---------------------------------------------------------
//   isGrace
//---------------------------------------------------------

bool ChordRest::is_grace() const { return is_chord() && to_chord(this)->is_grace(); }

//---------------------------------------------------------
//   isGraceBefore
//---------------------------------------------------------

bool ChordRest::is_grace_before() const {
    return is_chord() && (to_chord(this)->note_type() &
                          (NoteType::ACCIACCATURA | NoteType::APPOGGIATURA | NoteType::GRACE4 | NoteType::GRACE16 | NoteType::GRACE32));
}

//---------------------------------------------------------
//   isGraceAfter
//---------------------------------------------------------

bool ChordRest::is_grace_after() const {
    return is_chord() && (to_chord(this)->note_type() & (NoteType::GRACE8_AFTER | NoteType::GRACE16_AFTER | NoteType::GRACE32_AFTER));
}

//---------------------------------------------------------
//   hasBreathMark - determine if chordrest has breath-mark
//---------------------------------------------------------
Breath *ChordRest::has_breath_mark() const {
    Segment *s = measure()->find_segment(SegmentType::BREATH, end_tick());
    return s ? to_breath(s->element(track())) : nullptr;
}

//---------------------------------------------------------
//   nextSegmentAfterCR
//    returns first segment at tick CR->tick + CR->actualTicks
//    of given types
//---------------------------------------------------------

Segment *ChordRest::next_segment_after_cr(SegmentType types) const {
    Fraction end = tick() + actual_ticks();
    for (Segment *s = segment()->next1_mm(types); s; s = s->next1_mm(types)) {
        if (s->tick() >= end) {
            return s;
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void ChordRest::set_track(track_idx_t val) {
    EngravingItem::set_track(val);
    process_siblings([val](EngravingItem *e) { e->set_track(val); });
}

//---------------------------------------------------------
//   setScore
//---------------------------------------------------------

void ChordRest::set_score(Score *s) {
    EngravingItem::set_score(s);
    process_siblings([s](EngravingItem *e) { e->set_score(s); });
}

//---------------------------------------------------------
//   processSiblings
//---------------------------------------------------------

void ChordRest::process_siblings(std::function<void(EngravingItem *)> func) {
    if (beam_) {
        func(beam_);
    }
    if (tab_dur_) {
        func(tab_dur_);
    }
    for (Lyrics *l : lyrics_) {
        func(l);
    }
    if (tuplet()) {
        func(tuplet());
    }
    for (EngravingItem *e : el_) {
        func(e);
    }
}

//---------------------------------------------------------
//   nextArticulationOrLyric
//---------------------------------------------------------

EngravingItem *ChordRest::next_articulation_or_lyric(EngravingItem *e) {
    if (e->is_lyrics()) {
        // The next element after Lyrics is the LyricsLine (if it exists)...
        if (LyricsLine *line = to_lyrics(e)->separator()) {
            return line;
        }
    }
    if (e->is_lyrics_line()) {
        LyricsLine *lyrics_line = to_lyrics_line(e);
        Lyrics *lyrics = lyrics_line->lyrics();
        IF_ASSERT_FAILED(lyrics) { return nullptr; }
        // We were on a LyricsLine - now we'll try to move to the next Lyrics...
        e = lyrics;
    }

    if (!is_chord() || !e->is_articulation_family()) {
        // Move to the next Lyrics...
        auto i = std::find(lyrics_.begin(), lyrics_.end(), e);
        if (i == lyrics_.end() || i == lyrics_.end() - 1) {
            return nullptr;
        }
        return *(i + 1);
    }

    // Cycle through articulations...
    Chord *c = to_chord(this);
    auto i = std::find(c->articulations().begin(), c->articulations().end(), e);
    if (i == c->articulations().end()) {
        return nullptr;
    }
    if (i != c->articulations().end() - 1) {
        return *(i + 1);
    }
    if (!lyrics_.empty()) {
        return lyrics_[0];
    }

    return nullptr;
}

//---------------------------------------------------------
//   prevArticulationOrLyric
//---------------------------------------------------------

EngravingItem *ChordRest::prev_articulation_or_lyric(EngravingItem *e) {
    const std::vector<Articulation *> artics = is_chord() ? to_chord(this)->articulations() : std::vector<Articulation *>();

    if (e->is_lyrics_line()) {
        // The previous element to a LyricsLine is the associated Lyrics...
        return to_lyrics_line(e)->lyrics();
    }

    auto i = std::find(lyrics_.begin(), lyrics_.end(), e);
    if (i == lyrics_.end()) {
        // Cycle through articulations...
        if (artics.empty()) {
            return nullptr;
        }
        auto j = std::find(artics.begin(), artics.end(), e);
        if (j == artics.end() || j == artics.begin()) {
            return nullptr;
        }
        return *(j - 1);
    }
    if (i == lyrics_.begin()) {
        return !artics.empty() ? artics.back() : nullptr;
    }

    // Move to the previous Lyrics...
    Lyrics *lyrics = *(i - 1);
    IF_ASSERT_FAILED(lyrics) { return nullptr; }
    if (lyrics->separator()) {
        // Move to the LyricsLine of the previous Lyrics (if it exists)...
        return lyrics->separator();
    }
    return lyrics;
}

//---------------------------------------------------------
//   nextElement
//---------------------------------------------------------

EngravingItem *ChordRest::next_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().front();
    }
    switch (e->type()) {
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING:
        case ElementType::LYRICS:
        case ElementType::LYRICSLINE: {
            EngravingItem *next = next_articulation_or_lyric(e);
            if (next) {
                return next;
            } else {
                break;
            }
        }
        default: {
            if (is_chord() && !to_chord(this)->articulations().empty()) {
                return to_chord(this)->articulations()[0];
            } else if (!lyrics_.empty()) {
                return lyrics_[0];
            } else {
                break;
            }
        }
    }
    staff_idx_t staff_id = e->staff_idx();
    return segment()->next_element(staff_id);
}

//---------------------------------------------------------
//   prevElement
//---------------------------------------------------------

EngravingItem *ChordRest::prev_element() {
    EngravingItem *e = score()->selection().element();
    if (!e && !score()->selection().elements().empty()) {
        e = score()->selection().elements().back();
    }
    switch (e->type()) {
        case ElementType::ARTICULATION:
        case ElementType::ORNAMENT:
        case ElementType::TAPPING:
        case ElementType::LYRICS:
        case ElementType::LYRICSLINE: {
            EngravingItem *prev = prev_articulation_or_lyric(e);
            if (prev) {
                return prev;
            } else {
                if (is_chord()) {
                    return to_chord(this)->last_element_before_segment();
                }
            }
            // fall through
        }
        default: {
            break;
        }
    }

    Tuplet *tuplet = this->tuplet();
    if (tuplet && this == tuplet->elements().front()) {
        return tuplet;
    }

    staff_idx_t staff_id = e->staff_idx();
    EngravingItem *prev_item = segment()->prev_element(staff_id);
    if (prev_item && prev_item->is_note()) {
        const Chord *prev_chord = to_note(prev_item)->chord();
        if (prev_chord && !prev_chord->articulations().empty()) {
            return prev_chord->articulations().back();
        }
    }
    return prev_item;
}

//---------------------------------------------------------
//   lastElementBeforeSegment
//---------------------------------------------------------

EngravingItem *ChordRest::last_element_before_segment() {
    if (lyrics_.empty()) {
        return nullptr;
    }
    Lyrics *lyrics = lyrics_.back();
    IF_ASSERT_FAILED(lyrics) { return nullptr; }
    if (LyricsLine *lyrics_line = lyrics->separator()) {
        return lyrics_line;
    }
    return lyrics;
}

//---------------------------------------------------------
//   nextSegmentElement
//---------------------------------------------------------

EngravingItem *ChordRest::next_segment_element() { return segment()->first_in_next_segments(staff_idx()); }

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void ChordRest::scan_elements(std::function<void(EngravingItem *)> func) {
    if (beam_ && (beam_->elements().front() == this) && !measure()->stemless(staff_idx())) {
        beam_->scan_elements(func);
    }
    for (Lyrics *l : lyrics_) {
        l->scan_elements(func);
    }
    DurationElement *de = this;
    while (de->tuplet() && de->tuplet()->elements().front() == de) {
        de->tuplet()->scan_elements(func);
        de = de->tuplet();
    }
    if (tab_dur_) {
        func(tab_dur_);
    }
}

//---------------------------------------------------------
//   prevSegmentElement
//---------------------------------------------------------

EngravingItem *ChordRest::prev_segment_element() { return segment()->last_in_prev_segments(staff_idx()); }

//---------------------------------------------------------
//   isMelismaEnd
//    returns true if chordrest represents the end of a melisma
//---------------------------------------------------------

bool ChordRest::is_melisma_end() const { return melisma_end_; }

//---------------------------------------------------------
//   setMelismaEnd
//---------------------------------------------------------

void ChordRest::set_melisma_end(bool v) {
    melisma_end_ = v;
    // TODO: don't take "false" at face value
    // check to see if some other melisma ends here,
    // in which case we can leave this set to true
    // for now, rely on the fact that we'll generate the value correctly on layout
}

//---------------------------------------------------------
//   lyrics
//---------------------------------------------------------

Lyrics *ChordRest::lyrics(int verse) const {
    for (Lyrics *l : lyrics_) {
        if (l->verse() == verse) {
            return l;
        }
    }
    return 0;
}

Lyrics *ChordRest::lyrics(int verse, PlacementV p) const {
    for (Lyrics *l : lyrics_) {
        if (l->placement() == p && l->verse() == verse) {
            return l;
        }
    }
    return 0;
}

//---------------------------------------------------------
//   lastVerse
//    return last verse number (starting from 0)
//    return -1 if there are no lyrics;
//---------------------------------------------------------

int ChordRest::last_verse(PlacementV p) const {
    int last_verse = -1;

    for (Lyrics *l : lyrics_) {
        if (l->placement() == p && l->verse() > last_verse) {
            last_verse = l->verse();
        }
    }

    return last_verse;
}

//---------------------------------------------------------
//   removeMarkings
//    - this is normally called after cloning a chord to tie a note over the barline
//    - there is no special undo handling; the assumption is that undo will simply remove the cloned chord
//    - two note tremolos are converted into simple notes
//    - single note tremolos are optionally retained
//---------------------------------------------------------

void ChordRest::remove_markings(bool /* keepTremolo */) {
    muse::delete_all(el());
    clear_els();
    muse::delete_all(lyrics());
    lyrics().clear();
}

//---------------------------------------------------------
//   isBefore
//---------------------------------------------------------

bool ChordRest::is_before(const EngravingItem *o) const {
    if (!o || this == o) {
        return false;
    }

    const ChordRest *other_cr = nullptr;
    if (o->is_chord_rest()) {
        other_cr = to_chord_rest(o);
    } else if (o->is_note()) {
        other_cr = to_note(o)->chord();
    }

    if (!other_cr) {
        return EngravingItem::is_before(o);
    }

    Fraction this_tick = tick();
    Fraction other_tick = other_cr->tick();
    if (this_tick != other_tick) {
        return this_tick < other_tick;
    }

    bool this_is_grace_before = is_grace_before();
    bool other_is_grace_before = other_cr->is_grace_before();
    if (this_is_grace_before != other_is_grace_before) {
        return this_is_grace_before;
    } else if (this_is_grace_before) {
        return to_chord(this)->grace_index() < to_chord(other_cr)->grace_index();
    }

    bool this_is_grace_after = is_grace_after();
    bool other_is_grace_after = other_cr->is_grace_after();
    if (this_is_grace_after != other_is_grace_after) {
        return !this_is_grace_after;
    } else if (this_is_grace_after) {
        return to_chord(this)->grace_index() > to_chord(other_cr)->grace_index();
    }

    return false;
}

bool ChordRest::is_below_cross_beam(const BeamBase *beam_base) const { return staff_move() >= beam_base->cross_staff_idx(); }

void ChordRest::check_staff_move_validity() {
    if (!staff()) {
        return;
    }
    staff_idx_t idx = staff_move_ ? v_staff_idx() : staff_idx() + stored_staff_move_;
    const Staff *base_staff = staff();
    const StaffType *base_staff_type = base_staff->staff_type_for_element(this);
    const Staff *target_staff = score()->staff(idx);
    const StaffType *target_staff_type = target_staff ? target_staff->staff_type_for_element(this) : nullptr;
    // check that destination staff makes sense
    const TrackRange track_range = part()->track_range();
    staff_idx_t min_staff = track2staff(track_range.start_track);
    staff_idx_t max_staff = track2staff(track_range.end_track);
    bool is_destination_valid = target_staff && target_staff->visible() && idx >= min_staff && idx < max_staff &&
                                target_staff_type->group() == base_staff_type->group() &&
                                target_staff->is_linked() == base_staff->is_linked();
    if (!is_destination_valid) {
        LOGD("staffMove out of scope %zu + %d min %zu max %zu", staff_idx(), staff_move_, min_staff, max_staff);
        // If destination staff is invalid, reset to base staff
        if (staff_move_) {
            // Remember the intended staff move, so it can be re-applied if
            // destination staff becomes valid (e.g. unihidden)
            stored_staff_move_ = staff_move_;
        }
        set_staff_move(0);
    } else if (!staff_move_ && stored_staff_move_) {
        set_staff_move(stored_staff_move_);
        stored_staff_move_ = 0;
    }

    if (is_destination_valid) {
        // Move valid, clear stored move
        stored_staff_move_ = 0;
    }
}

bool ChordRest::has_following_jump_item() const {
    const Segment *seg = segment();
    const Measure *measure = seg ? seg->measure() : nullptr;
    if (!measure) {
        return false;
    }

    if (end_tick() != measure->end_tick()) {
        return false;
    }

    std::vector<Measure *> following_repeat_measures = find_following_repeat_measures(measure);

    return !following_repeat_measures.empty();
}

bool ChordRest::has_preceding_jump_item() const {
    const Segment *seg = segment();
    const Measure *measure = seg->measure();

    if (tick() != measure->tick()) {
        return false;
    }

    std::vector<Measure *> preceding_repeat_measures = find_previous_repeat_measures(measure);

    return !preceding_repeat_measures.empty();
}
} // namespace mu::engraving
