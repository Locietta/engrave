#ifndef MU_IMPORTEXPORT_GPSCORE_H
#define MU_IMPORTEXPORT_GPSCORE_H

#include "types/string.h"

namespace mu::iex::guitarpro {
class GPScore {
public:

    void set_title(const muse::String &title) { title_value = title; }

    muse::String title() const { return title_value; }

    void set_sub_title(const muse::String &input_sub_title) { sub_title_value = input_sub_title; }

    muse::String sub_title() const { return sub_title_value; }

    void set_artist(const muse::String &artist) { artist_value = artist; }

    muse::String artist() const { return artist_value; }

    void set_album(const muse::String &album) { album_value = album; }

    muse::String album() const { return album_value; }

    void set_poet(const muse::String &poet) { poet_value = poet; }

    muse::String poet() const { return poet_value; }

    void set_composer(const muse::String &composer) { composer_value = composer; }

    muse::String composer() const { return composer_value; }

    void set_multi_voice(bool input_multi_voice) { multi_voice_value = input_multi_voice; }

    bool multi_voice() const { return multi_voice_value; }

private:
    muse::String title_value;
    muse::String sub_title_value;
    muse::String artist_value;
    muse::String album_value;
    muse::String poet_value;
    muse::String composer_value;
    bool multi_voice_value = false;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPSCORE_H
