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

#include "ornament.h"

#include "accidental.h"
#include "chord.h"
#include "editing/editchord.h"
#include "editing/editparentheses.h"
#include "editing/transaction/transaction.h"
#include "editing/transpose.h"
#include "engravingitem.h"
#include "factory.h"
#include "key.h"
#include "note.h"
#include "score.h"
#include "shape.h"
#include "staff.h"
#include "utils.h"

using namespace mu::engraving;

Ornament::Ornament(DummyParentOr<ChordRest> parent) : Articulation(parent, ElementType::ORNAMENT) {
    interval_above_value = ::mu::engraving::default_ornament_interval;
    interval_below_value = ::mu::engraving::default_ornament_interval;
    show_accidental_value = OrnamentShowAccidental::DEFAULT;
    start_on_upper_note_value = false;
}

Ornament::Ornament(const Ornament &o) : Articulation(o) {
    interval_above_value = o.interval_above_value;
    interval_below_value = o.interval_below_value;
    show_accidental_value = o.show_accidental_value;
    start_on_upper_note_value = o.start_on_upper_note_value;
    show_cue_note_ = o.show_cue_note_;

    if (o.cue_note_chord_) {
        cue_note_chord_ = o.cue_note_chord_->clone();
    }

    for (size_t i = 0; i < accidentals_above_and_below_.size(); ++i) {
        Accidental *old_accidental = o.accidentals_above_and_below_[i];
        if (!old_accidental) {
            continue;
        }
        Accidental *new_accidental = old_accidental->clone();
        new_accidental->set_ownership_parent(this);
        accidentals_above_and_below_[i] = new_accidental;
    }
}

Ornament::~Ornament() {
    std::fill(std::begin(notes_above_and_below_), std::end(notes_above_and_below_), nullptr);
    std::fill(std::begin(accidentals_above_and_below_), std::end(accidentals_above_and_below_), nullptr);

    if (cue_note_chord_ && cue_note_chord_->notes().size()) {
        cue_note_chord_->notes().clear();
    }

    cue_note_chord_ = nullptr;
}

void Ornament::remove(EngravingItem *e) {
    if (e->is_accidental()) {
        for (Accidental *&acc : accidentals_above_and_below_) {
            if (e == acc) {
                acc = nullptr;
                e->removed();
            }
        }
    }
}

muse::TranslatableString Ornament::type_user_name() const {
    if (text_type() != ArticulationTextType::NO_TEXT) {
        return TranslatableString("engraving", "Ornament text");
    }

    return TranslatableString("engraving", "Ornament");
}

void Ornament::set_track(track_idx_t val) {
    for (Note *note : notes_above_and_below_) {
        if (note) {
            note->set_track(val);
        }
    }
    if (cue_note_chord_) {
        cue_note_chord_->set_track(val);
    }
    track_ = val;
}

void Ornament::scan_elements(std::function<void(EngravingItem *)> func) {
    func(this);
    for (Accidental *accidental : accidentals_above_and_below_) {
        if (accidental) {
            func(accidental);
        }
    }
    if (cue_note_chord_) {
        cue_note_chord_->scan_elements(func);
    }
}

PropertyValue Ornament::get_property(Pid property_id) const {
    switch (property_id) {
        case Pid::INTERVAL_ABOVE: return interval_above_value;
        case Pid::INTERVAL_BELOW: return interval_below_value;
        case Pid::ORNAMENT_SHOW_ACCIDENTAL: return show_accidental_value;
        case Pid::ORNAMENT_SHOW_CUE_NOTE: return show_cue_note_;
        case Pid::START_ON_UPPER_NOTE: return start_on_upper_note_value;
        default: return Articulation::get_property(property_id);
    }
}

Sid Ornament::get_property_style(Pid property_id) const {
    switch (property_id) {
        case Pid::ARTICULATION_ANCHOR: return Sid::ARTICULATION_ANCHOR_DEFAULT;
        default: return Articulation::get_property_style(property_id);
    }
}

PropertyValue Ornament::property_default(Pid id) const {
    switch (id) {
        case Pid::INTERVAL_ABOVE:
        case Pid::INTERVAL_BELOW: return ::mu::engraving::default_ornament_interval;
        case Pid::ORNAMENT_SHOW_ACCIDENTAL: return OrnamentShowAccidental::DEFAULT;
        case Pid::ORNAMENT_SHOW_CUE_NOTE: return AutoOnOff::AUTO;
        case Pid::START_ON_UPPER_NOTE: return false;
        case Pid::ARTICULATION_ANCHOR: return static_cast<int>(ArticulationAnchor::AUTO);
        default: return Articulation::property_default(id);
    }
}

bool Ornament::set_property(Pid property_id, const PropertyValue &v) {
    switch (property_id) {
        case Pid::INTERVAL_ABOVE: set_interval_above(v.value<OrnamentInterval>()); break;
        case Pid::INTERVAL_BELOW: set_interval_below(v.value<OrnamentInterval>()); break;
        case Pid::ORNAMENT_SHOW_ACCIDENTAL: set_show_accidental(v.value<OrnamentShowAccidental>()); break;
        case Pid::ORNAMENT_SHOW_CUE_NOTE: set_show_cue_note(v.value<AutoOnOff>()); break;
        case Pid::START_ON_UPPER_NOTE: set_start_on_upper_note(v.to_bool()); break;
        default: return Articulation::set_property(property_id, v);
    }
    trigger_layout();
    return true;
}

bool Ornament::has_interval_above() const {
    SymId id = sym_id();
    return id == SymId::ORNAMENT_TURN_INVERTED || id == SymId::ORNAMENT_TRILL || id == SymId::ORNAMENT_TURN_SLASH ||
           id == SymId::ORNAMENT_TURN || id == SymId::ORNAMENT_SHORT_TRILL || id == SymId::ORNAMENT_TREMBLEMENT ||
           id == SymId::ORNAMENT_PRALL_MORDENT || id == SymId::ORNAMENT_UP_PRALL || id == SymId::ORNAMENT_PRECOMP_MORDENT_UPPER_PREFIX ||
           id == SymId::ORNAMENT_UP_MORDENT || id == SymId::ORNAMENT_PRALL_UP;
}

bool Ornament::has_interval_below() const {
    SymId id = sym_id();
    return id == SymId::ORNAMENT_TURN_INVERTED || id == SymId::ORNAMENT_MORDENT || id == SymId::ORNAMENT_TURN_SLASH ||
           id == SymId::ORNAMENT_TURN || id == SymId::ORNAMENT_TREMBLEMENT || id == SymId::ORNAMENT_PRALL_MORDENT ||
           id == SymId::ORNAMENT_DOWN_MORDENT || id == SymId::ORNAMENT_PRALL_DOWN || id == SymId::ORNAMENT_PRALL_MORDENT ||
           id == SymId::ORNAMENT_PRALL_MORDENT;
}

bool Ornament::has_full_interval_choice() const {
    SymId id = sym_id();
    return id == SymId::ORNAMENT_TRILL;
}

bool Ornament::show_cue_note() {
    if (show_cue_note_ == AutoOnOff::AUTO) {
        return (has_full_interval_choice() && style().style_b(Sid::TRILL_ALWAYS_SHOW_CUE_NOTE)) ||
               interval_above_value.step != IntervalStep::SECOND;
    }

    return show_cue_note_ == AutoOnOff::ON;
}

void Ornament::compute_notes_above_and_below(AccidentalState *acc_state) {
    ChordRest *parent_chord_rest = chord_rest();
    Chord *parent_chord = parent_chord_rest && parent_chord_rest->is_chord() ? to_chord(parent_chord_rest) : nullptr;

    const Note *main_note = parent_chord ? parent_chord->up_note() : nullptr;

    if (!main_note) {
        return;
    }

    if (cue_note_chord_ && !cue_note_chord_->ownership_parent()) {
        cue_note_chord_->set_ownership_parent(parent_chord->segment());
    }

    for (size_t i = 0; i < notes_above_and_below_.size(); ++i) {
        bool above = (i == 0);
        bool has_int_above = has_interval_above();
        bool has_int_below = has_interval_below();

        if ((above && !has_int_above) || (!above && !has_int_below)) {
            continue;
        }

        Note *&note = notes_above_and_below_.at(i);
        if (!note && above && cue_note_chord_) {
            note = cue_note_chord_->up_note();
        }

        if (!note) {
            note = main_note->clone();
            Tie *tie = note->tie_for();
            if (tie) {
                score()->undo_remove_element((EngravingItem *) tie);
            }
        } else {
            note->set_tpc1(main_note->tpc1());
            note->set_tpc2(main_note->tpc2());
            note->set_pitch(main_note->pitch());
        }
        note->set_track(track());

        if (Accidental::is_microtonal(note->accidental_type())) {
            // If mainNote has microtonal accidental, don't clone it to the ornament note because microtonal intervals are not supported.
            note->set_accidental_type(Accidental::value2subtype(tpc2alter(note->tpc())));
        }

        bool auto_mode =
            (above && interval_above_value.type == IntervalType::AUTO) || (!above && interval_below_value.type == IntervalType::AUTO);
        if (auto_mode) {
            // NOTE: In AUTO mode, the ornament note should match not only any alteration from the
            // key signature, but also any alteration present in the measure before this point.
            int interval_steps = above ? static_cast<int>(interval_above_value.step) : -static_cast<int>(interval_below_value.step);
            note->transpose_diatonic(interval_steps, false, true);
            if (trill_old_compat_accidental_) {
                map_old_trill_accidental(note, main_note);
                trill_old_compat_accidental_ = nullptr;
            } else {
                int pitch_line = abs_step(note->tpc(), note->epitch());
                AccidentalVal accidental_val = acc_state->accidental_val(pitch_line);
                AccidentalVal note_accidental_val = tpc2alter(note->tpc());
                int accidental_diff = static_cast<int>(accidental_val) - static_cast<int>(note_accidental_val);
                note->transpose(Interval(0, accidental_diff), true);
            }
        } else {
            Interval interval = Interval::from_ornament_interval(above ? interval_above_value : interval_below_value);
            if (!above) {
                interval.flip();
            }
            note->transpose(interval, true);
        }

        AccidentalState copy_of_acc_state = *acc_state;
        note->update_accidental(&copy_of_acc_state);
        if (note->accidental()) {
            int pitch_line = abs_step(note->tpc(), note->epitch());
            acc_state->set_force_restate_accidental(pitch_line, true);
        }

        manage_accidental_visibility_rules(note);
    }

    update_accidentals_above_and_below();
    update_cue_note();
}

void Ornament::manage_accidental_visibility_rules(Note *note) {
    if (show_accidental_value == OrnamentShowAccidental::DEFAULT) {
        return;
    }

    AccidentalVal accidental_value = tpc2alter(note->tpc());
    AccidentalType accidental_type = Accidental::value2subtype(accidental_value);
    if (accidental_type == AccidentalType::NONE) {
        accidental_type = AccidentalType::NATURAL;
    }
    if (note->accidental_type() == accidental_type) {
        return;
    }

    bool show = false;
    if (show_accidental_value == OrnamentShowAccidental::ALWAYS) {
        show = true;
    } else if (show_accidental_value == OrnamentShowAccidental::ANY_ALTERATION) {
        Key key = staff()->key(tick());
        AccidentalState unaltered_state;
        unaltered_state.init(key);
        int pitch_line = abs_step(note->tpc(), note->epitch() + note->ottave_capo_fret());
        AccidentalVal unaltered_accidental_val = unaltered_state.accidental_val(pitch_line);
        show = accidental_value != unaltered_accidental_val;
    }

    if (show) {
        note->set_accidental_type(accidental_type);
        note->accidental()->set_role(AccidentalRole::AUTO);
    }
}

void Ornament::update_accidentals_above_and_below() {
    for (size_t i = 0; i < notes_above_and_below_.size(); ++i) {
        Note *note = notes_above_and_below_[i];
        Accidental *accidental = note ? note->accidental() : nullptr;
        Accidental *&cur_accidental = accidentals_above_and_below_[i];
        if (show_cue_note() || !accidental) {
            if (cur_accidental) {
                delete cur_accidental;
                cur_accidental = nullptr;
            }
            continue;
        }
        if (accidental) {
            if (!cur_accidental) {
                cur_accidental = accidental->clone();
                cur_accidental->set_ownership_parent(this);
                cur_accidental->set_placement(i == 0 ? PlacementV::ABOVE : PlacementV::BELOW);
            } else {
                cur_accidental->set_accidental_type(accidental->accidental_type());
                cur_accidental->set_role(accidental->role());
            }
        }
    }
}

void Ornament::update_cue_note() {
    ChordRest *parent_chord_rest = chord_rest();
    Chord *parent_chord = parent_chord_rest && parent_chord_rest->is_chord() ? to_chord(parent_chord_rest) : nullptr;

    if (!show_cue_note()) {
        if (note_above() && ownership_parent()) {
            note_above()->set_ownership_parent(parent_chord);
        }
        if (cue_note_chord_) {
            cue_note_chord_->notes().clear();
            delete cue_note_chord_;
            cue_note_chord_ = nullptr;
        }
        return;
    }

    if (!parent_chord) {
        return;
    }

    Note *cue_note = note_above();
    // If needed, create cue note
    if (!cue_note_chord_) {
        cue_note_chord_ = Factory::create_chord(parent_chord->segment());
        cue_note_chord_->set_small(true);
        cue_note->set_head_type(NoteHeadType::HEAD_QUARTER);
        cue_note_chord_->add(cue_note);
        cue_note->set_ownership_parent(cue_note_chord_);

        Transaction &tx = score()->transaction_manager()->current_or_dummy_transaction();
        EditParentheses::add_parentheses_to_notes(tx, {cue_note});
    }
    cue_note_chord_->set_track(track());
    cue_note_chord_->set_ownership_parent(parent_chord->segment());
    cue_note_chord_->set_staff_move(parent_chord->staff_move());
    cue_note->update_line();
    cue_note->set_is_trill_cue_note(true);
}

SymId Ornament::from_trill_type(TrillType trill_type) {
    switch (trill_type) {
        case TrillType::TRILL_LINE: return SymId::ORNAMENT_TRILL;
        case TrillType::UPPRALL_LINE: return SymId::ORNAMENT_UP_PRALL;
        case TrillType::DOWNPRALL_LINE: return SymId::ORNAMENT_DOWN_MORDENT;
        case TrillType::PRALLPRALL_LINE: return SymId::ORNAMENT_TRILL;
        default: return SymId::NO_SYM;
    }
}

void Ornament::map_old_trill_accidental(Note *note, const Note *main_note) {
    // Compatibility with trills pre-4.1
    AccidentalVal old_compat_value = Accidental::subtype2value(trill_old_compat_accidental_->accidental_type());
    AccidentalVal note_accidental_val = tpc2alter(note->tpc());
    int accidental_diff = static_cast<int>(old_compat_value) - static_cast<int>(note_accidental_val);
    note->transpose(Interval(0, accidental_diff), true);
    int semitones = note->pitch() - main_note->pitch();
    switch (semitones) {
        case 0: interval_above_value.type = IntervalType::DIMINISHED; break;
        case 1: interval_above_value.type = IntervalType::MINOR; break;
        case 2: interval_above_value.type = IntervalType::MAJOR; break;
        case 3: interval_above_value.type = IntervalType::AUGMENTED; break;
        default: break;
    }
}
