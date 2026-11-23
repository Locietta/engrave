#pragma once

#include <cassert>
#include <list>
#include <utility>

#include <gtl/btree.hpp>

#include "importmidi_fraction.h"

namespace mu::iex::midi {
// Chords and tuplets refer to each other's entries across splits and insertions.
// Only the time index may relocate its entries.
template <typename Event>
struct MidiTimeline {
    using key_type = ReducedFraction;
    using value_type = std::pair<const ReducedFraction, Event>;
    using Entries = std::list<value_type>;
    using iterator = typename Entries::iterator;
    using const_iterator = typename Entries::const_iterator;

    MidiTimeline() = default;

    MidiTimeline(const MidiTimeline &other) {
        for (const auto &event : other) {
            insert(event);
        }
    }

    MidiTimeline(MidiTimeline &&) = default;

    MidiTimeline &operator=(MidiTimeline other) {
        entries_.swap(other.entries_);
        index_.swap(other.index_);
        return *this;
    }

    auto begin() { return entries_.begin(); }

    auto end() { return entries_.end(); }

    auto begin() const { return entries_.begin(); }

    auto end() const { return entries_.end(); }

    auto cbegin() const { return entries_.cbegin(); }

    auto cend() const { return entries_.cend(); }

    auto rbegin() const { return entries_.rbegin(); }

    auto rend() const { return entries_.rend(); }

    auto size() const { return entries_.size(); }

    bool empty() const { return entries_.empty(); }

    auto lower_bound(const ReducedFraction &time) {
        auto pos = index_.lower_bound(time);
        return pos == index_.end() ? end() : pos->second;
    }

    const_iterator lower_bound(const ReducedFraction &time) const {
        auto pos = index_.lower_bound(time);
        return pos == index_.end() ? end() : const_iterator(pos->second);
    }

    auto upper_bound(const ReducedFraction &time) {
        auto pos = index_.upper_bound(time);
        return pos == index_.end() ? end() : pos->second;
    }

    const_iterator upper_bound(const ReducedFraction &time) const {
        auto pos = index_.upper_bound(time);
        return pos == index_.end() ? end() : const_iterator(pos->second);
    }

    auto equal_range(const ReducedFraction &time) { return std::pair(lower_bound(time), upper_bound(time)); }

    auto equal_range(const ReducedFraction &time) const { return std::pair(lower_bound(time), upper_bound(time)); }

    iterator insert(value_type event) {
        auto pos = index_.upper_bound(event.first);
        auto it = entries_.insert(pos == index_.end() ? end() : pos->second, std::move(event));
        try {
            index_.emplace_hint(pos, it->first, it);
        } catch (...) {
            entries_.erase(it);
            throw;
        }
        return it;
    }

    iterator erase(iterator it) {
        auto pos = index_.lower_bound(it->first);
        while (pos != index_.end() && pos->second != it) {
            ++pos;
        }
        assert(pos != index_.end());
        if (pos != index_.end()) {
            index_.erase(pos);
        }
        return entries_.erase(it);
    }

    void clear() {
        index_.clear();
        entries_.clear();
    }

private:
    Entries entries_;
    gtl::btree_multimap<ReducedFraction, iterator> index_;
};
} // namespace mu::iex::midi
