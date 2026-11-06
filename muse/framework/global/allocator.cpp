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
#include "allocator.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>

#include "global/stringutils.h"
#include "log.h"

using namespace muse;

int ObjectAllocator::s_used = 0;
size_t ObjectAllocator::default_block_size(1024 * 256); // 256 kB

static inline size_t align(size_t n) { return (n + sizeof(intptr_t) - 1) & ~(sizeof(intptr_t) - 1); }

// ============================================
// ObjectAllocator
// ============================================
void ObjectAllocator::used() {
#ifdef MUSE_ENABLE_CUSTOM_ALLOCATOR
    s_used++;
#endif
}

void ObjectAllocator::unused() {
#ifdef MUSE_ENABLE_CUSTOM_ALLOCATOR
    s_used--;
#endif
}

ObjectAllocator::ObjectAllocator(const char *module, const char *name, destroyer_t dtor) : module_(module), name_(name), dtor_(dtor) {
    AllocatorsRegister::instance()->reg(this);
}

ObjectAllocator::~ObjectAllocator() { AllocatorsRegister::instance()->unreg(this); }

const char *ObjectAllocator::module() const { return module_; }

const char *ObjectAllocator::name() const { return name_; }

void *ObjectAllocator::alloc(size_t size) {
    size = align(size);

    if (!chunk_size_) {
        chunk_size_ = size;
    }

    assert(chunk_size_ == size);

    if (!free_) {
        Block b = allocate_block(chunk_size_);
        blocks_.push_back(b);
        free_ = b.begin;
    }

    // The return value is the current position of
    // the allocation pointer:
    Chunk *free_chunk = free_;

    // Advance (bump) the allocation pointer to the next chunk.
    //
    // When no chunks left, the `free_` will be set to `nullptr`, and
    // this will cause allocation of a new block on the next request:
    free_ = free_->next;

    statistic_.total_allocated_count++;

    return free_chunk;
}

void ObjectAllocator::free(void *chunk) {
    // The freed chunk's next pointer points to the
    // current allocation pointer:
    reinterpret_cast<Chunk *>(chunk)->next = free_;

    // And the allocation pointer is now set
    // to the returned (free) chunk:
    free_ = reinterpret_cast<Chunk *>(chunk);

    statistic_.total_free_count++;
}

void ObjectAllocator::cleanup() {
    if (blocks_.empty()) {
        return;
    }

    std::set<Chunk *> free_chunks;
    {
        Chunk *free = free_;
        while (free) {
            free_chunks.insert(free);
            free = free->next;
        }
    }

    for (size_t bi = 0; bi < blocks_.size(); ++bi) {
        const Block &b = blocks_.at(bi);
        Chunk *chunk = b.begin;
        for (size_t i = 0; i < b.chunk_count - 1; ++i) {
            // if not free chunk, then destroy object
            if (free_chunks.find(chunk) == free_chunks.cend()) {
                dtor_(reinterpret_cast<void *>(chunk));
            }

            chunk->next = reinterpret_cast<Chunk *>(reinterpret_cast<uint8_t *>(chunk) + b.chunk_size);
            chunk = chunk->next;
        }

        if (free_chunks.find(chunk) == free_chunks.cend()) {
            dtor_(reinterpret_cast<void *>(chunk));
        }

        if (bi < (blocks_.size() - 1)) {
            chunk->next = blocks_.at(bi + 1).begin;
        } else {
            chunk->next = nullptr;
        }
    }

    free_ = blocks_.front().begin;
}

ObjectAllocator::Block ObjectAllocator::allocate_block(size_t chunk_size) const {
    size_t block_size = std::max(default_block_size, chunk_size);
    size_t chunk_count = block_size / chunk_size;

    Block b;
    b.begin = reinterpret_cast<Chunk *>(malloc(block_size));
    b.chunk_count = chunk_count;
    b.chunk_size = chunk_size;

    // Once the block is allocated, we need to chain all
    // the chunks in this block:

    Chunk *chunk = b.begin;

    for (size_t i = 0; i < chunk_count - 1; ++i) {
        chunk->next = reinterpret_cast<Chunk *>(reinterpret_cast<uint8_t *>(chunk) + chunk_size);
        chunk = chunk->next;
    }

    chunk->next = nullptr;

    return b;
}

void *ObjectAllocator::not_supported(const char *info) {
    LOGE() << name_ << ": " << info << " not supported";
    std::abort();
    //return nullptr; // NOTREACHED
}

ObjectAllocator::Info ObjectAllocator::state_info() const {
    Info info;
    info.module = module_;
    info.name = name_;
    info.chunk_size = chunk_size_;
    info.block_count = blocks_.size();
    info.total_allocated_count = statistic_.total_allocated_count;
    info.total_free_count = statistic_.total_free_count;

    for (const Block &b : blocks_) {
        info.total_chunks += b.chunk_count;
    }

    Chunk *free = free_;
    while (free) {
        ++info.free_chunks;
        free = free->next;
    }

    return info;
}

// ============================================
// AllocatorsRegister
// ============================================
void AllocatorsRegister::reg(ObjectAllocator *a) { allocators_.push_back(a); }

void AllocatorsRegister::unreg(ObjectAllocator *a) { allocators_.remove(a); }

void AllocatorsRegister::cleanup_all(const std::string &module) {
    for (ObjectAllocator *a : allocators_) {
        if (a->module() == module) {
            a->cleanup();
        }
    }
}

#define FORMAT(str, width) muse::strings::left_justified(str, width)
#define TITLE(str) FORMAT(std::string(str), 20)
#define VALUE(val) FORMAT(std::to_string(val), 20)

void AllocatorsRegister::print_statistic(const std::string &title) {
    std::stringstream stream;
    stream << "\n\n";
    stream << title << "\n";
    stream << "allocators: " << allocators_.size() << '\n';
    stream << TITLE("Object") << TITLE("Total alloc") << TITLE("Total free") << TITLE("Used (leak?)") << TITLE("Object size") << "\n";

    uint64_t total_bytes = 0;
    uint64_t total_allocated_count = 0;
    uint64_t total_free_count = 0;
    uint64_t total_used_count = 0;
    for (ObjectAllocator *a : allocators_) {
        ObjectAllocator::Info info = a->state_info();
        stream << FORMAT(info.name, 20) << VALUE(info.total_allocated_count) << VALUE(info.total_free_count) << VALUE(info.used_chunks())
               << VALUE(info.chunk_size) << "\n";

        total_allocated_count += info.total_allocated_count;
        total_free_count += info.total_free_count;
        total_used_count += info.used_chunks();
        total_bytes += info.allocated_bytes();
    }

    stream << "--------------------------------------------------------------------------------------------\n";
    stream << FORMAT("Total", 20) << VALUE(total_allocated_count) << VALUE(total_free_count) << VALUE(total_used_count) << "\n";
    stream << "Total allocated: " << total_bytes << " bytes\n";

    LOGD() << stream.str() << '\n';
}

void AllocatorsRegister::print_state(const std::string &title) {
    std::stringstream stream;
    stream << "\n\n";
    stream << title << "\n";
    stream << "allocators: " << allocators_.size() << '\n';
    stream << TITLE("Object") << TITLE("blockCount") << TITLE("totalChunks") << TITLE("freeChunks") << TITLE("chunkSize")
           << TITLE("allocatedBytes") << "\n";

    uint64_t total_bytes = 0;
    for (ObjectAllocator *a : allocators_) {
        ObjectAllocator::Info info = a->state_info();
        stream << FORMAT(info.name, 20) << VALUE(info.block_count) << VALUE(info.total_chunks) << VALUE(info.free_chunks)
               << VALUE(info.chunk_size) << VALUE(info.allocated_bytes()) << "\n";

        total_bytes += info.allocated_bytes();
    }

    stream << "-----------------------------------------------------\n";
    stream << "Total allocated: " << total_bytes << " bytes\n";

    LOGD() << stream.str() << '\n';
}
