/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include "dom/engravingitem.h"

namespace mu::engraving::rendering::score {
// Owns stable heap payloads during the typed-accessor migration. Item addresses
// are lifetime-qualified by explicit erase before destruction/address reuse.
// Generated shells retain their current identity until L3/L4.
struct LayoutDataStore {
    explicit LayoutDataStore(Score &score);
    ~LayoutDataStore();
    LayoutDataStore(const LayoutDataStore &) = delete;
    LayoutDataStore &operator=(const LayoutDataStore &) = delete;

    Score *score() const { return score_; }

    size_t size() const;
    const EngravingItem::LayoutData *find(const EngravingItem &item) const;
    EngravingItem::LayoutData *mut_data(EngravingItem &item);
    EngravingItem::LayoutData *adopt(EngravingItem &item, std::unique_ptr<EngravingItem::LayoutData> data);
    void erase(EngravingItem &item);
    // Invalidate geometry before retiring payloads and their generated children.
    // Call only after active layout borrowers have unwound.
    void clear();
    void detach_score();
    // Explicit producer pass. The callback may mutate payloads, not membership.
    void for_each_output(const std::function<void(EngravingItem &, EngravingItem::LayoutData &)> &complete);

private:
    struct Records;
    std::unique_ptr<Records> records_;
    Score *score_;
};
} // namespace mu::engraving::rendering::score
