// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "audio/engine/internal/synthesizers/fluidsynth/soundmapping.h"
#include "global/btree.h"
#include "global/containers.h"
#include "global/flatmap.h"
#include "global/flatset.h"
#include "global/fn.h"
#include "global/hashmap.h"
#include "global/unique_fn.h"
#include "soundmapping_oracle_fixture.h"
#include "types/enumset.h"

using namespace muse;
using namespace muse::mpe;
using namespace muse::audio::synth;

namespace {
struct ModuloLess {
    int modulus = 10;

    bool operator()(int lhs, int rhs) const { return (lhs % modulus) < (rhs % modulus); }
};

std::vector<test::StoredChannelEntry> collect_stored_channels(const ChannelMap &channels) {
    std::vector<test::StoredChannelEntry> result;
    for (const auto &[layer, voice_map] : channels.data()) {
        for (const auto &[art, mapping] : voice_map) {
            result.push_back({layer, art, mapping.first, mapping.second});
        }
    }
    return result;
}
} // namespace

TEST(MpeFlatMapTest, AssociativeHelpersAcrossStorageTypes) {
    const auto check_removal = []<class Set>(Set set) {
        EXPECT_EQ(muse::remove_if(set, [](int key) { return key % 2 == 1; }), 3u);
        ASSERT_EQ(set.size(), 1u);
        EXPECT_TRUE(muse::contains(set, 2));
    };
    check_removal(muse::FlatSet<int>{1, 7, 2, 5});
    check_removal(muse::BTreeMultiSet<int>{1, 1, 2, 5});
    check_removal(muse::SegmentedHashSet<int>{1, 7, 2, 5});

    muse::BTreeMultiMap<int, int> entries{{2, 20}, {2, 21}, {4, 40}};
    EXPECT_EQ(muse::values(entries, 2), (std::vector<int>{20, 21}));
    EXPECT_EQ(muse::unique_keys(entries), (muse::FlatSet<int>{2, 4}));

    const muse::BTreeMap<int, int> ordered{{9, 90}, {3, 30}, {7, 70}};
    ASSERT_NE(muse::find_less(ordered, 7), ordered.end());
    EXPECT_EQ(muse::find_less(ordered, 7)->second, 30);
    ASSERT_NE(muse::find_less_or_equal(ordered, 7), ordered.end());
    EXPECT_EQ(muse::find_less_or_equal(ordered, 7)->second, 70);
}

TEST(MpeFlatMapTest, StorageSelectionAndSortedRangeTraits) {
    using TrivialMap = FlatMap<int, int>;
    using NonTrivialMap = FlatMap<int, std::string>;
    using NestedMap = FlatMap<int, TrivialMap>;

    static_assert(std::is_trivially_copyable_v<FlatMapEntry<int, int>>);
    static_assert(std::is_trivially_copyable_v<ChannelMap::ChannelMapping>);
    static_assert(TrivialMap::is_amc_storage);
    static_assert(std::is_same_v<TrivialMap::Storage, amc::vector<FlatMapEntry<int, int>>>);

    static_assert(!NonTrivialMap::is_amc_storage);
    static_assert(std::is_same_v<NonTrivialMap::Storage, std::vector<FlatMapEntry<int, std::string>>>);

    static_assert(!NestedMap::is_amc_storage);
    static_assert(std::is_same_v<NestedMap::Storage, std::vector<FlatMapEntry<int, TrivialMap>>>);

    static_assert(std::is_same_v<sorted_key_t<TrivialMap>, int>);
    static_assert(std::is_same_v<sorted_value_t<TrivialMap>, int>);
    static_assert(std::is_same_v<sorted_key_t<NonTrivialMap>, int>);
    static_assert(std::is_same_v<sorted_value_t<NonTrivialMap>, std::string>);

    TrivialMap map{{2, 20}, {1, 10}};
    ASSERT_EQ(map.size(), 2u);
    EXPECT_EQ(entry_key(*map.begin()), 1);
    EXPECT_EQ(entry_value(*map.begin()), 10);

    const auto &[first_key, first_val] = *map.begin();
    EXPECT_EQ(first_key, 1);
    EXPECT_EQ(first_val, 10);
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

    std::vector<FlatMapEntry<int, int>> actual(map.begin(), map.end());
    std::vector<FlatMapEntry<int, int>> expected{{10, 100}, {20, 200}, {30, 300}, {40, 400}, {50, 500}};
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
    EXPECT_THROW(std::ignore = map.at(99), std::out_of_range);

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

    std::vector<FlatMapEntry<int, int>> remaining(map.begin(), map.end());
    EXPECT_EQ(remaining, (std::vector<FlatMapEntry<int, int>>{{1, 10}, {6, 60}}));

    map.clear();
    EXPECT_TRUE(map.empty());
}

TEST(MpeFlatMapTest, CustomComparator) {
    FlatMap<int, int, std::greater<int>> desc;
    desc.insert({1, 10});
    desc.insert({3, 30});
    desc.insert({2, 20});
    desc.insert({2, 200});

    std::vector<FlatMapEntry<int, int>> actual(desc.begin(), desc.end());
    EXPECT_EQ(actual, (std::vector<FlatMapEntry<int, int>>{{3, 30}, {2, 20}, {1, 10}}));
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

TEST(MpeFlatMapTest, SoundMappingProgramTablesAndLookups) {
    struct CategoryOracle {
        SoundCategory category;
        const std::vector<test::ExpectedProgramEntry> *expected;
    };

    const std::vector<test::ExpectedProgramEntry> empty_expected;
    const CategoryOracle categories[] = {
        {SoundCategory::KEYBOARDS, &test::expected_keyboards_programs()},
        {SoundCategory::STRINGS, &test::expected_strings_programs()},
        {SoundCategory::WINDS, &test::expected_winds_programs()},
        {SoundCategory::PERCUSSIONS, &test::expected_percussions_programs()},
        {SoundCategory::VOICES, &test::expected_voices_programs()},
        {SoundCategory::UNDEFINED, &empty_expected},
    };

    for (const auto &[category, expected_ptr] : categories) {
        const auto &expected = *expected_ptr;
        const ProgramMapping &table = mapping_by_category(category);
        ASSERT_EQ(table.size(), expected.size()) << static_cast<int>(category);

        std::size_t idx = 0;
        for (auto it = table.begin(); it != table.end(); ++it, ++idx) {
            if (it != table.begin()) {
                EXPECT_TRUE(std::prev(it)->first < it->first);
            }
            const test::ExpectedProgramEntry &exp = expected[idx];
            EXPECT_EQ(it->first.id, exp.id);
            const std::vector<SoundSubCategory> actual_subs(it->first.sub_categories.begin(), it->first.sub_categories.end());
            EXPECT_EQ(actual_subs, exp.sub_categories);
            EXPECT_EQ(it->second, exp.programs);

            const SoundSubCategories sub_categories(exp.sub_categories.begin(), exp.sub_categories.end());
            const SoundMappingKey lookup_key(exp.id, sub_categories);
            const auto found = table.find(lookup_key);
            ASSERT_NE(found, table.end());
            EXPECT_EQ(found->second, exp.programs);
            EXPECT_EQ(table.at(lookup_key), exp.programs);

            const PlaybackSetupData setup(exp.id, category, SoundSubCategories(sub_categories));
            EXPECT_EQ(find_programs(setup), exp.programs);
        }
    }

    // Duplicate keys in initializer lists keep the first entry, matching single-key insertion
    const std::initializer_list<ProgramMapping::value_type> dup_init = {
        {{SoundId::LUTE, {SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{SoundId::LUTE, {SoundSubCategory::PLUCKED}}, {midi::Program(99, 99)}},
    };
    const ProgramMapping flat_dup(dup_init);
    muse::BTreeMap<SoundMappingKey, midi::Programs> tree_dup;
    for (const auto &entry : dup_init) {
        tree_dup.insert({entry.first, entry.second});
    }
    ASSERT_EQ(flat_dup.size(), tree_dup.size());
    EXPECT_EQ(flat_dup.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}), tree_dup.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}));

    // Unsorted subcategory lists normalize to sorted sets and compare lexicographically
    const SoundMappingKey key_a{SoundId::BELL, {SoundSubCategory::PLATE, SoundSubCategory::METAL}};
    const SoundMappingKey key_b{SoundId::BELL, {SoundSubCategory::METAL, SoundSubCategory::PLATE, SoundSubCategory::METAL}};
    EXPECT_EQ(key_a, key_b);
    EXPECT_FALSE(key_a < key_b);
    EXPECT_FALSE(key_b < key_a);
    EXPECT_EQ(mapping_by_category(SoundCategory::PERCUSSIONS).at(key_b), (midi::Programs{midi::Program(0, 11)}));

    // find_programs strips PRIMARY and SECONDARY subcategories on keys whose programs differ from fallback Program(0, 0)
    const PlaybackSetupData synth_primary(SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS,
                                          {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_RAIN, SoundSubCategory::PRIMARY});
    EXPECT_EQ(find_programs(synth_primary), (midi::Programs{midi::Program(0, 96)}));

    const PlaybackSetupData synth_secondary(SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS,
                                            {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_RAIN, SoundSubCategory::SECONDARY});
    EXPECT_EQ(find_programs(synth_secondary), (midi::Programs{midi::Program(0, 96)}));

    const PlaybackSetupData celesta_both(SoundId::CELESTA, SoundCategory::KEYBOARDS,
                                         {SoundSubCategory::PRIMARY, SoundSubCategory::SECONDARY});
    EXPECT_EQ(find_programs(celesta_both), (midi::Programs{midi::Program(0, 8)}));

    const PlaybackSetupData violin_secondary(SoundId::VIOLIN, SoundCategory::STRINGS,
                                             {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION, SoundSubCategory::SECONDARY});
    EXPECT_EQ(find_programs(violin_secondary), (midi::Programs{midi::Program(20, 48)}));

    const PlaybackSetupData unmapped(SoundId::UNDEFINED, SoundCategory::KEYBOARDS, {});
    EXPECT_EQ(find_programs(unmapped), (midi::Programs{midi::Program(0, 0)}));
}

TEST(MpeFlatMapTest, SoundMappingArticulationTablesPinContents) {
    for (const test::ExpectedArticulationTable &table_oracle : test::expected_articulation_tables()) {
        const ArticulationMapping &actual = articulation_sounds(table_oracle.setup);
        ASSERT_EQ(actual.size(), table_oracle.entries.size());

        std::vector<test::ExpectedArticulationEntry> actual_entries;
        for (const auto &[type, prog] : actual) {
            actual_entries.push_back({type, prog});
            ASSERT_TRUE(actual.contains(type));
            EXPECT_EQ(actual.at(type), prog);
        }
        EXPECT_EQ(actual_entries, table_oracle.entries);
        EXPECT_FALSE(actual.contains(ArticulationType::STANDARD));
    }
}

TEST(MpeFlatMapTest, ChannelMapSameProgramAndExhaustionReusePreserveOrder) {
    // 1. Full oracle trace pinned from main (d6691101b0) covering init, same-layer reuse (before & after),
    // existing hit, new-layer allocation to budget limit, real exhaustion fallback storing actual_program,
    // and other-layer reuse under exhaustion.
    const PlaybackSetupData violin_sec(SoundId::VIOLIN, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION},
                                       true);
    ChannelMap traced;
    std::vector<test::ChannelCallbackEntry> callbacks;
    traced.on_channel_added = [&](const midi::channel_t ch, const midi::Program &prog) { callbacks.push_back({ch, prog}); };
    traced.init(violin_sec, std::nullopt, 3);

    EXPECT_EQ(traced.channel_count(), 1u);
    EXPECT_EQ(callbacks, test::expected_channel_init_callbacks());
    EXPECT_EQ(collect_stored_channels(traced), test::expected_channel_init_stored());

    const auto &steps = test::expected_channel_trace_steps();
    ASSERT_FALSE(steps.empty());
    for (std::size_t i = 0; i < steps.size(); ++i) {
        const test::ChannelTraceStep &step = steps[i];
        NoteRecord note;
        note.staff_layer = step.staff;
        note.voice_layer = step.voice;
        const ArticulationRecord rec{0, 100, step.articulation};
        const midi::channel_t actual_ch = traced.resolve_channel_for_event(note, std::span<const ArticulationRecord>{&rec, 1});

        EXPECT_EQ(actual_ch, step.expected_channel) << "step " << i;
        EXPECT_EQ(traced.channel_count(), step.expected_channel_count) << "step " << i;
        EXPECT_EQ(callbacks, step.expected_callbacks) << "step " << i;
        EXPECT_EQ(collect_stored_channels(traced), step.expected_stored) << "step " << i;
    }

    // 2. Multi-articulation front-insertion shift/reallocation check with 16-channel budget
    ChannelMap channels;
    channels.init(violin_sec, std::nullopt, 16);
    ASSERT_EQ(channels.channel_count(), 1u);

    auto resolve = [&](staff_layer_idx_t staff, voice_layer_idx_t voice, ArticulationType type) {
        NoteRecord note;
        note.staff_layer = staff;
        note.voice_layer = voice;
        const ArticulationRecord rec{0, 100, type};
        return channels.resolve_channel_for_event(note, std::span<const ArticulationRecord>{&rec, 1});
    };

    ASSERT_LT(ArticulationType::PALM_MUTE, ArticulationType::PIZZICATO);
    ASSERT_LT(ArticulationType::TREMOLO8TH, ArticulationType::TREMOLO64TH);

    const midi::channel_t ch_trem64 = resolve(0, 0, ArticulationType::TREMOLO64TH);
    const midi::channel_t ch_pizz = resolve(0, 0, ArticulationType::PIZZICATO);
    const midi::channel_t ch_mute = resolve(0, 0, ArticulationType::MUTE);
    EXPECT_EQ(ch_trem64, 1u);
    EXPECT_EQ(ch_pizz, 2u);
    EXPECT_EQ(ch_mute, 3u);
    EXPECT_EQ(channels.channel_count(), 4u);

    EXPECT_EQ(resolve(0, 0, ArticulationType::PALM_MUTE), ch_pizz);
    EXPECT_EQ(resolve(0, 0, ArticulationType::SNAP_PIZZICATO), ch_pizz);
    EXPECT_EQ(resolve(0, 0, ArticulationType::TREMOLO8TH), ch_trem64);
    EXPECT_EQ(resolve(0, 0, ArticulationType::TREMOLO16TH), ch_trem64);
    EXPECT_EQ(resolve(0, 0, ArticulationType::TREMOLO32ND), ch_trem64);
    EXPECT_EQ(channels.channel_count(), 4u);
}

TEST(MpeFlatMapTest, EnumSetMembershipAndOrder) {
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

TEST(MpeFlatMapTest, ProfileReplacementKeepsExistingAppliedPattern) {
    ArticulationsProfile profile;
    const auto make_pattern = [](duration_percentage_t factor) {
        ArticulationPatternSegment segment;
        segment.arrangement_pattern.duration_factor_value = factor;
        return ArticulationPattern{{0, std::move(segment)}};
    };
    profile.set_pattern(ArticulationType::STANDARD, make_pattern(7500));
    ArticulationsProfile equal_profile;
    equal_profile.set_pattern(ArticulationType::STANDARD, make_pattern(7500));
    EXPECT_EQ(profile, equal_profile);
    EXPECT_TRUE(profile.pattern(ArticulationType::STACCATO).empty());

    const ArticulationAppliedData applied(
        ArticulationMeta(ArticulationType::STACCATO, profile.pattern_ptr(ArticulationType::STACCATO, ArticulationType::STANDARD), 0, 1000),
        0, hundred_percent);
    profile.set_pattern(ArticulationType::STANDARD, make_pattern(5000));
    EXPECT_NE(profile, equal_profile);
    EXPECT_EQ(applied.applied_pattern_segment().arrangement_pattern.duration_factor_value, 7500);
    EXPECT_EQ(profile.pattern(ArticulationType::STANDARD).begin()->second.arrangement_pattern.duration_factor_value, 5000);
}

namespace {
int free_add_one(int x) noexcept { return x + 1; }

struct NonMovableArg {
    int value = 0;

    explicit NonMovableArg(int v) noexcept : value(v) {}

    NonMovableArg(const NonMovableArg &) = delete;
    NonMovableArg(NonMovableArg &&) = delete;
};

struct MemberHolder {
    int field = 7;

    int doubled() const { return field * 2; }

    int bump(int delta) {
        field += delta;
        return field;
    }
};

void check_unique_fn_storage() {
    using AddressFn = UniqueFn<std::uintptr_t() const noexcept>;
    static_assert(sizeof(AddressFn) == 4 * sizeof(void *));
    static_assert(alignof(AddressFn) == alignof(void *));
    static_assert(std::is_nothrow_move_constructible_v<AddressFn>);
    static_assert(std::is_nothrow_move_assignable_v<AddressFn>);

    AddressFn small = [value = 17]() noexcept { return reinterpret_cast<std::uintptr_t>(&value); };
    const auto small_address = small();
    AddressFn moved_small = std::move(small);
    EXPECT_FALSE(small);
    EXPECT_NE(moved_small(), small_address);

    UniqueFn<int()> move_only = [value = std::make_unique<int>(19)] { return *value; };
    auto moved_only = std::move(move_only);
    EXPECT_FALSE(move_only);
    auto &same = moved_only;
    moved_only = std::move(same);
    ASSERT_TRUE(moved_only);
    EXPECT_EQ(moved_only(), 19);

    struct Lifetime {
        int &destructions;

        ~Lifetime() { ++destructions; }
    };

    struct LargeTarget {
        std::array<int, 32> values;
        std::unique_ptr<Lifetime> lifetime;

        int operator()(std::uintptr_t &address) const {
            address = reinterpret_cast<std::uintptr_t>(this);
            return values.front();
        }
    };

    static_assert(sizeof(LargeTarget) > 3 * sizeof(void *));

    int destructions = 0;
    {
        UniqueFn<int(std::uintptr_t &) const> large = LargeTarget{{23}, std::make_unique<Lifetime>(destructions)};
        std::uintptr_t address = 0;
        EXPECT_EQ(large(address), 23);
        auto moved_large = std::move(large);
        EXPECT_FALSE(large);
        std::uintptr_t moved_address = 0;
        EXPECT_EQ(moved_large(moved_address), 23);
        EXPECT_EQ(moved_address, address);
        EXPECT_EQ(destructions, 0);
    }
    EXPECT_EQ(destructions, 1);

    // Over-aligned target with sizeof <= SBO size still falls back to aligned global heap allocation.
    struct alignas(16) SmallAlignedTarget {
        std::uint64_t a = 1;
        std::uint64_t b = 2;

        std::uintptr_t operator()() const noexcept { return reinterpret_cast<std::uintptr_t>(this); }
    };

    static_assert(sizeof(SmallAlignedTarget) <= 3 * sizeof(void *));
    AddressFn small_aligned = SmallAlignedTarget{};
    const auto small_aligned_addr = small_aligned();
    EXPECT_EQ(small_aligned_addr % 16, 0u);
    AddressFn moved_small_aligned = std::move(small_aligned);
    EXPECT_FALSE(small_aligned);
    EXPECT_EQ(moved_small_aligned(), small_aligned_addr);

    struct alignas(64) AlignedTarget {
        std::uintptr_t operator()() const noexcept { return reinterpret_cast<std::uintptr_t>(this); }
    };

    AddressFn aligned = AlignedTarget{};
    const auto aligned_address = aligned();
    EXPECT_EQ(aligned_address % 64, 0u);
    AddressFn moved_aligned = std::move(aligned);
    EXPECT_FALSE(aligned);
    EXPECT_EQ(moved_aligned(), aligned_address);

    // Heap allocation uses ::new / ::delete and ignores class-specific operator new overrides.
    struct NullClassNewTarget {
        std::array<int, 16> pad{};

        static void *operator new(std::size_t) noexcept { return nullptr; }

        static void operator delete(void *) noexcept {}

        int operator()() const { return 73; }
    };

    struct DeletedClassNewTarget {
        std::array<int, 16> pad{};

        static void *operator new(std::size_t) = delete;
        static void operator delete(void *) = delete;

        int operator()() const { return 79; }
    };

    UniqueFn<int()> from_null_new = NullClassNewTarget{};
    ASSERT_TRUE(static_cast<bool>(from_null_new));
    EXPECT_EQ(from_null_new(), 73);
    UniqueFn<int()> from_deleted_new = DeletedClassNewTarget{};
    ASSERT_TRUE(static_cast<bool>(from_deleted_new));
    EXPECT_EQ(from_deleted_new(), 79);
}

void check_unique_fn_failed_replacement() {
    struct ThrowingTarget {
        bool reject_copy = false;

        ThrowingTarget() = default;

        ThrowingTarget(const ThrowingTarget &other) : reject_copy(other.reject_copy) {
            if (reject_copy) {
                throw std::runtime_error("copy failed");
            }
        }

        ThrowingTarget(ThrowingTarget &&) { throw std::runtime_error("move failed"); }

        int operator()() const noexcept { return 31; }
    };

    using Fn = UniqueFn<int() noexcept>;
    static_assert(std::is_nothrow_move_constructible_v<Fn>);
    static_assert(std::is_nothrow_move_assignable_v<Fn>);

    ThrowingTarget source;
    Fn original = source;
    Fn moved = std::move(original);
    EXPECT_FALSE(original);
    original = std::move(moved);
    EXPECT_FALSE(moved);
    ASSERT_TRUE(original);
    EXPECT_EQ(original(), 31);

    source.reject_copy = true;
    EXPECT_THROW(original = source, std::runtime_error);
    ASSERT_TRUE(original);
    EXPECT_EQ(original(), 31);
}

void check_unique_fn_reentrant_replacement() {
    using Fn = UniqueFn<int()>;

    struct OnDestroy {
        Fn *owner;
        int *destructions;
        void (*callback)(Fn &);

        OnDestroy(Fn &fn, int &count, void (*action)(Fn &)) : owner(&fn), destructions(&count), callback(action) {}

        OnDestroy(OnDestroy &&other) noexcept
            : owner(std::exchange(other.owner, nullptr)), destructions(std::exchange(other.destructions, nullptr)),
              callback(other.callback) {}

        ~OnDestroy() {
            if (destructions) {
                ++(*destructions);
            }
            if (owner) {
                callback(*owner);
            }
        }

        int operator()() const { return 11; }
    };

    int reset_destructions = 0;
    Fn fn;
    fn = OnDestroy(fn, reset_destructions, [](Fn &owner) {
        EXPECT_FALSE(owner);
        owner.reset();
        owner = [] { return 37; };
    });
    fn.reset();
    EXPECT_EQ(reset_destructions, 1);
    ASSERT_TRUE(fn);
    EXPECT_EQ(fn(), 37);

    // Move-assignment publishes incoming rhs before running the old target's destructor once.
    int assign_destructions = 0;
    fn = OnDestroy(fn, assign_destructions, [](Fn &owner) {
        ASSERT_TRUE(owner);
        EXPECT_EQ(owner(), 41);
        owner = [] { return 43; };
    });
    Fn incoming = [] { return 41; };
    fn = std::move(incoming);
    EXPECT_FALSE(incoming);
    EXPECT_EQ(assign_destructions, 1);
    ASSERT_TRUE(fn);
    EXPECT_EQ(fn(), 43);

    // A non-trivially-destructible target with defaulted move ctor must neither double-destroy nor corrupt storage on move-assign.
    struct ReentrantDefaultedMove {
        std::unique_ptr<int> live;
        Fn *owner;
        int *destructions;

        ReentrantDefaultedMove(std::unique_ptr<int> v, Fn *o, int *d) noexcept : live(std::move(v)), owner(o), destructions(d) {}

        ReentrantDefaultedMove(ReentrantDefaultedMove &&) noexcept = default;

        ~ReentrantDefaultedMove() {
            if (live) {
                if (destructions) {
                    ++(*destructions);
                }
                if (owner) {
                    EXPECT_TRUE(*owner);
                    EXPECT_EQ((*owner)(), 61);
                    *owner = [p = std::make_unique<int>(67)] { return *p; };
                }
            }
        }

        int operator()() const { return *live; }
    };

    int defaulted_destructions = 0;
    fn = ReentrantDefaultedMove(std::make_unique<int>(59), &fn, &defaulted_destructions);
    Fn rhs = [] { return 61; };
    fn = std::move(rhs);
    EXPECT_FALSE(rhs);
    EXPECT_EQ(defaulted_destructions, 1);
    ASSERT_TRUE(fn);
    EXPECT_EQ(fn(), 67);

    // Scope-exit ~UniqueFnBase drains heap targets installed re-entrantly during destruction.
    int scope_inner_destructions = 0;
    int scope_outer_destructions = 0;
    {
        Fn scoped;

        struct ScopeInstall {
            Fn *owner;
            int *outer_count;
            int *inner_count;

            ScopeInstall(Fn *o, int *outer, int *inner) noexcept : owner(o), outer_count(outer), inner_count(inner) {}

            ScopeInstall(ScopeInstall &&other) noexcept
                : owner(std::exchange(other.owner, nullptr)), outer_count(std::exchange(other.outer_count, nullptr)),
                  inner_count(other.inner_count) {}

            ~ScopeInstall() {
                if (outer_count) {
                    ++(*outer_count);
                }
                if (owner) {
                    int *count = inner_count;
                    *owner = [guard = std::shared_ptr<int>(nullptr, [count](int *) { ++(*count); })] { return guard ? 1 : 0; };
                }
            }

            int operator()() const { return 0; }
        };

        scoped = ScopeInstall(&scoped, &scope_outer_destructions, &scope_inner_destructions);
    }
    EXPECT_EQ(scope_outer_destructions, 1);
    EXPECT_EQ(scope_inner_destructions, 1);

    fn = [&fn] {
        fn.reset();
        return 47;
    };
    EXPECT_EQ(fn(), 47);
    EXPECT_FALSE(fn);

    fn = [&fn] {
        fn = [] { return 53; };
        return 51;
    };
    EXPECT_EQ(fn(), 51);
    ASSERT_TRUE(fn);
    EXPECT_EQ(fn(), 53);
}
} // namespace

TEST(MpeFlatMapTest, FnRefAndUniqueFnSemantics) {
    static_assert(std::is_trivially_copyable_v<FnRef<int(int)>>);
    static_assert(sizeof(FnRef<int(int)>) == 2 * sizeof(void *));

    // Qualification widening is allowed; narrowing const/noexcept is rejected.
    static_assert(std::is_constructible_v<FnRef<int(int)>, FnRef<int(int) const>>);
    static_assert(std::is_constructible_v<FnRef<int(int)>, FnRef<int(int) noexcept>>);
    static_assert(std::is_constructible_v<FnRef<int(int)>, FnRef<int(int) const noexcept>>);
    static_assert(std::is_constructible_v<FnRef<int(int) const>, FnRef<int(int) const noexcept>>);
    static_assert(std::is_constructible_v<FnRef<int(int) noexcept>, FnRef<int(int) const noexcept>>);
    static_assert(!std::is_constructible_v<FnRef<int(int) const>, FnRef<int(int)>>);
    static_assert(!std::is_constructible_v<FnRef<int(int) noexcept>, FnRef<int(int)>>);

    // Dangling reference-to-temporary returns and runtime member pointers are rejected.
    auto returns_value = []() -> int { return 5; };
    static_assert(!std::is_constructible_v<FnRef<const int &()>, decltype(returns_value)>);
    static_assert(!std::is_constructible_v<UniqueFn<const int &()>, decltype(returns_value)>);
    static_assert(!std::is_constructible_v<FnRef<int(MemberHolder &)>, int MemberHolder::*>);
    static_assert(!std::is_assignable_v<FnRef<int(int)>, decltype(free_add_one)>);

    FnRef<int(int)> empty_ref;
    EXPECT_FALSE(static_cast<bool>(empty_ref));
    FnRef<int(int)> null_ref = nullptr;
    EXPECT_FALSE(static_cast<bool>(null_ref));

    // Cross-specialization conversion copies the erased binding directly (preserves emptiness and avoids dangling).
    FnRef<int(int) const> empty_const_ref;
    FnRef<int(int)> widened_empty = empty_const_ref;
    EXPECT_FALSE(static_cast<bool>(widened_empty));

    FnRef<int(int)> widened_from_temp = FnRef<int(int) const noexcept>(free_add_one);
    ASSERT_TRUE(static_cast<bool>(widened_from_temp));
    EXPECT_EQ(widened_from_temp(41), 42);

    int (*null_fp)(int) = nullptr;
    FnRef<int(int)> from_null_fp = null_fp;
    EXPECT_FALSE(static_cast<bool>(from_null_fp));

    std::function<int(int)> empty_std_fn;
    FnRef<int(int)> from_empty_std = empty_std_fn;
    EXPECT_FALSE(static_cast<bool>(from_empty_std));
    UniqueFn<int(int)> owned_from_empty_std = empty_std_fn;
    EXPECT_FALSE(static_cast<bool>(owned_from_empty_std));

    // Prvalue non-movable arguments pass through the erased thunk with zero extra moves.
    int seen_non_movable = 0;
    auto take_non_movable = [&](NonMovableArg &&arg) noexcept { seen_non_movable = arg.value; };
    FnRef<void(NonMovableArg) noexcept> nm_ref = take_non_movable;
    nm_ref(NonMovableArg(19));
    EXPECT_EQ(seen_non_movable, 19);
    UniqueFn<void(NonMovableArg) noexcept> nm_owned = take_non_movable;
    nm_owned(NonMovableArg(23));
    EXPECT_EQ(seen_non_movable, 23);

    int counter = 0;
    auto mutable_lambda = [n = 0, &counter](int step) mutable {
        n += step;
        counter = n;
        return n;
    };
    FnRef<int(int)> mut_ref = mutable_lambda;
    EXPECT_EQ(mut_ref(3), 3);
    EXPECT_EQ(mut_ref(4), 7);
    EXPECT_EQ(counter, 7);

    // Non-void return adapted into FnRef<void(...)> and UniqueFn<void(...)>.
    int void_side_effect = 0;
    auto bump_and_return = [&](int step) {
        void_side_effect += step;
        return void_side_effect;
    };
    FnRef<void(int)> discard_ref = bump_and_return;
    ASSERT_TRUE(static_cast<bool>(discard_ref));
    discard_ref(5);
    EXPECT_EQ(void_side_effect, 5);

    UniqueFn<void(int)> void_owned = bump_and_return;
    ASSERT_TRUE(static_cast<bool>(void_owned));
    void_owned(7);
    EXPECT_EQ(void_side_effect, 12);

    // Null member pointer yields empty UniqueFn; engaged member pointers invoke cleanly.
    int MemberHolder::*null_mp = nullptr;
    UniqueFn<int(MemberHolder &)> mp_empty = null_mp;
    EXPECT_FALSE(static_cast<bool>(mp_empty));
    UniqueFn<int(MemberHolder &)> mp_engaged = &MemberHolder::field;
    ASSERT_TRUE(static_cast<bool>(mp_engaged));
    MemberHolder holder{11};
    EXPECT_EQ(mp_engaged(holder), 11);

    int (MemberHolder::*null_mfp)() const = nullptr;
    UniqueFn<int(const MemberHolder &) const> mfp_empty = null_mfp;
    EXPECT_FALSE(static_cast<bool>(mfp_empty));
    UniqueFn<int(const MemberHolder &) const> mfp_const = &MemberHolder::doubled;
    EXPECT_EQ(mfp_const(holder), 22);
    UniqueFn<int(MemberHolder &, int)> mfp_mut = &MemberHolder::bump;
    EXPECT_EQ(mfp_mut(holder, 4), 15);
    EXPECT_EQ(holder.field, 15);

    std::function<int(int)> live_std_fn = [](int x) { return x + 9; };
    UniqueFn<int(int)> from_live_std = live_std_fn;
    ASSERT_TRUE(static_cast<bool>(from_live_std));
    EXPECT_EQ(from_live_std(3), 12);
    UniqueFn<int(int) const> strict_unique = [](int x) { return x * 5; };
    UniqueFn<int(int)> widened_unique = std::move(strict_unique);
    EXPECT_FALSE(static_cast<bool>(strict_unique));
    ASSERT_TRUE(static_cast<bool>(widened_unique));
    EXPECT_EQ(widened_unique(4), 20);

    struct ConstOverload {
        int operator()() { return 1; }

        int operator()() const { return 2; }
    };

    UniqueFn<int()> call_non_const = ConstOverload{};
    const UniqueFn<int() const> call_const = ConstOverload{};
    EXPECT_EQ(call_non_const(), 1);
    EXPECT_EQ(call_const(), 2);

    auto mutable_only = [n = 0]() mutable { return ++n; };
    auto throwing_fn = []() { return 1; };
    static_assert(!std::is_constructible_v<UniqueFn<int() const>, decltype(mutable_only)>);
    static_assert(!std::is_constructible_v<UniqueFn<int() noexcept>, decltype(throwing_fn)>);

    // UniqueFn supports implicit lambda conversion, move-only captures, null reset, and const callables.
    UniqueFn<int(int)> empty_owned;
    EXPECT_FALSE(static_cast<bool>(empty_owned));
    FnRef<int(int)> ref_to_empty_owned = empty_owned;
    EXPECT_FALSE(static_cast<bool>(ref_to_empty_owned));

    auto ptr = std::make_unique<int>(10);
    UniqueFn<int(int)> owned = [p = std::move(ptr)](int x) mutable {
        *p += x;
        return *p;
    };
    ASSERT_TRUE(static_cast<bool>(owned));
    EXPECT_EQ(owned(5), 15);
    FnRef<int(int)> ref_to_owned = owned;
    ASSERT_TRUE(static_cast<bool>(ref_to_owned));
    EXPECT_EQ(ref_to_owned(7), 22);

    // std::ref unwraps to the underlying target rather than binding the temporary wrapper.
    int ref_acc = 10;
    auto ref_lambda = [&ref_acc](int x) {
        ref_acc += x;
        return ref_acc;
    };
    FnRef<int(int)> unwrapped_ref = std::ref(ref_lambda);
    EXPECT_EQ(unwrapped_ref(5), 15);
    EXPECT_EQ(ref_acc, 15);

    owned = nullptr;
    EXPECT_FALSE(static_cast<bool>(owned));

    const UniqueFn<int(int) const> const_owned = [base = 100](int x) { return base + x; };
    ASSERT_TRUE(static_cast<bool>(const_owned));
    EXPECT_EQ(const_owned(23), 123);
    FnRef<int(int) const> const_ref = const_owned;
    ASSERT_TRUE(static_cast<bool>(const_ref));
    EXPECT_EQ(const_ref(7), 107);

    static_assert(!std::is_constructible_v<UniqueFn<int(int)>, FnRef<int(int)>>);
    static_assert(!std::is_constructible_v<UniqueFn<int(int) const>, FnRef<int(int)>>);
    static_assert(!std::is_constructible_v<UniqueFn<int(int) const>, FnRef<int(int) const>>);
    static_assert(!std::is_assignable_v<UniqueFn<int(int)> &, FnRef<int(int)>>);

    UniqueFn<int(int)> null_fp_owned = null_fp;
    EXPECT_FALSE(static_cast<bool>(null_fp_owned));

    UniqueFn<int(int)> from_empty_unique = UniqueFn<int(int) const>{};
    EXPECT_FALSE(from_empty_unique);
    check_unique_fn_storage();
    check_unique_fn_failed_replacement();
    check_unique_fn_reentrant_replacement();
}

TEST(MpeFlatMapTest, VocabularyContainerShimsAndTransparency) {
    // HashMap / HashSet transparent std::string_view lookup.
    HashMap<std::string, int> hmap{{"alpha", 1}, {"beta", 2}};
    const std::string_view sv = "beta";
    ASSERT_TRUE(hmap.contains(sv));
    ASSERT_NE(hmap.find(sv), hmap.end());
    EXPECT_EQ(hmap.find(sv)->second, 2);
    EXPECT_EQ(muse::value(hmap, sv), 2);
    EXPECT_EQ(muse::value(hmap, std::string_view("missing"), 99), 99);
    ASSERT_NE(hmap.find("alpha"), hmap.end());
    EXPECT_EQ(hmap.find("alpha")->second, 1);

    HashSet<std::string> hset{"one", "two"};
    EXPECT_TRUE(hset.contains(std::string_view("one")));
    EXPECT_FALSE(hset.contains("three"));

    // BTreeMap insert_or_assign: new key, overwrite, move-only value, ADL swap, and self-aliasing across splits.
    BTreeMap<int, std::string> bmap;
    auto [it1, ins1] = bmap.insert_or_assign(10, std::string("ten"));
    EXPECT_TRUE(ins1);
    EXPECT_EQ(it1->second, "ten");
    auto [it2, ins2] = bmap.insert_or_assign(10, std::string("TEN"));
    EXPECT_FALSE(ins2);
    EXPECT_EQ(it2->second, "TEN");

    for (int i = 0; i < 128; ++i) {
        const auto [it, inserted] = bmap.insert_or_assign(1000 + i, bmap.at(10));
        EXPECT_TRUE(inserted);
        EXPECT_EQ(it->second, "TEN");
    }

    BTreeMap<int, std::string> bmap_copy = bmap;
    EXPECT_EQ(bmap, bmap_copy);
    bmap_copy.insert_or_assign(10, std::string("changed"));
    EXPECT_NE(bmap, bmap_copy);
    using std::swap;
    swap(bmap, bmap_copy);
    EXPECT_EQ(bmap.at(10), "changed");
    EXPECT_EQ(bmap_copy.at(10), "TEN");

    BTreeMap<int, std::unique_ptr<int>> move_only_map;
    auto [mit1, mins1] = move_only_map.insert_or_assign(1, std::make_unique<int>(7));
    EXPECT_TRUE(mins1);
    EXPECT_EQ(*mit1->second, 7);
    auto [mit2, mins2] = move_only_map.insert_or_assign(1, std::make_unique<int>(9));
    EXPECT_FALSE(mins2);
    EXPECT_EQ(*mit2->second, 9);

    FlatSet<int> fset{5, 1, 3, 1};
    EXPECT_EQ(fset.size(), 3u);
    EXPECT_TRUE(fset.contains(3));
    EXPECT_FALSE(fset.contains(2));
}
