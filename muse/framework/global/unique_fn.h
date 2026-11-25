// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "fn.h"

namespace muse {

namespace detail {

// Rejects std::reference_wrapper<UniqueFn<SameSig>> (and const) to avoid libc++
// reference_wrapper converting-constructor constraint recursion.
template <class T, class Self>
inline constexpr bool is_self_ref_wrapper_v = false;

template <class T, class Self>
inline constexpr bool is_self_ref_wrapper_v<std::reference_wrapper<T>, Self> = std::is_same_v<std::remove_cv_t<T>, Self>;

template <class R, bool IsConst, bool IsNoexcept, class... Args>
struct UniqueFnBase {
    using is_nullable_callable = void;

    UniqueFnBase() noexcept = default;

    UniqueFnBase(std::nullptr_t) noexcept {}

    UniqueFnBase(UniqueFnBase &&other) noexcept {
        if (other.ops_) {
            std::memcpy(buf_, other.buf_, k_sbo_size);
            ops_ = std::exchange(other.ops_, nullptr);
        }
    }

    UniqueFnBase(const UniqueFnBase &) = delete;
    UniqueFnBase &operator=(const UniqueFnBase &) = delete;

    ~UniqueFnBase() {
        // A target destructor can install another target while the wrapper is being destroyed.
        while (ops_) {
            reset();
        }
    }

    UniqueFnBase &operator=(UniqueFnBase &&other) noexcept {
        if (this != &other) {
            const Ops *old_ops = ops_;
            void *old_ptr = nullptr;
            if (old_ops && old_ops->destroy) {
                std::memcpy(&old_ptr, buf_, sizeof(old_ptr));
            }
            if (other.ops_) {
                std::memcpy(buf_, other.buf_, k_sbo_size);
            }
            ops_ = std::exchange(other.ops_, nullptr);
            if (old_ops && old_ops->destroy) {
                old_ops->destroy(old_ptr);
            }
        }
        return *this;
    }

    template <class F>
    static consteval bool valid_target() {
        using Decayed = std::decay_t<F>;
        if constexpr (is_fn_ref_v<Decayed> || !std::is_constructible_v<Decayed, F>) {
            return false;
        } else {
            using Target = std::conditional_t<IsConst, const Decayed, Decayed>;
            constexpr bool invocable =
                IsNoexcept ? std::is_nothrow_invocable_r_v<R, Target &, Args...> : std::is_invocable_r_v<R, Target &, Args...>;
            if constexpr (!invocable) {
                return false;
            } else {
                return valid_callable_return<R, Target &, Args...>();
            }
        }
    }

    void reset() noexcept {
        if (const Ops *ops = std::exchange(ops_, nullptr)) {
            if (ops->destroy) {
                void *ptr = nullptr;
                std::memcpy(&ptr, buf_, sizeof(ptr));
                ops->destroy(ptr);
            }
        }
    }

    explicit operator bool() const noexcept { return ops_ != nullptr; }

private:
    static constexpr std::size_t k_sbo_size = 3 * sizeof(void *);
    static constexpr std::size_t k_sbo_align = alignof(void *);
    using InvokeByte = std::conditional_t<IsConst, const std::byte, std::byte>;

    struct Ops {
        R (*invoke)(InvokeByte *, Args &&...) noexcept(IsNoexcept);
        void (*destroy)(void *) noexcept;
    };

    // Trivially copyable, trivially move-constructible, and trivially destructible targets are
    // implicit-lifetime types that relocate via memcpy with no user code running during moves or reset.
    template <class T>
    static constexpr bool fits_inline = sizeof(T) <= k_sbo_size && alignof(T) <= k_sbo_align && std::is_trivially_copyable_v<T> &&
                                        std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>;

    template <class T, class Byte>
    static auto *target(Byte *buf) noexcept {
        using Target = std::conditional_t<std::is_const_v<Byte>, const T, T>;
        if constexpr (fits_inline<T>) {
            return std::launder(reinterpret_cast<Target *>(buf));
        } else {
            void *ptr = nullptr;
            std::memcpy(&ptr, buf, sizeof(ptr));
            return static_cast<Target *>(ptr);
        }
    }

    template <class T>
    static void destroy_heap(void *ptr) noexcept {
        ::delete static_cast<T *>(ptr);
    }

    template <class T>
    static constexpr Ops target_ops{
        [](InvokeByte *buf, Args &&...args) noexcept(IsNoexcept) -> R {
            return invoke_r_or_void<R>(*target<T>(buf), std::forward<Args>(args)...);
        },
        fits_inline<T> ? nullptr : &destroy_heap<T>,
    };

protected:
    alignas(k_sbo_align) std::byte buf_[k_sbo_size];
    const Ops *ops_ = nullptr;

    template <class F>
    void emplace(F &&fn) {
        if constexpr (NullableCallable<F>) {
            if (!fn) {
                return;
            }
        }

        using Decayed = std::decay_t<F>;
        if constexpr (fits_inline<Decayed>) {
            std::construct_at(reinterpret_cast<Decayed *>(buf_), std::forward<F>(fn));
        } else {
            void *ptr = ::new Decayed(std::forward<F>(fn));
            std::memcpy(buf_, &ptr, sizeof(ptr));
        }
        ops_ = &target_ops<Decayed>;
    }

    template <class F>
    void assign(F &&fn) {
        UniqueFnBase tmp;
        tmp.emplace(std::forward<F>(fn));
        *this = std::move(tmp);
    }
};

} // namespace detail

// Owning move-only callable with inline storage for small, trivially relocatable targets.
// Empty function pointers, member pointers, and nullable wrappers yield an empty UniqueFn.
template <class Sig>
struct UniqueFn;

template <class R, class... Args>
struct UniqueFn<R(Args...)> : detail::UniqueFnBase<R, false, false, Args...> {
    using Base = detail::UniqueFnBase<R, false, false, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::reset;

    UniqueFn() noexcept = default;
    UniqueFn(UniqueFn &&) noexcept = default;
    UniqueFn &operator=(UniqueFn &&) noexcept = default;

    UniqueFn &operator=(std::nullptr_t) noexcept {
        this->reset();
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn(F &&fn) {
        this->emplace(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) { return this->ops_->invoke(this->buf_, std::forward<Args>(args)...); }
};

template <class R, class... Args>
struct UniqueFn<R(Args...) const> : detail::UniqueFnBase<R, true, false, Args...> {
    using Base = detail::UniqueFnBase<R, true, false, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::reset;

    UniqueFn() noexcept = default;
    UniqueFn(UniqueFn &&) noexcept = default;
    UniqueFn &operator=(UniqueFn &&) noexcept = default;

    UniqueFn &operator=(std::nullptr_t) noexcept {
        this->reset();
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn(F &&fn) {
        this->emplace(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) const { return this->ops_->invoke(this->buf_, std::forward<Args>(args)...); }
};

template <class R, class... Args>
struct UniqueFn<R(Args...) noexcept> : detail::UniqueFnBase<R, false, true, Args...> {
    using Base = detail::UniqueFnBase<R, false, true, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::reset;

    UniqueFn() noexcept = default;
    UniqueFn(UniqueFn &&) noexcept = default;
    UniqueFn &operator=(UniqueFn &&) noexcept = default;

    UniqueFn &operator=(std::nullptr_t) noexcept {
        this->reset();
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn(F &&fn) {
        this->emplace(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) noexcept { return this->ops_->invoke(this->buf_, std::forward<Args>(args)...); }
};

template <class R, class... Args>
struct UniqueFn<R(Args...) const noexcept> : detail::UniqueFnBase<R, true, true, Args...> {
    using Base = detail::UniqueFnBase<R, true, true, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::reset;

    UniqueFn() noexcept = default;
    UniqueFn(UniqueFn &&) noexcept = default;
    UniqueFn &operator=(UniqueFn &&) noexcept = default;

    UniqueFn &operator=(std::nullptr_t) noexcept {
        this->reset();
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn(F &&fn) {
        this->emplace(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) const noexcept { return this->ops_->invoke(this->buf_, std::forward<Args>(args)...); }
};

} // namespace muse
