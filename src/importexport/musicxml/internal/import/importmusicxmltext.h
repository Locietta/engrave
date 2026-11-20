// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <optional>
#include <utility>

#include "global/types/string.h"

namespace mu::iex::musicxml {

struct RepeatDirection {
    std::u16string_view type;
    std::u16string_view segno_name;
    std::u16string_view segno_number;
    std::u16string_view coda_number;
};

// Numeral extraction historically requires a parenthesis to follow immediately.
RepeatDirection parse_repeat_direction(std::u16string_view text, bool allow_space_before_parenthesis = true);
bool matches_fingering(std::u16string_view text);
std::pair<muse::String, muse::String> process_instr_name(const muse::String &name);
std::optional<double> substitute_tempo_note(muse::String &words, bool sibelius, const muse::String &font_family);

} // namespace mu::iex::musicxml
