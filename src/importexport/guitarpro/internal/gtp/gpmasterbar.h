#ifndef MU_IMPORTEXPORT_GPMASTERBAR_H
#define MU_IMPORTEXPORT_GPMASTERBAR_H

#include "gpbar.h"

namespace mu::iex::guitarpro {
class GPMasterBar {
public:
    struct Repeat {
        enum class Type { NONE, START, END, START_END };
        Type type{Type::NONE};
        int count{1};
    };

    struct KeySig {
        enum class Accidentals {
            C_B = -7,
            G_B,
            D_B,
            A_B,
            E_B,
            B_B,
            F,
            C,
            G,
            D,
            A,
            E,
            B,
            F_S,
            C_S,
        };
        enum class Mode { MAJOR, MINOR };
        Accidentals accidental_count{Accidentals::C};
        Mode mode{Mode::MAJOR};
    };
    enum class TripletFeelType { TRIPLET8TH, TRIPLET16TH, DOTTED8TH, DOTTED16TH, SCOTTISH8TH, SCOTTISH16TH, NONE };

    enum class BarlineType { NORMAL, DOUBLE };

    struct TimeSig {
        int numerator{0};
        int denominator{0};
    };

    struct Fermata {
        enum class Type { SHORT, MEDIUM, LONG };
        Type type{Type::MEDIUM};
        float length{0};
        int offset_num;   //numerator of offset field in GP
        int offset_denom; //denominator of offset field in GP
    };

    struct Direction {
        enum class Type { REPEAT, JUMP, MARKER };

        Type type = Type::REPEAT;
        muse::String name;
    };

    ~GPMasterBar() = default;

    void add_gp_bar(std::unique_ptr<GPBar> &&b) { bars_value.push_back(std::move(b)); }

    void set_time_sig(const GPMasterBar::TimeSig &sig) { time_sig_value = sig; }

    TimeSig time_sig() const { return time_sig_value; }

    bool use_flats() const { return use_flats_value; }

    void set_key_sig(GPMasterBar::KeySig sig, bool input_use_flats = false) {
        key_sig_value = sig;
        use_flats_value = input_use_flats;
    }

    KeySig key_sig() const { return key_sig_value; }

    void set_fermatas(std::vector<Fermata> &&f) { fermatas_value.swap(f); }

    const std::vector<Fermata> &fermatas() const { return fermatas_value; }

    void set_repeat(Repeat r) { repeat_value = r; }

    Repeat repeat() const { return repeat_value; }

    void set_triplet_feel(TripletFeelType t) { triplet_feel_value = t; }

    TripletFeelType triplet_feel() const { return triplet_feel_value; }

    void set_barline_type(BarlineType t) { barline_type_value = t; }

    BarlineType barline_type() const { return barline_type_value; }

    void set_free_time(bool input_free_time) { free_time_value = input_free_time; }

    bool free_time() const { return free_time_value; }

    void set_alternative_ending(std::vector<int> &&r) { alternate_endings.swap(r); }

    const std::vector<int> &alternate_ending() const { return alternate_endings; }

    void set_section(std::pair<muse::String, muse::String> &&s) { section_value.swap(s); }

    const std::pair<muse::String, muse::String> &section() const { return section_value; }

    void set_directions(std::vector<Direction> &&d) { directions_value.swap(d); }

    const std::vector<Direction> &directions() const { return directions_value; }

    void set_id(int id) { id_value = id; }

    int id() const { return id_value; } //debug helper

    const std::vector<std::unique_ptr<GPBar>> &bars() const { return bars_value; }

private:

    int id_value{-1};
    std::vector<std::unique_ptr<GPBar>> bars_value;
    std::vector<Fermata> fermatas_value;
    std::vector<Direction> directions_value;
    TimeSig time_sig_value;
    KeySig key_sig_value;
    bool use_flats_value = false;
    Repeat repeat_value;
    std::vector<int> alternate_endings;
    TripletFeelType triplet_feel_value = TripletFeelType::NONE;
    BarlineType barline_type_value = BarlineType::NORMAL;
    bool free_time_value = false;
    std::pair<muse::String, muse::String> section_value;
    Direction direction;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPMASTERBAR_H
