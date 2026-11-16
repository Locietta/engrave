#include "engine_context.h"
#include "engrave/src/audio/engine_audio.h"

namespace music {
EngineAudio &EngineContext::audio() const {
    if (!audio_) {
        auto audio = std::make_unique<EngineAudio>(articulation_profiles());
        audio_ = {audio.release(), [](EngineAudio *value) { delete value; }};
    }
    return *audio_;
}
} // namespace music
