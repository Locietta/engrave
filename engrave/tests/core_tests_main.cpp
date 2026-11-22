// SPDX-License-Identifier: GPL-3.0-only
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <span>
#include <string_view>

#include <gtest/gtest.h>

#include "core_test_env.h"
#include "handles_test_fixture.h"
#include "read_test_fixture.h"

namespace engrave::test {

namespace {

bool looks_like_score(const std::filesystem::path &path) {
    static const std::set<std::string> extensions = {".mscx", ".mscz", ".musicxml", ".mxl", ".mid", ".midi",
                                                     ".gp",   ".gp3",  ".gp4",      ".gp5", ".gpx", ".ptb"};
    return extensions.contains(path.extension().string());
}

void collect_corpus(const std::filesystem::path &path, std::vector<std::filesystem::path> &out) {
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec)) {
        std::vector<std::filesystem::path> entries;
        for (const auto &entry : std::filesystem::directory_iterator(path, ec)) entries.push_back(entry.path());
        std::ranges::sort(entries);
        for (const auto &entry : entries) collect_corpus(entry, out);
        return;
    }
    if (std::filesystem::is_regular_file(path, ec) && looks_like_score(path)) out.push_back(path);
}

void clear_gtest_shard_env() {
#ifdef _WIN32
    _putenv("GTEST_TOTAL_SHARDS=");
    _putenv("GTEST_SHARD_INDEX=");
#else
    unsetenv("GTEST_TOTAL_SHARDS");
    unsetenv("GTEST_SHARD_INDEX");
#endif
}

} // namespace

CoreTestEnv &core_env() {
    static CoreTestEnv env;
    return env;
}

void CoreTestEnv::set_engine(std::unique_ptr<engrave::Engine> replacement) {
    describe_anchor.reset();
    engine = std::move(replacement);
    handles_test::g_engine = engine.get();
    read_test::g_engine = engine.get();
}

engrave::Document &CoreTestEnv::ensure_describe_anchor() {
    if (!describe_anchor) {
        const std::string anchor =
            R"(<score-partwise version="4.0"><part-list><score-part id="P1"><part-name>P</part-name></score-part></part-list>
<part id="P1"><measure number="1"><note><rest/><duration>4</duration></note></measure></part></score-partwise>)";
        auto opened = engine->open(std::as_bytes(std::span(anchor.data(), anchor.size())));
        if (!opened) {
            std::fprintf(stderr, "Engine::open (describe anchor): %s\n", opened.error().message.c_str());
            std::exit(1);
        }
        describe_anchor = std::move(*opened);
    }
    return *describe_anchor;
}

} // namespace engrave::test

int main(int argc, char **argv) {
    // Handles.H13IdsWithDeterministicEngine spawns this binary with --deterministic;
    // clear inherited shard env vars so the single-test child always executes.
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--deterministic") {
            engrave::test::clear_gtest_shard_env();
            break;
        }
    }

    testing::InitGoogleTest(&argc, argv);
    GTEST_FLAG_SET(death_test_style, "threadsafe");

    auto &env = engrave::test::core_env();
    env.program = argv[0];

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg.starts_with("--assets=")) {
            env.assets = std::filesystem::path(arg.substr(9));
            env.engine_args.emplace_back(arg);
        } else if (arg.starts_with("--golden-corpus=")) {
            engrave::test::collect_corpus(std::filesystem::path(arg.substr(16)), env.golden_corpus);
            env.engine_args.emplace_back(arg);
        } else if (arg.starts_with("--inputs-corpus=")) {
            engrave::test::collect_corpus(std::filesystem::path(arg.substr(16)), env.inputs_corpus);
        } else if (arg.starts_with("--corpus=")) {
            const auto path = std::filesystem::path(arg.substr(9));
            engrave::test::collect_corpus(path, env.golden_corpus);
            engrave::test::collect_corpus(path, env.inputs_corpus);
            env.engine_args.emplace_back(arg);
        } else if (arg.starts_with("--python=")) {
            env.python = std::filesystem::path(arg.substr(9));
        } else if (arg.starts_with("--mnx-out=")) {
            env.mnx_out = std::filesystem::path(arg.substr(10));
        } else if (arg == "--deterministic") {
            env.deterministic = true;
        } else if (arg == "--update-inventory") {
            env.update_inventory = true;
        }
    }

    if (!env.mnx_out.empty()) std::filesystem::create_directories(env.mnx_out);

    if (!env.assets.empty()) {
        // Failure before binding services must leave startup retryable.
        if (!env.deterministic) {
            auto missing = engrave::Engine::create({.resources = env.assets / "c5-missing-resources"});
            if (missing || missing.error().code != engrave::ErrorCode::NOT_FOUND) {
                std::fprintf(stderr, "C5 startup failure did not leave services unbound\n");
                return 1;
            }
        }
        auto created = engrave::Engine::create({.resources = env.assets, .deterministic_ids = env.deterministic});
        if (!created) {
            std::fprintf(stderr, "Engine::create: %s (pass --assets=<resources>)\n", created.error().message.c_str());
            return 1;
        }
        env.set_engine(std::move(*created));
    }

    handles_test::g_program = env.program;
    handles_test::g_assets = env.assets;
    handles_test::g_corpus = env.golden_corpus;
    handles_test::g_engine_args = env.engine_args;
    handles_test::g_deterministic = env.deterministic;
    handles_test::g_update_inventory = env.update_inventory;

    read_test::g_assets = env.assets;
    read_test::g_corpus = env.inputs_corpus;

    const int result = RUN_ALL_TESTS();

    env.set_engine(nullptr);
    return result;
}
