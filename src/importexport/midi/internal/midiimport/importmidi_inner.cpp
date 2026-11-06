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
#include "importmidi_inner.h"

#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

#include "engraving/dom/durationtype.h"
#include "engraving/dom/sig.h"
#include "global/types/string.h"
#include "importmidi_chord.h"
#include "importmidi_operations.h"
#include "log.h"

namespace mu::iex::midi {
MTrack::MTrack(const MTrack &other)
    : program(other.program), staff(other.staff), mtrack(other.mtrack), name(other.name), has_key(other.has_key),
      index_of_operation(other.index_of_operation), division(other.division), is_division_in_tps(other.is_division_in_tps),
      had_initial_notes(other.had_initial_notes), volumes(other.volumes), chords(other.chords) {
    update_tuplets_from_chords();
}

MTrack &MTrack::operator=(MTrack other) {
    std::swap(program, other.program);
    std::swap(staff, other.staff);
    std::swap(mtrack, other.mtrack);
    std::swap(name, other.name);
    std::swap(has_key, other.has_key);
    std::swap(index_of_operation, other.index_of_operation);
    std::swap(division, other.division);
    std::swap(is_division_in_tps, other.is_division_in_tps);
    std::swap(had_initial_notes, other.had_initial_notes);
    std::swap(volumes, other.volumes);
    std::swap(chords, other.chords);
    update_tuplets_from_chords();

    return *this;
}

// chords of the MTrack are considered to be created/copied already;
// tuplets here are extracted and inserted to the tuplet map of the MTrack
// from tuplet iterators in chords/notes
void MTrack::update_tuplets_from_chords() {
    for (auto &chord : chords) {
        if (chord.second.is_in_tuplet) {
            update_tuplet(chord.second.tuplet);
        }
        for (auto &note : chord.second.notes) {
            if (note.is_in_tuplet) {
                update_tuplet(note.tuplet);
            }
        }
    }
}

void MTrack::update_tuplet(std::multimap<ReducedFraction, MidiTuplet::TupletData>::iterator &tuplet_it) {
    auto found_tuplet = tuplets.end();
    const auto found_tuplets_range = tuplets.equal_range(tuplet_it->first);
    for (auto it = found_tuplets_range.first; it != found_tuplets_range.second; ++it) {
        if (it->second.voice == tuplet_it->second.voice) {
            found_tuplet = it;
            break;
        }
    }
    if (found_tuplet != tuplets.end()) {
        tuplet_it = found_tuplet;
    } else {
        tuplet_it = tuplets.insert({tuplet_it->first, tuplet_it->second});
    }
}

namespace Meter {
ReducedFraction user_time_sig_to_fraction(MidiOperations::TimeSigNumerator time_sig_numerator,
                                          MidiOperations::TimeSigDenominator time_sig_denominator) {
    int numerator = 4;
    int denominator = 4;

    switch (time_sig_numerator) {
        case MidiOperations::TimeSigNumerator::TWO: numerator = 2; break;
        case MidiOperations::TimeSigNumerator::THREE: numerator = 3; break;
        case MidiOperations::TimeSigNumerator::FOUR: numerator = 4; break;
        case MidiOperations::TimeSigNumerator::FIVE: numerator = 5; break;
        case MidiOperations::TimeSigNumerator::SIX: numerator = 6; break;
        case MidiOperations::TimeSigNumerator::SEVEN: numerator = 7; break;
        case MidiOperations::TimeSigNumerator::NINE: numerator = 9; break;
        case MidiOperations::TimeSigNumerator::TWELVE: numerator = 12; break;
        case MidiOperations::TimeSigNumerator::FIFTEEN: numerator = 15; break;
        case MidiOperations::TimeSigNumerator::TWENTY_ONE: numerator = 21; break;
        default: break;
    }

    switch (time_sig_denominator) {
        case MidiOperations::TimeSigDenominator::TWO: denominator = 2; break;
        case MidiOperations::TimeSigDenominator::FOUR: denominator = 4; break;
        case MidiOperations::TimeSigDenominator::EIGHT: denominator = 8; break;
        case MidiOperations::TimeSigDenominator::SIXTEEN: denominator = 16; break;
        case MidiOperations::TimeSigDenominator::THIRTY_TWO: denominator = 32; break;
        default: break;
    }

    return ReducedFraction(numerator, denominator);
}

MidiOperations::TimeSigNumerator fraction_numerator_to_user_value(int n) {
    MidiOperations::TimeSigNumerator numerator = MidiOperations::TimeSigNumerator::FOUR;

    if (n == 2) {
        numerator = MidiOperations::TimeSigNumerator::TWO;
    } else if (n == 3) {
        numerator = MidiOperations::TimeSigNumerator::THREE;
    } else if (n == 4) {
        numerator = MidiOperations::TimeSigNumerator::FOUR;
    } else if (n == 5) {
        numerator = MidiOperations::TimeSigNumerator::FIVE;
    } else if (n == 6) {
        numerator = MidiOperations::TimeSigNumerator::SIX;
    } else if (n == 7) {
        numerator = MidiOperations::TimeSigNumerator::SEVEN;
    } else if (n == 9) {
        numerator = MidiOperations::TimeSigNumerator::NINE;
    } else if (n == 12) {
        numerator = MidiOperations::TimeSigNumerator::TWELVE;
    } else if (n == 15) {
        numerator = MidiOperations::TimeSigNumerator::FIFTEEN;
    } else if (n == 21) {
        numerator = MidiOperations::TimeSigNumerator::TWENTY_ONE;
    } else {
        Q_ASSERT_X(false, "Meter::fractionNumeratorToUserValue", "Unknown numerator");
    }

    return numerator;
}

MidiOperations::TimeSigDenominator fraction_denominator_to_user_value(int z) {
    MidiOperations::TimeSigDenominator denominator = MidiOperations::TimeSigDenominator::FOUR;

    if (z == 2) {
        denominator = MidiOperations::TimeSigDenominator::TWO;
    } else if (z == 4) {
        denominator = MidiOperations::TimeSigDenominator::FOUR;
    } else if (z == 8) {
        denominator = MidiOperations::TimeSigDenominator::EIGHT;
    } else if (z == 16) {
        denominator = MidiOperations::TimeSigDenominator::SIXTEEN;
    } else if (z == 32) {
        denominator = MidiOperations::TimeSigDenominator::THIRTY_TWO;
    } else {
        Q_ASSERT_X(false, "Meter::fractionDenominatorToUserValue", "Unknown denominator");
    }

    return denominator;
}
} // namespace Meter

namespace MidiTuplet {
bool have_intersection(const std::pair<ReducedFraction, ReducedFraction> &interval1,
                       const std::pair<ReducedFraction, ReducedFraction> &interval2, bool strict_comparison) {
    if (strict_comparison) {
        return interval1.second > interval2.first && interval1.first < interval2.second;
    }
    return interval1.second >= interval2.first && interval1.first <= interval2.second;
}

bool have_intersection(const std::pair<ReducedFraction, ReducedFraction> &interval,
                       const std::vector<std::pair<ReducedFraction, ReducedFraction>> &intervals, bool strict_comparison) {
    for (const auto &i : intervals) {
        if (have_intersection(i, interval, strict_comparison)) {
            return true;
        }
    }
    return false;
}
} // namespace MidiTuplet

namespace MidiCharset {
namespace {
// Windows-1252 differs from ISO-8859-1 only in 0x80..0x9F.  Karaoke and MIDI
// files written on Windows commonly use this range (smart quotes, dashes,
// ellipsis, trademark), so decoding it as Latin-1 would silently corrupt the
// lyric text.  Undefined bytes are preserved as the corresponding C1 control
// code point instead of being dropped.
char16_t decode_windows1252_byte(unsigned char byte) {
    if (byte >= 0x80 && byte <= 0x9F) {
        static constexpr char16_t map[32] = {0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                                             0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
                                             0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178};
        return map[byte - 0x80];
    }
    return static_cast<char16_t>(byte);
}

muse::String from_single_byte(const std::string &text, bool windows1252) {
    std::u16string utf16;
    utf16.reserve(text.size());
    for (const char ch : text) {
        const auto byte = static_cast<unsigned char>(ch);
        utf16.push_back(windows1252 ? decode_windows1252_byte(byte) : static_cast<char16_t>(byte));
    }

    muse::String result;
    result.reserve(utf16.size());
    for (const char16_t ch : utf16) {
        result.append(muse::Char(ch));
    }
    return result;
}

std::string normalized_charset_name(const std::string &charset) {
    std::string normalized;
    normalized.reserve(charset.size());
    for (const char ch : charset) {
        if (ch == ' ' || ch == '_') {
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

muse::String decode_with_fallback(const std::string &text, const std::string &charset) {
    const std::string normalized = normalized_charset_name(charset);

    if (normalized.empty() || normalized == "utf-8" || normalized == "utf8") {
        if (muse::UtfCodec::is_valid_utf8(text)) {
            return muse::String::from_utf8(text);
        }
        // MIDI meta events have no encoding marker.  UTF-8 is the declared
        // default, but a byte sequence that is not valid UTF-8 is far more
        // likely to be Windows-1252 text than a file that should be rejected.
        LOGW() << "MIDI import: meta text is not valid UTF-8; falling back to Windows-1252";
        return from_single_byte(text, true);
    }

    if (normalized == "windows-1252" || normalized == "cp1252" || normalized == "win1252") {
        return from_single_byte(text, true);
    }
    if (normalized == "iso-8859-1" || normalized == "iso8859-1" || normalized == "latin1" || normalized == "latin-1") {
        return from_single_byte(text, false);
    }

    // A real multi-codec backend (ICU or libiconv) is required for the full
    // QTextCodec surface.  Until then, never guess an arbitrary encoding:
    // prefer strict UTF-8 and otherwise preserve Latin-1/Windows-1252 bytes.
    LOGW() << "MIDI import: unsupported charset" << charset << "in the Qt-free build; using UTF-8/Windows-1252 fallback";
    if (muse::UtfCodec::is_valid_utf8(text)) {
        return muse::String::from_utf8(text);
    }
    return from_single_byte(text, true);
}
} // namespace

muse::String convert_to_charset(const std::string &text) {
    const auto *data = midi_import_operations.data();
    return decode_with_fallback(text, data ? data->charset : default_charset());
}

std::string default_charset() { return "UTF-8"; }

std::string from_uchar(const unsigned char *text) { return text ? std::string(reinterpret_cast<const char *>(text)) : std::string(); }
} // namespace MidiCharset

namespace MidiBar {
ReducedFraction find_bar_start(const ReducedFraction &time, const engraving::TimeSigMap *sigmap) {
    int bar_index, beat, tick;
    sigmap->tick_values(time.ticks(), &bar_index, &beat, &tick);
    return ReducedFraction::from_ticks(sigmap->bar2tick(bar_index, 0));
}
} // namespace MidiBar

namespace MidiDuration {
double duration_count(const std::vector<std::pair<ReducedFraction, engraving::TDuration>> &durations) {
    double count = durations.size();
    for (const auto &d : durations) {
        if (d.second.dots()) {
            count += 0.5;
        }
    }
    return count;
}
} // namespace MidiDuration
} // namespace mu::iex::midi
