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

#include "articulation.h"

namespace mu::engraving {
struct AccidentalState;

struct Ornament final : Articulation {
private:
    OBJECT_ALLOCATOR(engraving, Ornament)
    DECLARE_CLASSOF(ElementType::ORNAMENT)

public:
    Ornament(DummyParentOr<ChordRest> parent);

    Ornament *clone() const override { return new Ornament(*this); }

    Ornament(const Ornament &o);
    ~Ornament();

    TranslatableString type_user_name() const override;

    static SymId from_trill_type(TrillType trill_type);

    PropertyValue get_property(Pid property_id) const override;
    Sid get_property_style(Pid property_id) const override;
    PropertyValue property_default(Pid id) const override;
    bool set_property(Pid property_id, const PropertyValue &v) override;

    bool has_interval_above() const;
    bool has_interval_below() const;
    bool has_full_interval_choice() const;
    bool show_cue_note();

    void compute_notes_above_and_below(AccidentalState *acc_state);

    void scan_elements(std::function<void(EngravingItem *)> func) override;

    void remove(EngravingItem *e) override;

    Chord *cue_note_chord() const { return cue_note_chord_; }

    void set_cue_note_chord(Chord *c) { cue_note_chord_ = c; }

    void set_note_above(Note *n) { notes_above_and_below_[0] = n; }

    const auto &accidentals_above_and_below() const { return accidentals_above_and_below_; }

    Accidental *accidental_above() const { return accidentals_above_and_below_[0]; }

    Accidental *accidental_below() const { return accidentals_above_and_below_[1]; }

    void set_accidental_above(Accidental *a) { accidentals_above_and_below_[0] = a; }

    void set_accidental_below(Accidental *a) { accidentals_above_and_below_[1] = a; }

    void set_trill_old_compat_accidental(Accidental *a) { trill_old_compat_accidental_ = a; }

    Note *note_above() const { return notes_above_and_below_[0]; }

    Note *note_below() const { return notes_above_and_below_[1]; }

    void set_track(track_idx_t val) override;

    void set_show_cue_note(AutoOnOff show) { show_cue_note_ = show; }

private:
    void update_accidentals_above_and_below();
    void update_cue_note();
    void map_old_trill_accidental(Note *note, const Note *main_note);
    void manage_accidental_visibility_rules(Note *note);

private:
    M_PROPERTY(OrnamentInterval, interval_above, set_interval_above)
    M_PROPERTY(OrnamentInterval, interval_below, set_interval_below)
    M_PROPERTY(OrnamentShowAccidental, show_accidental, set_show_accidental)
    M_PROPERTY(bool, start_on_upper_note, set_start_on_upper_note)

    std::array<Note *, 2> notes_above_and_below_{nullptr, nullptr}; // [0] = above, [1] = below

    std::array<Accidental *, 2> accidentals_above_and_below_{nullptr, nullptr}; // [0] = above, [1] = below

    Chord *cue_note_chord_ = nullptr;
    AutoOnOff show_cue_note_ = AutoOnOff::AUTO;

    Accidental *trill_old_compat_accidental_ = nullptr; // used temporarily to map old (i.e. pre-4.1) trill accidentals
};
} // namespace mu::engraving
