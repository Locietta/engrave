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
class AccidentalState;

class Ornament final : public Articulation {
    OBJECT_ALLOCATOR(engraving, Ornament)
    DECLARE_CLASSOF(ElementType::ORNAMENT)

public:
    Ornament(DummyParentOr<ChordRest> parent);

    Ornament *clone() const override { return new Ornament(*this); }

    Ornament(const Ornament &o);
    ~Ornament();

    TranslatableString typeUserName() const override;

    static SymId fromTrillType(TrillType trillType);

    PropertyValue getProperty(Pid propertyId) const override;
    Sid getPropertyStyle(Pid propertyId) const override;
    PropertyValue propertyDefault(Pid id) const override;
    bool setProperty(Pid propertyId, const PropertyValue &v) override;

    bool hasIntervalAbove() const;
    bool hasIntervalBelow() const;
    bool hasFullIntervalChoice() const;
    bool showCueNote();

    void computeNotesAboveAndBelow(AccidentalState *accState);

    void scanElements(std::function<void(EngravingItem *)> func) override;

    void remove(EngravingItem *e) override;

    Chord *cueNoteChord() const { return cueNoteChord_; }

    void setCueNoteChord(Chord *c) { cueNoteChord_ = c; }

    void setNoteAbove(Note *n) { notesAboveAndBelow_[0] = n; }

    const auto &accidentalsAboveAndBelow() const { return accidentalsAboveAndBelow_; }

    Accidental *accidentalAbove() const { return accidentalsAboveAndBelow_[0]; }

    Accidental *accidentalBelow() const { return accidentalsAboveAndBelow_[1]; }

    void setAccidentalAbove(Accidental *a) { accidentalsAboveAndBelow_[0] = a; }

    void setAccidentalBelow(Accidental *a) { accidentalsAboveAndBelow_[1] = a; }

    void setTrillOldCompatAccidental(Accidental *a) { trillOldCompatAccidental_ = a; }

    Note *noteAbove() const { return notesAboveAndBelow_[0]; }

    Note *noteBelow() const { return notesAboveAndBelow_[1]; }

    void setTrack(track_idx_t val) override;

    void setShowCueNote(AutoOnOff show) { showCueNote_ = show; }

private:
    void updateAccidentalsAboveAndBelow();
    void updateCueNote();
    void mapOldTrillAccidental(Note *note, const Note *mainNote);
    void manageAccidentalVisibilityRules(Note *note);

private:
    M_PROPERTY(OrnamentInterval, intervalAbove, setIntervalAbove)
    M_PROPERTY(OrnamentInterval, intervalBelow, setIntervalBelow)
    M_PROPERTY(OrnamentShowAccidental, showAccidental, setShowAccidental)
    M_PROPERTY(bool, startOnUpperNote, setStartOnUpperNote)

    std::array<Note *, 2> notesAboveAndBelow_{nullptr, nullptr}; // [0] = above, [1] = below

    std::array<Accidental *, 2> accidentalsAboveAndBelow_{nullptr, nullptr}; // [0] = above, [1] = below

    Chord *cueNoteChord_ = nullptr;
    AutoOnOff showCueNote_ = AutoOnOff::AUTO;

    Accidental *trillOldCompatAccidental_ = nullptr; // used temporarily to map old (i.e. pre-4.1) trill accidentals
};
} // namespace mu::engraving
