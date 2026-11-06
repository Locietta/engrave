/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited and others
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

#include "engraving/types/fraction.h"
#include "global/types/flags.h"

namespace mu::engraving {
struct TDuration;
} // namespace mu::engraving

namespace mu::iex::musicxml {
enum class MusicXmlStartStop : char;
enum class MusicXmlTupletFlag : char { NONE = 0, STOP_PREVIOUS = 1, START_NEW = 2, ADD_CHORD = 4, STOP_CURRENT = 8 };
typedef muse::Flags<MusicXmlTupletFlag> MusicXmlTupletFlags;

struct MusicXmlTupletState {
    MusicXmlTupletFlags determine_tuplet_action(const engraving::Fraction note_duration, const engraving::Fraction time_mod,
                                                const MusicXmlStartStop tuplet_start_stop, const engraving::TDuration normal_type,
                                                engraving::Fraction &missing_previous_duration,
                                                engraving::Fraction &missing_current_duration);
    static void determine_tuplet_fraction_and_full_duration(const engraving::Fraction duration, engraving::Fraction &fraction,
                                                            engraving::Fraction &full_duration);
    static engraving::Fraction missing_tuplet_duration(const engraving::Fraction duration);
private:
    void add_duration_to_tuplet(const engraving::Fraction duration, const engraving::Fraction time_mod);
    void smallest_type_and_count(const engraving::TDuration dur_type, int &type, int &count);
    void match_type_and_count(int &note_type, int &note_count);
    bool is_tuplet_filled(const engraving::TDuration normal_type, const engraving::Fraction time_mod);

    bool in_tuplet = false;
    bool implicit = false;
    int actual_notes = 1;
    int normal_notes = 1;
    engraving::Fraction duration{0, 1};
    int smallest_note_type = 0;  // smallest note type in the tuplet
    int smallest_note_count = 0; // number of smallest notes in the tuplet
};

using MusicXmlTupletStates = std::map<muse::String, MusicXmlTupletState>;
} // namespace mu::iex::musicxml
