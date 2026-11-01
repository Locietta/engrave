#pragma once

#include <cstdint>

namespace engrave::testing {
// Diagnostic seam only: query on the capture callback's owner thread. Does not
// call client code or change conversion. Zero outside the indicated phase.
struct Write470Progress {
    enum class Stage { None, Converting, Serializing };
    Stage stage = Stage::None;
    std::uint64_t units = 0;
    std::uint64_t working_bytes = 0;
};
Write470Progress write470_progress() noexcept;
}
