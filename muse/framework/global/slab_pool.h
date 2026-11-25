/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
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
    static_assert(alignof(T) >= 8 && sizeof(T) % 8 == 0);

    SlabPool() = default;
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
#ifndef NDEBUG
        assert(detached_count_ == 0);
#endif
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
        T *ptr;
        try {
            ptr = std::construct_at(reinterpret_cast<T *>(storage.bytes), std::forward<Args>(args)...);
        } catch (...) {
            poison(&storage, sizeof(storage));
            free_slot(chunk, local, slot);
            throw;
        }
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
#ifndef NDEBUG
        ++detached_count_;
#endif
        return payload(chunk, local);
    }

    void finish_destroy(std::uint32_t slot, T *ptr) noexcept {
        auto &chunk = *chunks_[slot / ChunkSize];
        const auto local = slot % ChunkSize;
        assert(!(chunk.live_mask & bit(local)) && ptr == payload(chunk, local));
        std::destroy_at(ptr);
#ifndef NDEBUG
        assert(detached_count_ > 0);
        --detached_count_;
#endif
        auto &storage = chunk.storage[local];
        poison(&storage, sizeof(storage));
        // A destructor can allocate from this pool. Publish the slot only after it returns.
        free_slot(chunk, local, slot);
    }

    Owner *first_live_owner() const noexcept {
        if (empty()) return nullptr;
        for (const auto &chunk : chunks_) {
            if (chunk->live_mask != 0) return chunk->meta[std::countr_zero(chunk->live_mask)].owner;
        }
        return nullptr;
    }

    // The callback must detach the passed owner. It may also erase or emplace other slots.
    // Insertions into already visited chunks are left for another pass.
    template <typename Fn>
    void drain_owners(Fn &&fn) {
        if (empty()) return;
        for (std::size_t i = 0; i < chunks_.size(); ++i) {
            auto &chunk = *chunks_[i];
            while (chunk.live_mask != 0) {
                fn(*chunk.meta[std::countr_zero(chunk.live_mask)].owner);
            }
        }
    }

    // Callbacks can modify payloads. Membership must stay fixed during iteration.
    template <typename Fn>
    void for_each_live(Fn &&fn) {
        for_each_live_slot([&](Chunk &chunk, std::size_t local) { fn(*chunk.meta[local].owner, *payload(chunk, local)); });
    }

    template <typename Fn>
    void for_each_live_data(Fn &&fn) {
        for_each_live_slot([&](Chunk &chunk, std::size_t local) { fn(*payload(chunk, local)); });
    }

    // All detached slots must have finished destruction before releasing storage.
    void release_all_chunks() noexcept {
        assert(empty());
#ifndef NDEBUG
        assert(detached_count_ == 0);
#endif
        for (const auto &chunk : chunks_) unpoison(chunk->storage, sizeof(chunk->storage));
        chunks_.clear();
        next_fresh_ = 0;
        free_head_ = invalid_slot;
    }

    std::uint32_t live_count() const noexcept { return live_count_; }

    bool empty() const noexcept { return live_count_ == 0; }

private:
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

    static constexpr std::uint64_t bit(std::size_t local) { return std::uint64_t{1} << local; }

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
    void for_each_live_slot(Fn &&fn) {
        if (empty()) return;
        for (const auto &chunk : chunks_) {
            for (auto live = chunk->live_mask; live != 0; live &= live - 1) {
                fn(*chunk, std::countr_zero(live));
            }
        }
    }

    std::vector<std::unique_ptr<Chunk>> chunks_;
    std::uint32_t live_count_ = 0;
#ifndef NDEBUG
    std::uint32_t detached_count_ = 0;
#endif
    std::uint32_t next_fresh_ = 0;
    std::uint32_t free_head_ = invalid_slot;
};
} // namespace muse

#undef MUSE_SLAB_POOL_ASAN
