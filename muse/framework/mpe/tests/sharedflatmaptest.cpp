// SPDX-License-Identifier: GPL-3.0-only
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "types/enumset.h"
#include "types/sharedflatmap.h"
#include "types/sharedmap.h"

using namespace muse;

TEST(SharedFlatMapTests, LazyEmptyAndCopyOnWrite) {
    SharedFlatMap<int, std::string> map;
    auto empty_copy = map;
    EXPECT_TRUE(map.unique()); // Both copies still have null handles, not a shared allocation.
    const auto &empty = map;
    EXPECT_EQ(empty.begin(), empty.end());
    EXPECT_EQ(empty.find(2), empty.end());
    EXPECT_EQ(empty.lower_bound(2), empty.end());
    EXPECT_EQ(empty.rbegin(), empty.rend());
    EXPECT_THROW(empty.at(2), std::out_of_range);
    map[2] = "two";
    EXPECT_TRUE(empty_copy.empty());
    auto copy = map;
    EXPECT_FALSE(map.unique());
    copy.at(2) = "changed";
    EXPECT_EQ(map.at(2), "two");
    copy.clear();
    EXPECT_TRUE(copy.empty());
    EXPECT_TRUE(copy.unique());
    EXPECT_EQ(copy, empty_copy);
    auto moved = std::move(copy);
    EXPECT_TRUE(std::as_const(copy).empty());
}

template <class Map>
void hint_oracle(Map &map) {
    map.insert(map.cbegin(), {2, "two"});
    map.insert(map.cend(), {4, "four"});
    map.insert(map.cbegin(), {0, "zero"});
    auto shared = map;
    map.emplace_hint(std::as_const(map).find(2), 3, "three");
    auto again = map;
    map.try_emplace(map.cbegin(), 0, "duplicate");
    auto third = map;
    map.try_emplace(map.cend(), 5, "five");
    map.insert({2, "duplicate"});
    map.emplace(2, "duplicate");
    map.insert_or_assign(4, "assigned");
    EXPECT_EQ(shared.size(), 3);
    EXPECT_EQ(shared.at(4), "four");
    EXPECT_EQ(map.at(0), "zero");
    EXPECT_EQ(map.at(2), "two");
}

TEST(SharedFlatMapTests, HintsAndDuplicatesMatchSharedMap) {
    SharedMap<int, std::string> old;
    SharedFlatMap<int, std::string> flat;
    hint_oracle(old);
    hint_oracle(flat);
    auto it = old.cbegin();
    for (const auto &entry : std::as_const(flat)) {
        ASSERT_NE(it, old.cend());
        EXPECT_EQ(entry.first, it->first);
        EXPECT_EQ(entry.second, it->second);
        ++it;
    }
    EXPECT_EQ(it, old.cend());
}

template <class Map>
void erase_merge_oracle() {
    Map map{{0, "zero"}, {1, "one"}, {2, "two"}, {3, "three"}};
    auto copy = map;
    map.erase(std::as_const(map).lower_bound(1), std::as_const(map).lower_bound(3));
    EXPECT_EQ(map.size(), 2);
    EXPECT_EQ(copy.size(), 4);
    auto copy2 = map;
    EXPECT_EQ(map.erase(map.cbegin())->first, 3);
    Map source{{3, "duplicate"}, {4, "four"}};
    auto source_copy = source;
    map.merge(source);
    EXPECT_EQ(map.at(3), "three");
    EXPECT_EQ(map.at(4), "four");
    EXPECT_EQ(source.size(), 1);
    EXPECT_EQ(source.at(3), "duplicate");
    EXPECT_EQ(source_copy.size(), 2);
}

TEST(SharedFlatMapTests, EraseAndMergeMatchSharedMap) {
    erase_merge_oracle<SharedMap<int, std::string>>();
    erase_merge_oracle<SharedFlatMap<int, std::string>>();
}

TEST(SharedFlatMapTests, RangeInsertAndInitializerDuplicates) {
    SharedFlatMap<int, int> map{{2, 20}, {1, 10}, {2, 99}};
    EXPECT_EQ(map.at(2), 20);
    auto copy = map;
    map.insert(map.cbegin(), map.cend());
    EXPECT_EQ(map, copy);
    map = {{5, 50}, {5, 99}};
    EXPECT_EQ(map.at(5), 50);
    map = {};
    EXPECT_TRUE(map.empty());
    map.erase(map.cbegin(), map.cend());
    EXPECT_TRUE(map.empty());
}

TEST(SharedFlatMapTests, EnumSetMembershipAndOrder) {
    enum class E { A, B, C, LAST };
    EnumSet<E, 3> set{E::C, E::A, E::C};
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(contains(set, E::A));
    EXPECT_FALSE(set.contains(E::B));
    auto it = set.begin();
    EXPECT_EQ(*it++, E::A);
    EXPECT_EQ(*it++, E::C);
    EXPECT_EQ(it, set.end());
}
