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

#include "tuplet.h"

#include "beam.h"
#include "chord.h"
#include "engravingitem.h"
#include "factory.h"
#include "log.h"
#include "measure.h"
#include "note.h"
#include "rest.h"
#include "score.h"
#include "style/textstyle.h"
#include "text.h"

using namespace mu;
using namespace mu::engraving;

namespace mu::engraving {
//---------------------------------------------------------
//   tupletStyle
//---------------------------------------------------------

static const ElementStyle tuplet_style{
    {Sid::TUPLET_DIRECTION, Pid::DIRECTION},
    {Sid::TUPLET_NUMBER_TYPE, Pid::NUMBER_TYPE},
    {Sid::TUPLET_BRACKET_TYPE, Pid::BRACKET_TYPE},
    {Sid::TUPLET_BRACKET_WIDTH, Pid::LINE_WIDTH},
    {Sid::TUPLET_FONT_FACE, Pid::FONT_FACE},
    {Sid::TUPLET_FONT_SIZE, Pid::FONT_SIZE},
    {Sid::TUPLET_FONT_STYLE, Pid::FONT_STYLE},
    {Sid::TUPLET_ALIGN, Pid::ALIGN},
    {Sid::TUPLET_POSITION, Pid::POSITION},
    {Sid::TUPLET_MIN_DISTANCE, Pid::MIN_DISTANCE},
    {Sid::TUPLET_FONT_SPATIUM_DEPENDENT, Pid::SIZE_SPATIUM_DEPENDENT},
};

//---------------------------------------------------------
//   Tuplet
//---------------------------------------------------------

Tuplet::Tuplet(DummyParentOr<Measure> parent) : DurationElement(ElementType::TUPLET, parent) {
    direction_ = DirectionV::AUTO;
    number_type_ = TupletNumberType::SHOW_NUMBER;
    bracket_type_ = TupletBracketType::AUTO_BRACKET;
    ratio_ = Fraction(1, 1);
    number_ = 0;
    has_bracket_ = false;
    is_up_ = true;
    init_element_style(&::mu::engraving::tuplet_style);
}

Tuplet::Tuplet(const Tuplet &t) : DurationElement(t) {
    tick_ = t.tick_;
    has_bracket_ = t.has_bracket_;
    ratio_ = t.ratio_;
    base_len_ = t.base_len_;
    direction_ = t.direction_;
    number_type_ = t.number_type_;
    bracket_type_ = t.bracket_type_;
    bracket_width_ = t.bracket_width_;

    is_up_ = t.is_up_;

    p1_ = t.p1_;
    p2_ = t.p2_;
    user_p1_ = t.user_p1_;
    user_p2_ = t.user_p2_;

    // recreated on layout
    number_ = 0;
}

//---------------------------------------------------------
//   ~Tuplet
//---------------------------------------------------------

Tuplet::~Tuplet() {
    being_destructed_ = true;

    for (DurationElement *de : all_elements_) {
        assert(de->tuplet() == this);
        de->set_tuplet(nullptr);
    }
    delete number_;
}

void Tuplet::set_ownership_parent(DummyParentOr<Measure> parent) { EngravingItem::set_ownership_parent(parent); }

//---------------------------------------------------------
//   setSelected
//---------------------------------------------------------

void Tuplet::set_selected(bool f) {
    EngravingItem::set_selected(f);
    if (number_) {
        number_->set_selected(f);
    }
}

//---------------------------------------------------------
//   setVisible
//---------------------------------------------------------

void Tuplet::set_visible(bool f) {
    EngravingItem::set_visible(f);
    if (number_) {
        number_->set_visible(f);
    }
}

//---------------------------------------------------------
//   setColor
//---------------------------------------------------------

void Tuplet::set_color(const Color &col) {
    EngravingItem::set_color(col);
    if (number_) {
        number_->set_color(col);
    }
}

EngravingObject *Tuplet::property_delegate(Pid id) const {
    switch (id) {
        case Pid::FONT_FACE:
        case Pid::FONT_STYLE:
        case Pid::FONT_SIZE:
        case Pid::ALIGN:
        case Pid::POSITION:
        case Pid::SIZE_SPATIUM_DEPENDENT: return number_;
        default: return nullptr;
    }
}

//---------------------------------------------------------
//   rtick
//---------------------------------------------------------

Fraction Tuplet::rtick() const { return tick() - measure()->tick(); }

//---------------------------------------------------------
//   resetNumberProperty
//   reset number properties to default values
//   Set FONT_ITALIC to true, because for tuplets number should be italic
//---------------------------------------------------------

void Tuplet::reset_number_property() { reset_number_property(number_); }

void Tuplet::reset_number_property(Text *number) {
    for (auto p : {Pid::FONT_FACE, Pid::FONT_STYLE, Pid::FONT_SIZE, Pid::ALIGN, Pid::POSITION, Pid::SIZE_SPATIUM_DEPENDENT}) {
        number->reset_property(p);
    }
}

//---------------------------------------------------------
//   calcHasBracket
//---------------------------------------------------------

bool Tuplet::calc_has_bracket(const DurationElement *cr1, const DurationElement *cr2) const {
    if (bracket_type_ != TupletBracketType::AUTO_BRACKET) {
        return bracket_type_ != TupletBracketType::SHOW_NO_BRACKET;
    }
    if (cr1 == cr2) { // Degenerate tuplet
        return false;
    }
    if (!cr1->is_chord() || !cr2->is_chord()) {
        return true;
    }
    const Chord *c1 = to_chord(cr1);
    const Chord *c2 = to_chord(cr2);

    Beam const *beam_start = c1->beam();
    Beam const *beam_end = c2->beam();
    if (!beam_start || !beam_end || beam_start != beam_end) {
        return true;
    }
    bool tuplet_starts_beam = beam_start->elements().front() == c1;
    bool tuplet_ends_beam = beam_end->elements().back() == c2;
    bool head_side = is_up() != (c1->up() || c2->up());
    bool is_cross = c1->v_staff_idx() != c2->v_staff_idx();
    if (tuplet_starts_beam && tuplet_ends_beam && (!head_side || is_cross)) {
        return false;
    }

    int beam_count = -1;
    for (DurationElement *e : current_elements_) {
        if (e->is_tuplet() || e->is_rest()) {
            return true;
        } else if (e->is_chord_rest()) {
            ChordRest *cr = to_chord_rest(e);
            if (cr->beam() == 0) {
                return true;
            }
            if (beam_count == -1) {
                beam_count = cr->beams();
            } else if (beam_count != cr->beams()) {
                return true;
            }
        }
    }
    if (beam_count < 1) {
        return true;
    }

    bool start_chord_breaks32 = false;
    bool start_chord_breaks64 = false;
    Chord *prev_start_chord = c1->prev();
    if (prev_start_chord) {
        beam_start->calc_beam_breaks(c1, prev_start_chord, beam_count - 1, start_chord_breaks32, start_chord_breaks64);
    }
    bool start_chord_defines_tuplet = start_chord_breaks32 || start_chord_breaks64 || tuplet_starts_beam;
    if (prev_start_chord) {
        start_chord_defines_tuplet = start_chord_defines_tuplet || prev_start_chord->beams() < beam_count;
    }

    bool end_chord_breaks32 = false;
    bool end_chord_breaks64 = false;
    Chord *next_end_chord = c2->next();
    if (next_end_chord) {
        beam_end->calc_beam_breaks(next_end_chord, c2, beam_count - 1, end_chord_breaks32, end_chord_breaks64);
    }
    bool end_chord_defines_tuplet = end_chord_breaks32 || end_chord_breaks64 || tuplet_ends_beam;
    if (next_end_chord) {
        end_chord_defines_tuplet = end_chord_defines_tuplet || next_end_chord->beams() < beam_count;
    }

    if (start_chord_defines_tuplet && end_chord_defines_tuplet && (!head_side || is_cross)) {
        return false;
    }

    return true;
}

//---------------------------------------------------------
//   scanElements
//---------------------------------------------------------

void Tuplet::scan_elements(std::function<void(EngravingItem *)> func) {
    if (number_) {
        number_->scan_elements(func);
    }

    func(this);
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Tuplet::add(EngravingItem *e) {
#ifndef NDEBUG
    for (DurationElement *el : current_elements_) {
        if (el == e) {
            LOGD("%p: %p %s already there", this, e, e->type_name());
            return;
        }
    }
#endif

    switch (e->type()) {
        case ElementType::CHORD:
        case ElementType::REST:
        case ElementType::TUPLET: {
            DurationElement *de = to_duration_element(e);
            de->set_tuplet(this);

            bool found = false;
            Fraction tick = de->rtick();
            if (tick != Fraction(-1, 1)) {
                for (unsigned int i = 0; i < current_elements_.size(); ++i) {
                    if (current_elements_[i]->rtick() > tick) {
                        current_elements_.insert(current_elements_.begin() + i, de);
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                current_elements_.push_back(de);
            }
        } break;

        default: LOGD("Tuplet::add() unknown element"); return;
    }

    e->added();
}

//---------------------------------------------------------
//   remove
//---------------------------------------------------------

void Tuplet::remove(EngravingItem *e) {
    switch (e->type()) {
            //            case ElementType::TEXT:
            //                  if (e == _number)
            //                        _number = 0;
            //                  break;
        case ElementType::CHORD:
        case ElementType::REST:
        case ElementType::TUPLET: {
            auto i = std::find(current_elements_.begin(), current_elements_.end(), to_duration_element(e));
            if (i == current_elements_.end()) {
                LOGD("Tuplet::remove: cannot find element <%s>", e->type_name());
                LOGD("  elements %zu", current_elements_.size());
            } else {
                current_elements_.erase(i);
                e->removed();
            }
        } break;
        default: LOGD("Tuplet::remove: unknown element"); break;
    }
}

void Tuplet::add_duration_element(DurationElement *de) {
    assert(!being_destructed_);
    all_elements_.insert(de);
}

void Tuplet::remove_duration_element(DurationElement *de) {
    if (being_destructed_) {
        return;
    }

    all_elements_.erase(de);
}

//---------------------------------------------------------
//   isEditable
//---------------------------------------------------------

//---------------------------------------------------------
//   reset
//---------------------------------------------------------

void Tuplet::reset() {
    undo_change_property(Pid::P1, PointF());
    undo_change_property(Pid::P2, PointF());
    EngravingItem::reset();
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Tuplet::dump() const {
    EngravingItem::dump();
    LOGD() << "ratio: " << ratio_.to_string();
}

//---------------------------------------------------------
//   setTrack
//---------------------------------------------------------

void Tuplet::set_track(track_idx_t val) {
    if (tuplet()) {
        tuplet()->set_track(val);
    }
    if (number_) {
        number_->set_track(val);
    }
    EngravingItem::set_track(val);
}

//---------------------------------------------------------
//   tickGreater
//---------------------------------------------------------

static bool tick_greater(const DurationElement *a, const DurationElement *b) { return a->tick() < b->tick(); }

//---------------------------------------------------------
//   sortElements
//---------------------------------------------------------

void Tuplet::sort_elements() { std::sort(current_elements_.begin(), current_elements_.end(), tick_greater); }

//---------------------------------------------------------
//   cross
//---------------------------------------------------------

bool Tuplet::cross() const {
    for (DurationElement *de : current_elements_) {
        if (!de) {
            continue;
        } else if (de->is_chord_rest()) {
            if (to_chord_rest(de)->staff_move()) {
                return true;
            }
        } else if (de->is_tuplet()) {
            if (to_tuplet(de)->cross()) {
                return true;
            }
        }
    }
    return false;
}

//---------------------------------------------------------
//   vStaffIdx
///  Staff index for layout based on the first ChordRest
//---------------------------------------------------------

staff_idx_t Tuplet::v_staff_idx() const {
    if (elements().empty()) {
        return muse::nidx;
    }

    const DurationElement *cr = elements().front();
    if (!cr) {
        return muse::nidx;
    }

    while (cr->is_tuplet()) {
        const Tuplet *t = to_tuplet(cr);
        if (t->elements().empty()) {
            return muse::nidx;
        }
        cr = t->elements().front();
    }

    return cr->v_staff_idx();
}

//---------------------------------------------------------
//   elementsDuration
///  Get the sum of the element fraction in the tuplet,
///  even if the tuplet is not complete yet
//---------------------------------------------------------

Fraction Tuplet::elements_duration() {
    Fraction f;
    for (DurationElement *el : current_elements_) {
        f += el->ticks();
    }
    return f;
}

//---------------------------------------------------------
//   getProperty
//---------------------------------------------------------

PropertyValue Tuplet::get_property(Pid property_id) const {
    if (EngravingObject *delegate = property_delegate(property_id)) {
        return delegate->get_property(property_id);
    }
    switch (property_id) {
        case Pid::DIRECTION: return PropertyValue::from_value<DirectionV>(direction_);
        case Pid::NUMBER_TYPE: return int(number_type_);
        case Pid::BRACKET_TYPE: return int(bracket_type_);
        case Pid::LINE_WIDTH: return bracket_width_;
        case Pid::NORMAL_NOTES: return ratio_.denominator();
        case Pid::ACTUAL_NOTES: return ratio_.numerator();
        case Pid::P1: return user_p1_;
        case Pid::P2: return user_p2_;
        default: break;
    }
    return DurationElement::get_property(property_id);
}

//---------------------------------------------------------
//   setProperty
//---------------------------------------------------------

bool Tuplet::set_property(Pid property_id, const PropertyValue &v) {
    if (EngravingObject *delegate = property_delegate(property_id)) {
        return delegate->set_property(property_id, v);
    }
    switch (property_id) {
        case Pid::DIRECTION: set_direction(v.value<DirectionV>()); break;
        case Pid::NUMBER_TYPE: set_number_type(TupletNumberType(v.to_int())); break;
        case Pid::BRACKET_TYPE: set_bracket_type(TupletBracketType(v.to_int())); break;
        case Pid::LINE_WIDTH: set_bracket_width(v.value<Spatium>()); break;
        case Pid::NORMAL_NOTES: ratio_.set_denominator(v.to_int()); break;
        case Pid::ACTUAL_NOTES: ratio_.set_numerator(v.to_int()); break;
        case Pid::P1: user_p1_ = v.value<PointF>(); break;
        case Pid::P2: user_p2_ = v.value<PointF>(); break;
        case Pid::TICK: set_tick(v.value<Fraction>()); break;
        default: return DurationElement::set_property(property_id, v);
    }
    if (!current_elements_.empty()) {
        current_elements_.front()->trigger_layout();
        current_elements_.back()->trigger_layout();
    }
    return true;
}

//---------------------------------------------------------
//   propertyDefault
//---------------------------------------------------------

PropertyValue Tuplet::property_default(Pid id) const {
    switch (id) {
        case Pid::TEXT_STYLE: return TextStyleType::TUPLET;
        case Pid::SYSTEM_FLAG: return false;
        case Pid::TEXT: return String();
        case Pid::NORMAL_NOTES:
        case Pid::ACTUAL_NOTES: return 0;
        case Pid::P1:
        case Pid::P2: return PointF();
        case Pid::POSITION: return style().style_v(Sid::TUPLET_POSITION);
        case Pid::ALIGN: return style().style_v(Sid::TUPLET_ALIGN);
        case Pid::FONT_FACE: return style().style_v(Sid::TUPLET_FONT_FACE);
        case Pid::FONT_SIZE: return style().style_v(Sid::TUPLET_FONT_SIZE);
        case Pid::FONT_STYLE: return style().style_v(Sid::TUPLET_FONT_STYLE);
        case Pid::SIZE_SPATIUM_DEPENDENT: return style().style_v(Sid::TUPLET_FONT_SPATIUM_DEPENDENT);
        default: break;
    }

    // TODO: what does this do? Why TextStyleType::DEFAULT?
    for (const auto &spp : *text_style(TextStyleType::DEFAULT)) {
        if (spp.pid == id) {
            return style_value(id, spp.sid);
        }
    }

    return DurationElement::property_default(id);
}

//---------------------------------------------------------
//   sanitizeTuplet
///    Check validity of tuplets and coherence between duration
///    and baselength. Needed for importing old files due to a bug
///    in the released version for corner-case tuplets.
///    See issue #136406 and Pull request #2881
//---------------------------------------------------------

void Tuplet::sanitize_tuplet() {
    if (ratio().numerator() == ratio().reduced().numerator()) { // return if the ratio is an irreducible fraction
        return;
    }
    Fraction base_len_duration = (Fraction(ratio().denominator(), 1) * base_len().fraction()).reduced();

    // Due to a bug present in 2.1 (and before), a tuplet with non-reduced ratio could be
    // in a corrupted state (mismatch between duration and base length).
    // A tentative will now be made to retrieve the correct duration by summing up all the
    // durations of the elements constituting the tuplet. This does not work for
    // not-completely filled tuplets, such as tuplets in voices > 0 with
    // gaps (for example, a tuplet in second voice with a deleted chordrest element)

    Fraction test_duration(0, 1);
    for (DurationElement *de : elements()) {
        if (!de) {
            continue;
        }
        Fraction element_duration(0, 1);
        if (de->is_tuplet()) {
            Tuplet *t = to_tuplet(de);
            t->sanitize_tuplet();
            element_duration = t->ticks();
        } else {
            element_duration = de->ticks();
        }
        test_duration += element_duration;
    }
    test_duration = test_duration / ratio();
    test_duration.reduce();
    if (elements().back()->end_tick() - elements().front()->tick() > test_duration) {
        return; // this tuplet has missing elements; do not sanitize
    }
    if (!(test_duration == base_len_duration && base_len_duration == ticks())) {
        Fraction f = test_duration * Fraction(1, ratio().denominator());
        f.reduce();
        Fraction fbl(1, f.denominator());
        if (TDuration::is_valid(fbl)) {
            set_ticks(test_duration);
            set_base_len(fbl);
            LOGD("Tuplet %p sanitized duration %d/%d   baseLen %d/%d", this, test_duration.numerator(), test_duration.denominator(), 1,
                 fbl.denominator());
        } else {
            LOGD("Impossible to sanitize the tuplet");
        }
    }
}

//---------------------------------------------------------
//   addMissingElement
//     Add a rest with the given start and end ticks.
//     Should only be called from Tuplet::addMissingElements().
//     Needed for importing files that saved incomplete tuplets.
//---------------------------------------------------------

Fraction Tuplet::add_missing_element(const Fraction &start_tick, const Fraction &end_tick) {
    Fraction f = (end_tick - start_tick) * ratio();
    TDuration d = TDuration(f, true);
    if (!d.is_valid()) {
        LOGD("Tuplet::addMissingElement(): invalid duration: %d/%d", f.numerator(), f.denominator());
        return Fraction::from_ticks(0);
    }
    f = d.fraction();
    Segment *segment = measure()->get_segment(SegmentType::CHORD_REST, start_tick);
    Rest *rest = Factory::create_rest(segment);
    rest->set_duration_type(d);
    rest->set_ticks(f);
    rest->set_track(track());
    rest->set_visible(false);

    segment->add(rest);
    add(rest);
    return f;
}

//---------------------------------------------------------
//   addMissingElements
//     Make this tuplet complete by filling in holes where
//     there ought to be rests. Needed for importing files
//     that saved incomplete tuplets.
//---------------------------------------------------------

void Tuplet::add_missing_elements() {
    if (tuplet()) {
        return; // do not correct nested tuplets
    }
    if (voice() == 0) {
        return; // nothing to do for tuplets in voice 1
    }

    Fraction missing_elements_duration = ticks() * ratio() - elements_duration();
    if (missing_elements_duration.is_zero()) {
        return;
    }

    // first, fill in any holes in the middle of the tuplet
    Fraction expected_tick = elements().front()->tick();

    const std::vector<DurationElement *> elements_copy = elements(); // mofified during loop
    for (const DurationElement *de : elements_copy) {
        if (!de) {
            continue;
        }
        if (de->tick() != expected_tick) {
            missing_elements_duration -= add_missing_element(expected_tick, de->tick());
            if (missing_elements_duration.is_zero()) {
                return;
            }
        }
        expected_tick += de->actual_ticks();
    }

    // calculate the tick where we would expect a tuplet of this duration to start
    // TODO: check:
    expected_tick = elements().front()->tick() - Fraction::from_ticks(elements().front()->tick().ticks() % ticks().ticks());
    if (expected_tick != elements().front()->tick()) {
        // try to fill a hole at the beginning of the tuplet
        Fraction first_available_tick = measure()->tick();
        Segment *segment = measure()->find_segment(SegmentType::CHORD_REST, elements().front()->tick());
        ChordRest *prev_chord_rest = segment && segment->prev() ? segment->prev()->next_chord_rest(track(), true) : nullptr;
        if (prev_chord_rest && prev_chord_rest->measure() == measure()) {
            first_available_tick = prev_chord_rest->end_tick();
        }
        if (first_available_tick != elements().front()->tick()) {
            Fraction f = missing_elements_duration / ratio();
            Fraction ticks_required = f;
            Fraction end_tick = elements().front()->tick();
            Fraction start_tick = std::max(first_available_tick, end_tick - ticks_required);
            if (expected_tick > start_tick) {
                start_tick = expected_tick;
            }
            missing_elements_duration -= add_missing_element(start_tick, end_tick);
            if (missing_elements_duration.is_zero()) {
                return;
            }
        }
    }
    // now fill a hole at the end of the tuplet
    Fraction start_tick = elements().back()->end_tick();
    Fraction end_tick = elements().front()->tick() + ticks();
    // just to be safe, find the next ChordRest in the track, and adjust endTick if necessary
    Segment *segment = measure()->find_segment(SegmentType::CHORD_REST, elements().back()->tick());
    ChordRest *next_chord_rest = segment && segment->next() ? segment->next()->next_chord_rest(track(), false) : nullptr;
    if (next_chord_rest && next_chord_rest->tick() < end_tick) {
        end_tick = next_chord_rest->tick();
    }
    missing_elements_duration -= add_missing_element(start_tick, end_tick);
    if (!missing_elements_duration.is_zero()) {
        LOGD("Tuplet::addMissingElements(): still missing duration of %d/%d", missing_elements_duration.numerator(),
             missing_elements_duration.denominator());
    }
}

int Tuplet::compute_tuplet_denominator(int numerator, Fraction total_duration) {
    if (numerator <= 0) {
        return 0;
    }
    Fraction note_duration = total_duration / numerator;
    Fraction base_len(1, 1);
    Fraction ratio = (total_duration / base_len).reduced();
    while (ratio.denominator() != 1 || base_len / 2 >= note_duration) {
        base_len /= 2;
        ratio = (total_duration / base_len).reduced();
    }
    return ratio.numerator();
}

EngravingItem *Tuplet::next_element() {
    ChordRest *first_element = to_chord_rest(elements().front());
    if (first_element->is_chord()) {
        Chord *chord = to_chord(first_element);
        return chord->first_grace_or_note();
    }
    return first_element;
}

EngravingItem *Tuplet::prev_element() {
    ChordRest *first_element = to_chord_rest(elements().front());
    staff_idx_t staff_id = first_element->staff_idx();
    EngravingItem *prev_item = first_element->segment()->prev_element(staff_id);
    return prev_item;
}
} // namespace mu::engraving
