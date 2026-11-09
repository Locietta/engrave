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
#include "mpe/flatmap.h"

using namespace muse;
using namespace muse::mpe;
using namespace muse::audio::synth;

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

TEST(MpeFlatMapTest, SoundMappingProgramTablesAndLookups) {
    struct CategoryExpectation {
        SoundCategory category;
        std::size_t expected_size;
    };

    const CategoryExpectation categories[] = {
        {SoundCategory::KEYBOARDS, 41u},    {SoundCategory::STRINGS, 79u}, {SoundCategory::WINDS, 222u},
        {SoundCategory::PERCUSSIONS, 150u}, {SoundCategory::VOICES, 12u},  {SoundCategory::UNDEFINED, 0u},
    };

    for (const auto &[category, expected_size] : categories) {
        const ProgramMapping &table = mapping_by_category(category);
        EXPECT_EQ(table.size(), expected_size) << static_cast<int>(category);

        for (auto it = table.begin(); it != table.end(); ++it) {
            if (it != table.begin()) {
                EXPECT_TRUE(std::prev(it)->first < it->first);
            }
            const auto found = table.find(it->first);
            ASSERT_NE(found, table.end());
            EXPECT_EQ(found->second, it->second);
        }
    }

    // Duplicate keys in initializer lists keep the first entry, matching std::map::insert
    const std::initializer_list<ProgramMapping::value_type> dup_init = {
        {{SoundId::LUTE, {SoundSubCategory::PLUCKED}}, {midi::Program(8, 25)}},
        {{SoundId::LUTE, {SoundSubCategory::PLUCKED}}, {midi::Program(99, 99)}},
    };
    const ProgramMapping flat_dup(dup_init);
    const std::map<SoundMappingKey, midi::Programs> std_dup(dup_init.begin(), dup_init.end());
    ASSERT_EQ(flat_dup.size(), std_dup.size());
    EXPECT_EQ(flat_dup.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}), std_dup.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}));

    const ProgramMapping &strings = mapping_by_category(SoundCategory::STRINGS);
    ASSERT_TRUE(strings.contains({SoundId::LUTE, {SoundSubCategory::PLUCKED}}));
    EXPECT_EQ(strings.at({SoundId::LUTE, {SoundSubCategory::PLUCKED}}), (midi::Programs{midi::Program(8, 25)}));

    const ProgramMapping &winds = mapping_by_category(SoundCategory::WINDS);
    ASSERT_TRUE(winds.contains({SoundId::HARMONICA, {SoundSubCategory::CHROMATIC}}));
    EXPECT_EQ(winds.at({SoundId::HARMONICA, {SoundSubCategory::CHROMATIC}}), (midi::Programs{midi::Program(0, 22)}));
    ASSERT_TRUE(winds.contains({SoundId::HORN, {SoundSubCategory::ALTO}}));
    EXPECT_EQ(winds.at({SoundId::HORN, {SoundSubCategory::ALTO}}), (midi::Programs{midi::Program(0, 60), midi::Program(0, 59)}));

    // Unsorted subcategory lists normalize to sorted sets and compare lexicographically
    const SoundMappingKey key_a{SoundId::BELL, {SoundSubCategory::PLATE, SoundSubCategory::METAL}};
    const SoundMappingKey key_b{SoundId::BELL, {SoundSubCategory::METAL, SoundSubCategory::PLATE, SoundSubCategory::METAL}};
    EXPECT_EQ(key_a, key_b);
    EXPECT_FALSE(key_a < key_b);
    EXPECT_FALSE(key_b < key_a);
    EXPECT_EQ(mapping_by_category(SoundCategory::PERCUSSIONS).at(key_b), (midi::Programs{midi::Program(0, 11)}));

    // find_programs strips PRIMARY and SECONDARY subcategories and falls back to Program(0, 0) when unmapped
    const PlaybackSetupData piano_primary(SoundId::PIANO, SoundCategory::KEYBOARDS,
                                          {SoundSubCategory::GRAND, SoundSubCategory::PRIMARY, SoundSubCategory::SECONDARY});
    EXPECT_EQ(find_programs(piano_primary), (midi::Programs{midi::Program(0, 0)}));

    const PlaybackSetupData synth_fx(SoundId::SYNTHESIZER, SoundCategory::KEYBOARDS,
                                     {SoundSubCategory::ELECTRIC, SoundSubCategory::FX_RAIN, SoundSubCategory::PRIMARY});
    EXPECT_EQ(find_programs(synth_fx), (midi::Programs{midi::Program(0, 96)}));

    const PlaybackSetupData unmapped(SoundId::UNDEFINED, SoundCategory::KEYBOARDS, {});
    EXPECT_EQ(find_programs(unmapped), (midi::Programs{midi::Program(0, 0)}));
}

TEST(MpeFlatMapTest, SoundMappingArticulationTablesPinContents) {
    using Pairs = std::vector<std::pair<ArticulationType, midi::Program>>;
    auto to_pairs = [](const ArticulationMapping &m) { return Pairs(m.begin(), m.end()); };

    const auto check_table = [&](const PlaybackSetupData &setup, const std::map<ArticulationType, midi::Program> &expected_map) {
        const ArticulationMapping &actual = articulation_sounds(setup);
        const Pairs expected(expected_map.begin(), expected_map.end());
        EXPECT_EQ(to_pairs(actual), expected);
        for (const auto &[type, prog] : expected) {
            ASSERT_TRUE(actual.contains(type));
            EXPECT_EQ(actual.at(type), prog);
        }
        EXPECT_FALSE(actual.contains(ArticulationType::STANDARD));
    };

    check_table({SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC}}, {{ArticulationType::JAZZ_TONE, {0, 26}},
                                                                                          {ArticulationType::PALM_MUTE, {0, 28}},
                                                                                          {ArticulationType::MUTE, {0, 28}},
                                                                                          {ArticulationType::HARMONIC, {0, 31}},
                                                                                          {ArticulationType::DISTORTION, {0, 30}},
                                                                                          {ArticulationType::OVERDRIVE, {0, 29}}});

    check_table({SoundId::GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC}}, {{ArticulationType::JAZZ_TONE, {0, 26}},
                                                                                          {ArticulationType::PALM_MUTE, {0, 28}},
                                                                                          {ArticulationType::MUTE, {0, 28}},
                                                                                          {ArticulationType::HARMONIC, {8, 31}}});

    check_table({SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ELECTRIC}}, {{ArticulationType::SLAP, {0, 36}},
                                                                                               {ArticulationType::POP, {0, 37}},
                                                                                               {ArticulationType::PALM_MUTE, {0, 28}},
                                                                                               {ArticulationType::MUTE, {0, 28}}});

    check_table({SoundId::BASS_GUITAR, SoundCategory::STRINGS, {SoundSubCategory::ACOUSTIC}}, {{ArticulationType::SLAP, {0, 36}},
                                                                                               {ArticulationType::POP, {0, 37}},
                                                                                               {ArticulationType::PIZZICATO, {0, 32}},
                                                                                               {ArticulationType::PALM_MUTE, {0, 32}},
                                                                                               {ArticulationType::MUTE, {0, 32}}});

    auto section_map = [](midi::bank_t bank) {
        return std::map<ArticulationType, midi::Program>{
            {ArticulationType::SNAP_PIZZICATO, {bank, 45}}, {ArticulationType::PIZZICATO, {bank, 45}},
            {ArticulationType::PALM_MUTE, {bank, 45}},      {ArticulationType::MUTE, {bank, 49}},
            {ArticulationType::TREMOLO8TH, {bank, 44}},     {ArticulationType::TREMOLO16TH, {bank, 44}},
            {ArticulationType::TREMOLO32ND, {bank, 44}},    {ArticulationType::TREMOLO64TH, {bank, 44}},
        };
    };

    auto solo_map = [](midi::bank_t bank, midi::program_t mute_prog) {
        return std::map<ArticulationType, midi::Program>{
            {ArticulationType::SNAP_PIZZICATO, {bank, 45}},
            {ArticulationType::PIZZICATO, {bank, 45}},
            {ArticulationType::PALM_MUTE, {bank, 45}},
            {ArticulationType::MUTE, {bank, mute_prog}},
        };
    };

    check_table({SoundId::VIOLIN, SoundCategory::STRINGS, {SoundSubCategory::SECTION}}, section_map(20));
    check_table({SoundId::VIOLIN, SoundCategory::STRINGS, {}}, solo_map(20, 40));
    check_table({SoundId::VIOLA, SoundCategory::STRINGS, {SoundSubCategory::SECTION}}, section_map(30));
    check_table({SoundId::VIOLA, SoundCategory::STRINGS, {}}, solo_map(30, 41));
    check_table({SoundId::VIOLONCELLO, SoundCategory::STRINGS, {SoundSubCategory::SECTION}}, section_map(40));
    check_table({SoundId::VIOLONCELLO, SoundCategory::STRINGS, {}}, solo_map(40, 49));
    check_table({SoundId::CONTRABASS, SoundCategory::STRINGS, {SoundSubCategory::SECTION}}, section_map(50));
    check_table({SoundId::CONTRABASS, SoundCategory::STRINGS, {}}, solo_map(50, 43));

    for (const SoundId viol : {SoundId::VIOL, SoundId::PARDESSUS_VIOL, SoundId::VIOLA_DA_GAMBA, SoundId::VIOLONE}) {
        check_table({viol, SoundCategory::STRINGS, {}}, section_map(0));
    }
    check_table({SoundId::HARP, SoundCategory::STRINGS, {}}, solo_map(0, 49));

    for (const SoundId brass_id : {SoundId::BUGLE, SoundId::EUPHONIUM, SoundId::HORN, SoundId::TRUMPET, SoundId::TROMBONE, SoundId::TUBA}) {
        check_table({brass_id, SoundCategory::WINDS, {}}, {{ArticulationType::MUTE, {0, 59}}});
    }
    check_table({SoundId::FLUTE, SoundCategory::WINDS, {}}, {});
    check_table({SoundId::PIANO, SoundCategory::KEYBOARDS, {}}, {});
}

TEST(MpeFlatMapTest, ChannelMapSameProgramAndExhaustionReusePreserveOrder) {
    // R3a: same-program reuse when inserting a lower ArticulationType before the matched entry
    const PlaybackSetupData violin_sec(SoundId::VIOLIN, SoundCategory::STRINGS, {SoundSubCategory::ORCHESTRAL, SoundSubCategory::SECTION},
                                       true);
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

    // Insert higher enum values first, then reuse their program with lower enum values (shifts & reallocates mapping)
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

    // R3b: exhaustion loop reuses matching programs from other layers without invalidating outer/inner maps
    ChannelMap capped;
    capped.init(violin_sec, std::nullopt, 2);
    auto resolve_capped = [&](staff_layer_idx_t staff, voice_layer_idx_t voice, ArticulationType type) {
        NoteRecord note;
        note.staff_layer = staff;
        note.voice_layer = voice;
        const ArticulationRecord rec{0, 100, type};
        return capped.resolve_channel_for_event(note, std::span<const ArticulationRecord>{&rec, 1});
    };

    EXPECT_EQ(resolve_capped(0, 0, ArticulationType::MUTE), 1u);
    ASSERT_EQ(capped.channel_count(), 2u);

    // New outer layers when channel budget is exhausted: reuse layer 0's STANDARD (ch 0) and MUTE (ch 1),
    // and fold unmapped PIZZICATO onto fallback channel 0
    EXPECT_EQ(resolve_capped(1, 0, ArticulationType::MUTE), 1u);
    EXPECT_EQ(resolve_capped(2, 0, ArticulationType::PIZZICATO), 0u);
    EXPECT_EQ(capped.channel_count(), 2u);

    // Outer and inner iteration orders match std::map ascending order
    layer_idx_t prev_layer = 0;
    bool first_layer = true;
    for (const auto &[layer, voice_map] : capped.data()) {
        if (!first_layer) {
            EXPECT_LT(prev_layer, layer);
        }
        first_layer = false;
        prev_layer = layer;

        ArticulationType prev_type = ArticulationType::UNDEFINED;
        for (const auto &[type, mapping] : voice_map) {
            EXPECT_LT(prev_type, type);
            prev_type = type;
        }
    }
}
