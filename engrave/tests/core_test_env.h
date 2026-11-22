// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <engrave/engrave.h>

namespace engrave::test {

struct CoreTestEnv {
    std::string program;
    std::filesystem::path assets;
    std::filesystem::path python;
    std::filesystem::path mnx_out;
    std::vector<std::string> engine_args;
    std::vector<std::filesystem::path> golden_corpus;
    std::vector<std::filesystem::path> inputs_corpus;
    bool deterministic = false;
    bool update_inventory = false;
    std::unique_ptr<engrave::Engine> engine;
    std::unique_ptr<engrave::Document> describe_anchor;

    void set_engine(std::unique_ptr<engrave::Engine> replacement);
    engrave::Document &ensure_describe_anchor();
};

CoreTestEnv &core_env();

} // namespace engrave::test
