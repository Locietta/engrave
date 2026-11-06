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

#include "engraving/dom/durationtype.h"
#include "engraving/types/fraction.h"
#include "serialization/xmlstreamreader.h"

namespace mu::iex::musicxml {
struct MusicXmlLogger;
struct MusicXmlParserPass1;

//---------------------------------------------------------
//   MusicXmlNoteDuration
//---------------------------------------------------------

/**
 Parse the note time related part of the /score-partwise/part/measure/note node.
 */

struct MusicXmlNoteDuration {
    MusicXmlNoteDuration(int divs, MusicXmlLogger *logger, MusicXmlParserPass1 *pass1)
        : divs_(divs), pass1_(pass1), logger_(logger) { /* nothing so far */ }

    muse::String check_timing(const muse::String &type, const bool rest, const bool grace);

    engraving::Fraction duration() const { return dura_; } // duration to use

    engraving::Fraction calculated_duration() const { return calc_dura_; } // value calculated from note type etcetera

    engraving::Fraction specified_duration() const { return spec_dura_; } // value read from the duration element

    int dots() const { return dots_; }

    engraving::TDuration normal_type() const { return normal_type_; }

    bool read_properties(muse::XmlStreamReader &e);

    engraving::Fraction time_mod() const { return time_mod_; }

private:
    void duration(muse::XmlStreamReader &e);
    void time_modification(muse::XmlStreamReader &e);
    const int divs_; // the current divisions value
    int dots_ = 0;
    engraving::Fraction calc_dura_;
    engraving::Fraction spec_dura_;
    engraving::Fraction dura_;
    engraving::TDuration normal_type_;
    engraving::Fraction time_mod_{1, 1}; // default to no time modification
    MusicXmlParserPass1 *pass1_ = nullptr;
    MusicXmlLogger *logger_ = nullptr; ///< Error logger
};
} // namespace mu::iex::musicxml
