/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MUSE_SLAB_POOL_ASAN
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) || defined(MUSE_SLAB_POOL_ASAN)
#include <sanitizer/asan_interface.h>
#endif

namespace muse {
template <typename T, typename Owner, std::size_t ChunkSize>
struct SlabPool {
    static_assert(ChunkSize > 0 && ChunkSize <= 64 && (ChunkSize & (ChunkSize - 1)) == 0);
    // Match ASan's 8-byte shadow-memory granularity so adjacent slots poison independently.
    static_assert(alignof(T) >= 8 && sizeof(T) % 8 == 0);

    SlabPool() noexcept = default;
    SlabPool(const SlabPool &) = delete;
    SlabPool &operator=(const SlabPool &) = delete;

    ~SlabPool() {
        while (!empty()) {
            for (std::size_t i = 0; i < chunks_.size(); ++i) {
                auto &chunk = *chunks_[i];
                while (chunk.live_mask != 0) {
                    const auto slot = static_cast<std::uint32_t>(i * ChunkSize + std::countr_zero(chunk.live_mask));
                    finish_destroy(slot, detach(slot));
                }
            }
        }
        assert(in_flight_count_ == 0);
        release_all_chunks();
    }

    template <typename... Args>
    std::pair<std::uint32_t, T *> emplace(Owner *owner, Args &&...args) {
        assert(owner);
        const auto slot = acquire_slot();
        auto &chunk = *chunks_[slot / ChunkSize];
        const auto local = slot % ChunkSize;
        auto &storage = chunk.storage[local];
        unpoison(&storage, sizeof(storage));
        const InFlightGuard in_flight{in_flight_count_};

        struct ConstructGuard {
            SlabPool &pool;
            Chunk &chunk;
            std::size_t local;
            std::uint32_t slot;
            SlotStorage &storage;
            bool constructed = false;

            ~ConstructGuard() noexcept {
                if (!constructed) {
                    poison(&storage, sizeof(storage));
                    pool.free_slot(chunk, local, slot);
                }
            }
        } guard{*this, chunk, local, slot, storage};

        T *ptr = std::construct_at(reinterpret_cast<T *>(storage.bytes), std::forward<Args>(args)...);
        guard.constructed = true;
        chunk.meta[local].owner = owner;
        chunk.live_mask |= bit(local);
        ++live_count_;
        return {slot, ptr};
    }

    T *detach(std::uint32_t slot) noexcept {
        assert(slot < next_fresh_);
        auto &chunk = *chunks_[slot / ChunkSize];
        const auto local = slot % ChunkSize;
        assert(chunk.live_mask & bit(local));
        chunk.live_mask &= ~bit(local);
        chunk.meta[local].owner = nullptr;
        --live_count_;
        ++in_flight_count_;
        return payload(chunk, local);
    }

    void finish_destroy(std::uint32_t slot, T *ptr) noexcept {
        auto &chunk = *chunks_[slot / ChunkSize];
        const auto local = slot % ChunkSize;
        assert(!(chunk.live_mask & bit(local)) && ptr == payload(chunk, local));
        std::destroy_at(ptr);
        assert(in_flight_count_ > 0);
        --in_flight_count_;
        auto &storage = chunk.storage[local];
        poison(&storage, sizeof(storage));
        // A destructor can allocate from this pool. Publish the slot only after it returns.
        free_slot(chunk, local, slot);
    }

    // The callback must detach the passed owner. It may also erase, emplace, or release chunks once empty.
    // Insertions into already visited chunks are left for another pass.
    template <typename Fn>
    void drain_owners(Fn &&fn) noexcept(std::is_nothrow_invocable_v<Fn &, Owner &>) {
        if (empty()) return;
        for (std::size_t i = 0; i < chunks_.size(); ++i) {
            while (i < chunks_.size() && chunks_[i]->live_mask != 0) {
                auto &chunk = *chunks_[i];
                fn(*chunk.meta[std::countr_zero(chunk.live_mask)].owner);
            }
        }
    }

    // Callbacks can modify payloads. Membership must stay fixed during iteration.
    template <typename Fn>
    void for_each_live(Fn &&fn) noexcept(std::is_nothrow_invocable_v<Fn &, Owner &, T &>) {
        for_each_live_slot([&](Chunk &chunk, std::size_t local) noexcept(std::is_nothrow_invocable_v<Fn &, Owner &, T &>) {
            fn(*chunk.meta[local].owner, *payload(chunk, local));
        });
    }

    template <typename Fn>
    void for_each_live_data(Fn &&fn) noexcept(std::is_nothrow_invocable_v<Fn &, T &>) {
        for_each_live_slot(
            [&](Chunk &chunk, std::size_t local) noexcept(std::is_nothrow_invocable_v<Fn &, T &>) { fn(*payload(chunk, local)); });
    }

    // Releases chunk storage only when no live, constructing, or detached slots remain.
    bool release_all_chunks() noexcept {
        if (!empty() || in_flight_count_ != 0) return false;
        for (const auto &chunk : chunks_) unpoison(chunk->storage, sizeof(chunk->storage));
        chunks_.clear();
        next_fresh_ = 0;
        free_head_ = invalid_slot;
        return true;
    }

    std::uint32_t live_count() const noexcept { return live_count_; }

    bool empty() const noexcept { return live_count_ == 0; }

private:
    struct InFlightGuard {
        std::uint32_t &count;

        explicit InFlightGuard(std::uint32_t &c) noexcept : count(c) { ++count; }

        ~InFlightGuard() noexcept { --count; }
    };

    struct alignas(T) SlotStorage {
        std::byte bytes[sizeof(T)];
    };

    struct Chunk {
        std::uint64_t live_mask = 0;

        union Meta {
            Owner *owner;
            std::uint32_t next_free;
        } meta[ChunkSize];

        SlotStorage storage[ChunkSize];
    };

    static constexpr std::uint32_t invalid_slot = ~std::uint32_t{0};

    static constexpr std::uint64_t bit(std::size_t local) noexcept { return std::uint64_t{1} << local; }

    static T *payload(Chunk &chunk, std::size_t local) noexcept { return std::launder(reinterpret_cast<T *>(chunk.storage[local].bytes)); }

    static void poison([[maybe_unused]] const void *address, [[maybe_unused]] std::size_t size) noexcept {
#if defined(__SANITIZE_ADDRESS__) || defined(MUSE_SLAB_POOL_ASAN)
        __asan_poison_memory_region(address, size);
#endif
    }

    static void unpoison([[maybe_unused]] const void *address, [[maybe_unused]] std::size_t size) noexcept {
#if defined(__SANITIZE_ADDRESS__) || defined(MUSE_SLAB_POOL_ASAN)
        __asan_unpoison_memory_region(address, size);
#endif
    }

    std::uint32_t acquire_slot() {
        if (free_head_ != invalid_slot) {
            const auto slot = free_head_;
            free_head_ = chunks_[slot / ChunkSize]->meta[slot % ChunkSize].next_free;
            return slot;
        }
        if (next_fresh_ == invalid_slot) throw std::length_error("slab pool slot limit");
        if (next_fresh_ / ChunkSize == chunks_.size()) {
            chunks_.push_back(std::make_unique_for_overwrite<Chunk>());
            auto &chunk = *chunks_.back();
            poison(chunk.storage, sizeof(chunk.storage));
        }
        return next_fresh_++;
    }

    void free_slot(Chunk &chunk, std::size_t local, std::uint32_t slot) noexcept {
        chunk.meta[local].next_free = free_head_;
        free_head_ = slot;
    }

    template <typename Fn>
    void for_each_live_slot(Fn &&fn) noexcept(std::is_nothrow_invocable_v<Fn &, Chunk &, std::size_t>) {
        if (empty()) return;
        for (const auto &chunk : chunks_) {
            for (auto live = chunk->live_mask; live != 0; live &= live - 1) {
                fn(*chunk, std::countr_zero(live));
            }
        }
    }

    std::vector<std::unique_ptr<Chunk>> chunks_;
    std::uint32_t live_count_ = 0;
    std::uint32_t in_flight_count_ = 0;
    std::uint32_t next_fresh_ = 0;
    std::uint32_t free_head_ = invalid_slot;
};
} // namespace muse

#undef MUSE_SLAB_POOL_ASAN
