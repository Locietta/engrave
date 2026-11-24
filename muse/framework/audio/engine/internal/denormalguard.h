// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cfenv>
#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#include <xmmintrin.h>
#endif

namespace muse::audio {
// Recursive reverb tails otherwise incur subnormal arithmetic traps. Own only
// the denormal bits on the calling thread so callbacks can use and change the
// rest of the FP state. Worker tasks synchronize both denormal bits and the
// coordinator's active rounding mode per block and restore their own state on
// exit, so persistent workers never depend on pool history.
struct ScopedDenormalGuard {
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
    using Register = std::uint32_t;
    static constexpr Register k_denormal_mask = (1U << 15) | (1U << 6); // FTZ and DAZ
    static constexpr Register k_round_mask = 3U << 13;                  // MXCSR RC
    static constexpr Register k_mode_mask = k_denormal_mask | k_round_mask;

    static Register read() noexcept { return _mm_getcsr(); }

    static void write(Register state) noexcept { _mm_setcsr(state); }
#elif defined(__aarch64__)
    using Register = std::uint64_t;
    static constexpr Register k_fpcr_fz = 1ULL << 24;
    static constexpr Register k_denormal_mask = k_fpcr_fz;
    static constexpr Register k_round_mask = 3ULL << 22; // FPCR RMode
    static constexpr Register k_mode_mask = k_denormal_mask | k_round_mask;

    static Register read() noexcept {
        Register fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        return fpcr;
    }

    static void write(Register fpcr) noexcept { asm volatile("msr fpcr, %0" : : "r"(fpcr)); }
#endif

    struct Mode {
        int rounding = FE_TONEAREST;
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
        Register hw_mode = k_denormal_mask;
#endif
    };

    static Mode capture_mode() noexcept {
        Mode mode;
        mode.rounding = std::fegetround();
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
        mode.hw_mode = read() & k_mode_mask;
#endif
        return mode;
    }

    static void apply_mode(const Mode &target, bool force_denormals) noexcept {
        if (target.rounding >= 0 && std::fegetround() != target.rounding) std::fesetround(target.rounding);
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
        const Register desired = force_denormals ? (target.hw_mode | k_denormal_mask) : target.hw_mode;
        const Register current = read();
        if ((current & k_mode_mask) != desired) write((current & ~k_mode_mask) | desired);
#else
        (void) force_denormals;
#endif
    }

    struct WorkerScope {
        explicit WorkerScope(const Mode &target) noexcept : entry_(capture_mode()) { apply_mode(target, true); }

        ~WorkerScope() { apply_mode(entry_, false); }

        WorkerScope(const WorkerScope &) = delete;
        WorkerScope &operator=(const WorkerScope &) = delete;
    private:
        Mode entry_;
    };

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
    static void enable() noexcept {
        const auto current = read();
        if ((current & k_denormal_mask) != k_denormal_mask) write(current | k_denormal_mask);
    }

    void restore() const noexcept {
        const auto current = read();
        if ((current & k_denormal_mask) != entry_denormal_mode) write((current & ~k_denormal_mask) | entry_denormal_mode);
    }

    ScopedDenormalGuard() noexcept { enable(); }

    ~ScopedDenormalGuard() { restore(); }

    const Register entry_denormal_mode = read() & k_denormal_mask;
#else
    static void enable() noexcept {}

    static void restore() noexcept {}

    ScopedDenormalGuard() = default;
    ~ScopedDenormalGuard() = default;
#endif
    // Pins the active render FP mode (denormals + rounding) across synthesis and
    // encoding so mid-render callback rounding changes stay visible to callbacks
    // and survive return without altering serial vs. parallel audio output.
    struct RenderModePin {
        explicit RenderModePin(ScopedDenormalGuard &owner) noexcept : guard_(owner) {
            guard_.pinned_mode_ = capture_mode();
            guard_.caller_mode_ = guard_.pinned_mode_;
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
            guard_.caller_mode_.hw_mode = (guard_.caller_mode_.hw_mode & ~k_denormal_mask) | guard_.entry_denormal_mode;
#endif
            guard_.pinned_ = true;
        }

        ~RenderModePin() {
            guard_.pinned_ = false;
            apply_mode(guard_.caller_mode_, true);
        }

        RenderModePin(const RenderModePin &) = delete;
        RenderModePin &operator=(const RenderModePin &) = delete;
    private:
        ScopedDenormalGuard &guard_;
    };

    // Hands the caller's own mode back while a user callback runs.
    struct Suspend {
        explicit Suspend(ScopedDenormalGuard &owner) noexcept : guard_(owner) {
            if (guard_.pinned_)
                apply_mode(guard_.caller_mode_, false);
            else
                guard_.restore();
        }

        ~Suspend() {
            if (guard_.pinned_) {
                guard_.caller_mode_ = capture_mode();
#if defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64) || defined(__aarch64__)
                guard_.caller_mode_.hw_mode = (guard_.caller_mode_.hw_mode & ~k_denormal_mask) | guard_.entry_denormal_mode;
#endif
                apply_mode(guard_.pinned_mode_, true);
            } else {
                guard_.enable();
            }
        }

        Suspend(const Suspend &) = delete;
        Suspend &operator=(const Suspend &) = delete;
    private:
        ScopedDenormalGuard &guard_;
    };

    ScopedDenormalGuard(const ScopedDenormalGuard &) = delete;
    ScopedDenormalGuard &operator=(const ScopedDenormalGuard &) = delete;
private:
    bool pinned_ = false;
    Mode pinned_mode_;
    Mode caller_mode_;
};
} // namespace muse::audio
