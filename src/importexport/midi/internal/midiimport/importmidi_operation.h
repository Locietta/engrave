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
#ifndef IMPORTMIDI_OPERATION_H
#define IMPORTMIDI_OPERATION_H

#include <set>

#include "importmidi_fraction.h"

namespace mu::iex::midi {
// all enums below should have default indexes like 0, 1, 2...
// text names for enum items are in TracksModel class

namespace MidiOperations {
enum class QuantValue { Q_4 = 0, Q_8, Q_16, Q_32, Q_64, Q_128, Q_256, Q_512, Q_1024, Q_INVALID = -1 };

enum class VoiceCount { V_1 = 0, V_2, V_3, V_4 };

enum class Swing { NONE = 0, SWING, SHUFFLE };

enum class TimeSigNumerator { TWO = 0, THREE, FOUR, FIVE, SIX, SEVEN, NINE, TWELVE, FIFTEEN, TWENTY_ONE };

enum class TimeSigDenominator { TWO = 0, FOUR, EIGHT, SIXTEEN, THIRTY_TWO };
} // namespace MidiOperations
} // namespace mu::iex::midi

#endif // IMPORTMIDI_OPERATION_H
