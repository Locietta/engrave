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

#include "serialization/xmlstreamreader.h"

#include "engraving/dom/accidental.h"

namespace mu::iex::musicxml {
class MusicXmlLogger;

//---------------------------------------------------------
//   musicXmlNotePitch
//---------------------------------------------------------

/**
 Parse the note pitch related part of the /score-partwise/part/measure/note node.
 */

class MusicXmlNotePitch {
public:
    MusicXmlNotePitch(MusicXmlLogger *logger) : logger_(logger) { /* nothing so far */ }

    void pitch(muse::XmlStreamReader &e);
    bool readProperties(muse::XmlStreamReader &e, engraving::Score *score);

    engraving::Accidental *acc() const { return acc_; }

    engraving::AccidentalType accType() const { return accType_; }

    int alter() const { return alter_; }

    double tuning() const { return tuning_; }

    int displayOctave() const { return displayOctave_; }

    int displayStep() const { return displayStep_; }

    void displayStepOctave(muse::XmlStreamReader &e);

    int octave() const { return octave_; }

    int step() const { return step_; }

    bool unpitched() const { return unpitched_; }

private:
    engraving::Accidental *acc_ = nullptr;                                // created based on accidental element
    engraving::AccidentalType accType_ = engraving::AccidentalType::NONE; // set by pitch() based on alter value (can be microtonal)
    int alter_ = 0;
    double tuning_ = 0.0;
    int displayStep_ = -1;   // invalid
    int displayOctave_ = -1; // invalid
    int octave_ = -1;
    int step_ = 0;
    bool unpitched_ = false;
    MusicXmlLogger *logger_ = nullptr; // Error logger
};
} // namespace mu::iex::musicxml
