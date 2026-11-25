#pragma once

#include <cstdint>
#include <optional>

#include "global/unique_fn.h"

namespace music {

struct ExportOptions {
    int page = 0;
    // PDF only: the pages the document holds, both ends inclusive. A negative
    // last_page means the score's last page.
    int first_page = 0;
    int last_page = -1;
    int excerpt_id = -1;
    // Public score export: write an independent score, with no links to the
    // source master and with an inline style for uncompressed master MSCX.
    bool standalone_score = false;
    // Internal checkpoints preserve native numeric values without refreshing
    // fonts or geometry.
    bool transaction_snapshot = false;
    bool compress = true;
    int png_dpi = 144;
    bool draw_page_background = false;
    bool transparent = true;
    muse::UniqueFn<bool() const> cancelled;
    std::optional<std::uint64_t> max_bytes;
    bool midi_expand_repeats = true;
    bool midi_export_rpns = true;
};

} // namespace music
