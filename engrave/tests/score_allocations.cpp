// SPDX-License-Identifier: GPL-3.0-only
// Windows counting probe only: patch the executable's UCRT import slots. Static
// engine and codec objects are covered; allocations internal to external DLLs
// (cairo, system libraries, CRT internals) are outside this accounting boundary.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>

#include <windows.h>
#include <malloc.h>

#include <engrave/engrave.h>

struct Metrics {
    enum { OP_NEW, OP_DELETE, OP_MALLOC, OP_REALLOC, OP_FREE, OP_CALLOC, OP_ALIGNED_NEW };

    static inline thread_local bool enabled = false;
    static inline std::uint64_t counts[1][7]{};
    static inline std::uint64_t bytes = 0;

    static void count(int op) {
        if (enabled) ++counts[0][op];
    }

    static void allocated(std::size_t n) {
        if (enabled) bytes += n;
    }
};

#ifndef ENGRAVE_PROBE_NO_HOOKS
namespace {
auto original_malloc = &std::malloc;
auto original_realloc = &std::realloc;
auto original_free = &std::free;
auto original_calloc = &std::calloc;

void *count_malloc(std::size_t n) {
    Metrics::count(Metrics::OP_MALLOC);
    Metrics::allocated(n);
    return original_malloc(n);
}

void *count_realloc(void *p, std::size_t n) {
    Metrics::count(Metrics::OP_REALLOC);
    Metrics::allocated(n);
    return original_realloc(p, n);
}

void count_free(void *p) {
    if (p) Metrics::count(Metrics::OP_FREE);
    original_free(p);
}

void *count_calloc(std::size_t n, std::size_t s) {
    Metrics::count(Metrics::OP_CALLOC);
    Metrics::allocated(n * s);

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
                installed |= 8;
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
    if (installed != 15) throw std::runtime_error("malloc/realloc/free/calloc imports not all hooked");
    const std::uint64_t bytes_before = Metrics::bytes;
    Metrics::enabled = true;
    auto volatile malloc_entry = &std::malloc;
    auto volatile realloc_entry = &std::realloc;
    auto volatile calloc_entry = &std::calloc;
    auto volatile free_entry = &std::free;
    auto *p = malloc_entry(16);
    if (!p) throw std::bad_alloc();
    auto *q = realloc_entry(p, 32);
    if (!q) {
        count_free(p);
        throw std::bad_alloc();
    }
    free_entry(q);
    auto *zeroed = calloc_entry(2, 16);
    if (!zeroed) throw std::bad_alloc();
    free_entry(zeroed);
    auto *object = ::operator new(16);
    ::operator delete(object);
    auto *aligned = ::operator new(64, std::align_val_t{64});
    ::operator delete(aligned, std::align_val_t{64});
    Metrics::enabled = false;
    // Exact counts and bytes: malloc(16) and new(16) reach count_malloc, realloc(32),
    // calloc(2, 16), aligned new(64); free, the calloc free and delete reach count_free.
    if (Metrics::counts[0][Metrics::OP_NEW] != 2 || Metrics::counts[0][Metrics::OP_DELETE] != 2 ||
        Metrics::counts[0][Metrics::OP_MALLOC] != 2 || Metrics::counts[0][Metrics::OP_REALLOC] != 1 ||
        Metrics::counts[0][Metrics::OP_FREE] != 3 || Metrics::counts[0][Metrics::OP_CALLOC] != 1 ||
        Metrics::counts[0][Metrics::OP_ALIGNED_NEW] != 1 || Metrics::bytes - bytes_before != 160)
        throw std::runtime_error("allocation hook self-check failed");
    for (auto &phase : Metrics::counts)
        for (auto &count : phase) count = 0;
    Metrics::bytes = 0;
}

void *operator new(std::size_t n) {
    Metrics::count(Metrics::OP_NEW);
    if (auto *p = count_malloc(n ? n : 1)) return p;
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
    Metrics::allocated(n);
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

#endif // ENGRAVE_PROBE_NO_HOOKS

// Counts requested allocation bytes (realloc counts its full requested size),
// not live/peak heap size. External DLL allocations are excluded. Engine setup
// is outside each sample; render ensures layout and captures all SVG pages.
int main(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "usage: score_allocations ASSETS INPUT_ROOT OUTPUT_JSON\n";
        return 2;
    }
    try {
#ifndef ENGRAVE_PROBE_NO_HOOKS
        install_allocation_hooks();
#endif
        auto engine = engrave::Engine::create({.resources = argv[1]});
        if (!engine) throw std::runtime_error(engine.error().message);
        constexpr const char *files[] = {"golden/layout_elements.mscx",
                                         "golden/tab_grace_prebend.mscx",
                                         "golden/repeat_with_2_voltas.mscx",
                                         "golden/hairpins_and_repeats.mscx",
                                         "golden/custom_part_name.mscx",
                                         "golden/ghost_note.mscx",
                                         "golden/lyrics.mscx",
                                         "audio/mix_parts.mscz",
                                         "evening.musicxml",
                                         "golden/bend.gp"};
        std::ofstream out(argv[3]);
        if (!out) throw std::runtime_error("cannot open output JSON");
        out << "{\"runs\":5,\"format\":\"SVG\",\"files\":[";
        std::uint64_t total_count = 0, total_bytes = 0;
        double total_ms = 0;
        bool first = true;
        for (auto file : files) {
            std::array<std::uint64_t, 5> counts{}, bytes{};
            std::array<double, 5> times{};
            for (int run = 0; run < 5; ++run) {
                const auto path = std::filesystem::path(argv[2]) / file;
                for (auto &n : Metrics::counts[0]) n = 0;
                Metrics::bytes = 0;
                const auto start = std::chrono::steady_clock::now();
                Metrics::enabled = true;
                {
                    auto doc = (*engine)->open(path);
                    if (!doc) throw std::runtime_error(doc.error().message);
                    auto rendered = (*doc)->render({.format = engrave::RenderFormat::SVG});
                    if (!rendered) throw std::runtime_error(rendered.error().message);
                }
                Metrics::enabled = false;
                times[run] = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                counts[run] = Metrics::counts[0][Metrics::OP_MALLOC] + Metrics::counts[0][Metrics::OP_REALLOC] +
                              Metrics::counts[0][Metrics::OP_CALLOC] + Metrics::counts[0][Metrics::OP_ALIGNED_NEW];
                bytes[run] = Metrics::bytes;
            }
            std::sort(counts.begin(), counts.end());
            std::sort(bytes.begin(), bytes.end());
            std::sort(times.begin(), times.end());
            if (!first) out << ',';
            first = false;
            out << "{\"file\":\"" << file << "\",\"allocations\":" << counts[2] << ",\"bytes\":" << bytes[2] << ",\"ms\":" << times[2]
                << '}';
            total_count += counts[2];
            total_bytes += bytes[2];
            total_ms += times[2];
        }
        out << "],\"total\":{\"allocations\":" << total_count << ",\"bytes\":" << total_bytes << ",\"ms\":" << total_ms << "}}\n";
        out.close();
        if (!out) throw std::runtime_error("cannot write output JSON");
        std::ifstream report(argv[3]);
        std::cout << report.rdbuf();
        return 0;
    } catch (const std::exception &e) {
        Metrics::enabled = false;
        std::cerr << e.what() << '\n';
        return 1;
    }
}
