// SPDX-License-Identifier: GPL-3.0-only
// Windows counting probe only: patch the executable's UCRT import slots. Static
// engine and codec objects are covered; allocations internal to external DLLs
// (cairo, system libraries, CRT internals) are outside this accounting boundary.
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

#include <windows.h>
#include <malloc.h>

#include "audio/engine/internal/playbackmetrics.h"

using Metrics = muse::audio::test::AllocationMetrics;

namespace {
auto original_malloc = &std::malloc;
auto original_realloc = &std::realloc;
auto original_free = &std::free;
auto original_calloc = &std::calloc;

void *count_malloc(std::size_t n) {
    Metrics::count(Metrics::OP_MALLOC);
    return original_malloc(n);
}

void *count_realloc(void *p, std::size_t n) {
    Metrics::count(Metrics::OP_REALLOC);
    return original_realloc(p, n);
}

void count_free(void *p) {
    if (p) Metrics::count(Metrics::OP_FREE);
    original_free(p);
}

void *count_calloc(std::size_t n, std::size_t s) {
    Metrics::count(Metrics::OP_CALLOC);
    return original_calloc(n, s);
}
} // namespace

void install_allocation_hooks() {
    auto *base = reinterpret_cast<unsigned char *>(GetModuleHandleW(nullptr));
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    auto *desc =
        reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    unsigned installed = 0;
    for (; desc->Name; ++desc) {
        if (!desc->OriginalFirstThunk) continue;
        auto *names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + desc->OriginalFirstThunk);
        auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(base + desc->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto *entry = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names->u1.AddressOfData);
            const char *name = reinterpret_cast<const char *>(entry->Name);
            ULONG_PTR replacement = 0;
            if (std::strcmp(name, "malloc") == 0) {
                original_malloc = reinterpret_cast<decltype(original_malloc)>(slots->u1.Function);
                replacement = reinterpret_cast<ULONG_PTR>(&count_malloc);
                installed |= 1;
            }
            if (std::strcmp(name, "realloc") == 0) {
                original_realloc = reinterpret_cast<decltype(original_realloc)>(slots->u1.Function);
                replacement = reinterpret_cast<ULONG_PTR>(&count_realloc);
                installed |= 2;
            }
            if (std::strcmp(name, "free") == 0) {
                original_free = reinterpret_cast<decltype(original_free)>(slots->u1.Function);
                replacement = reinterpret_cast<ULONG_PTR>(&count_free);
                installed |= 4;
            }
            if (std::strcmp(name, "calloc") == 0) {
                original_calloc = reinterpret_cast<decltype(original_calloc)>(slots->u1.Function);
                replacement = reinterpret_cast<ULONG_PTR>(&count_calloc);
            }
            if (!replacement) continue;
            DWORD previous = 0, ignored = 0;
            if (!VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &previous))
                throw std::runtime_error("IAT VirtualProtect failed");
            slots->u1.Function = replacement;
            if (!VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), previous, &ignored))
                throw std::runtime_error("IAT protection restore failed");
        }
    }
    if (installed != 7) throw std::runtime_error("malloc/realloc/free imports not all hooked");
    Metrics::enabled = true;
    auto *p = count_malloc(16);
    if (!p) throw std::bad_alloc();
    auto *q = count_realloc(p, 32);
    if (!q) {
        count_free(p);
        throw std::bad_alloc();
    }
    count_free(q);
    auto *object = ::operator new(16);
    ::operator delete(object);
    Metrics::enabled = false;
    if (Metrics::counts[0][Metrics::OP_NEW] != 1 || Metrics::counts[0][Metrics::OP_DELETE] != 1 ||
        Metrics::counts[0][Metrics::OP_MALLOC] < 2 || Metrics::counts[0][Metrics::OP_REALLOC] != 1 ||
        Metrics::counts[0][Metrics::OP_FREE] < 2)
        throw std::runtime_error("allocation hook self-check failed");
    for (auto &phase : Metrics::counts)
        for (auto &count : phase) count = 0;
}

void *operator new(std::size_t n) {
    Metrics::count(Metrics::OP_NEW);
    if (auto *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}

void *operator new[](std::size_t n) { return ::operator new(n); }

void operator delete(void *p) noexcept {
    if (p) Metrics::count(Metrics::OP_DELETE);
    std::free(p);
}

void operator delete[](void *p) noexcept { ::operator delete(p); }

void operator delete(void *p, std::size_t) noexcept { ::operator delete(p); }

void operator delete[](void *p, std::size_t) noexcept { ::operator delete(p); }

void *operator new(std::size_t n, std::align_val_t a) {
    Metrics::count(Metrics::OP_NEW);
    Metrics::count(Metrics::OP_ALIGNED_NEW);
    if (auto *p = _aligned_malloc(n ? n : 1, static_cast<std::size_t>(a))) return p;
    throw std::bad_alloc();
}

void *operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }

void operator delete(void *p, std::align_val_t) noexcept {
    if (p) Metrics::count(Metrics::OP_DELETE);
    _aligned_free(p);
}

void operator delete[](void *p, std::align_val_t a) noexcept { ::operator delete(p, a); }

void operator delete(void *p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }

void operator delete[](void *p, std::size_t, std::align_val_t a) noexcept { ::operator delete(p, a); }

void *operator new(std::size_t n, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(n);
    } catch (...) {
        return nullptr;
    }
}

void *operator new[](std::size_t n, const std::nothrow_t &tag) noexcept { return ::operator new(n, tag); }

void operator delete(void *p, const std::nothrow_t &) noexcept { ::operator delete(p); }

void operator delete[](void *p, const std::nothrow_t &) noexcept { ::operator delete(p); }

void *operator new(std::size_t n, std::align_val_t a, const std::nothrow_t &) noexcept {
    try {
        return ::operator new(n, a);
    } catch (...) {
        return nullptr;
    }
}

void *operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t &tag) noexcept { return ::operator new(n, a, tag); }

void operator delete(void *p, std::align_val_t a, const std::nothrow_t &) noexcept { ::operator delete(p, a); }

void operator delete[](void *p, std::align_val_t a, const std::nothrow_t &) noexcept { ::operator delete(p, a); }
