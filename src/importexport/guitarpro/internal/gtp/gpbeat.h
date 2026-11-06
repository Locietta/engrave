#ifndef MU_IMPORTEXPORT_GPBEAT_H
#define MU_IMPORTEXPORT_GPBEAT_H

#include <map>
#include <memory>
#include <string>

#include "gpnote.h"
#include "gprhythm.h"

namespace mu::iex::guitarpro {
class GPBeat {
public:
    enum class DynamicType { FFF, FF, F, MF, MP, P, PP, PPP };
    enum class LegatoType { NONE, START, MEDIATE, END };
    enum class Arpeggio { NONE, UP, DOWN };
    enum class Brush { NONE, UP, DOWN };
    enum class GraceNotes { NONE, ON_BEAT, BEFORE_BEAT };
    enum class VibratoWTremBar { NONE, SLIGHT, WIDE };
    enum class VibratoLeftHand { NONE, SLIGHT, WIDE };
    enum class Fadding { NONE, FADE_IN, FADE_OUT, VOLUME_SWELL };
    enum class Hairpin { NONE, CRESCENDO, DECRESCENDO };
    enum class PickStroke { NONE, UP, DOWN };
    enum class Wah { NONE, OPEN, CLOSED };
    enum class Golpe { NONE, FINGER, THUMB };
    enum class TappingHand { NONE, RIGHT, LEFT };
    enum class Rasgueado {
        NONE,
        II_1,
        MII_1,
        MII_2,
        PMP_1,
        PMP_2,
        PEI_1,
        PEI_2,
        PAI_1,
        PAI_2,
        AMI_1,
        AMI_2,
        PPP_1,
        AMII_1,
        AMIP_1,
        EAMI_1,
        EAMII_1,
        PEAMI_1
    };

    enum class OttavaType { NONE, MA15, VA8, VB8, MB15 };

    enum class HarmonicMarkType { NONE, ARTIFICIAL, PINCH, TAP, SEMI, FEED_BACK };

    struct HarmonicMarkInfo {
        bool artificial = false;
        bool pinch = false;
        bool tap = false;
        bool semi = false;
        bool feedback = false;
    };

    struct Tremolo {
        int numerator{-1};
        int denominator{-1};
    };

    struct Barre {
        int fret{-1};
        int string{-1};
    };

    struct StemOrientation {
        bool up = false; /// taken in consideration only if userDefined == true
        bool user_defined = false;
    };

    enum class BeamMode { AUTO, JOINED, BROKEN, BROKEN2, BROKEN2_JOINED };

    struct {
        bool operator()(const std::shared_ptr<GPNote> &a, const std::shared_ptr<GPNote> &b) const {
            auto a_pitch = a->midi_pitch().octave * 12 + a->midi_pitch().tone;
            auto b_pitch = b->midi_pitch().octave * 12 + b->midi_pitch().tone;
            return a_pitch > b_pitch;
        }
    } compare_pitch;

    using Whammy = GPNote::Bend;

    void add_gp_note(const std::shared_ptr<GPNote> &n) { notes_value.push_back(n); }

    void sort_gp_notes();

    void add_gp_rhythm(const std::shared_ptr<GPRhythm> &n) { rhythm = n; }

    void set_dynamic(GPBeat::DynamicType t) { dynamic_value = t; }

    void set_legato_type(GPBeat::LegatoType t) { legato = t; }

    void set_ottava_type(GPBeat::OttavaType input_ottava_type) { ottava_type_value = input_ottava_type; }

    GPBeat::OttavaType ottava_type() const { return ottava_type_value; }

    bool is_rest() const { return notes_value.empty(); }

    DynamicType dynamic() const { return dynamic_value; }

    LegatoType legato_type() const { return legato; }

    std::pair<int, GPRhythm::RhytmType> lenth() const;
    GPRhythm::Tuplet tuplet() const;

    void set_let_ring(bool input_let_ring) { let_ring_value = input_let_ring; }

    bool let_ring() const { return let_ring_value; }

    void set_palm_mute(bool input_palm_mute) { palm_mute_value = input_palm_mute; }

    bool palm_mute() const { return palm_mute_value; }

    void set_trill(bool trill) { trill_value = trill; }

    bool trill() const { return trill_value; }

    void add_harmonic_mark_type(GPBeat::HarmonicMarkType type);

    bool harmonic_mark_artificial() const { return harmonic_mark_info.artificial; }

    bool harmonic_mark_pinch() const { return harmonic_mark_info.pinch; }

    bool harmonic_mark_tap() const { return harmonic_mark_info.tap; }

    bool harmonic_mark_semi() const { return harmonic_mark_info.semi; }

    bool harmonic_mark_feedback() const { return harmonic_mark_info.feedback; }

    void set_tapping_hand(TappingHand hand) { tapping_hand_value = hand; }

    TappingHand tapping_hand() const { return tapping_hand_value; }

    void set_slapped(bool s) { slapped_value = s; }

    bool slapped() const { return slapped_value; }

    void set_popped(bool s) { popped_value = s; }

    bool popped() const { return popped_value; }

    void set_arpeggio(Arpeggio ar) { arpeggio_value = ar; }

    Arpeggio arpeggio() const { return arpeggio_value; }

    void set_arpeggio_stretch(double input_arpeggio_stretch) { arpeggio_stretch_value = input_arpeggio_stretch; }

    double arpeggio_stretch() const { return arpeggio_stretch_value; }

    void set_brush(Brush br) { brush_value = br; }

    Brush brush() const { return brush_value; }

    void set_grace_notes(GraceNotes gn) { grace_notes_value = gn; }

    GraceNotes grace_notes() const { return grace_notes_value; }

    void set_free_text(const muse::String &s) { free_text_value = s; }

    const muse::String &free_text() const { return free_text_value; }

    void set_time(int t) { time_value = t; }

    int time() const { return time_value; }

    void set_vibrato_w_trem_bar(VibratoWTremBar v) { vibrato_w_trem_bar_value = v; }

    VibratoWTremBar vibrato_w_trem_bar() const { return vibrato_w_trem_bar_value; }

    void set_vibrato_left_hand(VibratoLeftHand v) { vibrato_left_hand_value = v; }

    VibratoLeftHand vibrato_left_hand() const { return vibrato_left_hand_value; }

    void set_fadding(Fadding f) { fadding_value = f; }

    Fadding fadding() const { return fadding_value; }

    void set_hairpin(Hairpin h) { hairpin_value = h; }

    Hairpin hairpin() const { return hairpin_value; }

    void set_rasgueado(Rasgueado r) { rasgueado_value = r; }

    Rasgueado rasgueado() const { return rasgueado_value; }

    void set_pick_stroke(PickStroke p) { pick_stroke_value = p; }

    PickStroke pick_stroke() const { return pick_stroke_value; }

    void set_tremolo(Tremolo tr) { tremolo_value = tr; }

    Tremolo tremolo() const { return tremolo_value; }

    void set_wah(Wah w) { wah_value = w; }

    Wah wah() const { return wah_value; }

    void set_golpe(Golpe g) { golpe_ = g; }

    Golpe golpe() const { return golpe_; }

    void set_barre_fret(int v) { barre_value.fret = v; }

    void set_barre_string(int v) { barre_value.string = v; }

    Barre barre() const { return barre_value; }

    void set_id(int id) { id_value = id; }

    int id() const { return id_value; }

    void set_whammy(const Whammy &whammy) { whammy_ = whammy; }

    const Whammy &whammy() const { return whammy_; }

    bool has_whammy() const { return !whammy_.is_empty(); }

    void set_pick_scrape(bool pick_scrape) { pick_scrape_ = pick_scrape; }

    bool pick_scrape() const { return pick_scrape_; }

    void set_dead_slapped(bool dead_slapped) { dead_slapped_ = dead_slapped; }

    bool dead_slapped() const { return dead_slapped_; }

    void set_stem_orientation_up(bool up) { stem_orientation_.up = up; }

    void set_stem_orientation_user_defined(bool user_defined) { stem_orientation_.user_defined = user_defined; }

    bool stem_orientation_up() const { return stem_orientation_.up; }

    bool stem_orientation_user_defined() const { return stem_orientation_.user_defined; }

    void set_beam_mode(BeamMode mode) { beam_mode_ = mode; }

    BeamMode beam_mode() const { return beam_mode_; }

    const std::vector<std::shared_ptr<GPNote>> &notes() const { return notes_value; }

    //! NOTE In GP version 6.2.0+ already arranged lyrics are written into bits, so the lyrics field of the beat is enough.
    //! In earlier versions of GP, there was no arrangement of lyrics by bits,
    //! i.e. it was necessary to parse the general lyrics each time a tab was opened and arrange them by bits.
    //! At the same time, the same beat can be used in different bars, even if it has different lyrics
    //! (i.e. the lyrics did not participate in determining the uniqueness of the beat).
    //! Therefore, in order to write lyrics into such bits, a key was added - the bar number (master bar)

    struct Key {
        int32_t track_id{-1};
        int32_t mbar_id{-1};

        Key() {}

        Key(int32_t ti, int32_t mi) : track_id(ti), mbar_id(mi) {}

        bool operator<(const Key &other) const {
            if (track_id != other.track_id) {
                return track_id < other.track_id;
            }
            return mbar_id < other.mbar_id;
        }
    };

    void set_lyrics(const std::string &ly) { set_lyrics(-1, -1, ly); }

    void set_lyrics(int32_t ti, int32_t mi, const std::string &ly) { lyrics_value[Key(ti, mi)] = ly; }

    const std::string &lyrics(int32_t ti = -1, int32_t mi = -1) const {
        Key key(ti, mi);
        if (lyrics_value.find(key) == lyrics_value.end()) {
            if (ti != -1 && mi != -1) {
                return lyrics(-1, -1);
            }

            static std::string dummy;
            return dummy;
        }
        return lyrics_value.at(key);
    }

    //! NOTE Same problem as with lyrics
    void set_diagram_idx(int idx) { set_diagram_idx(-1, -1, idx); }

    void set_diagram_idx(int32_t ti, int32_t mi, int idx) { diagram_idx_value[Key(ti, mi)] = idx; }

    int diagram_idx(int32_t ti = -1, int32_t mi = -1) const {
        Key key(ti, mi);
        if (diagram_idx_value.find(key) == diagram_idx_value.end()) {
            if (ti != -1 && mi != -1) {
                return diagram_idx(-1, -1);
            }

            return -1;
        }
        return diagram_idx_value.at(key);
    }

private:
    int id_value = -1;
    std::vector<std::shared_ptr<GPNote>> notes_value;
    std::map<Key, std::string> lyrics_value;
    std::map<Key, int> diagram_idx_value;
    std::shared_ptr<GPRhythm> rhythm;
    DynamicType dynamic_value = DynamicType::MF;
    LegatoType legato = LegatoType::NONE;
    OttavaType ottava_type_value = OttavaType::NONE;
    bool let_ring_value = false;
    bool palm_mute_value = false;
    bool trill_value = false;
    TappingHand tapping_hand_value = TappingHand::NONE;
    bool slapped_value = false;
    bool popped_value = false;
    HarmonicMarkInfo harmonic_mark_info;
    Arpeggio arpeggio_value = Arpeggio::NONE;
    Brush brush_value = Brush::NONE;
    GraceNotes grace_notes_value = GraceNotes::NONE;
    int time_value = -1;
    muse::String free_text_value;
    VibratoWTremBar vibrato_w_trem_bar_value = VibratoWTremBar::NONE;
    VibratoLeftHand vibrato_left_hand_value = VibratoLeftHand::NONE;
    Fadding fadding_value = Fadding::NONE;
    Hairpin hairpin_value = Hairpin::NONE;
    Rasgueado rasgueado_value = Rasgueado::NONE;
    PickStroke pick_stroke_value = PickStroke::NONE;
    Tremolo tremolo_value;
    Wah wah_value = Wah::NONE;
    Golpe golpe_ = Golpe::NONE;
    Barre barre_value;
    double arpeggio_stretch_value = 0.0;
    Whammy whammy_;
    bool pick_scrape_ = false;
    bool dead_slapped_ = false;
    StemOrientation stem_orientation_;
    BeamMode beam_mode_ = BeamMode::AUTO;
};
} // namespace mu::iex::guitarpro

#endif // MU_IMPORTEXPORT_GPBEAT_H
