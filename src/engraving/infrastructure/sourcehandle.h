#pragma once

#include <compare>
#include <cstdint>

namespace mu::engraving {

// Runtime identity only. EIDs, not these slots, are stored in score files.
struct SourceHandle {
    uint64_t document = 0;
    uint32_t slot = 0;
    uint64_t generation = 0;

    bool is_valid() const { return document != 0 && slot != 0 && generation != 0; }

    friend auto operator<=>(const SourceHandle &, const SourceHandle &) = default;
};

} // namespace mu::engraving
