#pragma once

#include <cstdint>

namespace engrave::testing {
// Diagnostic seam only: query on the capture callback's owner thread. Does not
// call client code or change conversion. Zero outside the indicated phase.
struct Write470Progress {
    enum class Stage { NONE, CONVERTING, SERIALIZING };
    Stage stage = Stage::NONE;
    std::uint64_t units = 0;
    std::uint64_t working_bytes = 0;
};

Write470Progress write470_progress() noexcept;
} // namespace engrave::testing
