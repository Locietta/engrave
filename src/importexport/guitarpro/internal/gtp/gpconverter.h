#pragma once

#include <unordered_map>

#include "engraving/iengravingconfiguration.h"
#include "engraving/types/fraction.h"

#include "../continiouselementsbuilder.h"
#include "../guitarbendimport/guitarbendimporter.h"
#include "gpbar.h"
#include "gpbeat.h"
#include "gpdrumsetresolver.h"
#include "gpmasterbar.h"
#include "gpmastertracks.h"
#include "engraving/dom/stringdata.h"

namespace mu::iex::guitarpro {
class GPScore;
class GPTrack;
class GPDomModel;

class GPConverter {
public:
    GPConverter(mu::engraving::Score *score, std::unique_ptr<GPDomModel> &&input_gp_dom);

    void convert_gp();

    const std::unique_ptr<GPDomModel> &gp_dom() const;

private:

    static constexpr int perc_channel = 9;
    using track_idx_t = mu::engraving::track_idx_t;
    using Fraction = mu::engraving::Fraction;
    using Measure = mu::engraving::Measure;
    using ChordRest = mu::engraving::ChordRest;
    using Note = mu::engraving::Note;

    using ChordRestContainer = std::vector<std::pair<mu::engraving::ChordRest *, const GPBeat *>>;
    using TieMap = std::unordered_map<track_idx_t, std::vector<mu::engraving::Tie *>>;

    struct Context {
        int32_t master_bar_index = 0;
        track_idx_t cur_track = 0;
        Fraction cur_tick;
    };

    void convert(const std::vector<std::unique_ptr<GPMasterBar>> &m_bs);
    void clear_defected_spanner();

    void convert_master_bar(const GPMasterBar *m_b, Context ctx);
    void fix_empty_measures();
    void convert_bars(const std::vector<std::unique_ptr<GPBar>> &bars, Context ctx);
    void convert_bar(const GPBar *bar, Context ctx);
    void convert_voices(const std::vector<std::unique_ptr<GPVoice>> &, Context ctx);
    void convert_voice(const GPVoice *, Context ctx);
    void convert_beats(const std::vector<std::shared_ptr<GPBeat>> &beats, Context ctx);
    Fraction convert_beat(const GPBeat *beat, ChordRestContainer &grace_chords, Context ctx);
    void configure_grace_chord(const GPBeat *beat, ChordRest *cr, GPBeat::OttavaType type);
    void convert_notes(const std::vector<std::shared_ptr<GPNote>> &notes, ChordRest *cr);
    void convert_note(const GPNote *note, mu::engraving::ChordRest *cr);

    void set_up_gp_score(const GPScore *gpscore);
    void set_up_tracks(const std::map<int, std::unique_ptr<GPTrack>> &tracks);
    void set_up_track(const std::unique_ptr<GPTrack> &tracks);
    void collect_tempo_map(const GPMasterTracks *m_tr);
    void collect_fermatas(const GPMasterBar *m_b, mu::engraving::Measure *measure);
    void fix_percussion();

    Measure *add_measure(const GPMasterBar *m_b);
    void add_time_sig(const GPMasterBar *m_b, Measure *measure);
    void add_key_sig(const GPMasterBar *m_b, Measure *measure);
    void add_triplet_feel(const GPMasterBar *m_b, Measure *measure);
    void add_direction(const GPMasterBar *m_b, Measure *measure);
    void add_section(const GPMasterBar *m_b, Measure *measure);
    void add_repeat(const GPMasterBar *m_b, Measure *measure);
    void add_volta(const GPMasterBar *m_b, Measure *measure);
    void do_add_volta(const GPMasterBar *m_b, Measure *measure);
    void add_clef(const GPBar *bar, int cur_track);
    bool add_simile_mark(const GPBar *bar, int cur_track);
    void add_barline(const GPMasterBar *m_b, Measure *measure, int32_t master_bar_index);

    void add_tie(const GPNote *gpnote, Note *note, TieMap &ties);
    void add_fret_diagram(const GPBeat *gpnote, ChordRest *note, const Context &ctx, bool as_harmony = true);
    ChordRest *add_chord_rest(const GPBeat *beats, const Context &ctx);
    void add_ornament(const GPNote *gpnote, Note *note);
    void add_vibrato_left_hand(const GPNote *gpnote, Note *note);
    void add_trill(const GPNote *gpnote, Note *note);
    Note *add_harmonic(const GPNote *gpnote, Note *note);
    void add_fingering(const GPNote *gpnote, Note *note);
    void add_accent(const GPNote *gpnote, Note *note);
    void add_string_number(const GPNote *gpnote, Note *note);
    void add_tapping(const GPNote *gpnote, Note *note, engraving::TappingHand hand);
    void add_left_hand_tapping(const GPNote *gpnote);
    void add_right_hand_tapping(const GPNote *gpnote);
    void add_slide(const GPNote *gpnote, Note *note);
    void add_single_slide(const GPNote *gpnote, Note *note);
    void add_pick_scrape(const GPNote *gpnote, Note *note);
    void add_let_ring(const GPNote *gpnote, Note *note);
    void add_palm_mute(const GPNote *gpnote, Note *note);
    void collect_continuous_slide(const GPNote *gpnote, Note *note);
    void collect_hammer_on(const GPNote *gpnote, Note *note);
    void add_bend(const GPNote *gpnote, Note *note);
    void set_pitch(Note *note, const GPNote::MidiPitch &midi_pitch);
    void set_tpc(Note *note, int accidental);
    int calculate_drum_pitch(int element, int variation, const muse::String &instrument_name);
    void add_text_to_note(muse::String string, Note *note);

    void add_legato(const GPBeat *beat, ChordRest *cr);
    void add_ottava(const GPBeat *gpb, ChordRest *cr);
    void add_dynamic(const GPBeat *beat, ChordRest *cr);
    void add_tapping(const GPBeat *beat, ChordRest *cr);
    void add_slapped(const GPBeat *beat, ChordRest *cr);
    void add_popped(const GPBeat *beat, ChordRest *cr);
    void add_brush(const GPBeat *beat, ChordRest *cr);
    void add_arpeggio(const GPBeat *beat, ChordRest *cr);
    void add_timer(const GPBeat *beat, ChordRest *cr);
    void add_free_text(const GPBeat *beat, ChordRest *cr);
    void add_tuplet(const GPBeat *beat, ChordRest *cr);
    void add_let_ring(const GPBeat *gpbeat, ChordRest *cr);
    void add_palm_mute(const GPBeat *gpbeat, ChordRest *cr);
    void add_trill(const GPBeat *gpbeat, ChordRest *cr);
    void add_dive(const GPBeat *beat, ChordRest *cr);
    void add_pick_scrape(const GPBeat *beat, ChordRest *cr);
    void add_harmonic_mark(const GPBeat *gpbeat, ChordRest *cr);
    void setup_tuplet_style(mu::engraving::Tuplet *tuplet);
    void add_vibrato_w_trem_bar(const GPBeat *beat, ChordRest *cr);
    void add_vibrato_left_hand(const GPBeat *beat, ChordRest *cr);
    void add_fadding(const GPBeat *beat, ChordRest *cr);
    void add_hair_pin(const GPBeat *beat, ChordRest *cr);
    void add_rasgueado(const GPBeat *beat, ChordRest *cr);
    void add_pick_stroke(const GPBeat *beat, ChordRest *cr);
    void add_tremolo(const GPBeat *beat, ChordRest *cr);
    void add_wah(const GPBeat *beat, ChordRest *cr);
    void add_golpe(const GPBeat *beat, ChordRest *cr);
    void add_barre(const GPBeat *beat, ChordRest *cr);
    void add_lyrics(const GPBeat *beat, ChordRest *cr, const Context &ctx);
    void clear_defected_grace_chord(ChordRestContainer &grace_ghords);

    void add_continuous_slide_hammer_on();
    void add_fermatas();
    void add_tempo_map();
    void add_instrument_changes();
    void fill_uncompleted_measure(const Context &ctx);
    void hide_rests_in_empty_measures(track_idx_t start_track, track_idx_t end_track);
    int get_string_number_for(Note *p_note, int pitch) const;
    void fill_tuplet();
    bool tuplet_params_changed(const GPBeat *beat, const ChordRest *cr);
    void set_beam_mode(const GPBeat *beat, ChordRest *cr, Measure *measure, Fraction tick);
    void add_tuning();
    void add_capos();
    int capo_fret(const mu::engraving::Part *part) const;

    const std::shared_ptr<mu::engraving::IEngravingConfiguration> &engraving_configuration() const;

    mu::engraving::Score *score_value;
    std::unique_ptr<GPDomModel> gp_dom_value;

    enum class SlideHammerOn { LEGATO_SLIDE, SLIDE, HAMMER_ON };
    std::list<std::pair<Note *, SlideHammerOn>> slide_hammer_on_map;

    GPMasterBar::TimeSig last_time_sig;
    GPMasterBar::TripletFeelType last_triplet_feel = GPMasterBar::TripletFeelType::NONE;
    std::unordered_map<size_t, GPMasterBar::KeySig> last_key_sigs;
    std::list<std::pair<Measure *, GPMasterBar::Fermata>> fermatas;
    std::unordered_multimap<int, GPMasterTracks::Automation> tempo_map;
    std::unordered_map<track_idx_t, GPBar::Clef> clefs;
    std::unordered_map<track_idx_t, GPBeat::DynamicType> dynamics;
    std::unordered_map<track_idx_t, bool> has_capo_;
    std::unordered_map<track_idx_t, std::vector<mu::engraving::Tie *>> ties_value;    // map(track, tie)
    std::unordered_map<track_idx_t, std::vector<mu::engraving::Tie *>> harmonic_ties; // map(track, tie between harmonic note)
    std::unordered_map<Note *, int> original_pitches_;                                // info of changed pitches for keeping track of ties
    std::unordered_map<mu::engraving::Chord *, mu::engraving::TremoloType> tremolos_in_chords_;
    std::unordered_map<track_idx_t, mu::engraving::Slur *> slurs; // map(track, slur)
    std::map<uint64_t /* part ID */, int /* fret position */> capo_params_;

    mutable GPBeat *current_gp_beat_ = nullptr; // used for passing info from notes

    std::map<uint16_t, uint16_t> drum_extension;
    mu::engraving::Volta *last_volta = nullptr;
    int last_diagram_idx = -1;

    struct NextTupletInfo {
        Fraction ratio;
        Fraction duration;                 // duration of all current elements
        std::vector<ChordRest *> elements; // elements that will be added to tuplet
        track_idx_t track;
        mu::engraving::Tuplet *tuplet = nullptr;
        mu::engraving::Measure *measure = nullptr;
        int lowest_base = ::mu::iex::guitarpro::GPConverter::NextTupletInfo::initial_lowest_base; // expected denominator
        static constexpr int initial_lowest_base = 1024;
    } next_tuplet_info_;

    // Index is the number of sharps. Using flat keysigs for signatures with double sharps
    std::vector<int> sharps_to_key_converter_{0, 1, 2, 3, 4, 5, 6, 7, -4, -3, -2, -1};
    // Index is the number of sharps. Using sharp keysigs for signatures with double flats
    std::vector<int> sharps_to_flat_keys_converter_{0, 1, 2, 3, 4, -7, -6, -5, -4, -3, -2, -1};

    static constexpr mu::engraving::voice_idx_t voices = 4;

    bool show_capo_ = true; // TODO-gp : settings

    bool chord_exists_in_bar_ = false;
    std::array<bool, ::mu::iex::guitarpro::GPConverter::voices> chord_exists_for_voice_;

    mu::engraving::BeamMode previous_beam_mode_ = mu::engraving::BeamMode::AUTO;

    std::unique_ptr<GPDrumSetResolver> drum_resolver;
    std::unique_ptr<ContiniousElementsBuilder> continious_elements_builder_;
    std::unique_ptr<GuitarBendImporter> guitar_bend_importer_;

    std::unordered_map<mu::engraving::track_idx_t, const GPBeat *> last_whammy_beat_;
};
} // namespace mu::iex::guitarpro
