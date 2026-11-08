#include "gpconverter.h"

#include "../guitarprodrumset.h"
#include "../utils.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/bend.h"
#include "engraving/dom/box.h"
#include "engraving/dom/bracketitem.h"
#include "engraving/dom/capo.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordline.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/deadslapped.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/glissando.h"
#include "engraving/dom/gradualtempochange.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/dom/hammeronpulloff.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrchange.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/note.h"
#include "engraving/dom/ottava.h"
#include "engraving/dom/part.h"
#include "engraving/dom/pitchspelling.h"
#include "engraving/dom/rehearsalmark.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stringtunings.h"
#include "engraving/dom/tapping.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tripletfeel.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/editing/editchord.h"
#include "engraving/editing/editstaffbrackets.h"
#include "engraving/types/symid.h"
#include "gpdommodel.h"
#include "log.h"
#include "translation.h"

using namespace mu::engraving;

namespace mu::iex::guitarpro {
static mu::engraving::JumpType jump_type(const String &type_string) {
    static const std::map<String, JumpType> types{
        {u"DaCapo", JumpType::DC},
        {u"DaSegno", JumpType::DS},
        {u"DaCapoAlFine", JumpType::DC_AL_FINE},
        {u"DaCapoAlCoda", JumpType::DC_AL_CODA},
        {u"DaCapoAlDoubleCoda", JumpType::DC_AL_DBLCODA},
        {u"DaSegnoAlCoda", JumpType::DS_AL_CODA},
        {u"DaSegnoAlDoubleCoda", JumpType::DS_AL_DBLCODA},
        {u"DaSegnoAlFine", JumpType::DS_AL_FINE},
        {u"DaSegnoSegno", JumpType::DSS},
        {u"DaSegnoSegnoAlCoda", JumpType::DSS_AL_CODA},
        {u"DaSegnoSegnoAlDoubleCoda", JumpType::DSS_AL_DBLCODA},
        {u"DaSegnoSegnoAlFine", JumpType::DSS_AL_FINE},
    };

    auto it = types.find(type_string);
    if (it != types.end()) {
        return it->second;
    }

    LOGE() << "wrong jump type";
    return JumpType::USER;
}

static mu::engraving::MarkerType marker_type(const String &type_string) {
    static const std::map<String, MarkerType> types{
        {u"Segno", MarkerType::SEGNO},
        {u"SegnoSegno", MarkerType::VARSEGNO},
        {u"Coda", MarkerType::CODA},
        {u"DoubleCoda", MarkerType::VARCODA},
        {u"Fine", MarkerType::FINE},
        {u"DaCoda", MarkerType::DA_CODA},
        {u"DaDoubleCoda", MarkerType::DA_DBLCODA},
    };

    auto it = types.find(type_string);
    if (it != types.end()) {
        return it->second;
    }

    LOGE() << "wrong direction marker type";
    return MarkerType::USER;
}

static mu::engraving::TripletFeelType triplet_feel_type(GPMasterBar::TripletFeelType tf) {
    static const std::map<GPMasterBar::TripletFeelType, TripletFeelType> types{
        {GPMasterBar::TripletFeelType::TRIPLET8TH, TripletFeelType::TRIPLET_8TH},
        {GPMasterBar::TripletFeelType::TRIPLET16TH, TripletFeelType::TRIPLET_16TH},
        {GPMasterBar::TripletFeelType::DOTTED8TH, TripletFeelType::DOTTED_8TH},
        {GPMasterBar::TripletFeelType::DOTTED16TH, TripletFeelType::DOTTED_16TH},
        {GPMasterBar::TripletFeelType::SCOTTISH8TH, TripletFeelType::SCOTTISH_8TH},
        {GPMasterBar::TripletFeelType::SCOTTISH16TH, TripletFeelType::SCOTTISH_16TH},
        {GPMasterBar::TripletFeelType::NONE, TripletFeelType::NONE}};

    auto it = types.find(tf);
    if (it != types.end()) {
        return it->second;
    }

    return TripletFeelType::NONE;
}

static std::pair<bool, mu::engraving::OttavaType> ottava_type(GPBeat::OttavaType t) {
    static const std::map<GPBeat::OttavaType, mu::engraving::OttavaType> types{{GPBeat::OttavaType::VA8, OttavaType::OTTAVA_8VA},
                                                                               {GPBeat::OttavaType::VB8, OttavaType::OTTAVA_8VB},
                                                                               {GPBeat::OttavaType::MA15, OttavaType::OTTAVA_15MA},
                                                                               {GPBeat::OttavaType::MB15, OttavaType::OTTAVA_15MB}};

    auto it = types.find(t);
    if (it != types.end()) {
        return {true, it->second};
    }

    return {false, OttavaType::OTTAVA_8VA};
}

static GPBeat::HarmonicMarkType harmonic_type_note_to_beat(GPNote::Harmonic::Type t) {
    static const std::map<GPNote::Harmonic::Type, GPBeat::HarmonicMarkType> types{
        {GPNote::Harmonic::Type::ARTIFICIAL, GPBeat::HarmonicMarkType::ARTIFICIAL},
        {GPNote::Harmonic::Type::PINCH, GPBeat::HarmonicMarkType::PINCH},
        {GPNote::Harmonic::Type::TAP, GPBeat::HarmonicMarkType::TAP},
        {GPNote::Harmonic::Type::SEMI, GPBeat::HarmonicMarkType::SEMI},
        {GPNote::Harmonic::Type::FEED_BACK, GPBeat::HarmonicMarkType::FEED_BACK}};

    auto it = types.find(t);
    if (it != types.end()) {
        return it->second;
    }

    return GPBeat::HarmonicMarkType::NONE;
}

static ContiniousElementsBuilder::ImportType ottava_to_import_type(GPBeat::OttavaType t) {
    static const std::map<GPBeat::OttavaType, ContiniousElementsBuilder::ImportType> types{
        {GPBeat::OttavaType::MA15, ContiniousElementsBuilder::ImportType::OTTAVA_MA15},
        {GPBeat::OttavaType::VA8, ContiniousElementsBuilder::ImportType::OTTAVA_VA8},
        {GPBeat::OttavaType::VB8, ContiniousElementsBuilder::ImportType::OTTAVA_VB8},
        {GPBeat::OttavaType::MB15, ContiniousElementsBuilder::ImportType::OTTAVA_MB15}};

    auto it = types.find(t);
    if (it != types.end()) {
        return it->second;
    }

    return ContiniousElementsBuilder::ImportType::NONE;
}

static ContiniousElementsBuilder::ImportType hairpin_to_import_type(GPBeat::Hairpin t) {
    static const std::map<GPBeat::Hairpin, ContiniousElementsBuilder::ImportType> types{
        {GPBeat::Hairpin::CRESCENDO, ContiniousElementsBuilder::ImportType::HAIRPIN_CRESCENDO},
        {GPBeat::Hairpin::DECRESCENDO, ContiniousElementsBuilder::ImportType::HAIRPIN_DIMINUENDO}};

    auto it = types.find(t);
    if (it != types.end()) {
        return it->second;
    }

    return ContiniousElementsBuilder::ImportType::NONE;
}

static void set_pitch_by_ottava_type(mu::engraving::Note *note, mu::engraving::OttavaType type) {
    note->set_pitch(clamp_pitch_octaved(note->pitch() - ::mu::engraving::ottava_default[int(type)].shift));
}

static std::unordered_map<uint64_t, mu::engraving::StringData> string_datas;

const std::shared_ptr<IEngravingConfiguration> &GPConverter::engraving_configuration() const { return score_value->configuration(); }

GPConverter::GPConverter(Score *score, std::unique_ptr<GPDomModel> &&gp_dom) : score_value(score), gp_dom_value(std::move(gp_dom)) {
    drum_extension = {
        {91, 40},  //Snare(rim shot)
        {92, 42},  //Hi Hat (half)
        {93, 51},  //Ride (edje)
        {94, 59},  //Ride (choke)
        {95, 55},  //Splash (choke)
        {96, 52},  //Chine(choke)
        {97, 49},  //Crash high (choke)
        {98, 57},  //Crash medium (choke)
        {99, 56},  //Cowbell low (hit)
        {100, 56}, //Cowbell low (tip)
        {101, 56}, //Cowbell medium (tip)
        {102, 56}, //Cowbell high (hit)
        {103, 56}, //Cowbell high (tip)
        {104, 60}, //Hand (mute)
        {105, 60}, //Hand (slap)
        {106, 61}, //Hand (mute)
        {107, 61}, //Hand (slap)
        {108, 64}, //Conga low (slap)
        {109, 64}, //Conga low (mute)
        {110, 64}, //Conga high (slap)
        {111, 54}, //Tambourine (return)
        {112, 54}, //Tambourine (roll)
        {113, 54}, //Tambourine (hand)
        {114, 43}, //Grancassa (hit)
        {115, 49}, //Piatti (hit)
        {116, 49}, //Piatti (hand)
        {117, 69}, //Cabasa (return)
        {118, 70}, //Left Maraca (return)
        {119, 70}, //Right Maraca (hit)
        {120, 70}, //Right Maraca (return)
        {122, 82}, //Shaker (return)
        {123, 84}, //Bell Tree (return)
        {124, 62}, //Golpe (thumb)
        {125, 62}, //Golpe (finger)
        {126, 59}, //Ride (middle)
        {127, 59}  //Ride (bell)
    };

    drum_resolver = std::make_unique<GPDrumSetResolver>();
    drum_resolver->init_gp_drum();
    continious_elements_builder_ = std::make_unique<ContiniousElementsBuilder>(score_value);
    guitar_bend_importer_ = std::make_unique<GuitarBendImporter>(score_value);
}

const std::unique_ptr<GPDomModel> &GPConverter::gp_dom() const { return gp_dom_value; }

void GPConverter::convert_gp() {
    set_up_gp_score(gp_dom_value->score());
    set_up_tracks(gp_dom_value->tracks());
    collect_tempo_map(gp_dom_value->master_tracks());

    convert(gp_dom_value->master_bars());

    clear_defected_spanner();
    fix_percussion();
    add_capos();
    utils::add_play_count_texts(score_value);
}

void GPConverter::fix_percussion() {
    // After reading some of the GP files, there're
    // parts, which contain only one percussion instrument.
    // In this case the program value for this instrument will
    // be set to the pitch value of the notes of the instrument,
    // thus we need to reset this value to 0.
    for (size_t i = 0; i < score_value->parts().size(); ++i) {
        mu::engraving::Part *p_part = score_value->parts()[i];
        IF_ASSERT_FAILED(!!p_part) { continue; }

        mu::engraving::Instrument *p_instrument = p_part->instrument();
        if (!p_instrument) {
            continue;
        }

        if (!p_instrument->use_drumset()) {
            continue;
        }

        if (p_part->midi_channel() == ::mu::iex::guitarpro::GPConverter::perc_channel) {
            p_part->set_midi_program(0, 128);
        }
    }
}

void GPConverter::fill_tuplet() {
    if (next_tuplet_info_.elements.empty() || !next_tuplet_info_.tuplet) {
        LOGE() << "not able to add elements to tuplet";
        return;
    }

    Tuplet *tuplet = next_tuplet_info_.tuplet;
    ChordRest *first_cr = next_tuplet_info_.elements.front();
    tuplet->set_track(first_cr->track());
    tuplet->set_ownership_parent(first_cr->measure());
    tuplet->set_tick(first_cr->tick());
    tuplet->set_base_len(Fraction(1, next_tuplet_info_.lowest_base));
    tuplet->set_ticks(next_tuplet_info_.duration);
    setup_tuplet_style(tuplet);

    for (ChordRest *elem : next_tuplet_info_.elements) {
        tuplet->add(elem);
    }

    /// TODO: solve correctly within engraving
    /// avoiding brackets on single note
    if (tuplet->elements().size() == 1) {
        tuplet->set_bracket_type(TupletBracketType::SHOW_NO_BRACKET);
    }

    next_tuplet_info_.elements.clear();
    next_tuplet_info_.tuplet = nullptr;
    next_tuplet_info_.duration = Fraction();
    next_tuplet_info_.lowest_base = NextTupletInfo::initial_lowest_base;
}

void GPConverter::convert(const std::vector<std::unique_ptr<GPMasterBar>> &master_bars) {
    for (uint32_t mi = 0; mi < master_bars.size(); ++mi) {
        Context ctx;
        ctx.master_bar_index = mi;
        convert_master_bar(master_bars.at(mi).get(), ctx);
    }

    // glueing line segment elements separated with rests
    continious_elements_builder_->add_elements_to_score();

    add_tempo_map();
    add_instrument_changes();
    guitar_bend_importer_->add_elements_to_score();

    add_fermatas();
    add_continuous_slide_hammer_on();
    add_tuning();
}

void GPConverter::convert_master_bar(const GPMasterBar *m_b, Context ctx) {
    Measure *measure = add_measure(m_b);

    add_time_sig(m_b, measure);
    add_key_sig(m_b, measure);
    add_barline(m_b, measure, ctx.master_bar_index);
    add_repeat(m_b, measure);
    collect_fermatas(m_b, measure);
    convert_bars(m_b->bars(), ctx);
    add_triplet_feel(m_b, measure);
    add_section(m_b, measure);
    add_direction(m_b, measure);

    fix_empty_measures();
}

void GPConverter::fix_empty_measures() {
    // Get all ChordRest elems and sort them by staves
    // Also store root Segment ptr will need it later to delete some rest elems
    std::map<track_idx_t, std::vector<std::pair<Segment *, EngravingItem *>>> elems;

    size_t ntracks = score_value->ntracks();
    SegmentType type = SegmentType::CHORD_REST;
    Measure *last_measure = score_value->last_measure();

    for (Segment *s = last_measure->segments().first(type); s; s = s->next(type)) {
        for (track_idx_t i = 0; i < ntracks; ++i) {
            auto e = s->element(i);
            if (!e) {
                continue;
            }
            elems[i / ::mu::iex::guitarpro::GPConverter::voices].push_back({s, e});
        }
    }

    for (const auto &[staff_idx, seg_item_pairs] : elems) {
        bool should_clear = true;
        for (const auto &p : seg_item_pairs) {
            // Don't need to do anything if there is not "rest" elem on a staff
            if (!p.second->is_rest()) {
                should_clear = false;
                break;
            }
        }
        if (should_clear) {
            // Keep only one rest element in a bar and make its duration V_MEASURE
            // that way layout can recognize this bar as "empty"
            // and properly render mmrests
            if (seg_item_pairs.empty()) {
                continue;
            }
            for (size_t i = 1; i < seg_item_pairs.size(); ++i) {
                Rest *rest = to_rest(seg_item_pairs.at(i).second);
                if (Tuplet *tuplet = rest->tuplet()) {
                    tuplet->remove(rest);
                }

                seg_item_pairs.at(i).first->remove(rest);
            }

            Rest *rest = to_rest(seg_item_pairs.at(0).second);
            rest->set_ticks(last_measure->ticks());
            rest->set_duration_type(DurationType::V_MEASURE);
            if (Tuplet *tuplet = rest->tuplet()) {
                tuplet->remove(rest);
                if (tuplet->elements().empty()) {
                    if (tuplet->tuplet()) {
                        tuplet->tuplet()->remove(tuplet);
                    }

                    delete tuplet;
                }
            }
        }
    }
}

void GPConverter::convert_bars(const std::vector<std::unique_ptr<GPBar>> &bars, Context ctx) {
    ctx.cur_track = 0;
    for (const auto &bar : bars) {
        chord_exists_in_bar_ = false;
        chord_exists_for_voice_.fill(false);
        convert_bar(bar.get(), ctx);
        ctx.cur_track += ::mu::iex::guitarpro::GPConverter::voices;
    }
}

void GPConverter::convert_bar(const GPBar *bar, Context ctx) {
    add_clef(bar, static_cast<int>(ctx.cur_track));

    if (add_simile_mark(bar, static_cast<int>(ctx.cur_track))) {
        return;
    }

    convert_voices(bar->voices(), ctx);
    hide_rests_in_empty_measures(ctx.cur_track, ctx.cur_track + ::mu::iex::guitarpro::GPConverter::voices);
}

void GPConverter::add_barline(const GPMasterBar *m_b, Measure *measure, int32_t master_bar_index) {
    static bool inside_free_time = false;
    size_t staves = score_value->staves().size();
    bool last_measure = (master_bar_index == static_cast<int32_t>(gp_dom_value->master_bars().size() - 1));

    if (!last_measure && m_b->barline_type() == GPMasterBar::BarlineType::NORMAL) {
        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            measure->set_end_bar_line_type(mu::engraving::BarLineType::NORMAL, staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
        }
    }

    if (!last_measure && m_b->barline_type() == GPMasterBar::BarlineType::DOUBLE) {
        for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
            measure->set_end_bar_line_type(mu::engraving::BarLineType::DOUBLE, staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
        }
    }

    GPMasterBar::TimeSig sig = m_b->time_sig();
    auto score_time_sig = Fraction(sig.numerator, sig.denominator);

    if (m_b->free_time()) {
        if (!last_measure && m_b->barline_type() != GPMasterBar::BarlineType::DOUBLE) {
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                measure->set_end_bar_line_type(mu::engraving::BarLineType::BROKEN, staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
            }
        }
        if (!inside_free_time) {
            inside_free_time = true;

            // Free time text
            for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
                Segment *s = measure->get_segment(SegmentType::TIME_SIG, measure->tick());
                StaffText *st = Factory::create_staff_text(s);
                st->set_track(staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
                st->set_plain_text(muse::mtrc("iex_guitarpro", "Free time", "time signature"));
                s->add(st);

                // if timeSig is different, it was added before, here we handle "freetime"
                if (last_time_sig.numerator != sig.numerator || last_time_sig.denominator != sig.denominator) {
                    return;
                }
                TimeSig *ts = Factory::create_time_sig(s);
                ts->set_sig(score_time_sig);
                ts->set_track(staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
                ts->set_large_parentheses(true);
                s->add(ts);
            }
        }
    } else {
        inside_free_time = false;
    }
    last_time_sig.numerator = sig.numerator;
    last_time_sig.denominator = sig.denominator;
}

void GPConverter::convert_voices(const std::vector<std::unique_ptr<GPVoice>> &voices, Context ctx) {
    if (voices.empty()) {
        ctx.cur_tick = score_value->last_measure()->tick();
        fill_uncompleted_measure(ctx);
    }

    track_idx_t current_track_first_voice = ctx.cur_track;
    for (const auto &voice : voices) {
        ctx.cur_track = current_track_first_voice + voice->position();
        convert_voice(voice.get(), ctx);
    }

    bool has_first_voice =
        std::any_of(voices.begin(), voices.end(), [](const std::unique_ptr<GPVoice> &voice) { return voice->position() == 0; });

    if (!has_first_voice && score_value->last_measure()) {
        score_value->set_rest(score_value->last_measure()->tick(), current_track_first_voice, score_value->last_measure()->ticks(), true,
                              nullptr);
    }
}

void GPConverter::convert_voice(const GPVoice *voice, Context ctx) {
    ctx.cur_tick = score_value->last_measure()->tick();
    convert_beats(voice->beats(), ctx);
}

void GPConverter::convert_beats(const std::vector<std::shared_ptr<GPBeat>> &beats, Context ctx) {
    ChordRestContainer grace_chords;
    for (const auto &beat : beats) {
        current_gp_beat_ = beat.get();
        ctx.cur_tick = convert_beat(beat.get(), grace_chords, ctx);
    }

    fill_uncompleted_measure(ctx);
    clear_defected_grace_chord(grace_chords);
}

Fraction GPConverter::convert_beat(const GPBeat *beat, ChordRestContainer &grace_chords, Context ctx) {
    Measure *last_measure = score_value->last_measure();

    if (ctx.cur_tick >= last_measure->ticks() + last_measure->tick()) {
        return ctx.cur_tick;
    }

    auto cur_segment = last_measure->get_segment(SegmentType::CHORD_REST, ctx.cur_tick);

    ChordRest *cr = add_chord_rest(beat, ctx);
    if (cr) {
        cur_segment->set_ticks(cr->ticks());
    }
    if (engraving_configuration()->guitar_pro_import_experimental() && beat->dead_slapped() && cr->is_rest()) {
        Rest *rest = to_rest(cr);
        cur_segment->add(rest);
        DeadSlapped *dc = Factory::create_dead_slapped(rest);
        dc->set_track(rest->track());
        rest->add(dc);
    } else {
        if (beat->grace_notes() != GPBeat::GraceNotes::NONE) {
            if (cr->is_rest()) {
                delete cr;
                return ctx.cur_tick;
            }

            cur_segment->add(cr);

            grace_chords.push_back({cr, beat});

            return ctx.cur_tick;
        }

        cur_segment->add(cr);

        if (cr->is_chord()) {
            chord_exists_for_voice_[ctx.cur_track % ::mu::iex::guitarpro::GPConverter::voices] = true;
            chord_exists_in_bar_ = true;

            if (beat->stem_orientation_user_defined()) {
                to_chord(cr)->set_stem_direction(beat->stem_orientation_up() ? DirectionV::UP : DirectionV::DOWN);
            }

            set_beam_mode(beat, cr, last_measure, ctx.cur_tick);
        }

        if (cr->is_chord() && !grace_chords.empty()) {
            int gr_index = 0;

            for (auto [p_gr_chord, p_beat] : grace_chords) {
                configure_grace_chord(p_beat, p_gr_chord, beat->ottava_type());
                if (p_gr_chord->is_chord()) {
                    to_chord(p_gr_chord)->set_grace_index(gr_index++);
                }

                Fraction fr(1, (grace_chords.size() == 1 ? 1 : 2) * 8);
                p_gr_chord->set_duration_type(TDuration(fr));

                cr->add(p_gr_chord);
                add_legato(p_beat, p_gr_chord);
            }
            grace_chords.clear();
        }

        convert_notes(beat->notes(), cr);

        add_tuplet(beat, cr);
        add_timer(beat, cr);
        add_free_text(beat, cr);
        add_fadding(beat, cr);
        add_hair_pin(beat, cr);
        add_tremolo(beat, cr);
        add_pick_stroke(beat, cr);
        add_dynamic(beat, cr);
        add_wah(beat, cr);
        add_golpe(beat, cr);
        add_fret_diagram(beat, cr, ctx);
        add_barre(beat, cr);
        add_tapping(beat, cr);
        add_slapped(beat, cr);
        add_popped(beat, cr);
        add_brush(beat, cr);
        add_arpeggio(beat, cr);
        add_lyrics(beat, cr, ctx);
        add_legato(beat, cr);

        // line segment elements
        add_ottava(beat, cr);
        add_let_ring(beat, cr);
        add_palm_mute(beat, cr);
        add_trill(beat, cr);
        add_rasgueado(beat, cr);
        add_dive(beat, cr);
        add_pick_scrape(beat, cr);
        add_harmonic_mark(beat, cr);
        add_vibrato_left_hand(beat, cr);
        add_vibrato_w_trem_bar(beat, cr);
    }

    ctx.cur_tick += cr->actual_ticks();

    return ctx.cur_tick;
}

void GPConverter::convert_notes(const std::vector<std::shared_ptr<GPNote>> &notes, ChordRest *cr) {
    for (const auto &note : notes) {
        convert_note(note.get(), cr);
    }

    //! NOTE: later notes order is used in linked staff to create ties, glissando
    if (cr->is_chord()) {
        Chord *ch = to_chord(cr);
        ch->sort_notes();
        mu::iex::guitarpro::utils::create_ghost_note_paren_groups(ch);
    }
}

void GPConverter::convert_note(const GPNote *gpnote, ChordRest *cr) {
    if (!cr->is_chord()) {
        return;
    }

    Chord *ch = to_chord(cr);

    Note *note = mu::engraving::Factory::create_note(ch);
    note->set_track(cr->track());
    cr->add(note);
    set_pitch(note, gpnote->midi_pitch());
    set_tpc(note, gpnote->accidental());

    Note *harmonic_note = add_harmonic(gpnote, note);
    add_bend(gpnote, note);

    add_let_ring(gpnote, note);
    add_palm_mute(gpnote, note);
    note->set_ghost(gpnote->ghost_note());

    note->set_dead_note(gpnote->muted());
    add_accent(gpnote, note);
    add_slide(gpnote, note);
    add_pick_scrape(gpnote, note);
    collect_hammer_on(gpnote, note);
    add_right_hand_tapping(gpnote);
    add_left_hand_tapping(gpnote);
    add_string_number(gpnote, note);
    add_ornament(gpnote, note);
    add_vibrato_left_hand(gpnote, note);
    add_trill(gpnote, note);
    add_fingering(gpnote, note);
    add_tie(gpnote, note, ties_value);
    if (harmonic_note) {
        add_tie(gpnote, harmonic_note, harmonic_ties);
    }
}

void GPConverter::configure_grace_chord(const GPBeat *beat, ChordRest *cr, GPBeat::OttavaType type) {
    convert_notes(beat->notes(), cr);

    if (!cr->is_chord()) {
        return;
    }

    Chord *gr_chord = to_chord(cr);

    if (GPBeat::GraceNotes::ON_BEAT == beat->grace_notes()) {
        gr_chord->set_note_type(NoteType::APPOGGIATURA);
    } else {
        gr_chord->set_note_type(NoteType::ACCIACCATURA);
    }

    const auto &[found_ottava, mu_ottava_type] = ottava_type(type);
    if (found_ottava) {
        for (Note *note : gr_chord->notes()) {
            set_pitch_by_ottava_type(note, mu_ottava_type);
        }
    }
}

void GPConverter::add_time_sig(const GPMasterBar *m_b, Measure *measure) {
    GPMasterBar::TimeSig sig = m_b->time_sig();
    Fraction tick = measure->tick();
    auto score_time_sig = Fraction(sig.numerator, sig.denominator);
    measure->set_ticks(score_time_sig);
    size_t staves = score_value->staves().size();

    if (last_time_sig.numerator == sig.numerator && last_time_sig.denominator == sig.denominator) {
        return;
    }
    last_time_sig.numerator = sig.numerator;
    last_time_sig.denominator = sig.denominator;

    for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
        Staff *staff = score_value->staff(staff_idx);
        if (staff->staff_type()->gen_timesig()) {
            Segment *s = measure->get_segment(SegmentType::TIME_SIG, tick);
            TimeSig *t = Factory::create_time_sig(s);
            track_idx_t cur_track = staff_idx * ::mu::iex::guitarpro::GPConverter::voices;
            t->set_track(cur_track);
            t->set_sig(score_time_sig);
            if (m_b->free_time()) {
                t->set_large_parentheses(true);
            }
            s->add(t);
        }
    }
}

void GPConverter::add_repeat(const GPMasterBar *m_b, Measure *measure) {
    add_volta(m_b, measure);

    if (m_b->repeat().type == GPMasterBar::Repeat::Type::NONE) {
        return;
    }

    if (m_b->repeat().type == GPMasterBar::Repeat::Type::START) {
        measure->set_repeat_start(true);
    }
    if (m_b->repeat().type == GPMasterBar::Repeat::Type::END) {
        measure->set_repeat_end(true);
    }
    if (m_b->repeat().type == GPMasterBar::Repeat::Type::START_END) {
        measure->set_repeat_start(true);
        measure->set_repeat_end(true);
    }
    measure->set_repeat_count(m_b->repeat().count);
}

void GPConverter::add_volta(const GPMasterBar *m_b, Measure *measure) {
    if (m_b->alternate_ending().empty()) {
        last_volta = nullptr;
        return;
    }

    if (last_volta && last_volta->endings().size() != m_b->alternate_ending().size()) {
        last_volta = nullptr;
    }

    if (last_volta) {
        for (const auto &end : m_b->alternate_ending()) {
            if (!last_volta->has_ending(end)) {
                last_volta = nullptr;
                break;
            }
        }
    }

    do_add_volta(m_b, measure);
}

void GPConverter::do_add_volta(const GPMasterBar *m_b, Measure *measure) {
    mu::engraving::Volta *volta;
    if (last_volta) {
        volta = last_volta;
        score_value->remove_element(volta);
    } else {
        volta = new mu::engraving::Volta(score_value->dummy());
        volta->set_tick(measure->tick());
        last_volta = volta;
    }

    volta->endings().clear();
    volta->set_tick2(measure->end_tick());

    String str;
    for (const auto &end : m_b->alternate_ending()) {
        volta->endings().push_back(end);
        if (str.is_empty()) {
            str += String(u"%1").arg(end);
        } else {
            str += String(u"-%1").arg(end);
        }
    }
    volta->set_text(str);

    score_value->add_element(volta);
}

void GPConverter::add_direction(const GPMasterBar *m_b, Measure *measure) {
    for (const auto &dir : m_b->directions()) {
        if (dir.type == GPMasterBar::Direction::Type::JUMP) {
            Jump *jump = Factory::create_jump(measure);
            jump->set_jump_type(jump_type(dir.name));
            jump->set_track(0);
            measure->add(jump);
        } else {
            Marker *marker = Factory::create_marker(measure);
            marker->set_marker_type(marker_type(dir.name));
            marker->reset_property(Pid::LABEL);
            marker->set_track(0);
            measure->add(marker);
        }
    }
}

void GPConverter::add_section(const GPMasterBar *m_b, Measure *measure) {
    if (m_b->section().first.is_empty() && m_b->section().second.is_empty()) {
        return;
    }

    if (!m_b->section().first.is_empty()) {
        Segment *s = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
        RehearsalMark *t = Factory::create_rehearsal_mark(s);
        t->set_plain_text(m_b->section().first);
        t->set_type(RehearsalMark::Type::MAIN);
        t->set_track(0);
        s->add(t);
    }
    if (!m_b->section().second.is_empty()) {
        Segment *s = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
        RehearsalMark *t = Factory::create_rehearsal_mark(s);
        t->set_plain_text(m_b->section().second);
        t->set_type(RehearsalMark::Type::ADDITIONAL);
        t->set_track(0);
        s->add(t);
    }
}

void GPConverter::add_triplet_feel(const GPMasterBar *m_b, Measure *measure) {
    if (m_b->triplet_feel() == last_triplet_feel) {
        return; // if last triplet of last measure is equal current dont create new system text
    }

    Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
    TripletFeelType type = triplet_feel_type(m_b->triplet_feel());
    TripletFeel *triplet_feel = Factory::create_triplet_feel(segment, type);
    last_triplet_feel = m_b->triplet_feel();
    triplet_feel->set_track(0);
    segment->add(triplet_feel);
}

void GPConverter::add_key_sig(const GPMasterBar *m_b, Measure *measure) {
    auto convert_key_sig = [](GPMasterBar::KeySig::Accidentals k_s) {
        if (k_s == GPMasterBar::KeySig::Accidentals::C_B) {
            return Key::C_B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::G_B) {
            return Key::G_B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::D_B) {
            return Key::D_B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::A_B) {
            return Key::A_B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::E_B) {
            return Key::E_B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::B_B) {
            return Key::B_B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::F) {
            return Key::F;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::C) {
            return Key::C;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::G) {
            return Key::G;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::D) {
            return Key::D;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::A) {
            return Key::A;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::E) {
            return Key::E;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::B) {
            return Key::B;
        } else if (k_s == GPMasterBar::KeySig::Accidentals::F_S) {
            return Key::F_S;
        } else {
            return Key::C_S;
        }
    };

    auto convert_mode = [](GPMasterBar::KeySig::Mode m) {
        if (m == GPMasterBar::KeySig::Mode::MAJOR) {
            return KeyMode::MAJOR;
        } else {
            return KeyMode::MINOR;
        }
    };

    using KS = GPMasterBar::KeySig::Accidentals;

    Fraction tick = measure->tick();
    size_t staves = score_value->staves().size();

    auto get_num_sharps = [this](int key) {
        if (key > 0) {
            for (size_t i = 0; i < sharps_to_key_converter_.size(); ++i) {
                if (sharps_to_key_converter_.at(i) == key) {
                    return i;
                }
            }
        } else {
            for (size_t i = 0; i < sharps_to_flat_keys_converter_.size(); ++i) {
                if (sharps_to_flat_keys_converter_.at(i) == key) {
                    return i;
                }
            }
        }
        // Never should get here
        return muse::nidx;
    };

    for (size_t staff_idx = 0; staff_idx < staves; ++staff_idx) {
        if (!tick.is_zero() && last_key_sigs[staff_idx].accidental_count == m_b->key_sig().accidental_count) {
            continue;
        }
        int key = static_cast<int>(m_b->key_sig().accidental_count);

        bool use_flats = m_b->use_flats() || key < 0;
        size_t num_sharps = get_num_sharps(key);
        IF_ASSERT_FAILED(num_sharps != muse::nidx) {
            LOGE() << "Unprocessable key for key signature";
            num_sharps = 0;
        }
        num_sharps = static_cast<int>(num_sharps) % 12;
        if (use_flats) {
            key = sharps_to_flat_keys_converter_.at(num_sharps);
        } else {
            key = sharps_to_key_converter_.at(num_sharps);
        }

        auto score_key_sig = convert_key_sig(static_cast<KS>(key));
        auto score_mode = convert_mode(m_b->key_sig().mode);

        Staff *staff = score_value->staff(staff_idx);
        if (staff->staff_type()->gen_timesig()) {
            Segment *s = measure->get_segment(SegmentType::KEY_SIG, tick);
            KeySig *t = mu::engraving::Factory::create_key_sig(s);
            t->set_track(staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
            t->set_key(score_key_sig);
            t->set_mode(score_mode);
            s->add(t);
            last_key_sigs[staff_idx] = m_b->key_sig();
        }
    }
}

void GPConverter::set_up_gp_score(const GPScore *gpscore) {
    engraving_configuration()->set_guitar_pro_multivoice_enabled(gpscore->multi_voice());

    std::vector<String> field_names = {gpscore->title(), gpscore->sub_title(), gpscore->artist(),
                                       gpscore->album(), gpscore->composer(),  gpscore->poet()};

    bool create_title_field =
        std::any_of(field_names.begin(), field_names.end(), [](const String &field_name) { return !field_name.is_empty(); });

    if (!create_title_field && !engraving_configuration()->guitar_pro_import_experimental()) {
        return;
    }

    MeasureBase *m = nullptr;
    if (!score_value->measures()->first()) {
        m = Factory::create_title_v_box(score_value);
        score_value->measures()->append(m);
    } else {
        m = score_value->measures()->first();
        if (!m->is_v_box()) {
            MeasureBase *mb = Factory::create_title_v_box(score_value);
            score_value->add_measure(mb, m);
            m = mb;
        }
    }

    if (!gpscore->title().is_empty() || engraving_configuration()->guitar_pro_import_experimental()) {
        Text *s = Factory::create_text(m, TextStyleType::TITLE);
        s->set_plain_text(gpscore->title());
        m->add(s);
    }
    if (!gpscore->sub_title().is_empty() || !gpscore->artist().is_empty() || !gpscore->album().is_empty() ||
        engraving_configuration()->guitar_pro_import_experimental()) {
        Text *s = Factory::create_text(m, TextStyleType::SUBTITLE);
        String str;
        if (!gpscore->sub_title().is_empty()) {
            str.append(gpscore->sub_title());
        }
        if (!gpscore->artist().is_empty()) {
            if (!str.is_empty()) {
                str.append(u'\n');
            }
            str.append(gpscore->artist());
        }
        if (!gpscore->album().is_empty()) {
            if (!str.is_empty()) {
                str.append(u'\n');
            }
            str.append(gpscore->album());
        }
        s->set_plain_text(str);
        m->add(s);
    }
    if (!gpscore->composer().is_empty()) {
        Text *s = Factory::create_text(m, TextStyleType::COMPOSER);
        s->set_plain_text(muse::mtrc("iex_guitarpro", "Music by %1").arg(gpscore->composer()));
        m->add(s);
    }

    if (!gpscore->poet().is_empty() || engraving_configuration()->guitar_pro_import_experimental()) {
        Text *s = Factory::create_text(m, TextStyleType::LYRICIST);
        if (!gpscore->poet().is_empty()) {
            s->set_plain_text(muse::mtrc("iex_guitarpro", "Words by %1").arg(gpscore->poet()));
        }

        m->add(s);
    }
}

void GPConverter::set_up_tracks(const std::map<int, std::unique_ptr<GPTrack>> &tracks) {
    string_datas.clear();
    for (const auto &track : tracks) {
        set_up_track(track.second);
    }
}

void GPConverter::set_up_track(const std::unique_ptr<GPTrack> &t_r) {
    int programm = t_r->programm();
    int midi_channel = t_r->midi_channel();

    int idx = t_r->idx() + 1; // 0 is invalid index

    Part *part = new Part(score_value);
    part->set_plain_long_name(t_r->name());
    part->set_plain_short_name(t_r->short_name());
    part->set_id(idx);

    score_value->append_part(part);
    for (size_t staff_idx = 0; staff_idx < t_r->staff_count(); staff_idx++) {
        Staff *s = Factory::create_staff(part);
        score_value->append_staff(s);
    }

    if (t_r->staff_count() > 1) {
        EditStaffBrackets::add_bracket(score_value, part->staff(0)->idx(),
                                       mu::engraving::Factory::create_bracket_item(score_value->dummy(), BracketType::BRACE, 2));
        part->staff(0)->set_bar_line_span(true);
    }

    part->set_midi_program(programm);
    part->set_midi_channel(midi_channel);

    int vol_val = static_cast<int>(std::lround(t_r->rse().volume * 127));
    part->instrument()->channel(0)->set_volume(std::clamp(vol_val, 0, 127));

    int pan_val = static_cast<int>(std::lround(t_r->rse().pan * 127));
    part->instrument()->channel(0)->set_pan(std::clamp(pan_val, 0, 127));

    Instrument *instr = part->instrument();
    instr->set_single_note_dynamics(false);
    instr->set_transpose(t_r->transpose());

    if (midi_channel == ::mu::iex::guitarpro::GPConverter::perc_channel) {
        String drum_instr_name = t_r->instrument();
        if (!drum_instr_name.empty()) {
            part->set_short_name(drum_instr_name);
        }

        Staff *staff = part->staff(0);
        StaffTypes type = StaffTypes::PERC_DEFAULT;
        if (auto it = drumset::perc_staff_lines_from_instrument.find(t_r->name().to_std_string());
            it != drumset::perc_staff_lines_from_instrument.end()) {
            drumset::init_guitar_pro_percussion_set(it->second);
            drumset::set_instrument_drumset(part->instrument(), it->second);
            switch (it->second.num_lines) {
                case 1: type = StaffTypes::PERC_1LINE; break;
                case 2: type = StaffTypes::PERC_2LINE; break;
                case 3: type = StaffTypes::PERC_3LINE; break;
                default: type = StaffTypes::PERC_DEFAULT; break;
            }
        } else {
            drumset::init_guitar_pro_drumset();
            part->instrument()->set_drumset(drumset::gp_drumset);
        }
        staff->set_staff_type(Fraction(0, 1), *StaffType::preset(type));
    } else {
        auto staff_properties = t_r->staff_properties();

        int capo_fret = staff_properties.capo_fret;

        capo_params_.insert_or_assign(part->id().to_uint64(), capo_fret);
        auto tuning = staff_properties.tuning;
        bool use_preset_table = staff_properties.ignore_flats;

        std::array<uint64_t, 3> flat_presets{0x3f3a36312c27, 0x3c37332e2924, 0x3f3a36312c25};

        uint64_t k = 0;
        for (size_t i = 0; i < tuning.size(); ++i) {
            k |= (uint64_t) tuning[i] << 8 * i;
        }
        bool use_flats =
            use_preset_table ? std::find(flat_presets.begin(), flat_presets.end(), k) != flat_presets.end() : staff_properties.use_flats;
        auto fret_count = staff_properties.fret_count;

        if (tuning.empty()) {
            tuning = utils::standard_tuning_for(25, 6); // 6 string guitar
        }

        int transpose = t_r->transpose();
        for (auto &t : tuning) {
            t -= transpose;
        }

        StringData string_data = StringData(fret_count, static_cast<int>(tuning.size()), tuning.data(), use_flats);
        string_datas.insert_or_assign(part->id().to_uint64(), string_data);
        // We're using Tuning String for non-standard string data
        // Instrument string data should be set to the standard
        tuning = utils::standard_tuning_for(programm, (int) tuning.size());
        for (auto &t : tuning) {
            t -= t_r->transpose();
        }
        string_data = StringData(fret_count, static_cast<int>(tuning.size()), tuning.data());
        instr->set_string_data(string_data);
    }
}

void GPConverter::collect_tempo_map(const GPMasterTracks *m_tr) {
    for (const auto &temp : m_tr->tempo_map()) {
        tempo_map.insert(std::make_pair(temp.bar, temp));
    }
}

void GPConverter::collect_fermatas(const GPMasterBar *m_b, Measure *measure) {
    for (const auto &ferm : m_b->fermatas()) {
        this->fermatas.push_back(std::make_pair(measure, ferm));
    }
}

void GPConverter::fill_uncompleted_measure(const Context &ctx) {
    if (next_tuplet_info_.tuplet) {
        fill_tuplet();
    }

    Measure *last_measure = score_value->last_measure();
    int tick_offset = last_measure->ticks().ticks() + last_measure->tick().ticks() - ctx.cur_tick.ticks();
    if (tick_offset > 0) {
        score_value->set_rest(ctx.cur_tick, ctx.cur_track, Fraction::from_ticks(tick_offset), true, nullptr);
        continious_elements_builder_->notify_uncompleted_measure();
    }
}

void GPConverter::hide_rests_in_empty_measures(track_idx_t start_track, track_idx_t end_track) {
    for (Segment *segment = score_value->last_measure()->first(SegmentType::CHORD_REST); segment;
         segment = segment->next(SegmentType::CHORD_REST)) {
        for (track_idx_t track_idx = start_track; track_idx < end_track; track_idx++) {
            EngravingItem *element = segment->element(track_idx);
            if (!element || !element->is_rest()) {
                continue;
            }

            Rest *rest = to_rest(element);
            size_t voice = track_idx % ::mu::iex::guitarpro::GPConverter::voices;
            bool main_voice = (voice == 0);

            // hiding rests in secondary voices for measures without any chords
            if (!chord_exists_in_bar_) {
                rest->set_visible(main_voice);
                continue;
            }

            // hiding rests in voices without chords
            if (!chord_exists_for_voice_[voice]) {
                rest->set_visible(false);
            }
        }
    }
}

void GPConverter::add_continuous_slide_hammer_on() {
    auto search_end_note = [](Note *start) -> Note * {
        ChordRest *next_cr;
        if (to_chord(start->parent())->ChordRest::is_grace()) {
            //! this case when start note is a grace note so end note can be next note in grace notes
            //! or parent note of grace notes
            Chord *start_chord = to_chord(start->parent());

            Chord *parent_grace = to_chord(start->parent()->parent());

            auto it = parent_grace->grace_notes().begin();
            for (; it != parent_grace->grace_notes().end(); ++it) {
                if (*it == start_chord) {
                    break;
                }
            }

            if (it == parent_grace->grace_notes().end()) {
                next_cr = nullptr;
            } else if (std::next(it) == parent_grace->grace_notes().end()) {
                next_cr = parent_grace;
            } else {
                next_cr = *(++it);
            }
        } else {
            next_cr = start->chord()->segment()->next1()->next_chord_rest(start->track());
            if (!next_cr) {
                return nullptr;
            }

            if (next_cr->is_chord() && !to_chord(next_cr)->grace_notes().empty()) {
                Chord *first_grace = to_chord(next_cr)->grace_notes().front();
                bool is_dive_grace = false;
                if (!first_grace->notes().empty()) {
                    Note *gn = first_grace->notes().front();
                    is_dive_grace = gn->ghost() || gn->dive_for() || gn->dive_back();
                }
                if (!is_dive_grace) {
                    next_cr = first_grace;
                }
            }
        }

        if (!next_cr) {
            return nullptr;
        }

        if (!next_cr->is_chord()) {
            return nullptr;
        }
        auto next_chord = to_chord(next_cr);
        for (auto note : next_chord->notes()) {
            if (note->string() == start->string() && (note->harmonic() == start->harmonic())) {
                return note;
            }
        }

        return next_chord->up_note();
    };

    std::unordered_map<Note *, Slur *> legato_slides;
    std::unordered_map<Note *, HammerOnPullOff *> hammer_on_pull_offs;
    std::unordered_set<Chord *> hammer_on_in_chord;
    for (const auto &slide : slide_hammer_on_map) {
        Note *start_note = slide.first;
        Note *end_note = search_end_note(start_note);
        if (!end_note || start_note->string() == -1 || start_note->fret() == end_note->fret()) {
            //mistake in GP: such kind od slides shouldn't exist
            continue;
        }

        Note *current_start = nullptr;
        if (start_note->bend_for()) {
            Note *bend_note = start_note;
            GuitarBend *bend = bend_note->bend_for();

            while (bend) {
                if (bend->is_dive()) {
                    break;
                }

                bend_note = bend->end_note();
                IF_ASSERT_FAILED(bend_note) {
                    LOGE() << "glissando start note may be incorrect";
                    break;
                }

                if (!bend_note->chord()->is_grace_after()) {
                    break;
                }

                current_start = bend_note;
                bend = bend_note->bend_for();
            }

            if (current_start) {
                start_note = current_start;
            }
        }

        Fraction start_tick = start_note->chord()->tick();
        Fraction end_tick = end_note->chord()->tick();
        track_idx_t track = start_note->track();

        /// Layout info
        if (slide.second == SlideHammerOn::LEGATO_SLIDE || slide.second == SlideHammerOn::SLIDE) {
            Glissando *gl = mu::engraving::Factory::create_glissando(start_note);
            gl->set_start_element(start_note);
            gl->set_track(track);
            gl->set_tick(start_tick);
            gl->set_tick2(end_note->chord()->tick());
            gl->set_end_element(end_note);
            gl->set_text(u"");
            gl->set_glissando_type(GlissandoType::STRAIGHT);
            gl->set_glissando_shift(slide.second == SlideHammerOn::SLIDE);
            gl->set_glissando_style(start_note->part()->instrument(start_tick)->glissando_style());
            score_value->add_element(gl);
        }

        if (slide.second == SlideHammerOn::LEGATO_SLIDE) {
            if (legato_slides.count(start_note) == 0) {
                Slur *slur = mu::engraving::Factory::create_slur(score_value->dummy());
                if (slide.second == SlideHammerOn::LEGATO_SLIDE) {
                    slur->set_connected_element(mu::engraving::Slur::ConnectedElement::GLISSANDO);
                }

                slur->set_start_element(start_note->chord());
                slur->set_track(track);
                slur->set_tick(start_tick);
                slur->set_tick2(end_tick);
                legato_slides[end_note] = slur;
                slur->set_end_element(end_note->chord());
                score_value->add_spanner(slur);
            } else {
                Slur *slur = legato_slides[start_note];
                slur->set_tick2(end_tick);
                slur->set_end_element(end_note->chord());
                legato_slides.erase(start_note);
                legato_slides[end_note] = slur;
            }
        } else if (slide.second == SlideHammerOn::HAMMER_ON) {
            Chord *start_chord = start_note->chord();
            if (hammer_on_in_chord.find(start_chord) != hammer_on_in_chord.end()) {
                continue;
            }

            if (hammer_on_pull_offs.count(start_note) == 0) {
                HammerOnPullOff *hammer_on_pull_off = Factory::create_hammer_on_pull_off(score_value->dummy());
                hammer_on_pull_off->set_track(start_note->track());
                hammer_on_pull_off->set_tick(start_note->tick());
                hammer_on_pull_off->set_tick2(end_note->tick());
                hammer_on_pull_off->set_start_element(start_chord);
                hammer_on_pull_off->set_end_element(end_note->chord());
                score_value->add_element(hammer_on_pull_off);
                hammer_on_pull_offs[end_note] = hammer_on_pull_off;
                hammer_on_in_chord.insert(start_chord);
            } else {
                HammerOnPullOff *hammer_on_pull_off = hammer_on_pull_offs[start_note];
                hammer_on_pull_off->set_tick2(end_tick);
                hammer_on_pull_off->set_end_element(end_note->chord());
                hammer_on_pull_offs.erase(start_note);
                hammer_on_pull_offs[end_note] = hammer_on_pull_off;
            }
        }
    }
}

void GPConverter::add_fermatas() {
    auto fermata_type = [](const GPMasterBar::Fermata &fer) {
        if (fer.type == GPMasterBar::Fermata::Type::LONG) {
            return SymId::FERMATA_LONG_ABOVE;
        } else if (fer.type == GPMasterBar::Fermata::Type::SHORT) {
            return SymId::FERMATA_SHORT_ABOVE;
        }
        return SymId::FERMATA_ABOVE;
    };

    for (const auto &fr : this->fermatas) {
        const auto &measure = fr.first;
        const auto &gp_fermata = fr.second;
        Fraction tick = Fraction::from_ticks(mu::engraving::Constants::division * gp_fermata.offset_num / gp_fermata.offset_denom);
        // bellow how gtp fermata timeStretch converting to MU timeStretch
        float converting_length = 1.5f - gp_fermata.length * 0.5f + gp_fermata.length * gp_fermata.length * 3;
        Segment *seg = measure->get_segment_r(SegmentType::CHORD_REST, tick);

        for (size_t staff_idx = 0; staff_idx < score_value->staves().size(); staff_idx++) {
            Fermata *fermata = mu::engraving::Factory::create_fermata(seg);
            SymId type = fermata_type(fr.second);
            fermata->set_sym_id_and_time_stretch(type);
            fermata->set_time_stretch(converting_length);
            fermata->set_track(staff_idx * ::mu::iex::guitarpro::GPConverter::voices);
            seg->add(fermata);
        }
    }
}

static Segment *find_closest_segment(Measure *m, Fraction tick) {
    Segment *segment = m->find_segment(SegmentType::CHORD_REST, tick);
    if (!segment) {
        segment = m->get_segment(SegmentType::CHORD_REST, tick);
        Segment *prev = segment->prev1(SegmentType::CHORD_REST);
        Segment *next = segment->next1(SegmentType::CHORD_REST);
        if (prev && next) {
            segment = (next->tick() - segment->tick() < segment->tick() - prev->tick() ? next : prev);
        } else if (prev) {
            segment = prev;
        } else if (next) {
            segment = next;
        }
    }

    return segment;
}

void GPConverter::add_tempo_map() {
    auto real_tempo = [](const GPMasterTracks::Automation &temp) {
        //real tempo - beats per second
        //formula ro convert tempo from GTP tempo values
        double tempo = temp.value;

        if (temp.tempo_unit == 0) {
            return tempo;
        }

        if (temp.tempo_unit != 5) {
            return tempo * temp.tempo_unit / 2;
        }

        if (temp.tempo_unit == 5) {
            return tempo + tempo * 2;
        }

        return tempo;
    };

    int measure_idx = 0;
    GradualTempoChange *last_gradual_tempo_change = nullptr;
    int previous_tempo = -1;

    for (auto m = score_value->first_measure(); m; m = m->next_measure()) {
        auto range = tempo_map.equal_range(measure_idx); //one measure can have several tempo changing
        measure_idx++;
        for (auto temp_it = range.first; temp_it != range.second; temp_it++) {
            Fraction tick = m->tick() + Fraction::from_ticks(temp_it->second.position * Constants::division * 4 * m->ticks().numerator() /
                                                             m->ticks().denominator());
            Segment *segment = find_closest_segment(m, tick);
            double real_temp = real_tempo(temp_it->second);
            TempoText *tt = Factory::create_tempo_text(segment);
            tt->set_tempo((double) real_temp / 60);
            String &label_text = temp_it->second.text;
            String tempo_text = String(u"<sym>metNoteQuarterUp</sym> = %1").arg(round(real_temp));

            if (!label_text.is_empty()) {
                tempo_text.prepend(label_text.append(Char(' ')));
            }

            tt->set_xml_text(tempo_text);
            tt->set_track(0);
            segment->add(tt);

            if (last_gradual_tempo_change) {
                last_gradual_tempo_change->set_tick2(tick);
                GradualTempoChangeType tempo_change_type = GradualTempoChangeType::UNDEFINED;
                String tempo_change_text;

                if (real_temp > previous_tempo) {
                    tempo_change_type = GradualTempoChangeType::ACCELERANDO;
                    tempo_change_text = u"accel.";
                } else {
                    tempo_change_type = GradualTempoChangeType::RALLENTANDO;
                    tempo_change_text = u"rall.";
                }

                last_gradual_tempo_change->set_tempo_change_type(tempo_change_type);
                last_gradual_tempo_change->set_begin_text(tempo_change_text);
                last_gradual_tempo_change->set_continue_text(String(u"(%1)").arg(tempo_change_text));
                score_value->add_element(last_gradual_tempo_change);
                last_gradual_tempo_change = nullptr;
            }

            if (temp_it->second.linear) {
                GradualTempoChange *tempo_change = Factory::create_gradual_tempo_change(score_value->dummy());
                tempo_change->set_tick(tick);
                tempo_change->set_track(0);
                last_gradual_tempo_change = tempo_change;
            }

            previous_tempo = real_temp;
        }
    }
}

void GPConverter::add_instrument_changes() {
    for (const auto &track : gp_dom_value->tracks()) {
        for (const auto &sound_automation : track.second->sound_automations()) {
            int bar = sound_automation.second.bar;
            float pos = sound_automation.second.position;

            int track_idx = track.second->idx();

            int midi_programm = 0;
            String instr_name;

            auto it = track.second->sounds().find(sound_automation.second.value);
            if (it == track.second->sounds().end()) {
                midi_programm = track.second->programm();
                instr_name = sound_automation.second.value;
            } else {
                midi_programm = it->second.programm;
                instr_name = it->second.label;
            }

            if (bar == 0 && pos == 0 && midi_programm == track.second->programm()) {
                // skipping the default instrument in the beginning of the track
                continue;
            }

            Measure *m = score_value->cr_measure(bar);
            Segment *seg = m->first(SegmentType::CHORD_REST);
            if (pos != 0) {
                // searching for correct segment to put instrument change
                Fraction tick = m->tick() + Fraction::from_ticks(pos * Constants::division);
                Segment *positioned_segment = m->find_segment(SegmentType::CHORD_REST, tick);
                if (positioned_segment) {
                    seg = positioned_segment;
                }
            }

            Instrument instr;
            instr.set_transpose(track.second->transpose());
            instr.set_string_data(*score_value->parts()[track_idx]->instrument()->string_data());
            instr.channel(0)->set_program(midi_programm);
            if (track.second->midi_channel() == ::mu::iex::guitarpro::GPConverter::perc_channel) {
                instr.set_drumset(drumset::gp_drumset);
            }

            InstrumentChange *instr_ch = Factory::create_instrument_change(seg, instr);
            instr_ch->set_track(track_idx * ::mu::iex::guitarpro::GPConverter::voices);
            instr_ch->set_xml_text(instr_name);

            seg->add(instr_ch);
        }
    }
}

bool GPConverter::add_simile_mark(const GPBar *bar, int cur_track) {
    if (bar->simile_mark() == GPBar::SimileMark::NONE) {
        return false;
    }

    Measure *measure = score_value->last_measure();
    Segment *segment = measure->get_segment(SegmentType::CHORD_REST, measure->tick());
    staff_idx_t staff_idx = track2staff(cur_track);

    switch (bar->simile_mark()) {
        case GPBar::SimileMark::SIMPLE: {
            MeasureRepeat *mr = Factory::create_measure_repeat(segment);
            mr->set_track(cur_track);
            mr->set_ticks(measure->ticks());
            mr->set_num_measures(1);
            segment->add(mr);
            measure->set_measure_repeat_count(1, staff_idx);
            break;
        }
        case GPBar::SimileMark::FIRST_OF_DOUBLE: {
            MeasureRepeat *mr = Factory::create_measure_repeat(segment);
            mr->set_track(staff2track(staff_idx));
            mr->set_ticks(measure->ticks());
            mr->set_num_measures(2);
            segment->add(mr);
            measure->set_measure_repeat_count(1, staff_idx);
            break;
        }
        case GPBar::SimileMark::SECOND_OF_DOUBLE: {
            Rest *r = Factory::create_rest(segment);
            r->set_track(staff2track(staff_idx));
            r->set_ticks(measure->ticks());
            r->set_duration_type(DurationType::V_MEASURE);
            segment->add(r);
            measure->set_measure_repeat_count(2, staff_idx);
            break;
        }
        case GPBar::SimileMark::NONE: break;
    }
    return true;
}

void GPConverter::add_clef(const GPBar *bar, int cur_track) {
    auto convert_clef = [](GPBar::Clef cl) {
        if (cl.type == GPBar::ClefType::NEUTRAL) {
            return ClefType::PERC;
        } else if (cl.type == GPBar::ClefType::G2) {
            switch (cl.ottavia) {
                case GPBar::OttaviaType::VA8: return ClefType::G8_VA;
                case GPBar::OttaviaType::VB8: return ClefType::G8_VB;
                case GPBar::OttaviaType::MA15: return ClefType::G15_MA;
                case GPBar::OttaviaType::MB15: return ClefType::G15_MB;
                default: return ClefType::G;
            }
        } else if (cl.type == GPBar::ClefType::F4) {
            switch (cl.ottavia) {
                case GPBar::OttaviaType::VA8: return ClefType::F_8VA;
                case GPBar::OttaviaType::VB8: return ClefType::F8_VB;
                case GPBar::OttaviaType::MA15: return ClefType::F_15MA;
                case GPBar::OttaviaType::MB15: return ClefType::F15_MB;
                default: return ClefType::F;
            }
        } else if (cl.type == GPBar::ClefType::C3) {
            return ClefType::C3;
        } else if (cl.type == GPBar::ClefType::C4 && cl.ottavia == GPBar::OttaviaType::VB8) {
            return ClefType::C4_8VB;
        } else if (cl.type == GPBar::ClefType::C4) {
            return ClefType::C4;
        }
        return ClefType::G;
    };

    Measure *last_measure = score_value->last_measure();
    Fraction tick = last_measure->tick();
    ClefType clef = convert_clef(bar->clef());

    if (tick == Fraction{0, 1}) {
        clefs[cur_track] = bar->clef();
        Staff *s = score_value->staff(track2staff(cur_track));
        s->set_default_clef_type(ClefTypeList(clef, clef));
        return;
    }

    if (clefs.find(cur_track) != clefs.end() && clefs.at(cur_track) == bar->clef()) {
        return;
    }

    Segment *s = last_measure->get_segment(SegmentType::HEADER_CLEF, tick);
    Clef *cl = mu::engraving::Factory::create_clef(s);
    cl->set_track(cur_track);
    cl->set_clef_type(clef);

    s->add(cl);
    clefs[cur_track] = bar->clef();
}

Measure *GPConverter::add_measure(const GPMasterBar *m_b) {
    Fraction tick = score_value->measures()->last() ? score_value->measures()->last()->end_tick() : Fraction(0, 1);
    Measure *measure = Factory::create_measure(score_value);
    measure->set_tick(tick);
    GPMasterBar::TimeSig sig = m_b->time_sig();
    auto score_time_sig = Fraction(sig.numerator, sig.denominator);
    measure->set_timesig(score_time_sig);
    measure->set_ticks(score_time_sig);
    score_value->measures()->append(measure);

    return measure;
}

ChordRest *GPConverter::add_chord_rest(const GPBeat *beat, const Context &ctx) {
    auto rhythm = [](GPRhythm::RhytmType rhythm) {
        if (rhythm == GPRhythm::RhytmType::WHOLE) {
            return 1;
        } else if (rhythm == GPRhythm::RhytmType::HALF) {
            return 2;
        } else if (rhythm == GPRhythm::RhytmType::QUARTER) {
            return 4;
        } else if (rhythm == GPRhythm::RhytmType::EIGHTH) {
            return 8;
        } else if (rhythm == GPRhythm::RhytmType::SIXTEENTH) {
            return 16;
        } else if (rhythm == GPRhythm::RhytmType::THIRTY_SECOND) {
            return 32;
        } else {
            return 64;
        }
    };

    ChordRest *cr{nullptr};
    if (beat->is_rest()) {
        cr = Factory::create_rest(score_value->dummy());
    } else {
        cr = Factory::create_chord(score_value->dummy());
    }

    cr->set_track(ctx.cur_track);

    Fraction fr(1, rhythm(beat->lenth().second));
    for (int dot = 1; dot <= beat->lenth().first; dot++) {
        fr += Fraction(1, rhythm(beat->lenth().second) * 2 * dot);
    }

    cr->set_ticks(fr);
    cr->set_duration_type(TDuration(fr));

    return cr;
}

void GPConverter::add_fingering(const GPNote *gpnote, Note *note) {
    if (gpnote->left_fingering().is_empty() && gpnote->right_fingering().is_empty()) {
        return;
    }

    auto score_finger = [](const String &str) {
        if (str == u"Open") {
            return String(u'O');
        }
        return str;
    };

    if (!gpnote->left_fingering().is_empty()) {
        Fingering *f = Factory::create_fingering(note);
        f->set_plain_text(score_finger(gpnote->left_fingering()));
        note->add(f);
    }
    if (!gpnote->right_fingering().is_empty()) {
        Fingering *f = Factory::create_fingering(note);
        f->set_plain_text(gpnote->right_fingering());
        note->add(f);
    }
}

void GPConverter::add_string_number(const GPNote *gpnote, Note *note) {
    if (!gpnote->show_string_number()) {
        return;
    }

    Fingering *f = Factory::create_fingering(note);

    f->set_plain_text(String::number(note->string() + 1));
    f->set_text_style_type(TextStyleType::STRING_NUMBER);
    f->set_frame_type(FrameType::CIRCLE);
    note->add(f);
}

void GPConverter::add_trill(const GPNote *gpnote, [[maybe_unused]] Note *note) {
    if (gpnote->trill().auxillary_fret == -1) {
        return;
    }

    current_gp_beat_->set_trill(true);
}

void GPConverter::add_trill(const GPBeat *gpbeat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(cr, ElementType::TRILL, ContiniousElementsBuilder::ImportType::TRILL,
                                                           gpbeat->trill());
}

void GPConverter::add_ornament(const GPNote *gpnote, Note *note) {
    if (gpnote->ornament() == GPNote::Ornament::NONE) {
        return;
    }

    auto score_ornament = [](const auto &orn) {
        if (orn == GPNote::Ornament::TURN) {
            return SymId::ORNAMENT_TURN;
        } else if (orn == GPNote::Ornament::INVERTED_TURN) {
            return SymId::ORNAMENT_TURN_INVERTED;
        } else if (orn == GPNote::Ornament::LOWER_MORDENT) {
            return SymId::ORNAMENT_MORDENT;
        } else if (orn == GPNote::Ornament::UPPER_MORDENT) {
            return SymId::ORNAMENT_SHORT_TRILL;
        }
        return SymId::ORNAMENT_UP_PRALL;
    };

    Articulation *art = mu::engraving::Factory::create_articulation(score_value->dummy());
    art->set_sym_id(score_ornament(gpnote->ornament()));
    if (!EditChord::toggle_articulation(score_value, note, art)) {
        delete art;
    }
}

void GPConverter::add_vibrato_left_hand(const GPNote *gpnote, Note *) {
    if (gpnote->vibrato_type() == GPNote::VibratoType::NONE) {
        return;
    }

    current_gp_beat_->set_vibrato_left_hand(gpnote->vibrato_type() == GPNote::VibratoType::SLIGHT ? GPBeat::VibratoLeftHand::SLIGHT :
                                                                                                    GPBeat::VibratoLeftHand::WIDE);
}

Note *GPConverter::add_harmonic(const GPNote *gpnote, Note *note) {
    if (gpnote->harmonic().type == GPNote::Harmonic::Type::NONE) {
        return nullptr;
    }

    Note *hnote = nullptr;
    if (gpnote->harmonic().type != GPNote::Harmonic::Type::NATURAL) {
        hnote = mu::engraving::Factory::create_note(note->chord());
        hnote->set_track(note->track());
        hnote->set_string(note->string());
        hnote->set_pitch(note->pitch());
        hnote->set_fret(note->fret());

        /// @note option to show or not additional harmonic fret in "<>" to be implemented
        ///note->setDisplayFret(Note::DisplayFretOption::ArtificialHarmonic);
        ///hnote->setDisplayFret(Note::DisplayFretOption::Hide);
        hnote->set_display_fret(Note::DisplayFretOption::HIDE);

        hnote->set_tpc_from_pitch();
        note->chord()->add(hnote);
        hnote->set_play(false);
        note->set_play(true);

        note->set_harmonic_fret(note->fret() + gpnote->harmonic().fret);
    } else {
        note->set_harmonic_fret(gpnote->harmonic().fret);
        note->set_display_fret(Note::DisplayFretOption::NATURAL_HARMONIC);
    }

    Note *harmonic_note = hnote ? hnote : note;

    int gpro_harmonic_type = static_cast<int>(gpnote->harmonic().type);
    int harmonic_fret = utils::harmonic_overtone(note, gpnote->harmonic().fret, gpro_harmonic_type);
    int string = harmonic_note->string();
    int harmonic_pitch = harmonic_note->part()->instrument()->string_data()->get_pitch(
        string, harmonic_fret + capo_fret(harmonic_note->part()), harmonic_note->staff());

    harmonic_note->set_pitch(harmonic_pitch);
    harmonic_note->set_tpc_from_pitch();
    harmonic_note->set_harmonic(true);
    note->set_harmonic_pitch_offset(harmonic_note->pitch() - note->pitch());

    if (GPNote::Harmonic::is_artificial(gpnote->harmonic().type) && current_gp_beat_) {
        current_gp_beat_->add_harmonic_mark_type(harmonic_type_note_to_beat(gpnote->harmonic().type));
    }

    return harmonic_note;
}

void GPConverter::add_accent(const GPNote *gpnote, Note *note) {
    if (gpnote->accents().none()) {
        return;
    }

    auto symbols_ids = note->chord()->articulation_symbol_ids();

    auto accent_type = [](size_t flag_idx) {
        if (flag_idx == 0) {
            return SymId::ARTIC_STACCATO_ABOVE;
        } else if (flag_idx == 2) {
            return SymId::ARTIC_MARCATO_ABOVE;
        } else if (flag_idx == 3) {
            return SymId::ARTIC_ACCENT_ABOVE;
        } else {
            return SymId::DYNAMIC_SFORZANDO;
        }
    };

    for (size_t flag_idx = 0; flag_idx < gpnote->accents().size(); flag_idx++) {
        if (gpnote->accents()[flag_idx] && symbols_ids.find(accent_type(flag_idx)) == symbols_ids.end()) {
            Articulation *art = mu::engraving::Factory::create_articulation(note->chord());
            art->set_sym_id(accent_type(flag_idx));
            note->chord()->add(art);
        }
    }
}

void GPConverter::add_left_hand_tapping(const GPNote *gpnote) {
    if (gpnote->left_hand_tapped() && current_gp_beat_) {
        current_gp_beat_->set_tapping_hand(GPBeat::TappingHand::LEFT);
    }
}

void GPConverter::add_right_hand_tapping(const GPNote *gpnote) {
    if (gpnote->right_hand_tapping() && current_gp_beat_) {
        current_gp_beat_->set_tapping_hand(GPBeat::TappingHand::RIGHT);
    }
}

void GPConverter::add_slide(const GPNote *gpnote, Note *note) {
    if (gpnote->slides().none()) {
        return;
    }

    add_single_slide(gpnote, note);
    collect_continuous_slide(gpnote, note);
}

void GPConverter::add_single_slide(const GPNote *gpnote, Note *note) {
    auto slide_type = [](size_t flag_idx) -> ChordLineType {
        switch (flag_idx) {
            case 2: return ChordLineType::FALL;
            case 3: return ChordLineType::DOIT;
            case 4: return ChordLineType::SCOOP;
            case 5: return ChordLineType::PLOP;
            default: LOGE() << "wrong slide type"; return ChordLineType::NOTYPE;
        }
    };

    for (size_t flag_idx = 2; flag_idx < gpnote->slides().size(); flag_idx++) {
        if (gpnote->slides()[flag_idx]) {
            ChordLine *cl = Factory::create_chord_line(note->chord());
            cl->set_chord_line_type(slide_type(flag_idx));
            cl->set_straight(true);
            note->chord()->add(cl);
            cl->set_note(note);
        }
    }
}

void GPConverter::collect_continuous_slide(const GPNote *gpnote, Note *note) {
    if (gpnote->slides()[0]) {
        slide_hammer_on_map.push_back(std::pair(note, SlideHammerOn::SLIDE));
    }
    if (gpnote->slides()[1]) {
        slide_hammer_on_map.push_back(std::pair(note, SlideHammerOn::LEGATO_SLIDE));
    }
}

void GPConverter::add_pick_scrape(const GPNote *gpnote, Note *note) {
    if (gpnote->pick_scrape() != GPNote::PickScrape::NONE && current_gp_beat_) {
        if (engraving_configuration()->guitar_pro_import_experimental()) {
            ChordLine *cl = mu::engraving::Factory::create_chord_line(note->chord());
            cl->set_chord_line_type(gpnote->pick_scrape() == GPNote::PickScrape::DOWN ? ChordLineType::FALL : ChordLineType::DOIT);
            cl->set_wavy(true);
            note->chord()->add(cl);
            cl->set_note(note);
        }

        current_gp_beat_->set_pick_scrape(true);
    }
}

void GPConverter::collect_hammer_on(const GPNote *gpnote, Note *note) {
    if (gpnote->hammer_on() == GPNote::HammerOn::START) {
        slide_hammer_on_map.push_back(std::pair(note, SlideHammerOn::HAMMER_ON));
    }
}

static mu::engraving::PitchValues gp_bend_curve_to_pitch_values(const GPNote::Bend &b) {
    using namespace mu::engraving;

    auto gp_time_to_mu_time = [](float time) { return time * PitchValue::max_time / 100; };

    //! NOTE: In GPX format, -1 means "not set". Normalize to sensible defaults.
    const float origin_offset = std::max(0.f, b.origin_offset);
    const float dest_offset = std::max(0.f, b.destination_offset);
    const bool has_middle_value = (b.middle_value != -1) && !(b.middle_offset1 == 12 && b.middle_offset2 == 12);

    PitchValues pitch_values;

    pitch_values.push_back(PitchValue(gp_time_to_mu_time(origin_offset), b.origin_value));
    PitchValue last_point = pitch_values.back();

    if (has_middle_value) {
        if (PitchValue value(gp_time_to_mu_time(b.middle_offset1), b.middle_value);
            b.middle_offset1 >= 0 && b.middle_offset1 < dest_offset && value != last_point) {
            pitch_values.push_back(std::move(value));
        }

        if (PitchValue value(gp_time_to_mu_time(b.middle_offset2), b.middle_value);
            b.middle_offset2 >= 0 && b.middle_offset2 != b.middle_offset1 && b.middle_offset2 < dest_offset && value != last_point) {
            pitch_values.push_back(std::move(value));
        }

        if (b.middle_offset1 == -1 && b.middle_offset2 == -1 && b.middle_value != -1) {
            //!@NOTE It seems when middle point is places exactly in the middle
            //!of bend  GP6 stores this value equal -1
            if (dest_offset > 50) {
                pitch_values.push_back(PitchValue(gp_time_to_mu_time(50), b.middle_value));
            }
        }
    }

    if (b.destination_offset <= 0) {
        if (has_middle_value) {
            PitchValue fix_gpx_value = PitchValue(gp_time_to_mu_time(50), b.middle_value);
            if (b.middle_value > b.destination_value && pitch_values.back() != fix_gpx_value) {
                pitch_values.push_back(fix_gpx_value);
            }
        }
        pitch_values.push_back(PitchValue(gp_time_to_mu_time(100), b.destination_value)); //! In .gpx this value might be exist
    } else {
        if (PitchValue value(gp_time_to_mu_time(dest_offset), b.destination_value); value != pitch_values.back()) {
            pitch_values.push_back(std::move(value));
        }
    }

    return pitch_values;
}

void GPConverter::add_bend(const GPNote *gpnote, Note *note) {
    if (!gpnote->bend() || gpnote->bend()->is_empty()) {
        return;
    }

    using namespace mu::engraving;

    PitchValues pitch_values = gp_bend_curve_to_pitch_values(*gpnote->bend());

    if (pitch_values.size() < 2) {
        return;
    }

    /// not adding "hold" on 0 pitch
    int max_pitch = (std::max_element(pitch_values.begin(), pitch_values.end(), [](const PitchValue &l, const PitchValue &r) {
                        return l.pitch < r.pitch;
                    }))->pitch;

    if (max_pitch == 0) {
        return;
    }

    guitar_bend_importer_->collect_bend(note, pitch_values);
}

void GPConverter::set_pitch(Note *note, const GPNote::MidiPitch &midi_pitch) {
    int32_t fret = midi_pitch.fret;
    int32_t musescore_string{-1};
    if (midi_pitch.string != -1) {
        musescore_string = static_cast<int32_t>(note->part()->instrument()->string_data()->strings()) - 1 - midi_pitch.string;
    }

    int pitch = 0;
    if (midi_pitch.midi != -1) {
        pitch = midi_pitch.midi;
    } else if (midi_pitch.octave != -1 || midi_pitch.tone != 0) {
        pitch = midi_pitch.octave * 12 + midi_pitch.tone;
    } else if (midi_pitch.variation != -1) {
        pitch = calculate_drum_pitch(midi_pitch.element, midi_pitch.variation, note->part()->short_name());
    } else if (note->part()->midi_channel() == ::mu::iex::guitarpro::GPConverter::perc_channel) {
        //!@NOTE This is a case, when part of the note is a
        //       single drum instrument. It seems, that GP
        //       sets to -1 all midi parameters for the note,
        //       but sets the midi pitch value to the program
        //       instead.
        pitch = note->part()->instrument()->channel(0)->program();
    } else {
        if (string_datas.empty()) {
            pitch =
                note->part()->instrument()->string_data()->get_pitch(musescore_string, midi_pitch.fret + capo_fret(note->part()), nullptr) +
                note->part()->instrument()->transpose().chromatic;
        } else {
            if (const auto sd = string_datas.find(note->part()->id().to_uint64()); sd != string_datas.end()) {
                pitch = sd->second.get_pitch(musescore_string, midi_pitch.fret + capo_fret(note->part()), nullptr) +
                        note->part()->instrument()->transpose().chromatic;
            } else {
                pitch = note->part()->instrument()->string_data()->get_pitch(musescore_string, midi_pitch.fret + capo_fret(note->part()),
                                                                             nullptr) +
                        note->part()->instrument()->transpose().chromatic;
            }
        }
    }

    pitch = std::clamp(pitch, 0, 127);

    if (musescore_string == -1) {
        musescore_string = get_string_number_for(note, pitch);

        fret = note->part()->instrument()->string_data()->fret(pitch, musescore_string, nullptr);
    }

    bool has_drum_staff = note->part()->has_drum_staff();
    if (!engraving_configuration()->guitar_pro_import_experimental() && has_drum_staff) {
        auto it = drum_extension.find(pitch);
        if (it != drum_extension.end()) {
            pitch = it->second;
        }
    }

    note->set_fret(fret);

    if (note->part()->instrument(note->tick())->use_drumset()) {
        note->set_string(0);
    } else {
        note->set_string(musescore_string);
    }

    note->set_pitch(pitch);
}

void GPConverter::set_tpc(Note *note, int accidental) {
    if (0 == accidental || accidental == GPNote::invalid_accidental) {
        note->set_tpc_from_pitch();
    } else {
        note->set_tpc_from_pitch(accidental < 0 ? Prefer::FLATS : Prefer::SHARPS);
    }
}

int GPConverter::calculate_drum_pitch(int element, int variation, const String &instrument_name) {
    return drum_resolver->pitch(element, variation, instrument_name);
}

void GPConverter::add_dynamic(const GPBeat *gpb, ChordRest *cr) {
    if (!cr->is_chord()) {
        return;
    }

    if (dynamics.count(cr->track())) {
        if (dynamics.at(cr->track()) == gpb->dynamic()) {
            return;
        }
    }

    auto convert_dynamic = [](GPBeat::DynamicType t) {
        if (t == GPBeat::DynamicType::FFF) {
            return u"fff";
        } else if (t == GPBeat::DynamicType::FF) {
            return u"ff";
        } else if (t == GPBeat::DynamicType::FF) {
            return u"ff";
        } else if (t == GPBeat::DynamicType::F) {
            return u"f";
        } else if (t == GPBeat::DynamicType::MF) {
            return u"mf";
        } else if (t == GPBeat::DynamicType::MP) {
            return u"mp";
        } else if (t == GPBeat::DynamicType::P) {
            return u"p";
        } else if (t == GPBeat::DynamicType::PP) {
            return u"pp";
        }
        return u"ppp";
    };

    Dynamic *dynamic = Factory::create_dynamic(cr->segment());
    dynamic->set_track(cr->track());
    dynamic->set_dynamic_type(convert_dynamic(gpb->dynamic()));
    cr->segment()->add(dynamic);
    dynamics[cr->track()] = gpb->dynamic();
}

void GPConverter::add_tie(const GPNote *gpnote, Note *note, TieMap &ties) {
    if (gpnote->tie_type() == GPNote::TieType::NONE) {
        return;
    }

    auto start_tie = [](Note *start_note, TieMap &ties, track_idx_t cur_track) {
        Tie *tie = Factory::create_tie(start_note);
        start_note->add(tie);
        ties[cur_track].push_back(tie);
    };

    auto end_tie = [this](Note *end_note, TieMap &ties, track_idx_t cur_track) {
        auto &ties_on_track = ties[cur_track];
        for (auto it = ties_on_track.rbegin(); it != ties_on_track.rend(); it++) {
            Tie *tie = *it;
            Note *start_note = tie->start_note();
            int start_pitch = start_note->pitch();
            if (original_pitches_.find(start_note) != original_pitches_.end()) {
                start_pitch = original_pitches_[start_note];
                original_pitches_.erase(start_note);
            }

            if (start_pitch == end_note->pitch() && start_note->string() == end_note->string()) {
                tie->set_end_note(end_note);
                end_note->set_tie_back(tie);
                muse::remove(ties_on_track, tie);

                /// adding tremolos to tied note
                Chord *start_chord = to_chord(start_note->parent());
                Chord *end_chord = to_chord(end_note->parent());
                if (tremolos_in_chords_.find(start_chord) != tremolos_in_chords_.end()) {
                    TremoloType type = tremolos_in_chords_.at(start_chord);
                    DO_ASSERT(!is_tremolo_two_chord(type));
                    TremoloSingleChord *t = Factory::create_tremolo_single_chord(end_chord);
                    t->set_tremolo_type(type);
                    end_chord->add(t);
                    muse::remove(tremolos_in_chords_, start_chord);
                    tremolos_in_chords_[end_chord] = type;
                }

                break;
            }
        }
    };

    if (gpnote->tie_type() == GPNote::TieType::START) {
        start_tie(note, ties, note->track());
    } else if (gpnote->tie_type() == GPNote::TieType::MEDIATE) {
        end_tie(note, ties, note->track());
        start_tie(note, ties, note->track());
    } else if (gpnote->tie_type() == GPNote::TieType::END) {
        end_tie(note, ties, note->track());
    }
}

void GPConverter::add_legato(const GPBeat *beat, ChordRest *cr) {
    if (beat->legato_type() == GPBeat::LegatoType::NONE) {
        return;
    }

    using slurMap = std::unordered_map<track_idx_t, Slur *>;

    auto start_slur = [](ChordRest *cr, Score *sc, slurMap &slurs, track_idx_t cur_track, Fraction cur_tick) {
        if (auto it = slurs.find(cur_track); it != std::end(slurs)) {
            slurs.erase(it);
        }

        Slur *slur = Factory::create_slur(sc->dummy());
        slur->set_start_element(cr);
        slur->set_track(cur_track);
        slur->set_tick(cur_tick);
        slurs[cur_track] = slur;
        sc->add_spanner(slur);
    };

    auto mediate_slur = [](ChordRest *cr, slurMap &slurs, track_idx_t cur_track, Fraction cur_tick) {
        Slur *slur = slurs[cur_track];
        if (!slur) {
            return;
        }
        slur->set_track(cur_track);
        slur->set_track2(cur_track);
        slur->set_tick2(cur_tick);
        slur->set_end_element(cr);
    };

    auto end_slur = [](ChordRest *cr, slurMap &slurs, track_idx_t cur_track, Fraction cur_tick) {
        Slur *slur = slurs[cur_track];
        if (!slur) {
            return;
        }
        slur->set_track2(cur_track);
        slur->set_tick2(cur_tick);
        slur->set_end_element(cr);
    };

    if (beat->legato_type() == GPBeat::LegatoType::START) {
        start_slur(cr, score_value, this->slurs, cr->track(), cr->tick());
    } else if (beat->legato_type() == GPBeat::LegatoType::MEDIATE) {
        mediate_slur(cr, this->slurs, cr->track(), cr->tick());
    } else if (beat->legato_type() == GPBeat::LegatoType::END) {
        end_slur(cr, this->slurs, cr->track(), cr->tick());
    }
}

void GPConverter::add_ottava(const GPBeat *gpb, ChordRest *cr) {
    GPBeat::OttavaType gp_type = gpb->ottava_type();
    continious_elements_builder_->build_continious_element(cr, ElementType::OTTAVA, ottava_to_import_type(gp_type),
                                                           gp_type != GPBeat::OttavaType::NONE);

    if (!cr->is_chord()) {
        return;
    }

    Chord *chord = to_chord(cr);
    const auto &[found_ottava, mu_ottava_type] = ottava_type(gp_type);
    if (!found_ottava) {
        return;
    }

    for (mu::engraving::Note *note : chord->notes()) {
        int pitch = note->pitch();
        set_pitch_by_ottava_type(note, mu_ottava_type);

        // pitch changed because of octave
        if (pitch != note->pitch()) {
            original_pitches_[note] = pitch;
        }
    }
}

void GPConverter::add_let_ring(const GPNote *gpnote, [[maybe_unused]] Note *note) {
    if (gpnote->let_ring() && current_gp_beat_) {
        current_gp_beat_->set_let_ring(true);
    }
}

void GPConverter::add_palm_mute(const GPNote *gpnote, Note * /*note*/) {
    if (gpnote->palm_mute() && current_gp_beat_) {
        current_gp_beat_->set_palm_mute(true);
    }
}

void GPConverter::add_let_ring(const GPBeat *gpbeat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(cr, ElementType::LET_RING, ContiniousElementsBuilder::ImportType::LET_RING,
                                                           gpbeat->let_ring());
}

void GPConverter::add_palm_mute(const GPBeat *gpbeat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(cr, ElementType::PALM_MUTE, ContiniousElementsBuilder::ImportType::PALM_MUTE,
                                                           gpbeat->palm_mute());
}

void GPConverter::add_dive(const GPBeat *beat, ChordRest *cr) {
    if (!beat->has_whammy() || !cr->is_chord()) {
        return;
    }

    //! GPX reuses the same GPBeat pointer for multiple beat positions — treat as hold.
    const track_idx_t track = cr->track();
    PitchValues pitch_values;
    if (last_whammy_beat_[track] == beat && beat->whammy().destination_value != 0) {
        const float hold_value = beat->whammy().destination_value;
        pitch_values.push_back(PitchValue(0, hold_value));
        pitch_values.push_back(PitchValue(PitchValue::max_time, hold_value));
    } else {
        pitch_values = gp_bend_curve_to_pitch_values(beat->whammy());
    }
    last_whammy_beat_[track] = beat;

    if (pitch_values.size() < 2) {
        return;
    }

    guitar_bend_importer_->collect_dive(to_chord(cr), pitch_values);
}

void GPConverter::add_pick_scrape(const GPBeat *beat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(cr, ElementType::PICK_SCRAPE, ContiniousElementsBuilder::ImportType::PICK_SCRAPE,
                                                           beat->pick_scrape());
}

void GPConverter::add_rasgueado(const GPBeat *beat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(cr, ElementType::RASGUEADO, ContiniousElementsBuilder::ImportType::RASGUEADO,
                                                           beat->rasgueado() != GPBeat::Rasgueado::NONE);
}

void GPConverter::add_harmonic_mark(const GPBeat *gpbeat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::HARMONIC_MARK, ContiniousElementsBuilder::ImportType::HARMONIC_ARTIFICIAL, gpbeat->harmonic_mark_artificial(),
        ContiniousElementsBuilder::HarmonicMarkSubType::ARTIFICIAL);

    continious_elements_builder_->build_continious_element(
        cr, ElementType::HARMONIC_MARK, ContiniousElementsBuilder::ImportType::HARMONIC_PINCH, gpbeat->harmonic_mark_pinch(),
        ContiniousElementsBuilder::HarmonicMarkSubType::PINCH);

    continious_elements_builder_->build_continious_element(cr, ElementType::HARMONIC_MARK,
                                                           ContiniousElementsBuilder::ImportType::HARMONIC_TAP, gpbeat->harmonic_mark_tap(),
                                                           ContiniousElementsBuilder::HarmonicMarkSubType::TAP);

    continious_elements_builder_->build_continious_element(
        cr, ElementType::HARMONIC_MARK, ContiniousElementsBuilder::ImportType::HARMONIC_SEMI, gpbeat->harmonic_mark_semi(),
        ContiniousElementsBuilder::HarmonicMarkSubType::SEMI);

    continious_elements_builder_->build_continious_element(
        cr, ElementType::HARMONIC_MARK, ContiniousElementsBuilder::ImportType::HARMONIC_FEEDBACK, gpbeat->harmonic_mark_feedback(),
        ContiniousElementsBuilder::HarmonicMarkSubType::FEEDBACK);
}

void GPConverter::add_fret_diagram(const GPBeat *gpnote, ChordRest *cr, const Context &ctx, bool as_harmony) {
    int gp_track_idx = static_cast<int>(muse::index_of(score_value->parts(), cr->part()));
    int dia_id = gpnote->diagram_idx(gp_track_idx, ctx.master_bar_index);

    if (last_diagram_idx == dia_id) {
        return;
    }

    last_diagram_idx = dia_id;

    if (dia_id == -1) {
        return;
    }

    auto track_it = gp_dom_value->tracks().find(gp_track_idx);
    if (track_it == gp_dom_value->tracks().cend()) {
        return;
    }

    if (track_it->second->diagram().count(dia_id) == 0) {
        return;
    }

    GPTrack::Diagram diagram = track_it->second->diagram().at(dia_id);

    /// currently importing fret diagrams as chord names
    if (as_harmony) {
        Harmony *h = Factory::create_harmony(cr->segment());
        h->set_track(cr->track());
        h->set_harmony_type(HarmonyType::STANDARD);
        h->set_harmony(diagram.name); // F#dim7
        h->set_plain_text(h->harmony_name());
        cr->segment()->add(h);
        return;
    }

    FretDiagram *fret_diagram = mu::engraving::Factory::create_fret_diagram(cr->segment());
    fret_diagram->set_track(cr->track());
    fret_diagram->set_strings(diagram.string_count);
    fret_diagram->set_fret_offset(diagram.base_fret);

    for (int st = 0; st < diagram.string_count; st++) {
        fret_diagram->set_marker(st, FretMarkerType::CROSS);
    }

    for (const auto &fret : diagram.frets) {
        if (fret.second == 0) {
            fret_diagram->set_marker(fret.first, FretMarkerType::CIRCLE);
        } else {
            fret_diagram->set_dot(fret.first, fret.second);
        }
    }

    cr->segment()->add(fret_diagram);
}

void GPConverter::add_tapping(const GPBeat *beat, ChordRest *cr) {
    if (beat->tapping_hand() == GPBeat::TappingHand::NONE || !cr->is_chord()) {
        return;
    }

    Chord *chord = to_chord(cr);
    Tapping *tapping = Factory::create_tapping(chord);
    tapping->set_track(chord->track());
    tapping->set_hand(beat->tapping_hand() == GPBeat::TappingHand::LEFT ? engraving::TappingHand::LEFT : engraving::TappingHand::RIGHT);
    chord->add(tapping);
}

void GPConverter::add_slapped(const GPBeat *beat, ChordRest *cr) {
    if (!beat->slapped() || !cr->is_chord()) {
        return;
    }

    Articulation *art = mu::engraving::Factory::create_articulation(cr);
    art->set_text_type(ArticulationTextType::SLAP);
    cr->add(art);
}

void GPConverter::add_popped(const GPBeat *beat, ChordRest *cr) {
    if (!beat->popped() || !cr->is_chord()) {
        return;
    }

    Articulation *art = mu::engraving::Factory::create_articulation(cr);
    art->set_text_type(ArticulationTextType::POP);
    cr->add(art);
}

void GPConverter::add_brush(const GPBeat *beat, ChordRest *cr) {
    if (beat->brush() == GPBeat::Brush::NONE) {
        return;
    }

    auto brush_type = [](auto &&gp_arp) {
        if (gp_arp == GPBeat::Brush::UP) {
            return ArpeggioType::DOWN_STRAIGHT;
        } else {
            return ArpeggioType::UP_STRAIGHT;
        }
    };

    Arpeggio *arp = mu::engraving::Factory::create_arpeggio(score_value->dummy());
    arp->set_arpeggio_type(brush_type(beat->brush()));
    arp->set_stretch(beat->arpeggio_stretch());

    cr->add(arp);
}

void GPConverter::add_arpeggio(const GPBeat *beat, ChordRest *cr) {
    if (beat->arpeggio() == GPBeat::Arpeggio::NONE) {
        return;
    }

    auto arpeggio_type = [](auto &&gp_arp) {
        if (gp_arp == GPBeat::Arpeggio::UP) {
            return ArpeggioType::DOWN;
        } else {
            return ArpeggioType::UP;
        }
    };

    Arpeggio *arp = mu::engraving::Factory::create_arpeggio(score_value->dummy());
    arp->set_arpeggio_type(arpeggio_type(beat->arpeggio()));
    arp->set_stretch(beat->arpeggio_stretch());
    cr->add(arp);
}

void GPConverter::add_timer(const GPBeat *beat, ChordRest *cr) {
    if (beat->time() == -1) {
        return;
    }

    int time = beat->time() / 1000;
    int minutes = time / 60;
    int seconds = time % 60;
    StaffText *st = Factory::create_staff_text(cr->segment());
    st->set_plain_text(String::number(minutes) + u':' + (seconds < 10 ? u'0' + String::number(seconds) : String::number(seconds)));
    st->set_track(cr->track());
    cr->segment()->add(st);
}

void GPConverter::add_free_text(const GPBeat *beat, ChordRest *cr) {
    if (beat->free_text().is_empty()) {
        return;
    }

    auto text = Factory::create_staff_text(cr->segment());
    text->set_track(cr->track());
    text->set_plain_text(beat->free_text());
    cr->segment()->add(text);
}

void GPConverter::setup_tuplet_style(Tuplet *tuplet) {
    bool real = false;

    switch (tuplet->ratio().numerator()) {
        case 2: {
            real = (tuplet->ratio().denominator() == 3);
            break;
        }
        case 3:
        case 4: {
            real = ((tuplet->ratio().denominator() == 2) || (tuplet->ratio().denominator() == 6));
            break;
        }
        case 5:
        case 6:
        case 7: {
            real = (tuplet->ratio().denominator() == 4);
            break;
        }
        case 9:
        case 10:
        case 11:
        case 12:
        case 13: {
            real = (tuplet->ratio().denominator() == 8);
            break;
        }
    }
    if (!real) {
        tuplet->set_number_type(TupletNumberType::SHOW_RELATION);
        tuplet->set_property_flags(Pid::NUMBER_TYPE, PropertyFlags::UNSTYLED);
    }
}

bool GPConverter::tuplet_params_changed(const GPBeat *beat, const ChordRest *cr) {
    return beat->tuplet().num == -1 || Fraction(beat->tuplet().num, beat->tuplet().denom) != next_tuplet_info_.ratio ||
           cr->track() != next_tuplet_info_.track || cr->measure() != next_tuplet_info_.measure ||
           (next_tuplet_info_.elements.size() > 1 &&
            next_tuplet_info_.duration == Fraction(next_tuplet_info_.ratio.denominator(), next_tuplet_info_.lowest_base));
}

void GPConverter::add_tuplet(const GPBeat *beat, ChordRest *cr) {
    Fraction current_tuplet_type = Fraction(beat->tuplet().num, beat->tuplet().denom);

    if (next_tuplet_info_.tuplet && tuplet_params_changed(beat, cr)) {
        fill_tuplet();
    }

    if (beat->tuplet().num == -1) {
        return;
    }

    next_tuplet_info_.lowest_base = std::min(next_tuplet_info_.lowest_base, cr->ticks().denominator());

    if (!next_tuplet_info_.tuplet) {
        next_tuplet_info_.tuplet = Factory::create_tuplet(score_value->dummy());
        next_tuplet_info_.tuplet->set_ratio(current_tuplet_type);
        next_tuplet_info_.ratio = current_tuplet_type;
        next_tuplet_info_.measure = cr->measure();
        next_tuplet_info_.track = cr->track();
    }

    next_tuplet_info_.elements.push_back(cr);
    cr->set_tuplet(next_tuplet_info_.tuplet);
    next_tuplet_info_.duration += cr->actual_ticks();
}

void GPConverter::add_vibrato_w_trem_bar(const GPBeat *beat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::VIBRATO, ContiniousElementsBuilder::ImportType::VIBRATO_W_TREM_BAR_SLIGHT,
        beat->vibrato_w_trem_bar() == GPBeat::VibratoWTremBar::SLIGHT, ContiniousElementsBuilder::VibratoSubType::W_TREM_BAR_SLIGHT);

    continious_elements_builder_->build_continious_element(
        cr, ElementType::VIBRATO, ContiniousElementsBuilder::ImportType::VIBRATO_W_TREM_BAR_WIDE,
        beat->vibrato_w_trem_bar() == GPBeat::VibratoWTremBar::WIDE, ContiniousElementsBuilder::VibratoSubType::W_TREM_BAR_WIDE);
}

void GPConverter::add_vibrato_left_hand(const GPBeat *beat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(
        cr, ElementType::VIBRATO, ContiniousElementsBuilder::ImportType::VIBRATO_LEFT_HAND_SLIGHT,
        beat->vibrato_left_hand() == GPBeat::VibratoLeftHand::SLIGHT, ContiniousElementsBuilder::VibratoSubType::LEFT_HAND_SLIGHT);

    continious_elements_builder_->build_continious_element(
        cr, ElementType::VIBRATO, ContiniousElementsBuilder::ImportType::VIBRATO_LEFT_HAND_WIDE,
        beat->vibrato_left_hand() == GPBeat::VibratoLeftHand::WIDE, ContiniousElementsBuilder::VibratoSubType::LEFT_HAND_WIDE);
}

void GPConverter::add_fadding(const GPBeat *beat, ChordRest *cr) {
    if (beat->fadding() == GPBeat::Fadding::NONE) {
        return;
    }
    if (!cr->is_chord()) {
        return;
    }

    auto score_fadding = [](const auto &fad) {
        if (fad == GPBeat::Fadding::FADE_IN) {
            return SymId::GUITAR_FADE_IN;
        } else if (fad == GPBeat::Fadding::FADE_OUT) {
            return SymId::GUITAR_FADE_OUT;
        } else {
            return SymId::GUITAR_VOLUME_SWELL;
        }
    };

    Articulation *art = mu::engraving::Factory::create_articulation(score_value->dummy());
    art->set_sym_id(score_fadding(beat->fadding()));
    if (!EditChord::toggle_articulation(score_value, to_chord(cr)->up_note(), art)) {
        delete art;
    }
}

void GPConverter::add_hair_pin(const GPBeat *beat, ChordRest *cr) {
    continious_elements_builder_->build_continious_element(cr, ElementType::HAIRPIN, hairpin_to_import_type(beat->hairpin()),
                                                           beat->hairpin() != GPBeat::Hairpin::NONE);
}

void GPConverter::add_pick_stroke(const GPBeat *beat, ChordRest *cr) {
    if (beat->pick_stroke() == GPBeat::PickStroke::NONE) {
        return;
    }
    if (!cr->is_chord()) {
        return;
    }

    auto score_pick_stroke = [](const auto &p) {
        if (p == GPBeat::PickStroke::UP) {
            return SymId::STRINGS_UP_BOW;
        } else {
            return SymId::STRINGS_DOWN_BOW;
        }
    };

    Articulation *art = mu::engraving::Factory::create_articulation(score_value->dummy());
    art->set_sym_id(score_pick_stroke(beat->pick_stroke()));
    if (!EditChord::toggle_articulation(score_value, to_chord(cr)->up_note(), art)) {
        delete art;
    }
}

void GPConverter::add_tremolo(const GPBeat *beat, ChordRest *cr) {
    if (!cr->is_chord() || beat->tremolo().numerator == -1) {
        return;
    }

    auto score_tremolo = [](const GPBeat::Tremolo tr) {
        if (tr.denominator == 2) {
            return TremoloType::R8;
        }
        if (tr.denominator == 4) {
            return TremoloType::R16;
        } else {
            return TremoloType::R32;
        }
    };

    TremoloSingleChord *t = Factory::create_tremolo_single_chord(score_value->dummy());
    t->set_tremolo_type(score_tremolo(beat->tremolo()));
    Chord *ch = to_chord(cr);
    ch->add(t);
    tremolos_in_chords_[ch] = t->tremolo_type();
}

void GPConverter::add_wah(const GPBeat *beat, ChordRest *cr) {
    if (beat->wah() == GPBeat::Wah::NONE) {
        return;
    }
    if (!cr->is_chord()) {
        return;
    }

    auto score_wah = [](const auto &wah) {
        if (wah == GPBeat::Wah::OPEN) {
            return SymId::BRASS_MUTE_OPEN;
        }
        return SymId::BRASS_MUTE_CLOSED;
    };

    Articulation *art = Factory::create_articulation(score_value->dummy());
    art->set_sym_id(score_wah(beat->wah()));
    if (!EditChord::toggle_articulation(score_value, to_chord(cr)->up_note(), art)) {
        delete art;
    }
}

void GPConverter::add_golpe(const GPBeat *beat, ChordRest *cr) {
    if (beat->golpe() == GPBeat::Golpe::NONE) {
        return;
    }
    if (!cr->is_chord()) {
        return;
    }

    Articulation *art = Factory::create_articulation(score_value->dummy());
    art->set_sym_id(SymId::GUITAR_GOLPE);

    if (beat->golpe() == GPBeat::Golpe::THUMB) {
        art->set_anchor(ArticulationAnchor::BOTTOM);
    }

    if (!EditChord::toggle_articulation(score_value, to_chord(cr)->up_note(), art)) {
        delete art;
    }
}

void GPConverter::add_barre(const GPBeat *beat, ChordRest *cr) {
    if (beat->barre().fret == -1) {
        return;
    }
    if (!cr->is_chord()) {
        return;
    }

    auto barre_type = [](const GPBeat::Barre &barre) {
        if (barre.string == 1) {
            return std::string("1/2B ");
        } else {
            return std::string("B ");
        }
    };

    std::string barre_fret;
    int fret = beat->barre().fret;

    for (int i = 0; i < (fret / 10); i++) {
        barre_fret += "X";
    }
    int target_mod10 = fret % 10;
    if (target_mod10 == 9) {
        barre_fret += "IX";
    } else if (target_mod10 == 4) {
        barre_fret += "IV";
    } else {
        if (target_mod10 >= 5) {
            barre_fret += "V";
            target_mod10 -= 5;
        }
        for (int j = 0; j < target_mod10; j++) {
            barre_fret += "I";
        }
    }

    add_text_to_note(String::from_std_string(barre_type(beat->barre()) + barre_fret), to_chord(cr)->up_note());
}

void GPConverter::add_lyrics(const GPBeat *beat, ChordRest *cr, const Context &ctx) {
    ID gp_track_idx = cr->part()->id();

    const std::string &lyr_str = beat->lyrics(gp_track_idx.to_uint64(), ctx.master_bar_index);
    if (lyr_str.empty()) {
        return;
    }

    Lyrics *lyr = Factory::create_lyrics(score_value->dummy());

    if (lyr_str.back() == '-') {
        lyr->set_syllabic(LyricsSyllabic::MIDDLE);
        lyr->set_plain_text(String::from_utf8(lyr_str.substr(0, lyr_str.size() - 1).c_str()));
    } else {
        lyr->set_plain_text(String::from_utf8(lyr_str.c_str()));
    }
    cr->add(lyr);
}

void GPConverter::clear_defected_grace_chord(ChordRestContainer &grace_ghords) {
    for ([[maybe_unused]] auto [p_cr, gp_beat] : grace_ghords) {

        if (!p_cr) {
            continue;
        }

        if (p_cr->is_chord()) {
            Chord *p_chord = to_chord(p_cr);
            for (Note *p_note : p_chord->notes()) {
                auto it = slide_hammer_on_map.begin(), e = slide_hammer_on_map.end();
                for (; it != e; ++it) {
                    if (it->first == p_note) {
                        slide_hammer_on_map.erase(it);
                        break;
                    }
                }
            }

            for (Chord *p_grace_chord : p_chord->grace_notes()) {
                for (Note *p_note : p_grace_chord->notes()) {
                    auto it = slide_hammer_on_map.begin(), e = slide_hammer_on_map.end();
                    for (; it != e; ++it) {
                        if (it->first == p_note) {
                            slide_hammer_on_map.erase(it);
                            break;
                        }
                    }
                }
            }
        }
        delete p_cr;
    }
    grace_ghords.clear();
}

void GPConverter::add_text_to_note(muse::String string, Note *note) {
    Segment *segment = note->chord()->segment();
    StaffText *text = Factory::create_staff_text(segment);

    if (!string.is_empty()) {
        bool use_harmony = string[string.size() - 1] == '\\';
        if (use_harmony) {
            string.chop(1);
        }
    }
    text->set_plain_text(string);
    text->set_track(note->chord()->track());
    segment->add(text);
}

void GPConverter::clear_defected_spanner() {
    for (const auto &[track, elems] : ties_value) {
        for (Tie *tie : elems) {
            if (tie->start_note()) {
                tie->start_note()->set_tie_for(nullptr);
            }

            if (tie->end_note()) {
                tie->end_note()->set_tie_back(nullptr);
            }

            delete tie;
        }
    }
}

int GPConverter::get_string_number_for(mu::engraving::Note *p_note, int pitch) const {
    const auto &string_table = p_note->part()->instrument()->string_data()->string_list();

    const size_t string_table_size = string_table.size();

    if (!string_table_size) {
        return -1;
    }

    const int32_t last_idx = static_cast<int32_t>(string_table_size) - 1;

    for (int32_t idx = last_idx; idx >= 0; --idx) {
        auto string = string_table[idx];
        const int32_t curren_string_number = last_idx - idx;

        if (pitch >= string.pitch) {
            bool empty_string = true;

            for (const auto &note_tmp : p_note->chord()->notes()) {
                if (note_tmp->string() == curren_string_number) {
                    empty_string = false;
                    break;
                }
            }

            if (empty_string) {
                return curren_string_number;
            }
        }
    }

    return -1;
}

void GPConverter::set_beam_mode(const GPBeat *beat, ChordRest *cr, Measure *measure, Fraction tick) {
    BeamMode beam_mode = BeamMode::AUTO;

    if (beat->beam_mode() == GPBeat::BeamMode::BROKEN) {
        beam_mode = BeamMode::NONE;
    } else if (beat->beam_mode() == GPBeat::BeamMode::JOINED) {
        beam_mode = BeamMode::MID;
    } else if (beat->beam_mode() == GPBeat::BeamMode::BROKEN2 || beat->beam_mode() == GPBeat::BeamMode::BROKEN2_JOINED) {
        int measure_denom = measure->ticks().denominator();
        double fract = (double) tick.numerator() / tick.denominator() * measure_denom;

        if ((int) fract == fract && beat->beam_mode() != GPBeat::BeamMode::BROKEN2_JOINED) {
            /// keep auto direction for some beams, so BEGIN16/BEGIN32 modes work properly
            /// (forcing divide of beam groups, TODO-gp: make possible to show broken2 type from guitar pro
            beam_mode = BeamMode::AUTO;
        } else if (cr->ticks() > Fraction(1, 32)) {
            beam_mode = BeamMode::BEGIN16;
        } else {
            beam_mode = BeamMode::BEGIN32;
        }
    }

    cr->set_beam_mode(previous_beam_mode_);
    previous_beam_mode_ = beam_mode;
}

void GPConverter::add_tuning() {
    const Measure *m = score_value->first_measure();

    // NOTE: GP doesn't support multiple tunings on one part
    // We're safe to just take the very first chord rest segment
    // and check if it has any non-standard tuning
    const Fraction &f{0, 1};

    for (auto p : score_value->parts()) {
        for (auto s : p->staves()) {
            if (!s->is_primary_staff()) {
                continue;
            }

            Segment *seg = m->find_segment(SegmentType::CHORD_REST, f);

            IF_ASSERT_FAILED(seg) {
                LOGE() << "First measure MUST has a chord rest segment after import";
                return;
            }

            if (string_datas.find(p->id().to_uint64()) == string_datas.end()) {
                continue;
            }

            const StringData sd = string_datas.at(p->id().to_uint64());
            std::vector<int> tuning(sd.strings());
            for (size_t i = 0; i < tuning.size(); ++i) {
                tuning[i] = sd.string_list().at(i).pitch + p->instrument()->transpose().chromatic;
            }

            if (utils::is_standard_tuning(p->instrument()->channel(0)->program(), tuning)) {
                continue;
            }

            StringTunings *tun = Factory::create_string_tunings(seg);
            tun->set_string_data(sd);
            tun->set_track(staff2track(s->idx()));
            seg->add(tun);
        }
    }
}

int GPConverter::capo_fret(const Part *part) const {
    const auto it = capo_params_.find(part->id().to_uint64());
    return it != capo_params_.end() ? it->second : 0;
}

void GPConverter::add_capos() {
    const Measure *first_measure = score_value->first_measure();
    const size_t staves_count = score_value->staves().size();

    Segment *segment = first_measure->first(SegmentType::CHORD_REST);
    for (size_t i = 0; i < staves_count; ++i) {
        Staff *staff = score_value->staff(i);
        const auto &it = capo_params_.find(staff->part()->id().to_uint64());

        if (it != capo_params_.end() && it->second > 0) {
            size_t track = i * ::mu::iex::guitarpro::GPConverter::voices;

            CapoParams params;
            params.active = true;
            params.transpose_mode = CapoParams::TransposeMode::TAB_ONLY;
            params.fret_position = it->second;

            Capo *capo = Factory::create_capo(score_value->dummy());
            capo->set_track(track);
            capo->set_params(params);
            segment->add(capo);

            staff->insert_capo_params({0, 1}, params, true);
        }
    }
}
} // namespace mu::iex::guitarpro
