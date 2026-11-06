#ifndef MU_IMPORTEXPORT_GPNOTE_H
#define MU_IMPORTEXPORT_GPNOTE_H

#include <unordered_set>
#include <memory>
#include <bitset>

#include <global/types/string.h>

#include "inoteproperty.h"

namespace mu::iex::guitarpro {
struct GPNote {
    enum class TieType { NONE, START, MEDIATE, END };
    enum class VibratoType { NONE, SLIGHT, WIDE };
    enum class Ornament { NONE, LOWER_MORDENT, UPPER_MORDENT, INVERTED_TURN, TURN };
    enum class HammerOn { NONE, START, END };

    enum class PickScrape { NONE, DOWN, UP };

    struct Harmonic {
        enum class Type { NONE, NATURAL, ARTIFICIAL, PINCH, TAP, SEMI, FEED_BACK, TYPES };
        float fret{0};
        Type type{Type::NONE};

        static bool is_artificial(Type type) {
            return type == Type::ARTIFICIAL || type == Type::PINCH || type == Type::TAP || type == Type::SEMI || type == Type::FEED_BACK;
        }
    };

    struct MidiPitch {
        int midi{-1};
        int string{-1};
        int fret{-1};
        int octave{-1};
        int tone{0};
        int element{0};
        int variation{-1};
    };

    struct Bend {
        float destination_offset{-1};
        float destination_value{-1};
        float middle_offset1{-1};
        float middle_offset2{-1};
        float middle_value{-1};
        float origin_offset{-1};
        float origin_value{-1};

        bool is_empty() const {
            return destination_value == -1 && destination_offset == -1 && middle_value == -1 && middle_offset1 == -1 &&
                   middle_offset2 == -1 && origin_offset == -1 && origin_value == -1;
        }
    };

    struct Trill {
        int auxillary_fret{-1};
        int speed{-1};
    };

    void add_properties(std::unordered_set<std::unique_ptr<INoteProperty>> &&pr) { properties_value.swap(pr); }

    void set_tie_type(GPNote::TieType t) { tie = t; }

    void set_midi(int m) { midi_pitch_value.midi = m; }

    int midi() const { return midi_pitch_value.midi; }

    void set_string(int s) { midi_pitch_value.string = s; }

    int string() const { return midi_pitch_value.string; }

    void set_fret(int f) { midi_pitch_value.fret = f; }

    int fret() const { return midi_pitch_value.fret; }

    void set_octave(int o) { midi_pitch_value.octave = o; }

    void set_tone(int t) { midi_pitch_value.tone = t; }

    void set_variation(int t) { midi_pitch_value.variation = t; }

    void set_element(int t) { midi_pitch_value.element = t; }

    void set_accidental(int acc) { accidental_value = acc; }

    int accidental() const { return accidental_value; }

    void set_harmonic_fret(float f) { harmonic_value.fret = f; }

    void set_harmonic_type(Harmonic::Type t) { harmonic_value.type = t; }

    const Harmonic &harmonic() const { return harmonic_value; }

    void set_let_ring(bool lr) { let_ring_value = lr; }

    bool let_ring() const { return let_ring_value; }

    void set_palm_mute(bool pm) { palm_muted = pm; }

    bool palm_mute() const { return palm_muted; }

    const MidiPitch &midi_pitch() const { return midi_pitch_value; }

    TieType tie_type() const { return tie; }

    void set_bend(std::unique_ptr<Bend> &&b) { bend_value.swap(b); }

    const Bend *bend() const { return bend_value.get(); }

    void set_ghost_note(bool b) { ghost_note_value = b; }

    bool ghost_note() const { return ghost_note_value; }

    void set_mute(bool m) { muted_value = m; }

    //! muted = dead note
    bool muted() const { return muted_value; }

    void set_right_hand_tapping(bool t) { right_hand_tapping_value = t; }

    bool right_hand_tapping() const { return right_hand_tapping_value; }

    void set_left_hand_tapped(bool v) { left_hand_tapped_value = v; }

    bool left_hand_tapped() const { return left_hand_tapped_value; }

    void set_accent(const std::bitset<4> &b) { accent = b; }

    const std::bitset<4> &accents() const { return accent; }

    void set_slides(const std::bitset<6> &s) { slides_value = s; }

    const std::bitset<6> &slides() const { return slides_value; }

    void set_left_fingering(const muse::String &ch) { left_fingering_value = ch; }

    const muse::String &left_fingering() const { return left_fingering_value; }

    void set_right_fingering(const muse::String &ch) { right_fingering_value = ch; }

    const muse::String &right_fingering() const { return right_fingering_value; }

    void set_show_string_number(bool show) { show_string_number_ = show; }

    bool show_string_number() const { return show_string_number_; }

    void set_vibrato_type(VibratoType v) { vibrato = v; }

    VibratoType vibrato_type() const { return vibrato; }

    void set_trill_fret(int fr) { trill_value.auxillary_fret = fr; }

    void set_trill_speed(int tr) { trill_value.speed = tr; }

    Trill trill() const { return trill_value; }

    void set_ornament(Ornament o) { ornament_value = o; }

    Ornament ornament() const { return ornament_value; }

    void set_hammer_on(HammerOn h) { hammer = h; }

    HammerOn hammer_on() const { return hammer; }

    void set_pick_scrape(PickScrape p) { pick_scrape_value = p; }

    PickScrape pick_scrape() const { return pick_scrape_value; }

    void set_id(int id) { id_value = id; }

    int id() const { return id_value; }

    const std::unordered_set<std::unique_ptr<INoteProperty>> &properties() const { return properties_value; }

    static constexpr int invalid_accidental = -10;

private:
    int id_value{-1};
    TieType tie{TieType::NONE};
    std::unordered_set<std::unique_ptr<INoteProperty>> properties_value;
    MidiPitch midi_pitch_value;

    int accidental_value = ::mu::iex::guitarpro::GPNote::invalid_accidental;
    Harmonic harmonic_value;
    std::unique_ptr<Bend> bend_value;
    bool let_ring_value{false};
    bool palm_muted{false};
    bool ghost_note_value{false};
    bool muted_value{false};
    //[0] - staccato, [1] - unknown, [2] - heavily accidental, [3] - accidental
    std::bitset<4> accent{0};
    //[0] shifSlide, [1] - legatoSlide, [2] - slideDownWard, [3] - slidewUpWard, [4] - slideInFormBelow, [5] - slideInFormAbove,
    std::bitset<6> slides_value{0};
    muse::String left_fingering_value;
    muse::String right_fingering_value;
    VibratoType vibrato{VibratoType::NONE};
    Trill trill_value;
    Ornament ornament_value{Ornament::NONE};
    bool right_hand_tapping_value = false;
    bool left_hand_tapped_value = false;
    HammerOn hammer{HammerOn::NONE};
    PickScrape pick_scrape_value{PickScrape::NONE};
    bool show_string_number_ = false;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPNOTE_H
