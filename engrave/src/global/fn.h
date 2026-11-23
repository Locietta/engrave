#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include <proxy/proxy.h>

namespace music {

template <class... Overloads>
using CallableFacade = pro::facade_builder::add_convention<pro::operator_dispatch<"()">, Overloads...>::build;

template <class... Overloads>
using UniqueFn = pro::proxy<CallableFacade<Overloads...>>;

template <class... Overloads>
UniqueFn<Overloads...> make_unique_fn(auto &&fn) {
    return pro::make_proxy<CallableFacade<Overloads...>>(std::forward<decltype(fn)>(fn));
}

// Non-owning callable view for synchronous parameters whose target outlives the call.
template <class... Overloads>
struct FnRef {
    using Facade = CallableFacade<Overloads...>;
    mutable pro::proxy_view<Facade> view;

    FnRef() = default;

    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FnRef> && pro::proxiable_target<std::remove_reference_t<F>, Facade>)
    FnRef(F &&fn) noexcept : view(pro::make_proxy_view<Facade>(fn)) {}

    explicit operator bool() const noexcept { return view.has_value(); }

    template <class... Args>
    decltype(auto) operator()(Args &&...args) const {
        return (*view)(std::forward<Args>(args)...);
    }
};

} // namespace music
