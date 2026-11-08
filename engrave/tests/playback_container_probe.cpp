// SPDX-License-Identifier: GPL-3.0-only
#include <cstdint>
#include <flat_map>
#include <iostream>
#include <stdexcept>
#include <type_traits>

#include <amc/vector.hpp>
#include <ankerl/unordered_dense.h>
#include <proxy/proxy.h>

namespace {
void check(bool value) {
    if (!value) throw std::runtime_error("container probe failed");
}

struct MoveWitness {
    inline static int moves = 0;
    const MoveWitness *self = this;
    MoveWitness() = default;
    MoveWitness(const MoveWitness &) = delete;

    MoveWitness(MoveWitness &&other) noexcept : self(this) {
        check(other.self == &other);
        ++moves;
    }

    ~MoveWitness() {
        if (self != this) std::terminate();
    }
};

PRO_DEF_MEM_DISPATCH(MemValue, value);

struct ValueFacade : pro::facade_builder::add_convention<MemValue, int() const>::build {};

struct Value {
    int value() const { return 42; }
};
} // namespace

int main() {
    static_assert(!std::is_trivially_copyable_v<MoveWitness>);
    static_assert(std::is_same_v<amc::vector<int>::size_type, std::uint32_t>);
    static_assert(__cpp_lib_flat_map >= 202207L);
    amc::vector<MoveWitness> values;
    values.emplace_back();
    values.reserve(64);
    check(MoveWitness::moves > 0 && values.front().self == &values.front());
    // Counts are narrowed BEFORE amc is entered: checked_grow must guard this.
    constexpr std::uint64_t too_large = std::uint64_t{1} << 32;
    check(static_cast<amc::vector<int>::size_type>(too_large) == 0);
    std::flat_map<int, int> flat{{2, 20}, {1, 10}};
    check(flat.begin()->first == 1);
    ankerl::unordered_dense::set<int> seen;
    seen.insert(3);
    check(seen.contains(3));
    pro::proxy<ValueFacade> proxy = pro::make_proxy<ValueFacade, Value>();
    check(proxy->value() == 42);
    std::cout << "flat_map=" << __cpp_lib_flat_map << " cxx=" << __cplusplus << " move_calls=" << MoveWitness::moves
              << " uint32_narrowing=0 unordered_dense=ok proxy=42\n";
}
