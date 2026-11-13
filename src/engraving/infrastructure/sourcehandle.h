#pragma once

#include <compare>
#include <cstdint>

namespace mu::engraving {

struct EngravingObject;
enum struct SourceCollection { SEGMENTS, MEASURES, STAVES };

// Runtime identity only. EIDs, not these slots, are stored in score files.
struct SourceHandle {
    uint64_t document = 0;
    uint32_t slot = 0;
    uint64_t generation = 0;

    bool is_valid() const { return document != 0 && slot != 0 && generation != 0; }

    friend auto operator<=>(const SourceHandle &, const SourceHandle &) = default;
};

// Actual collection membership, distinct from constructor parentage.
void source_child_added(EngravingObject &owner, EngravingObject *item);
void source_child_removed(EngravingObject &owner, EngravingObject *item);
void touch_source_subtree(EngravingObject &item);
bool source_attached(EngravingObject &item);
bool source_object(const EngravingObject &item);
void source_collection_cleared(EngravingObject &owner, SourceCollection collection);

} // namespace mu::engraving
