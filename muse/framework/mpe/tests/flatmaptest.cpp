// SPDX-License-Identifier: GPL-3.0-only

#include <algorithm>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "audio/engine/internal/synthesizers/fluidsynth/soundmapping.h"
#include "global/flatmap.h"
#include "soundmapping_oracle_fixture.h"

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

            const SoundSubCategories std_subs(exp.sub_categories.begin(), exp.sub_categories.end());
            const SoundMappingKey lookup_key(exp.id, std_subs);
            const auto found = table.find(lookup_key);
            ASSERT_NE(found, table.end());
            EXPECT_EQ(found->second, exp.programs);
            EXPECT_EQ(table.at(lookup_key), exp.programs);

            const PlaybackSetupData setup(exp.id, category, SoundSubCategories(std_subs));
            EXPECT_EQ(find_programs(setup), exp.programs);
        }
    }

    // Duplicate keys in initializer lists keep the first entry, matching std::map::insert
    const std::initializer_list<ProgramMapping::value_type> dup_init = {
        {{SoundId::LUTE, {SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{SoundId::LUTE, {SoundSubCategory::PLUCKED}}, {midi::Program(99, 99)}},
    };
    const ProgramMapping flat_dup(dup_init);
    std::map<SoundMappingKey, midi::Programs> std_dup;
    for (const auto &entry : dup_init) {
        std_dup.insert({entry.first, entry.second});
    }
    ASSERT_EQ(flat_dup.size(), std_dup.size());
    EXPECT_EQ(flat_dup.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}), std_dup.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}));

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
