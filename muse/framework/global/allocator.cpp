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
size_t ObjectAllocator::DEFAULT_BLOCK_SIZE(1024 * 256); // 256 kB

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

    if (!chunkSize_) {
        chunkSize_ = size;
    }

    assert(chunkSize_ == size);

    if (!free_) {
        Block b = allocateBlock(chunkSize_);
        blocks_.push_back(b);
        free_ = b.begin;
    }

    // The return value is the current position of
    // the allocation pointer:
    Chunk *freeChunk = free_;

    // Advance (bump) the allocation pointer to the next chunk.
    //
    // When no chunks left, the `free_` will be set to `nullptr`, and
    // this will cause allocation of a new block on the next request:
    free_ = free_->next;

    statistic_.totalAllocatedCount++;

    return freeChunk;
}

void ObjectAllocator::free(void *chunk) {
    // The freed chunk's next pointer points to the
    // current allocation pointer:
    reinterpret_cast<Chunk *>(chunk)->next = free_;

    // And the allocation pointer is now set
    // to the returned (free) chunk:
    free_ = reinterpret_cast<Chunk *>(chunk);

    statistic_.totalFreeCount++;
}

void ObjectAllocator::cleanup() {
    if (blocks_.empty()) {
        return;
    }

    std::set<Chunk *> freeChunks;
    {
        Chunk *free = free_;
        while (free) {
            freeChunks.insert(free);
            free = free->next;
        }
    }

    for (size_t bi = 0; bi < blocks_.size(); ++bi) {
        const Block &b = blocks_.at(bi);
        Chunk *chunk = b.begin;
        for (size_t i = 0; i < b.chunkCount - 1; ++i) {
            // if not free chunk, then destroy object
            if (freeChunks.find(chunk) == freeChunks.cend()) {
                dtor_(reinterpret_cast<void *>(chunk));
            }

            chunk->next = reinterpret_cast<Chunk *>(reinterpret_cast<uint8_t *>(chunk) + b.chunkSize);
            chunk = chunk->next;
        }

        if (freeChunks.find(chunk) == freeChunks.cend()) {
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

ObjectAllocator::Block ObjectAllocator::allocateBlock(size_t chunkSize) const {
    size_t blockSize = std::max(DEFAULT_BLOCK_SIZE, chunkSize);
    size_t chunkCount = blockSize / chunkSize;

    Block b;
    b.begin = reinterpret_cast<Chunk *>(malloc(blockSize));
    b.chunkCount = chunkCount;
    b.chunkSize = chunkSize;

    // Once the block is allocated, we need to chain all
    // the chunks in this block:

    Chunk *chunk = b.begin;

    for (size_t i = 0; i < chunkCount - 1; ++i) {
        chunk->next = reinterpret_cast<Chunk *>(reinterpret_cast<uint8_t *>(chunk) + chunkSize);
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

ObjectAllocator::Info ObjectAllocator::stateInfo() const {
    Info info;
    info.module = module_;
    info.name = name_;
    info.chunkSize = chunkSize_;
    info.blockCount = blocks_.size();
    info.totalAllocatedCount = statistic_.totalAllocatedCount;
    info.totalFreeCount = statistic_.totalFreeCount;

    for (const Block &b : blocks_) {
        info.totalChunks += b.chunkCount;
    }

    Chunk *free = free_;
    while (free) {
        ++info.freeChunks;
        free = free->next;
    }

    return info;
}

// ============================================
// AllocatorsRegister
// ============================================
void AllocatorsRegister::reg(ObjectAllocator *a) { allocators_.push_back(a); }

void AllocatorsRegister::unreg(ObjectAllocator *a) { allocators_.remove(a); }

void AllocatorsRegister::cleanupAll(const std::string &module) {
    for (ObjectAllocator *a : allocators_) {
        if (a->module() == module) {
            a->cleanup();
        }
    }
}

#define FORMAT(str, width) muse::strings::leftJustified(str, width)
#define TITLE(str) FORMAT(std::string(str), 20)
#define VALUE(val) FORMAT(std::to_string(val), 20)

void AllocatorsRegister::printStatistic(const std::string &title) {
    std::stringstream stream;
    stream << "\n\n";
    stream << title << "\n";
    stream << "allocators: " << allocators_.size() << '\n';
    stream << TITLE("Object") << TITLE("Total alloc") << TITLE("Total free") << TITLE("Used (leak?)") << TITLE("Object size") << "\n";

    uint64_t totalBytes = 0;
    uint64_t totalAllocatedCount = 0;
    uint64_t totalFreeCount = 0;
    uint64_t totalUsedCount = 0;
    for (ObjectAllocator *a : allocators_) {
        ObjectAllocator::Info info = a->stateInfo();
        stream << FORMAT(info.name, 20) << VALUE(info.totalAllocatedCount) << VALUE(info.totalFreeCount) << VALUE(info.usedChunks())
               << VALUE(info.chunkSize) << "\n";

        totalAllocatedCount += info.totalAllocatedCount;
        totalFreeCount += info.totalFreeCount;
        totalUsedCount += info.usedChunks();
        totalBytes += info.allocatedBytes();
    }

    stream << "--------------------------------------------------------------------------------------------\n";
    stream << FORMAT("Total", 20) << VALUE(totalAllocatedCount) << VALUE(totalFreeCount) << VALUE(totalUsedCount) << "\n";
    stream << "Total allocated: " << totalBytes << " bytes\n";

    LOGD() << stream.str() << '\n';
}

void AllocatorsRegister::printState(const std::string &title) {
    std::stringstream stream;
    stream << "\n\n";
    stream << title << "\n";
    stream << "allocators: " << allocators_.size() << '\n';
    stream << TITLE("Object") << TITLE("blockCount") << TITLE("totalChunks") << TITLE("freeChunks") << TITLE("chunkSize")
           << TITLE("allocatedBytes") << "\n";

    uint64_t totalBytes = 0;
    for (ObjectAllocator *a : allocators_) {
        ObjectAllocator::Info info = a->stateInfo();
        stream << FORMAT(info.name, 20) << VALUE(info.blockCount) << VALUE(info.totalChunks) << VALUE(info.freeChunks)
               << VALUE(info.chunkSize) << VALUE(info.allocatedBytes()) << "\n";

        totalBytes += info.allocatedBytes();
    }

    stream << "-----------------------------------------------------\n";
    stream << "Total allocated: " << totalBytes << " bytes\n";

    LOGD() << stream.str() << '\n';
}
