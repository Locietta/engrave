#include <stdexcept>

#include "engrave/src/audio/engine_audio.h"
#include "engrave/src/runtime/runtime_assets.h"
#include "state.h"
#include <engrave/engine.h>

namespace engrave {
Result<void> Engine::set_soundfonts(const std::vector<std::filesystem::path> &files) {
    impl_->state.check_thread();
    std::vector<std::filesystem::path> retained;
    for (const auto &file : files) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(file, ec))
            return std::unexpected(Error{ErrorCode::NOT_FOUND, "no SoundFont file '" + utf8(file) + "'"});
        try {
            retained.push_back(
                path_from_utf8(impl_->state.context->assets().freeze_sound_font(utf8(std::filesystem::absolute(file))).path));
        } catch (const std::runtime_error &error) {
            return std::unexpected(Error{ErrorCode::INVALID_INPUT, error.what()});
        }
    }
    if (!retained.empty()) {
        std::vector<std::string> paths;
        for (const auto &file : retained) paths.push_back(utf8(file));
        std::string error;
        if (!music::validate_engine_soundfonts(paths, &error)) return std::unexpected(Error{ErrorCode::INVALID_INPUT, error});
    }
    impl_->state.soundfonts.swap(retained);
    return {};
}

} // namespace engrave
