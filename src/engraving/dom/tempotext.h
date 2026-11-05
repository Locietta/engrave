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

#ifndef MU_ENGRAVING_TEMPOTEXT_H
#define MU_ENGRAVING_TEMPOTEXT_H

#include "durationtype.h"
#include "textbase.h"

namespace mu::engraving {
enum class TempoTextType : signed char {
    NORMAL,
    A_TEMPO,
    TEMPO_PRIMO,
};

//-------------------------------------------------------------------
//   @@ TempoText
///    Tempo marker which determines the midi tempo.
//
//   @P tempo       float     tempo in quarter notes (crochets) per second
//   @P followText  bool      determine tempo from text
//-------------------------------------------------------------------

class TempoText final : public TextBase {
    OBJECT_ALLOCATOR(engraving, TempoText)
    DECLARE_CLASSOF(ElementType::TEMPO_TEXT)

public:
    TempoText(DummyParentOr<Segment> parent);

    TempoText *clone() const override { return new TempoText(*this); }

    Segment *segment() const { return toSegment(ownershipParent()); }

    Measure *measure() const { return toMeasure(ownershipParent()->ownershipParent()); }

    TempoTextType tempoTextType() const { return tempoTextType_; }

    void setTempoTextType(TempoTextType);

    BeatsPerSecond tempo() const;
    double tempoBpm() const;
    void setTempo(BeatsPerSecond v);

    bool isRelative() const { return isRelative_; }

    void setRelative(double v) {
        isRelative_ = true;
        relative_ = v;
    }

    double relativeValue() const { return relative_; }

    bool isNormal() const { return tempoTextType_ == TempoTextType::NORMAL; }

    void setNormal() { setTempoTextType(TempoTextType::NORMAL); }

    bool isATempo() const { return tempoTextType_ == TempoTextType::A_TEMPO; }

    void setATempo() { setTempoTextType(TempoTextType::A_TEMPO); }

    bool isTempoPrimo() const { return tempoTextType_ == TempoTextType::TEMPO_PRIMO; }

    void setTempoPrimo() { setTempoTextType(TempoTextType::TEMPO_PRIMO); }

    bool playTempoText() const { return playTempoText_; }

    void setPlayTempoText(bool v) { playTempoText_ = v; }

    bool followText() const { return followText_; }

    void setFollowText(bool v) { followText_ = v; }

    void updateTempo();

    TDuration duration() const;

    static int findTempoDuration(const String &s, int &len, TDuration &dur);
    static String duration2tempoTextString(const TDuration dur);
    static String duration2userName(const TDuration t);

    PropertyValue getProperty(Pid propertyId) const override;
    bool setProperty(Pid propertyId, const PropertyValue &) override;
    PropertyValue propertyDefault(Pid id) const override;

    bool positionRelativeToNoteheadRest() const override { return true; }

    String tempoInfo() const;

    static constexpr double DEFAULT_SYM_SIZE_RATIO = 5.0 / 3.0;

protected:
    void commitText() override;

    void undoChangeProperty(Pid id, const PropertyValue &, PropertyFlags ps) override;

    TempoTextType tempoTextType_;
    BeatsPerSecond tempo_;    // beats per second
    bool followText_ = false; // parse text to determine tempo
    bool playTempoText_ = true;
    double relative_ = 0.0;
    bool isRelative_ = false;
    bool alignRightOfRehearsalMark_ = true;
};
} // namespace mu::engraving
#endif
