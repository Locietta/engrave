// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include "types/fraction.h"

namespace mu::engraving {
struct Score;
struct Measure;
struct Lyrics;
struct NormalizeState {
    bool dirty = true;
    bool running = false;
    bool includes_hidden_staves = false;
    Fraction start{0, 1};
    Fraction end{-1, 1};
    std::uint64_t generation = 1;
    std::uint64_t normalized_generation = 0;
};
struct NormalizeOptions {
    // S3 enables this at the mutation boundaries. Layout retains S2 eligibility.
    bool include_hidden_staves = false;
};
void invalidate_normalization(Score &score, const Fraction &start, const Fraction &end);
void normalize_score(Score &score, NormalizeOptions options = {});
// Explicit command barrier; deliberately not wired to batch steps until S3.
void prepare_semantic_edit(Score &score);
void normalize_measure_notes(Measure &measure, bool include_hidden_staves);
Fraction normalize_lyric_ticks(Lyrics &lyrics);
int normalize_measure_number(Measure &measure, int number);
}
