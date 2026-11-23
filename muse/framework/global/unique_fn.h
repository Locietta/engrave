// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>

#include <proxy/proxy.h>

#include "fn.h"

namespace muse {

namespace detail {

template <class Sig>
using UniqueFnFacade = pro::facade_builder::add_convention<pro::operator_dispatch<"()">, Sig>::build;

// Rejects std::reference_wrapper<UniqueFn<SameSig>> (and const) to avoid libc++
// reference_wrapper converting-constructor constraint recursion.
template <class T, class Self>
inline constexpr bool is_self_ref_wrapper_v = false;

template <class T, class Self>
inline constexpr bool is_self_ref_wrapper_v<std::reference_wrapper<T>, Self> = std::is_same_v<std::remove_cv_t<T>, Self>;

template <class ProxySig, class R, bool IsConst, bool IsNoexcept, class... Args>
struct UniqueFnBase {
    using is_nullable_callable = void;
    using Facade = UniqueFnFacade<ProxySig>;

    pro::proxy<Facade> storage_;

    UniqueFnBase() noexcept = default;

    UniqueFnBase(std::nullptr_t) noexcept {}

    UniqueFnBase(UniqueFnBase &&) noexcept = default;
    UniqueFnBase &operator=(UniqueFnBase &&) noexcept = default;
    UniqueFnBase(const UniqueFnBase &) = delete;
    UniqueFnBase &operator=(const UniqueFnBase &) = delete;

    template <class F>
    static consteval bool valid_target() {
        using Decayed = std::decay_t<F>;
        if constexpr (!std::is_constructible_v<Decayed, F>) {
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

    template <class F>
    void assign(F &&fn) {
        using Decayed = std::decay_t<F>;
        if constexpr (NullableCallable<F>) {
            if (!fn) {
                storage_.reset();
                return;
            }
        }
        if constexpr (!std::is_member_pointer_v<Decayed> && pro::proxiable_target<Decayed, Facade>) {
            storage_ = pro::make_proxy<Facade>(std::forward<F>(fn));
        } else {
            // Adapt member pointers and callables whose return type converts to R (or is
            // discarded for void) or whose signature differs by implicit conversions.
            if constexpr (IsConst) {
                storage_ = pro::make_proxy<Facade>([f = Decayed(std::forward<F>(fn))](Args &&...args) noexcept(IsNoexcept) -> R {
                    return invoke_r_or_void<R>(f, std::forward<Args>(args)...);
                });
            } else {
                storage_ = pro::make_proxy<Facade>([f = Decayed(std::forward<F>(fn))](Args &&...args) mutable noexcept(IsNoexcept) -> R {
                    return invoke_r_or_void<R>(f, std::forward<Args>(args)...);
                });
            }
        }
    }

    void reset() noexcept { storage_.reset(); }

    explicit operator bool() const noexcept { return storage_.has_value(); }
};

} // namespace detail

// Owning move-only callable (C++23 std::move_only_function semantics), backed by
// ngcpp-proxy while Linux xclang libc++ does not ship std::move_only_function.
// Empty function pointers, member pointers, standard function wrappers, and UniqueFn targets
// construct an empty UniqueFn.
template <class Sig>
struct UniqueFn;

template <class R, class... Args>
struct UniqueFn<R(Args...)> : detail::UniqueFnBase<R(Args &&...), R, false, false, Args...> {
    using Base = detail::UniqueFnBase<R(Args &&...), R, false, false, Args...>;
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
        this->assign(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) { return (*this->storage_)(std::forward<Args>(args)...); }
};

template <class R, class... Args>
struct UniqueFn<R(Args...) const> : detail::UniqueFnBase<R(Args &&...) const, R, true, false, Args...> {
    using Base = detail::UniqueFnBase<R(Args &&...) const, R, true, false, Args...>;
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
        this->assign(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) const { return (*this->storage_)(std::forward<Args>(args)...); }
};

template <class R, class... Args>
struct UniqueFn<R(Args...) noexcept> : detail::UniqueFnBase<R(Args &&...) noexcept, R, false, true, Args...> {
    using Base = detail::UniqueFnBase<R(Args &&...) noexcept, R, false, true, Args...>;
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
        this->assign(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) noexcept { return (*this->storage_)(std::forward<Args>(args)...); }
};

template <class R, class... Args>
struct UniqueFn<R(Args...) const noexcept> : detail::UniqueFnBase<R(Args &&...) const noexcept, R, true, true, Args...> {
    using Base = detail::UniqueFnBase<R(Args &&...) const noexcept, R, true, true, Args...>;
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
        this->assign(std::forward<F>(fn));
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, UniqueFn> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 !detail::is_self_ref_wrapper_v<std::remove_cvref_t<F>, UniqueFn> && Base::template valid_target<F>())
    UniqueFn &operator=(F &&fn) {
        this->assign(std::forward<F>(fn));
        return *this;
    }

    R operator()(Args... args) const noexcept { return (*this->storage_)(std::forward<Args>(args)...); }
};

} // namespace muse
