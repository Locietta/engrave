// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "types/fraction.h"

namespace mu::engraving {
struct Score;
struct Measure;
struct Lyrics;
struct MasterScore;

struct NormalizeState {
    bool dirty = true;
    bool running = false;
    bool includes_hidden_staves = false;
    bool rebuild_midi_mapping = false;
};

enum struct NormalizeMode { TRANSACTION, LOAD, REPLAY };

struct NormalizeOptions {
    NormalizeMode mode = NormalizeMode::TRANSACTION;
    // Semantic state is current even for staves with no display geometry.
    bool include_hidden_staves = true;
};

void invalidate_normalization(Score &score);
void normalize_score(Score &score, NormalizeOptions options = {});
void normalize_document(MasterScore &master, NormalizeOptions options = {});
// Explicit command barrier; getters only read the published semantic state.
void prepare_semantic_edit(Score &score);
void normalize_measure_notes(Measure &measure, bool include_hidden_staves);
Fraction normalize_lyric_ticks(Lyrics &lyrics);
int normalize_measure_number(Measure &measure, int number);
} // namespace mu::engraving
