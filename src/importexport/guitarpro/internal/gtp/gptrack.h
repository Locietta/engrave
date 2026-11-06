#ifndef MU_IMPORTEXPORT_GPTRACK_H
#define MU_IMPORTEXPORT_GPTRACK_H

#include <unordered_map>
#include <string>

#include "types/string.h"

namespace mu::iex::guitarpro {
struct GPTrack {
    struct RSE {
        float pan{0};
        float volume{0};
        float chorus{0};
        float reverb{0};
        float phase{0};
        float tremolo{0};
    };

    struct StaffProperties {
        int fret_count{24};
        int capo_fret{0};
        std::vector<int> tuning;
        bool use_flats{false};
        bool ignore_flats{false};
    };

    struct InstrumentString {
        int num{0};
        int tunning{0};
    };

    struct Diagram {
        int id{0};
        muse::String name;
        int string_count{0};
        int fret_count{0};
        int base_fret{0};
        std::unordered_map<int, int> frets;
        // there are some other tags
        // which cant be used in MU
    };

    struct Sound {
        int programm{0};
        muse::String name;
        muse::String label;
        muse::String path;
        muse::String role;
    };

    struct SoundAutomation {
        muse::String type;
        muse::String value;
        int bar = 0;
        bool linear = 0;
        float position = 0;
    };

    GPTrack(int idx) : idx_value(idx) {}

    virtual ~GPTrack() = default;

    void set_name(const muse::String &n) { name_value = n; }

    muse::String name() const { return name_value; }

    void set_short_name(const muse::String &s) { short_name_value = s; }

    muse::String short_name() const { return short_name_value; }

    void set_instrument(const muse::String &s) { instrument_value = s; }

    muse::String instrument() const { return instrument_value; }

    void set_rse(const RSE &r) { rse_value = r; }

    const RSE &rse() const { return rse_value; }

    void set_programm(int pr) { programm_value = pr; }

    int programm() const { return programm_value; }

    void set_midi_channel(int ch) { midi_channel_value = ch; }

    int midi_channel() const { return midi_channel_value; }

    void set_is_guitar(bool arg) { is_guitar_value = arg; }

    bool is_guitar() const { return is_guitar_value; }

    void set_staff_properties(const StaffProperties &st) { staff_properties_value = st; }

    const StaffProperties &staff_properties() const { return staff_properties_value; }

    void add_sound(Sound sound);

    struct SoundAutomationPos {
        int bar = 0;
        float pos = 0;

        bool operator<(const SoundAutomationPos &other) const { return std::tie(bar, pos) < std::tie(other.bar, other.pos); }
    };

    void add_sound_automation(SoundAutomation val) { automations.insert({{val.bar, val.position}, val}); }

    const std::unordered_map<muse::String, Sound> &sounds() { return sounds_value; }

    const std::map<SoundAutomationPos, SoundAutomation> &sound_automations() { return automations; }

    std::vector<InstrumentString> strings() const {
        std::vector<InstrumentString> ss;
        for (size_t i = 0; i < staff_properties_value.tuning.size(); ++i) {
            ss.push_back({static_cast<int>(i + 1), staff_properties_value.tuning.at(i)});
        }

        return ss;
    }

    void set_staff_count(size_t n) { staff_count_value = n; }

    size_t staff_count() const { return staff_count_value; }

    void set_transpose(int t) { transpose_value = t; }

    int transpose() const { return transpose_value; }

    void add_diagram(std::pair<int, Diagram> &&d) { diagrams.insert(d); }

    const std::unordered_map<int, Diagram> &diagram() const { return diagrams; }

    void set_lyrics(const std::string &l) { lyrics_value = l; }

    const std::string &lyrics() const { return lyrics_value; }

    void set_lyrics_offset(int n) { lyrics_offset_value = n; }

    int lyrics_offset() const { return lyrics_offset_value; }

    void set_line_count(int n) { line_count_value = n; }

    int line_count() const { return line_count_value; }

    int idx() const { return idx_value; }

protected:

    muse::String name_value;
    muse::String short_name_value;
    muse::String instrument_value;
    RSE rse_value;
    int programm_value{0};
    int midi_channel_value{0};
    bool is_guitar_value{false};
    int idx_value{-1};
    size_t staff_count_value{1};
    StaffProperties staff_properties_value;
    std::unordered_map<muse::String, Sound> sounds_value;
    std::map<SoundAutomationPos, SoundAutomation> automations;
    int transpose_value{0};
    std::unordered_map<int, Diagram> diagrams;
    std::string lyrics_value;
    int lyrics_offset_value = {0};
    int line_count_value{5}; // for percussion lines
};

struct GP6Track : GPTrack {
    GP6Track(int idx) : GPTrack(idx) {}

private:
};

struct GP7Track : GPTrack {
    GP7Track(int idx) : GPTrack(idx) {}

private:
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_GPTRACK_H
