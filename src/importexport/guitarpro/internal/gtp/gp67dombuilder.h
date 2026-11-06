#ifndef MU_IMPORTEXPORT_GP67DOMBUILDER_H
#define MU_IMPORTEXPORT_GP67DOMBUILDER_H

#include <memory>

#include "gpdommodel.h"
#include "gptrack.h"
#include "igpdombuilder.h"

namespace mu::iex::guitarpro {
struct GP67DomBuilder : IGPDomBuilder {
    GP67DomBuilder();

    void build_gp_dom_model(muse::XmlDomElement *dom_elem) override;
    std::unique_ptr<GPDomModel> get_gp_dom_model() override;

protected:

    void build_gp_score(muse::XmlDomNode *score_node);
    void build_gp_master_tracks(muse::XmlDomNode *master_track);
    void build_gp_audio_tracks(muse::XmlDomNode *audio_track);
    void build_gp_tracks(muse::XmlDomNode *tracks_node, muse::XmlDomNode *version_node);
    void build_gp_master_bars(muse::XmlDomNode *master_bars);
    void build_gp_bars(muse::XmlDomNode *bars);
    void build_gp_voices(muse::XmlDomNode *voices_node);
    void build_gp_beats(muse::XmlDomNode *beats);
    void build_gp_notes(muse::XmlDomNode *notes_node);
    void build_gp_rhythms(muse::XmlDomNode *rhythms);

    void break_lyrics_on_beats_if_need();
    bool is_lyrics_on_beats() const;

    virtual std::pair<int, std::unique_ptr<GPTrack>> create_gp_track(muse::XmlDomNode *track_node, muse::XmlDomNode *version_node) = 0;

    std::unique_ptr<GPMasterTracks> create_gp_master_track(muse::XmlDomNode *metadata);
    std::unique_ptr<GPMasterBar> create_gp_master_bar(muse::XmlDomNode *master_bar_node);
    std::pair<int, std::unique_ptr<GPBar>> create_gp_bar(muse::XmlDomNode *bar_node);
    std::pair<int, std::unique_ptr<GPVoice>> create_gp_voice(muse::XmlDomNode *voice_node);
    std::pair<int, std::shared_ptr<GPBeat>> create_gp_beat(muse::XmlDomNode *beat_node);
    std::pair<int, std::shared_ptr<GPNote>> create_gp_note(muse::XmlDomNode *note_node);
    std::pair<int, std::shared_ptr<GPRhythm>> create_gp_rhythm(muse::XmlDomNode *rhythm_node);

    void read_note_x_properties(const muse::XmlDomNode &properties_node, GPNote *n);
    void read_note_properties(muse::XmlDomNode *properties_node, GPNote *n);
    void read_beat_x_properties(const muse::XmlDomNode &properties_node, GPBeat *b);
    std::unique_ptr<GPNote::Bend> create_bend(muse::XmlDomNode *property_node);
    GPBeat::Whammy read_whammy(const muse::XmlDomElement &elem) const;
    void read_harmonic(muse::XmlDomNode *property_node, GPNote *note) const;

    std::vector<GPMasterTracks::Automation> read_tempo_map(muse::XmlDomNode *current_node);
    GPTrack::RSE read_track_rse(muse::XmlDomNode *track_child_node) const;
    GPMasterBar::KeySig read_key_sig(muse::XmlDomNode *key_node) const;
    bool read_use_flats(muse::XmlDomNode *key_node) const;
    GPMasterBar::TimeSig read_time_sig(muse::XmlDomNode *time_node) const;
    void read_track_properties(muse::XmlDomNode *properties_node, GPTrack *track, bool ignore_tuning_flats) const;
    void read_beat_properties(const muse::XmlDomNode &properties_node, GPBeat *beat) const;
    void read_diagram(const muse::XmlDomNode &items, GPTrack *track) const;
    void read_lyrics(const muse::XmlDomNode &items, GPTrack *track) const;
    std::vector<GPMasterBar::Fermata> read_fermatas(muse::XmlDomNode *fermatas_node) const;
    std::vector<GPMasterBar::Direction> read_repeats_jumps(muse::XmlDomNode *repeats_jumps_node) const;
    std::pair<muse::String, muse::String> read_master_bar_section(const muse::XmlDomNode &section_node) const;
    GPMasterBar::Repeat read_repeat(muse::XmlDomNode *repeat_node) const;
    std::vector<int> read_ending(muse::XmlDomNode *end_node) const;

    std::unordered_map<int, std::shared_ptr<GPNote>> notes;
    std::unordered_map<int, std::shared_ptr<GPRhythm>> rhythms_value;
    std::unordered_map<int, std::shared_ptr<GPBeat>> beats_value;
    std::unordered_map<int, std::unique_ptr<GPVoice>> voices;
    std::unordered_map<int, std::unique_ptr<GPBar>> bars_value;

    std::unique_ptr<GPDomModel> gp_dom;
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_GP67DOMBUILDER_H
