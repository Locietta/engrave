// SPDX-License-Identifier: GPL-3.0-only

#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "mpe/flatmap.h"

using namespace muse;
using namespace muse::mpe;

namespace {
struct ModuloLess {
    int modulus = 10;

    bool operator()(int lhs, int rhs) const { return (lhs % modulus) < (rhs % modulus); }
};
} // namespace

TEST(MpeFlatMapTest, StorageSelectionAndSortedRangeTraits) {
    using TrivialMap = FlatMap<int, int>;
    using NonTrivialMap = FlatMap<int, std::string>;
    using NestedMap = FlatMap<int, TrivialMap>;

    static_assert(TrivialMap::is_amc_storage);
    static_assert(std::is_same_v<TrivialMap::Storage, amc::vector<std::pair<int, int>>>);

    static_assert(!NonTrivialMap::is_amc_storage);
    static_assert(std::is_same_v<NonTrivialMap::Storage, std::vector<std::pair<int, std::string>>>);

    static_assert(!NestedMap::is_amc_storage);
    static_assert(std::is_same_v<NestedMap::Storage, std::vector<std::pair<int, TrivialMap>>>);

    static_assert(std::is_same_v<sorted_key_t<TrivialMap>, int>);
    static_assert(std::is_same_v<sorted_value_t<TrivialMap>, int>);
    static_assert(std::is_same_v<sorted_key_t<NonTrivialMap>, int>);
    static_assert(std::is_same_v<sorted_value_t<NonTrivialMap>, std::string>);

    TrivialMap map{{2, 20}, {1, 10}};
    ASSERT_EQ(map.size(), 2u);
    EXPECT_EQ(entry_key(*map.begin()), 1);
    EXPECT_EQ(entry_value(*map.begin()), 10);
}

TEST(MpeFlatMapTest, OrderingAndLookupBounds) {
    FlatMap<int, int> map;
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);

    map.insert({30, 300});
    map.insert({10, 100});
    map.insert({50, 500});
    map.insert({20, 200});
    map.insert({40, 400});

    EXPECT_FALSE(map.empty());
    ASSERT_EQ(map.size(), 5u);

    std::vector<std::pair<int, int>> actual(map.begin(), map.end());
    std::vector<std::pair<int, int>> expected{{10, 100}, {20, 200}, {30, 300}, {40, 400}, {50, 500}};
    EXPECT_EQ(actual, expected);

    std::vector<int> rev_keys;
    for (auto it = map.rbegin(); it != map.rend(); ++it) {
        rev_keys.push_back(it->first);
    }
    EXPECT_EQ(rev_keys, (std::vector<int>{50, 40, 30, 20, 10}));

    EXPECT_EQ(map.find(30)->second, 300);
    EXPECT_EQ(map.find(25), map.end());
    EXPECT_TRUE(map.contains(10));
    EXPECT_FALSE(map.contains(15));
    EXPECT_EQ(map.count(40), 1u);
    EXPECT_EQ(map.count(45), 0u);
    EXPECT_EQ(map.at(20), 200);
    EXPECT_THROW((void) map.at(99), std::out_of_range);

    EXPECT_EQ(map.lower_bound(5)->first, 10);
    EXPECT_EQ(map.lower_bound(20)->first, 20);
    EXPECT_EQ(map.lower_bound(25)->first, 30);
    EXPECT_EQ(map.lower_bound(60), map.end());

    EXPECT_EQ(map.upper_bound(5)->first, 10);
    EXPECT_EQ(map.upper_bound(20)->first, 30);
    EXPECT_EQ(map.upper_bound(25)->first, 30);
    EXPECT_EQ(map.upper_bound(50), map.end());
}

TEST(MpeFlatMapTest, InsertDoesNotOverwriteAndMutations) {
    FlatMap<int, std::string> map{{2, "two-first"}, {1, "one"}, {2, "two-duplicate"}, {3, "three"}};
    ASSERT_EQ(map.size(), 3u);
    EXPECT_EQ(map.at(2), "two-first");

    auto [it1, inserted1] = map.insert({2, "two-overwrite-attempt"});
    EXPECT_FALSE(inserted1);
    EXPECT_EQ(it1->first, 2);
    EXPECT_EQ(it1->second, "two-first");

    auto [it2, inserted2] = map.try_emplace(2, "two-try-emplace-attempt");
    EXPECT_FALSE(inserted2);
    EXPECT_EQ(it2->second, "two-first");

    auto [it3, inserted3] = map.try_emplace(0, "zero");
    EXPECT_TRUE(inserted3);
    EXPECT_EQ(it3->first, 0);
    EXPECT_EQ(it3->second, "zero");

    auto [it4, inserted4] = map.insert_or_assign(2, "two-updated");
    EXPECT_FALSE(inserted4);
    EXPECT_EQ(it4->second, "two-updated");
    EXPECT_EQ(map.at(2), "two-updated");

    auto [it5, inserted5] = map.insert_or_assign(4, "four");
    EXPECT_TRUE(inserted5);
    EXPECT_EQ(it5->first, 4);
    EXPECT_EQ(it5->second, "four");

    EXPECT_EQ(map[4], "four");
    map[5] = "five";
    EXPECT_EQ(map.at(5), "five");
    EXPECT_EQ(map.size(), 6u);

    FlatMap<int, int> trivial_map;
    EXPECT_THROW(trivial_map.reserve(static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1u), StoreLimit);
}

TEST(MpeFlatMapTest, EraseByKeyIteratorAndRange) {
    FlatMap<int, int> map{{1, 10}, {2, 20}, {3, 30}, {4, 40}, {5, 50}, {6, 60}};
    EXPECT_EQ(map.erase(99), 0u);
    EXPECT_EQ(map.erase(3), 1u);
    EXPECT_FALSE(map.contains(3));
    EXPECT_EQ(map.size(), 5u);

    auto next_it = map.erase(map.find(2));
    ASSERT_NE(next_it, map.end());
    EXPECT_EQ(next_it->first, 4);

    auto range_it = map.erase(map.find(4), map.find(6));
    ASSERT_NE(range_it, map.end());
    EXPECT_EQ(range_it->first, 6);

    std::vector<std::pair<int, int>> remaining(map.begin(), map.end());
    EXPECT_EQ(remaining, (std::vector<std::pair<int, int>>{{1, 10}, {6, 60}}));

    map.clear();
    EXPECT_TRUE(map.empty());
}

TEST(MpeFlatMapTest, CustomComparator) {
    FlatMap<int, int, std::greater<int>> desc;
    desc.insert({1, 10});
    desc.insert({3, 30});
    desc.insert({2, 20});
    desc.insert({2, 200});

    std::vector<std::pair<int, int>> actual(desc.begin(), desc.end());
    EXPECT_EQ(actual, (std::vector<std::pair<int, int>>{{3, 30}, {2, 20}, {1, 10}}));
    EXPECT_EQ(desc.lower_bound(2)->first, 2);
    EXPECT_EQ(desc.upper_bound(2)->first, 1);
    EXPECT_EQ(desc.find(3)->second, 30);
    EXPECT_EQ(desc.erase(2), 1u);
    EXPECT_FALSE(desc.contains(2));

    FlatMap<int, std::string, ModuloLess> mod_map(ModuloLess{10});
    EXPECT_TRUE(mod_map.insert({12, "two"}).second);
    EXPECT_TRUE(mod_map.insert({11, "one"}).second);
    EXPECT_FALSE(mod_map.insert({22, "duplicate-mod-two"}).second);
    ASSERT_EQ(mod_map.size(), 2u);
    EXPECT_EQ(mod_map.begin()->first, 11);
    EXPECT_EQ(mod_map.find(32)->second, "two");
}
