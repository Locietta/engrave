/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef MUSE_GLOBAL_ALLOCATOR_H
#define MUSE_GLOBAL_ALLOCATOR_H

#include <cstdint>
#include <vector>
#include <list>
#include <string>

namespace muse {
#define OBJECT_ALLOCATOR(Module, ClassName)                                                                                        \
public:                                                                                                                            \
    static muse::ObjectAllocator &allocator() {                                                                                    \
        static muse::ObjectAllocator a(#Module, #ClassName, muse::ObjectAllocator::destroyer<ClassName>);                          \
        return a;                                                                                                                  \
    }                                                                                                                              \
    static void *operator new(size_t sz) { return muse::ObjectAllocator::enabled() ? allocator().alloc(sz) : ::operator new(sz); } \
    static void operator delete(void *ptr) {                                                                                       \
        if (muse::ObjectAllocator::enabled()) {                                                                                    \
            allocator().free(ptr);                                                                                                 \
        } else {                                                                                                                   \
            ::operator delete(ptr);                                                                                                \
        }                                                                                                                          \
    }                                                                                                                              \
    static void *operator new[](size_t sz) {                                                                                       \
        return muse::ObjectAllocator::enabled() ? allocator().not_supported("new[]") : ::operator new[](sz);                       \
    }                                                                                                                              \
    static void *operator new(size_t sz, void *ptr) {                                                                              \
        return muse::ObjectAllocator::enabled() ? allocator().not_supported("new(size_t, void*)") : ::operator new(sz, ptr);       \
    }                                                                                                                              \
    static void operator delete[](void *ptr) {                                                                                     \
        if (muse::ObjectAllocator::enabled()) {                                                                                    \
            allocator().not_supported("delete[]");                                                                                 \
        } else {                                                                                                                   \
            ::operator delete[](ptr);                                                                                              \
        }                                                                                                                          \
    }                                                                                                                              \
private:

struct ObjectAllocator {

    using destroyer_t = void (*)(void *);

    ObjectAllocator(const char *module, const char *name, destroyer_t dtor);
    ~ObjectAllocator();

    static size_t default_block_size;

    const char *module() const;
    const char *name() const;

    void *alloc(size_t size);
    void free(void *ptr);
    void cleanup();

    template <class T>
    static void destroyer(void *ptr) {
        T *p = reinterpret_cast<T *>(ptr);
        p->~T();
    }

    void *not_supported(const char *info);

    struct Info {
        std::string module;
        std::string name;
        size_t chunk_size = 0;
        size_t block_count = 0;
        size_t total_chunks = 0;
        size_t free_chunks = 0;

        uint64_t total_allocated_count = 0;
        uint64_t total_free_count = 0;

        uint64_t used_chunks() const { return total_chunks - free_chunks; }

        uint64_t allocated_bytes() const { return total_chunks * chunk_size; }
    };

    Info state_info() const;

    static bool enabled() { return s_used; }

    static void used();
    static void unused();

    static int s_used;
private:

    struct Chunk {
        /**
         * When a chunk is free, the `next` contains the
         * address of the next chunk in a list.
         *
         * When it's allocated, this space is used by
         * the user.
         */
        Chunk *next = nullptr;
    };

    struct Block {
        Chunk *begin = nullptr;
        size_t chunk_count = 0;
        size_t chunk_size = 0;
    };

    Block allocate_block(size_t chunk_size) const;

    const char *module_ = nullptr;
    const char *name_ = nullptr;
    size_t chunk_size_ = 0;
    destroyer_t dtor_ = nullptr;
    Chunk *free_ = nullptr;
    std::vector<Block> blocks_;

    struct Statistic {
        uint64_t total_allocated_count = 0;
        uint64_t total_free_count = 0;
    };

    Statistic statistic_;
};

struct AllocatorsRegister {

    static AllocatorsRegister *instance() {
        static AllocatorsRegister r;
        return &r;
    }

    void reg(ObjectAllocator *a);
    void unreg(ObjectAllocator *a);

    void cleanup_all(const std::string &module);

    void print_statistic(const std::string &title);
    void print_state(const std::string &title);

private:
    std::list<ObjectAllocator *> allocators_;
};
} // namespace muse

#endif // MUSE_GLOBAL_ALLOCATOR_H
