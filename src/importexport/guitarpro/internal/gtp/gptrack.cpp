#include "gptrack.h"

namespace mu::iex::guitarpro {
void GPTrack::add_sound(Sound sound) {
    muse::String key = sound.path + u";" + sound.name + u";" + sound.role;

    sounds_value.insert({key, sound});
}
} // namespace mu::iex::guitarpro
