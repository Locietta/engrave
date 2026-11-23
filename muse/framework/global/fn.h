// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace muse {

template <class Sig>
struct FnRef;

namespace detail {

template <class T>
inline constexpr bool is_fn_ref_v = false;

template <class S>
inline constexpr bool is_fn_ref_v<FnRef<S>> = true;

template <class T>
inline constexpr bool is_std_function_v = false;

template <class S>
inline constexpr bool is_std_function_v<std::function<S>> = true;

template <class T>
inline constexpr bool is_ref_wrapper_v = false;

template <class T>
inline constexpr bool is_ref_wrapper_v<std::reference_wrapper<T>> = true;

template <class F>
concept NullableCallable = std::is_pointer_v<std::remove_cvref_t<F>> || std::is_member_pointer_v<std::remove_cvref_t<F>> ||
                           is_std_function_v<std::remove_cvref_t<F>> || requires { typename std::remove_cvref_t<F>::is_nullable_callable; };

template <class R, class F, class... Args>
consteval bool valid_callable_return() {
    if constexpr (std::is_void_v<R>) {
        return true;
    } else {
        using Result = std::invoke_result_t<F, Args...>;
#if defined(__cpp_lib_reference_from_temporary) && __cpp_lib_reference_from_temporary >= 202202L
        return !std::reference_converts_from_temporary_v<R, Result>;
#elif defined(__clang__)
        return !__reference_converts_from_temporary(R, Result);
#else
        return !(std::is_reference_v<R> && !std::is_reference_v<Result>);
#endif
    }
}

union FnRefStorage {
    void *obj = nullptr;
    void (*fn)();
};

template <class R, class... Args>
constexpr R invoke_r_or_void(auto &&fn, Args &&...args) {
    if constexpr (std::is_void_v<R>) {
        std::invoke(std::forward<decltype(fn)>(fn), std::forward<Args>(args)...);
    } else {
        return std::invoke(std::forward<decltype(fn)>(fn), std::forward<Args>(args)...);
    }
}

template <class R, bool IsConst, bool IsNoexcept, class... Args>
struct FnRefBase {
    using InvokeFn = R (*)(FnRefStorage, Args &&...) noexcept(IsNoexcept);

    FnRefStorage storage_{};
    InvokeFn invoke_ = nullptr;

    constexpr FnRefBase() noexcept = default;

    constexpr FnRefBase(std::nullptr_t) noexcept {}

    template <class F>
    static consteval bool valid_target() {
        using Raw = std::remove_reference_t<F>;
        using Decayed = std::decay_t<F>;
        if constexpr (is_fn_ref_v<std::remove_cvref_t<F>> || std::is_member_pointer_v<Decayed> || std::is_volatile_v<Raw>) {
            return false;
        } else if constexpr (is_ref_wrapper_v<std::remove_cvref_t<F>>) {
            return valid_target<typename std::remove_cvref_t<F>::type &>();
        } else if constexpr (std::is_function_v<Raw> ||
                             (std::is_pointer_v<Decayed> && std::is_function_v<std::remove_pointer_t<Decayed>>) ) {
            constexpr bool invocable =
                IsNoexcept ? std::is_nothrow_invocable_r_v<R, Decayed, Args...> : std::is_invocable_r_v<R, Decayed, Args...>;
            if constexpr (!invocable) {
                return false;
            } else {
                return valid_callable_return<R, Decayed, Args...>();
            }
        } else {
            using Target = std::conditional_t<IsConst, const std::remove_const_t<Raw>, Raw>;
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
    void bind(F &&fn) noexcept {
        using Raw = std::remove_reference_t<F>;
        using Decayed = std::decay_t<F>;

        if constexpr (is_ref_wrapper_v<std::remove_cvref_t<F>>) {
            bind(fn.get());
        } else if constexpr (std::is_function_v<Raw>) {
            using FnPtr = Raw *;
            storage_.fn = reinterpret_cast<void (*)()>(&fn);
            invoke_ = [](FnRefStorage s, Args &&...args) noexcept(IsNoexcept) -> R {
                return invoke_r_or_void<R>(reinterpret_cast<FnPtr>(s.fn), std::forward<Args>(args)...);
            };
        } else if constexpr (std::is_pointer_v<Decayed> && std::is_function_v<std::remove_pointer_t<Decayed>>) {
            if (!fn) {
                storage_.fn = nullptr;
                invoke_ = nullptr;
                return;
            }
            storage_.fn = reinterpret_cast<void (*)()>(fn);
            invoke_ = [](FnRefStorage s, Args &&...args) noexcept(IsNoexcept) -> R {
                return invoke_r_or_void<R>(reinterpret_cast<Decayed>(s.fn), std::forward<Args>(args)...);
            };
        } else {
            if constexpr (NullableCallable<F>) {
                if (!fn) {
                    storage_.obj = nullptr;
                    invoke_ = nullptr;
                    return;
                }
            }
            using Target = std::conditional_t<IsConst, const std::remove_const_t<Raw>, Raw>;
            storage_.obj = const_cast<void *>(static_cast<const void *>(std::addressof(fn)));
            invoke_ = [](FnRefStorage s, Args &&...args) noexcept(IsNoexcept) -> R {
                return invoke_r_or_void<R>(*static_cast<Target *>(s.obj), std::forward<Args>(args)...);
            };
        }
    }

    constexpr explicit operator bool() const noexcept { return invoke_ != nullptr; }

    constexpr R operator()(Args... args) const noexcept(IsNoexcept) { return invoke_(storage_, std::forward<Args>(args)...); }
};

} // namespace detail

// Non-owning, trivially copyable callable view for synchronous callbacks whose
// target (and, when viewing a UniqueFn/std::function, its engaged state) outlives
// the call (C++26 std::function_ref semantics).
template <class Sig>
struct FnRef;

template <class R, class... Args>
struct FnRef<R(Args...)> : detail::FnRefBase<R, false, false, Args...> {
    using Base = detail::FnRefBase<R, false, false, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::operator();

    constexpr FnRef() noexcept = default;
    constexpr FnRef(const FnRef &) noexcept = default;
    constexpr FnRef &operator=(const FnRef &) noexcept = default;

    constexpr FnRef &operator=(std::nullptr_t) noexcept {
        this->storage_.obj = nullptr;
        this->invoke_ = nullptr;
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t>)
    FnRef &operator=(F &&) = delete;

    // Direct rebind from stricter-qualified FnRef specializations without wrapping the view.
    constexpr FnRef(const FnRef<R(Args...) const> &other) noexcept {
        this->storage_ = other.storage_;
        this->invoke_ = other.invoke_;
    }

    constexpr FnRef(const FnRef<R(Args...) noexcept> &other) noexcept {
        this->storage_ = other.storage_;
        this->invoke_ = other.invoke_;
    }

    constexpr FnRef(const FnRef<R(Args...) const noexcept> &other) noexcept {
        this->storage_ = other.storage_;
        this->invoke_ = other.invoke_;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 Base::template valid_target<F>())
    FnRef(F &&fn) noexcept {
        this->bind(std::forward<F>(fn));
    }
};

template <class R, class... Args>
struct FnRef<R(Args...) const> : detail::FnRefBase<R, true, false, Args...> {
    using Base = detail::FnRefBase<R, true, false, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::operator();

    constexpr FnRef() noexcept = default;
    constexpr FnRef(const FnRef &) noexcept = default;
    constexpr FnRef &operator=(const FnRef &) noexcept = default;

    constexpr FnRef &operator=(std::nullptr_t) noexcept {
        this->storage_.obj = nullptr;
        this->invoke_ = nullptr;
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t>)
    FnRef &operator=(F &&) = delete;

    constexpr FnRef(const FnRef<R(Args...) const noexcept> &other) noexcept {
        this->storage_ = other.storage_;
        this->invoke_ = other.invoke_;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 Base::template valid_target<F>())
    FnRef(F &&fn) noexcept {
        this->bind(std::forward<F>(fn));
    }
};

template <class R, class... Args>
struct FnRef<R(Args...) noexcept> : detail::FnRefBase<R, false, true, Args...> {
    using Base = detail::FnRefBase<R, false, true, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::operator();

    constexpr FnRef() noexcept = default;
    constexpr FnRef(const FnRef &) noexcept = default;
    constexpr FnRef &operator=(const FnRef &) noexcept = default;

    constexpr FnRef &operator=(std::nullptr_t) noexcept {
        this->storage_.obj = nullptr;
        this->invoke_ = nullptr;
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t>)
    FnRef &operator=(F &&) = delete;

    constexpr FnRef(const FnRef<R(Args...) const noexcept> &other) noexcept {
        this->storage_ = other.storage_;
        this->invoke_ = other.invoke_;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 Base::template valid_target<F>())
    FnRef(F &&fn) noexcept {
        this->bind(std::forward<F>(fn));
    }
};

template <class R, class... Args>
struct FnRef<R(Args...) const noexcept> : detail::FnRefBase<R, true, true, Args...> {
    using Base = detail::FnRefBase<R, true, true, Args...>;
    using Base::Base;
    using Base::operator bool;
    using Base::operator();

    constexpr FnRef() noexcept = default;
    constexpr FnRef(const FnRef &) noexcept = default;
    constexpr FnRef &operator=(const FnRef &) noexcept = default;

    constexpr FnRef &operator=(std::nullptr_t) noexcept {
        this->storage_.obj = nullptr;
        this->invoke_ = nullptr;
        return *this;
    }

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t>)
    FnRef &operator=(F &&) = delete;

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && !std::same_as<std::remove_cvref_t<F>, std::nullptr_t> &&
                 Base::template valid_target<F>())
    FnRef(F &&fn) noexcept {
        this->bind(std::forward<F>(fn));
    }
};

} // namespace muse
