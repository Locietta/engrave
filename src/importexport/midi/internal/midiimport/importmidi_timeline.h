#pragma once

#include <iterator>
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
        entries.swap(other.entries);
        index.swap(other.index);
        return *this;
    }

    auto begin() { return entries.begin(); }

    auto end() { return entries.end(); }

    auto begin() const { return entries.begin(); }

    auto end() const { return entries.end(); }

    auto cbegin() const { return entries.cbegin(); }

    auto cend() const { return entries.cend(); }

    auto rbegin() const { return entries.rbegin(); }

    auto rend() const { return entries.rend(); }

    auto size() const { return entries.size(); }

    bool empty() const { return entries.empty(); }

    auto lower_bound(const ReducedFraction &time) {
        auto pos = index.lower_bound(time);
        return pos == index.end() ? end() : pos->second;
    }

    const_iterator lower_bound(const ReducedFraction &time) const {
        auto pos = index.lower_bound(time);
        return pos == index.end() ? end() : const_iterator(pos->second);
    }

    auto upper_bound(const ReducedFraction &time) {
        auto pos = index.upper_bound(time);
        return pos == index.end() ? end() : pos->second;
    }

    const_iterator upper_bound(const ReducedFraction &time) const {
        auto pos = index.upper_bound(time);
        return pos == index.end() ? end() : const_iterator(pos->second);
    }

    auto equal_range(const ReducedFraction &time) { return std::pair(lower_bound(time), upper_bound(time)); }

    auto equal_range(const ReducedFraction &time) const { return std::pair(lower_bound(time), upper_bound(time)); }

    iterator insert(value_type event) {
        auto pos = index.upper_bound(event.first);
        auto it = entries.insert(pos == index.end() ? end() : pos->second, std::move(event));
        try {
            index.emplace_hint(pos, it->first, it);
        } catch (...) {
            entries.erase(it);
            throw;
        }
        return it;
    }

    iterator erase(iterator it) {
        auto pos = index.lower_bound(it->first);
        while (pos->second != it) {
            ++pos;
        }
        index.erase(pos);
        return entries.erase(it);
    }

    void clear() {
        index.clear();
        entries.clear();
    }

private:
    Entries entries;
    gtl::btree_multimap<ReducedFraction, iterator> index;
};
} // namespace mu::iex::midi
