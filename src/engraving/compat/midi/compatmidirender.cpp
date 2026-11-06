#include "compatmidirender.h"

#include <cmath>
#include <tuple>

#include "dom/arpeggio.h"
#include "dom/articulation.h"
#include "dom/chord.h"
#include "dom/drumset.h"
#include "dom/durationtype.h"
#include "dom/dynamic.h"
#include "dom/glissando.h"
#include "dom/instrument.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/note.h"
#include "dom/noteevent.h"
#include "dom/part.h"
#include "dom/repeatlist.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/slur.h"
#include "dom/staff.h"
#include "dom/swing.h"
#include "dom/synthesizerstate.h"
#include "dom/tie.h"
#include "dom/tremolosinglechord.h"
#include "dom/tremolotwochord.h"
#include "dom/trill.h"
#include "dom/utils.h"
#include "dom/volta.h"
#include "easeinout.h"
#include "editing/navigation.h"
#include "global/realfn.h"
#include "log.h"
#include "types/bps.h"
#include "types/constants.h"

namespace mu::engraving {
static int slide_ticks(const Chord *chord);
static int grace_bend_ticks(const Chord *chord);

void CompatMidiRender::render_score(Score *score, EventsHolder &events, const CompatMidiRendererInternal::Context &ctx,
                                    bool expand_repeats) {
    score->master_score()->set_expand_repeats(expand_repeats);
    CompatMidiRendererInternal internal{score};
    internal.render_score(events, ctx, expand_repeats);
}

int CompatMidiRender::get_controller_for_snd(Score *score, int global_snd_controller) {
    const int default_cc = CTRL_BREATH;

    SynthesizerState s = score->synthesizer_state();
    int method = s.method();
    int controller = s.cc_to_use();

    if (method == -1) {
        controller = (global_snd_controller == -1) ? default_cc : global_snd_controller;
    }

    return controller;
}

void CompatMidiRender::create_play_events(const Score *score, Measure const *start, Measure const *const end,
                                          const CompatMidiRendererInternal::Context &context) {
    if (!start) {
        start = score->first_measure();
    }

    track_idx_t etrack = score->nstaves() * ::mu::engraving::voices;
    for (track_idx_t track = 0; track < etrack; ++track) {
        bool range_ended = false;
        Chord *prev_chord = nullptr;
        for (Measure const *m = start; m; m = m->next_measure()) {
            constexpr SegmentType st = SegmentType::CHORD_REST;

            if (m == end) {
                range_ended = true;
            }
            if (range_ended) {
                // The range has ended, but we should collect events
                // for tied notes. So we'll check if this is the case.
                const Segment *seg = m->first(st);
                const EngravingItem *e = seg->element(track);
                bool tie = false;
                if (e && e->is_chord()) {
                    for (const mu::engraving::Note *n : to_chord(e)->notes()) {
                        if (n->tie_back()) {
                            tie = true;
                            break;
                        }
                    }
                }
                if (!tie) {
                    break;
                }
            }

            // skip linked staves, except primary
            if (!m->score()->staff(track / ::mu::engraving::voices)->is_primary_staff()) {
                continue;
            }
            for (Segment *seg = m->first(st); seg; seg = seg->next(st)) {
                EngravingItem *item = seg->element(track);
                if (!item) {
                    continue;
                }

                if (!item->is_chord()) {
                    prev_chord = nullptr;
                    continue;
                }

                Chord *chord = to_chord(item);
                Chord *next_chord = nullptr;
                ChordRestNavigateOptions options;
                options.skip_grace = true;
                if (ChordRest *chr = Navigation::next_chord_rest(chord, options); chr && chr->is_chord()) {
                    next_chord = to_chord(chr);
                }

                if (!next_chord) {
                    // check if next chord is in next measure
                    Measure *next_measure = m->next_measure();
                    if (next_measure) {
                        next_chord = CompatMidiRender::get_chord_from_segment(next_measure->first(st), track);
                    }
                }

                CompatMidiRender::create_play_events(score, context, chord, prev_chord, next_chord);
                prev_chord = chord;
            }
        }
    }
}

void CompatMidiRender::create_play_events(const Score *score, const CompatMidiRendererInternal::Context &context, Chord *chord,
                                          Chord *prev_chord, Chord *next_chord) {
    int gate_time = 100;

    Fraction tick = chord->tick();
    Slur *slur = 0;
    for (auto sp : score->spanner_map().map()) {
        if (!sp.second->is_slur() || sp.second->staff_idx() != chord->staff_idx()) {
            continue;
        }
        Slur *s = to_slur(sp.second);
        if (tick >= s->tick() && tick < s->tick2()) {
            slur = s;
            break;
        }
    }
    // gateTime is 100% for slurred notes
    if (!slur) {
        Instrument *instr = chord->part()->instrument(tick);
        instr->update_gate_time(&gate_time, String());
    }

    int ontime = 0;
    int trailtime = 0;

    CompatMidiRender::create_grace_notes_play_events(
        score, tick, chord, ontime, trailtime); // ontime and trailtime are modified by this call depending on grace notes before and after
    trailtime = CompatMidiRender::adjust_trailtime(trailtime, chord, next_chord);

    SwingParameters st = chord->staff()->swing(tick);
    // Check if swing needs to be applied
    if (st.swing_unit && !chord->tuplet()) {
        Swing::swing_adjust_params(chord, st, ontime, gate_time);
    }
    //
    //    render normal (and articulated) chords
    //
    std::vector<NoteEventList> el = CompatMidiRender::render_chord(context, chord, prev_chord, gate_time, ontime, trailtime);
    if (chord->play_event_type() == PlayEventType::AUTO) {
        chord->set_note_event_lists(el);
    }
    // don't change event list if type is PlayEventType::User
}

//---------------------------------------------------------
//   renderChord
//    ontime and trailtime in 1/1000 of duration
//    ontime signifies how much gap to leave, i.e., how late the note should start because of graceNotesBefore which have already been rendered
//    trailtime signifies how much gap to leave after the note to allow for graceNotesAfter to be rendered
//---------------------------------------------------------

std::vector<NoteEventList> CompatMidiRender::render_chord(const CompatMidiRendererInternal::Context &context, Chord *chord,
                                                          Chord *prev_chord, int gate_time, int ontime, int trailtime) {
    const std::vector<mu::engraving::Note *> &notes = chord->notes();
    if (notes.empty()) {
        return std::vector<NoteEventList>();
    }

    std::vector<NoteEventList> ell(notes.size(), NoteEventList());

    bool arpeggio = false;

    /// when note has glissando or slide from note, tremolo should be shortened
    int base_length = NoteEvent::note_length - ontime - trailtime;
    double tremolo_length = (double) base_length / NoteEvent::note_length;

    bool glissando_start =
        std::any_of(notes.begin(), notes.end(), [](mu::engraving::Note *note) { return CompatMidiRender::note_is_glissando_start(note); });

    if (glissando_start) {
        tremolo_length = 0.5 * base_length / NoteEvent::note_length;
    } else {
        bool has_slide_out = std::any_of(notes.begin(), notes.end(), [](mu::engraving::Note *note) { return note->has_slide_from_note(); });

        if (has_slide_out) {
            tremolo_length = (base_length - CompatMidiRender::slide_length_chord_dependent(chord)) / (double) NoteEvent::note_length;
        }
    }

    bool tremolo = false;

    if (chord->arpeggio() && chord->arpeggio()->play_arpeggio()) {
        CompatMidiRender::render_arpeggio(chord, ell, ontime);
        arpeggio = true;
    } else {
        if (chord->tremolo_type() != TremoloType::INVALID_TREMOLO) {
            CompatMidiRender::render_tremolo(chord, ell, ontime, tremolo_length);
            tremolo = true;
        }

        CompatMidiRender::render_chord_articulation(context, chord, ell, gate_time, (double) ontime / NoteEvent::note_length, tremolo);
    }

    // Check each note and apply gateTime
    for (size_t i : get_notes_indexes_to_render(chord)) {
        mu::engraving::Note *note = chord->notes()[i];
        NoteEventList *el = &ell[i];

        CompatMidiRender::create_slide_out_note_play_events(note, el, ontime, tremolo);
        if (arpeggio) {
            continue; // don't add extra events and apply gateTime to arpeggio
        }
        // If we are here then we still need to render the note.
        // Render its body if necessary and apply gateTime.
        if (el->empty() && chord->tremolo_chord_type() != TremoloChordType::TREMOLO_SECOND_CHORD) {
            el->push_back(NoteEvent(0, ontime, 1000 - trailtime,
                                    !note->ghost() ? NoteEvent::default_velocity_multiplier : NoteEvent::ghost_velocity_multiplier));

            Glissando *gl = CompatMidiRender::back_glissando(note);
            if (gl && CompatMidiRender::is_glissando_valid(gl) && !gl->glissando_shift()) {
                el->back().set_slide(true);
            }
        }

        CompatMidiRender::create_slide_in_note_play_events(note, prev_chord, el);

        for (NoteEvent &e : *el) {
            e.set_len(e.len() * gate_time / 100);
        }

        std::sort(el->begin(), el->end(), [](const NoteEvent &left, const NoteEvent &right) {
            int l1 = left.ontime();
            int l2 = -left.offset();
            int r1 = right.ontime();
            int r2 = -right.offset();

            return std::tie(l1, l2) < std::tie(r1, r2);
        });
    }

    return ell;
}

//---------------------------------------------------------
//   renderArpeggio
//---------------------------------------------------------

void CompatMidiRender::render_arpeggio(Chord *chord, std::vector<NoteEventList> &ell, int ontime) {
    int notes = int(chord->notes().size());
    int l = 64;
    while (l && (l * notes > chord->up_note()->play_ticks())) {
        l = 2 * l / 3;
    }
    int start, end, step;
    bool up = chord->arpeggio()->arpeggio_type() != ArpeggioType::DOWN && chord->arpeggio()->arpeggio_type() != ArpeggioType::DOWN_STRAIGHT;
    if (up) {
        start = 0;
        end = notes;
        step = 1;
    } else {
        start = notes - 1;
        end = -1;
        step = -1;
    }
    int j = 0;
    for (int i = start; i != end; i += step) {
        NoteEventList *events = &(ell)[i];
        events->clear();

        auto tempo_ratio = chord->score()->multiplied_tempo(chord->tick()).val / Constants::default_tempo.val;
        int ot = (l * j * 1000) / chord->up_note()->play_ticks() * tempo_ratio * chord->arpeggio()->stretch();
        ot = std::clamp(ot + ontime, ot, 1000);

        events->push_back(NoteEvent(0, ot, 1000 - ot));
        j++;
    }
}

//---------------------------------------------------------
//   renderTremolo
//---------------------------------------------------------

void CompatMidiRender::render_tremolo(Chord *chord, std::vector<NoteEventList> &ell, int &ontime,
                                      double tremolo_part_of_chord /* = 1.0 */) {
    if (muse::real_is_null(tremolo_part_of_chord)) {
        return;
    }

    struct TremoloAdapter {
        TremoloSingleChord *single_chord = nullptr;
        TremoloTwoChord *two_chord = nullptr;

        TremoloAdapter(TremoloSingleChord *s, TremoloTwoChord *t) : single_chord(s), two_chord(t) {}

        TremoloType tremolo_type() const {
            return single_chord ? single_chord->tremolo_type() : (two_chord ? two_chord->tremolo_type() : TremoloType::INVALID_TREMOLO);
        }

        int lines() const { return single_chord ? single_chord->lines() : (two_chord ? two_chord->lines() : 0); }
    };

    TremoloAdapter tremolo = TremoloAdapter(chord->tremolo_single_chord(), chord->tremolo_two_chord());

    Segment *seg = chord->segment();
    //TremoloDispatcher* tremolo = chord->tremoloDispatcher();
    int notes = int(chord->notes().size());

    // check if tremolo was rendered before for drum staff
    const Drumset *ds = CompatMidiRender::get_drumset(chord);
    if (ds) {
        for (mu::engraving::Note *n : chord->notes()) {
            DrumInstrumentVariant div = ds->find_variant(n->pitch(), chord->articulations(), tremolo.tremolo_type());
            if (div.pitch != ::mu::engraving::invalid_pitch && div.tremolo == tremolo.tremolo_type()) {
                return; // already rendered
            }
        }
    }

    // we cannot render buzz roll with MIDI events only
    if (tremolo.tremolo_type() == TremoloType::BUZZ_ROLL) {
        return;
    }

    // render tremolo with multiple events
    if (chord->tremolo_chord_type() == TremoloChordType::TREMOLO_FIRST_CHORD) {
        int t = Constants::division / (1 << (tremolo.lines() + chord->duration_type().hooks()));
        if (t == 0) { // avoid crash on very short tremolo
            t = 1;
        }
        SegmentType st = SegmentType::CHORD_REST;
        Segment *seg2 = seg->next(st);
        track_idx_t track = chord->track();
        while (seg2 && !seg2->element(track)) {
            seg2 = seg2->next(st);
        }

        if (!seg2) {
            return;
        }

        EngravingItem *s2_el = seg2->element(track);
        if (s2_el) {
            if (!s2_el->is_chord()) {
                return;
            }
        } else {
            return;
        }

        Chord *c2 = to_chord(s2_el);
        if (c2->is_chord()) {
            int notes2 = int(c2->notes().size());
            int tnotes = std::max(notes, notes2);
            int tticks = chord->ticks().ticks() * 2; // use twice the size
            int n = tticks / t;
            n /= 2;
            int l = 2000 * t / tticks;
            for (int k = 0; k < tnotes; ++k) {
                NoteEventList *events;
                if (k < notes) {
                    // first chord has note
                    events = &ell[k];
                    events->clear();
                } else {
                    // otherwise reuse note 0
                    events = &ell[0];
                }
                if (k < notes && k < notes2) {
                    // both chords have note
                    int p1 = chord->notes()[k]->pitch();
                    int p2 = c2->notes()[k]->pitch();
                    int dpitch = p2 - p1;
                    for (int i = 0; i < n; ++i) {
                        events->push_back(NoteEvent(0, l * i * 2, l));
                        events->push_back(NoteEvent(dpitch, l * i * 2 + l, l));
                    }
                } else if (k < notes) {
                    // only first chord has note
                    for (int i = 0; i < n; ++i) {
                        events->push_back(NoteEvent(0, l * i * 2, l));
                    }
                } else {
                    // only second chord has note
                    // reuse note 0 of first chord
                    int p1 = chord->notes()[0]->pitch();
                    int p2 = c2->notes()[k]->pitch();
                    int dpitch = p2 - p1;
                    for (int i = 0; i < n; ++i) {
                        events->push_back(NoteEvent(dpitch, l * i * 2 + l, l));
                    }
                }
            }
        } else {
            LOGD("Chord::renderTremolo: cannot find 2. chord");
        }
    } else if (chord->tremolo_chord_type() == TremoloChordType::TREMOLO_SECOND_CHORD) {
        for (int k = 0; k < notes; ++k) {
            NoteEventList *events = &(ell)[k];
            events->clear();
        }
    } else if (chord->tremolo_chord_type() == TremoloChordType::TREMOLO_SINGLE) {
        int t = Constants::division / (1 << (tremolo.lines() + chord->duration_type().hooks()));
        if (t == 0) { // avoid crash on very short tremolo
            t = 1;
        }

        int tremolo_events_size = chord->ticks().ticks() / t * tremolo_part_of_chord;

        constexpr int full_event_time = 1000;
        int tremolo_time = full_event_time * tremolo_part_of_chord;
        int tremolo_event_step = tremolo_time / tremolo_events_size;
        ontime += tremolo_time;

        for (int k = 0; k < notes; ++k) {
            NoteEventList *events = &(ell)[k];
            events->clear();
            for (int i = 0; i < tremolo_events_size; i++) {
                events->push_back(NoteEvent(0, tremolo_event_step * i, tremolo_event_step));
            }
        }
    }
}

//---------------------------------------------------------
//   renderChordArticulation
//---------------------------------------------------------

void CompatMidiRender::render_chord_articulation(const CompatMidiRendererInternal::Context &context, Chord *chord,
                                                 std::vector<NoteEventList> &ell, int &gate_time, double grace_on_beat_proportion,
                                                 bool tremolo_before /* = false */) {
    Segment *seg = chord->segment();
    Instrument *instr = chord->part()->instrument(seg->tick());

    for (unsigned k = 0; k < chord->notes().size(); ++k) {
        NoteEventList *events = &ell[k];
        Note *note = chord->notes()[k];
        Trill *trill;

        if (note_is_glissando_start(note)) {
            CompatMidiRender::render_glissando(events, note, grace_on_beat_proportion, tremolo_before);
        } else if (chord->staff()->is_pitched_staff(chord->tick()) && (trill = find_first_trill(chord)) != nullptr) {
            CompatMidiRender::render_note_articulation(events, note, false, trill->trill_type(), trill->ornament_style());
        } else {
            for (Articulation *a : chord->articulations()) {
                if (!a->play_articulation()) {
                    continue;
                }
                if (!CompatMidiRender::render_note_articulation(events, note, false, a->sym_id(), a->ornament_style())) {
                    CompatMidiRender::update_gate_time(instr, gate_time, a->articulation_name(), context);
                }
            }
        }
    }
}

void CompatMidiRender::update_gate_time(const Instrument *instrument, int &gate_time, const String &articulation_name,
                                        const CompatMidiRendererInternal::Context &context) {
    using Ctx = CompatMidiRendererInternal::Context;
    if (context.use_default_articulations) {
        auto it = Ctx::s_built_in_articulations_values.find(articulation_name);
        if (it != Ctx::s_built_in_articulations_values.end()) {
            gate_time = std::min(gate_time, it->second.gate_time);
        } else {
            instrument->update_gate_time(&gate_time, articulation_name);
        }

        return;
    }

    auto articulations_for_instrument_it = context.articulations_without_values_by_instrument.find(instrument->id());
    if (articulations_for_instrument_it != context.articulations_without_values_by_instrument.end()) {
        const auto &articulations_for_instrument = articulations_for_instrument_it->second;
        if (articulations_for_instrument.find(articulation_name) != articulations_for_instrument.end()) {
            gate_time = std::min(gate_time, Ctx::s_built_in_articulations_values[articulation_name].gate_time);
        } else {
            instrument->update_gate_time(&gate_time, articulation_name);
        }
    }
}

//---------------------------------------------------------
//   createGraceNotesPlayEvent
// as a side effect of createGraceNotesPlayEvents, ontime and trailtime (passed by ref)
// are modified.  ontime reflects the time needed to play the grace-notes-before, and
// trailtime reflects the time for the grace-notes-after.  These are used by the caller
// to effect the on/off time of the main note
//---------------------------------------------------------

void CompatMidiRender::create_grace_notes_play_events(const Score *score, const Fraction &tick, Chord *chord, int &ontime, int &trailtime) {
    std::vector<Chord *> gnb = chord->grace_notes_before(true);
    std::vector<Chord *> gna = chord->grace_notes_after(true);
    int nb = int(gnb.size());
    int na = int(gna.size());
    if (0 == nb + na) {
        return; // return immediately if no grace notes to deal with
    }
    // return immediately if the chord has a trill or articulation which effectively plays the graces notes.
    if (CompatMidiRendererInternal::grace_notes_merged(chord)) {
        return;
    }
    // if there are graceNotesBefore and also graceNotesAfter, and the before grace notes are
    // not ACCIACCATURA, then the total time of all of them will be 50% of the time of the main note.
    // if the before grace notes are ACCIACCATURA then the grace notes after (if there are any).
    // get 50% of the time of the main note.
    // this is achieved by the two floating point weights: weighta and weightb whose total is 1.0
    // assuring that all the grace notes get the same duration, and their total is 50%.
    // exception is if the note is dotted or double-dotted; see below.
    float weighta = float(na) / (nb + na);

    int grace_duration = 0;
    bool drumset = (CompatMidiRender::get_drumset(chord) != nullptr);
    const double ticks_per_second = score->multiplied_tempo(tick).val * Constants::division;
    const double chord_time_ms = (chord->actual_ticks().ticks() / ticks_per_second) * 1000;
    if (drumset) {
        int flam_duration = 15;                                //ms
        grace_duration = flam_duration / chord_time_ms * 1000; //ratio 1/1000 from the main note length
        ontime = grace_duration * nb;
    } else if (nb) {
        //
        //  render grace notes:
        //  simplified implementation:
        //  - grace notes start on the beat of the main note
        //  - duration: appoggiatura: 0.5  * duration of main note (2/3 for dotted notes, 4/7 for double-dotted)
        //              acciacatura: min of 0.5 * duration or 65ms fixed (independent of duration or tempo)
        //  - for appoggiaturas, the duration is divided by the number of grace notes
        //  - the grace note duration as notated does not matter
        //
        Chord *grace_chord = gnb[0];
        const auto &grace_notes = grace_chord->notes();
        bool grace_bend = std::any_of(grace_notes.begin(), grace_notes.end(), [](Note *note) { return note->is_grace_bend_start(); });

        if (grace_chord->note_type() == NoteType::ACCIACCATURA || grace_bend) {
            ontime = 0;
            grace_duration = 0;
            weighta = 1.0;
        } else {
            const double grace_time_ms = (grace_chord->actual_ticks().ticks() / ticks_per_second) * 1000;

            // 1000 occurs below as a unit for ontime
            ontime = std::min(500, static_cast<int>((grace_time_ms / chord_time_ms) * 1000));
            grace_duration = ontime / nb;
            weighta = 1.0;
            trailtime += ontime;
        }
    }

    for (int i = 0, on = 0; i < nb; ++i) {
        std::vector<NoteEventList> el;
        Chord *gc = gnb.at(i);
        size_t nn = gc->notes().size();
        for (unsigned ii = 0; ii < nn; ++ii) {
            NoteEventList nel;
            nel.push_back(NoteEvent(0, on, grace_duration));
            el.push_back(nel);
        }

        if (gc->play_event_type() == PlayEventType::AUTO) {
            gc->set_note_event_lists(el);
        }

        on += grace_duration;
    }

    bool grace_bend_after = std::any_of(gna.begin(), gna.end(), [](const Chord *grace_chord) {
        for (Note *note : grace_chord->notes()) {
            if (note->bend_back()) {
                return true;
            }
        }

        return false;
    });

    if (!grace_bend_after && na) {
        if (chord->dots() == 1) {
            trailtime = floor(667 * weighta);
        } else if (chord->dots() == 2) {
            trailtime = floor(571 * weighta);
        } else {
            trailtime = floor(500 * weighta);
        }
        int grace_duration1 = trailtime / na;
        int on = 1000 - trailtime;
        for (int i = 0; i < na; ++i) {
            std::vector<NoteEventList> el;
            Chord *gc = gna.at(i);
            size_t nn = gc->notes().size();
            for (size_t ii = 0; ii < nn; ++ii) {
                NoteEventList nel;
                nel.push_back(NoteEvent(0, on, grace_duration1)); // NoteEvent(pitch,ontime,len)
                el.push_back(nel);
            }

            if (gc->play_event_type() == PlayEventType::AUTO) {
                gc->set_note_event_lists(el);
            }
            on += grace_duration1;
        }
    }
}

//---------------------------------------------------------
//   renderGlissando
//---------------------------------------------------------

void CompatMidiRender::render_glissando(NoteEventList *events, Note *notestart, double grace_on_beat_proportion,
                                        bool tremolo_before /* = false */) {
    std::vector<int> empty = {};
    std::vector<int> body;
    for (Spanner *s : notestart->spanner_for()) {
        if (s->is_glissando() && s->play_spanner() && Glissando::pitch_steps(s, body)) {
            CompatMidiRender::render_note_articulation(events, notestart, true, Constants::division, empty, body, false, true, empty, 16, 0,
                                                       grace_on_beat_proportion, tremolo_before);
        }
    }
}

//---------------------------------------------------------
//   renderNoteArticulation
// prefix, vector of int, normally something like {0,-1,0,1} modeling the prefix of tremblement relative to the base note
// body, vector of int, normally something like {0,-1,0,1} modeling the possibly repeated tremblement relative to the base note
// tickspernote, number of ticks, either _16h or _32nd, i.e., Constants::division/4 or Constants::division/8
// repeatp, true means repeat the body as many times as possible to fill the time slice.
// sustainp, true means the last note of the body is sustained to fill remaining time slice
//---------------------------------------------------------

bool CompatMidiRender::render_note_articulation(NoteEventList *events, Note *note, bool chromatic, int requested_ticks_per_note,
                                                const std::vector<int> &prefix, const std::vector<int> &body, bool repeatp, bool sustainp,
                                                const std::vector<int> &suffix, int fastest_freq /* = 64 */,
                                                int slowest_freq /* = 8 */, // 64 Hz and 8 Hz
                                                double grace_on_beat_proportion /* = 0 */, bool tremolo_before /* = false */) {
    if (!tremolo_before) {
        events->clear();
    }

    Chord *chord = note->chord();
    int maxticks = CompatMidiRender::total_tied_note_ticks(note);
    int space = 1000 * maxticks;
    int numrepeat = 1;
    int sustain = 0;
    int ontime = 0;

    ///@NOTE grace note + glissando combination is treated separetely, in this method grace notes for glissando are not added
    /// graceOnBeatProportion is used for calculating grace note "on beat" offset
    int gnb = note_is_glissando_start(note) ? 0 : int(note->chord()->grace_notes_before().size());
    int p = int(prefix.size());
    int b = int(body.size());
    int s = int(suffix.size());
    int gna = int(note->chord()->grace_notes_after().size());

    int ticks_per_note = 0;

    if (gnb + p + b + s + gna <= 0) {
        return false;
    }

    Fraction tick = chord->tick();
    BeatsPerSecond tempo = chord->score()->multiplied_tempo(tick);
    int ticks_per_second = tempo.val * Constants::division;

    int min_ticks_per_note = int(ticks_per_second / fastest_freq);
    int max_ticks_per_note = (0 == slowest_freq) ? 0 : int(ticks_per_second / slowest_freq);

    // for fast tempos, we have to slow down the tremblement frequency, i.e., increase the ticks per note
    if (requested_ticks_per_note >= min_ticks_per_note) {
    } else { // try to divide the requested frequency by a power of 2 if possible, if not, use the maximum frequency, ie., minTicksPerNote
        ticks_per_note = requested_ticks_per_note;
        while (ticks_per_note < min_ticks_per_note) {
            ticks_per_note *= 2; // decrease the tremblement frequency
        }
        if (ticks_per_note > max_ticks_per_note) {
            ticks_per_note = min_ticks_per_note;
        }
    }

    ticks_per_note = std::max(requested_ticks_per_note, min_ticks_per_note);

    if (slowest_freq <= 0) { // no slowest freq given such as something silly like glissando with 4 notes over 8 counts.
    } else if (ticks_per_note <= max_ticks_per_note) { // in a good range, so we don't need to adjust ticksPerNote
    } else {
        // for slow tempos, such as adagio, we may need to speed up the tremblement frequency, i.e., decrease the ticks per note, to make it sound reasonable.
        ticks_per_note = requested_ticks_per_note;
        while (ticks_per_note > max_ticks_per_note) {
            ticks_per_note /= 2;
        }
        if (ticks_per_note < min_ticks_per_note) {
            ticks_per_note = min_ticks_per_note;
        }
    }
    // calculate whether to shorten the duration value.
    if (ticks_per_note * (gnb + p + b + s + gna) <= maxticks) {
        // plenty of space to play the notes without changing the requested trill note duration
    } else if (ticks_per_note == min_ticks_per_note) {
        return false; // the ornament is impossible to implement respecting the minimum duration and all the notes it contains
    } else {
        ticks_per_note = maxticks / (gnb + p + b + s + gna); // integer division ignoring remainder
        if (slowest_freq <= 0) {
        } else if (ticks_per_note < min_ticks_per_note) {
            return false;
        }
    }

    int millespernote = space * ticks_per_note / maxticks; // rescale duration into per mille

    // local function:
    // look ahead in the given vector to see if the current note is the same pitch as the next note or next several notes.
    // If so, increment the duration by the appropriate note duration, and increment the index, j, to the next note index
    // of a different pitch.
    // The total duration of the tied note is returned, and the index is modified.
    auto tie_forward = [millespernote](int &j, const std::vector<int> &vec) {
        int size = int(vec.size());
        int duration = millespernote;
        while (j < size - 1 && vec[j] == vec[j + 1]) {
            duration += millespernote;
            j++;
        }
        return duration;
    };

    // local function:
    //   append a NoteEvent either by calculating an articulationExcursion or by
    //   the given chromatic relative pitch.
    //   RETURNS the new ontime value.  The caller is expected to assign this value.
    auto make_event = [note, chord, chromatic, events](int pitch, int ontime, int duration,
                                                       double velocity_multiplier = NoteEvent::default_velocity_multiplier,
                                                       bool play = true) {
        if (note->ghost()) {
            velocity_multiplier *= NoteEvent::ghost_velocity_multiplier;
        }
        events->push_back(NoteEvent(chromatic ? pitch : chromatic_pitch_steps(note, note, pitch), ontime / chord->actual_ticks().ticks(),
                                    duration / chord->actual_ticks().ticks(), velocity_multiplier, play));

        return ontime + duration;
    };

    // calculate the number of times to repeat the body, and sustain the last note of the body
    // 1000 = P + numrepeat*B+sustain + S
    if (repeatp) {
        numrepeat = (space - millespernote * (gnb + p + s + gna)) / (millespernote * b);
    }
    if (sustainp) {
        sustain = space - millespernote * (gnb + p + numrepeat * b + s + gna);
    }

    // render the prefix
    for (int j = 0; j < p; j++) {
        ontime = make_event(prefix[j], ontime, tie_forward(j, prefix));
    }

    if (b > 0) {
        // Check that we are doing a glissando
        enum GlissandoState { NOT_FOUND, VALID, INVALID } glissando_state = GlissandoState::NOT_FOUND;

        std::vector<int> on_times;
        for (Spanner *spanner : note->spanner_for()) {
            if (spanner->is_glissando()) {
                Glissando *glissando = to_glissando(spanner);
                if (!is_glissando_valid(glissando)) {
                    glissando_state = GlissandoState::INVALID;
                    break;
                }

                EaseInOut ease_in_out(static_cast<double>(glissando->ease_in()) / 100.0,
                                      static_cast<double>(glissando->ease_out()) / 100.0);

                // shifting glissando sounds to the second half of glissando duration
                int total_duration = millespernote * b;
                constexpr double glissando_part = 0.33;

                int glissando_duration =
                    tremolo_before ? total_duration * glissando_part : total_duration * (1 - grace_on_beat_proportion) * glissando_part;
                ease_in_out.time_list(b - 1, glissando_duration, &on_times);

                if (!on_times.empty()) {
                    on_times[0] += grace_on_beat_proportion * total_duration;
                }

                if (on_times.size() > 1) {
                    int offset = on_times[1];
                    for (size_t i = 1; i < on_times.size(); i++) {
                        on_times[i] += total_duration - glissando_duration - offset;
                    }
                }

                glissando_state = GlissandoState::VALID;
                break;
            }
        }
        if (glissando_state == GlissandoState::VALID) {
            const double default_velocity_multiplier = 1.0;

            // render the body, i.e. the glissando
            if (on_times.size() > 1) {
                make_event(body[0], on_times[0], on_times[1] - on_times[0], default_velocity_multiplier, !note->tie_back());

                Glissando *gl = back_glissando(note);
                if (gl && is_glissando_valid(gl) && !gl->glissando_shift()) {
                    events->back().set_slide(true);
                }
            }

            for (int j = 1; j < b - 1; j++) {
                make_event(body[j], on_times[j], on_times[j + 1] - on_times[j], default_velocity_multiplier);
                events->back().set_slide(true);
            }

            if (b > 1) {
                make_event(body[b - 1], on_times[b - 1], (millespernote * b - on_times[b - 1]) + sustain, default_velocity_multiplier);
                events->back().set_slide(true);
            }
        } else if (glissando_state != GlissandoState::INVALID) {
            // render the body, but not the final repetition
            for (int r = 0; r < numrepeat - 1; r++) {
                for (int j = 0; j < b; j++) {
                    ontime = make_event(body[j], ontime, millespernote);
                }
            }
            // render the final repetition of body, but not the final note of the repetition
            for (int j = 0; j < b - 1; j++) {
                ontime = make_event(body[j], ontime, millespernote);
            }
            // render the final note of the final repeat of body
            ontime = make_event(body[b - 1], ontime, millespernote + sustain);
        }
    }
    // render the suffix
    for (int j = 0; j < s; j++) {
        ontime = make_event(suffix[j], ontime, tie_forward(j, suffix));
    }

    return true;
}

//---------------------------------------------------------
//   renderNoteArticulation
//---------------------------------------------------------

bool CompatMidiRender::render_note_articulation(NoteEventList *events, Note *note, bool chromatic, SymId articulation_type,
                                                OrnamentStyle ornament_style) {
    if (!note->staff()->is_pitched_staff(note->tick())) { // not enough info in tab staff
        return false;
    }

    std::vector<int> emptypattern = {};
    for (auto &oe : excursions) {
        if (oe.atype == articulation_type && (0 == oe.ostyles.size() || oe.ostyles.end() != oe.ostyles.find(ornament_style))) {
            return render_note_articulation(events, note, chromatic, oe.duration, oe.prefix, oe.body, oe.repeatp, oe.sustainp, oe.suffix);
        }
    }
    return false;
}

//---------------------------------------------------------
//   renderNoteArticulation
//---------------------------------------------------------

bool CompatMidiRender::render_note_articulation(NoteEventList *events, Note *note, bool chromatic, TrillType trill_type,
                                                OrnamentStyle ornament_style) {
    std::map<TrillType, SymId> articulation_map = {{TrillType::TRILL_LINE, SymId::ORNAMENT_TRILL},
                                                   {TrillType::UPPRALL_LINE, SymId::ORNAMENT_UP_PRALL},
                                                   {TrillType::DOWNPRALL_LINE, SymId::ORNAMENT_PRECOMP_MORDENT_UPPER_PREFIX},
                                                   {TrillType::PRALLPRALL_LINE, SymId::ORNAMENT_TRILL}};
    auto it = articulation_map.find(trill_type);
    if (it == articulation_map.cend()) {
        return false;
    } else {
        return CompatMidiRender::render_note_articulation(events, note, chromatic, it->second, ornament_style);
    }
}

void CompatMidiRender::create_slide_in_note_play_events(Note *note, Chord *prev_chord, NoteEventList *el) {
    if (!note->has_slide_to_note()) {
        return;
    }

    int slide_notes = 0;
    bool up_slide = note->slide_to_type() == Note::SlideType::UP_TO_NOTE;
    if (up_slide) {
        slide_notes = std::clamp(note->fret() - 1, 0, NoteEvent::slide_amount);
    } else {
        slide_notes = NoteEvent::slide_amount;
    }

    if (slide_notes == 0) {
        return;
    }

    int total_slide_duration = 0;
    double current_ticks = note->chord()->ticks().ticks();
    if (prev_chord) {
        /// event length is about current chord's events, but we adjust previous
        total_slide_duration = (double) slide_length_chord_dependent(prev_chord) / current_ticks * prev_chord->ticks().ticks();
    } else {
        total_slide_duration = (double) NoteEvent::note_length * ::mu::engraving::slide_duration / current_ticks;
    }

    const int slide_duration = total_slide_duration / slide_notes;

    int slide_on = 0;
    int pitch_offset = up_slide ? -1 : 1;
    int pitch = pitch_offset * slide_notes;
    for (int i = 0; i < slide_notes; ++i) {
        NoteEvent event{pitch, 0, slide_duration, 1.0, true, total_slide_duration - slide_on};
        event.set_slide(true);
        el->push_back(std::move(event));
        slide_on += slide_duration;
        pitch -= pitch_offset;
    }
}

void CompatMidiRender::create_slide_out_note_play_events(Note *note, NoteEventList *el, int on_time, bool has_tremolo) {
    if (!note->has_slide_from_note()) {
        return;
    }

    const int slide_notes = NoteEvent::slide_amount;
    const int total_slide_duration = slide_length_chord_dependent(note->chord());
    const int slide_duration = total_slide_duration / slide_notes;

    int slide_on = NoteEvent::note_length - total_slide_duration;
    double velocity = !note->ghost() ? NoteEvent::default_velocity_multiplier : NoteEvent::ghost_velocity_multiplier;
    if (!has_tremolo) {
        el->push_back(NoteEvent(0, on_time, slide_on - on_time, velocity, !note->tie_back()));
    }

    int pitch = 0;
    int pitch_offset = note->slide_from_type() == Note::SlideType::UP_FROM_NOTE ? 1 : -1;
    for (int i = 0; i < slide_notes; ++i) {
        pitch += pitch_offset;
        NoteEvent event{pitch, slide_on, slide_duration, velocity};
        event.set_slide(true);
        el->push_back(std::move(event));
        slide_on += slide_duration;
    }
}

Chord *CompatMidiRender::get_chord_from_segment(Segment *segment, track_idx_t track) {
    if (segment) {
        EngravingItem *item = segment->element(track);
        if (item && item->is_chord()) {
            return to_chord(item);
        }
    }

    return nullptr;
}

//---------------------------------------------------------
// findFirstTrill
//  search the spanners in the score, finding the first one
//  which overlaps this chord and is of type ElementType::TRILL
//---------------------------------------------------------

Trill *CompatMidiRender::find_first_trill(Chord *chord) {
    auto spanners = chord->score()->spanner_map().find_overlapping(1 + chord->tick().ticks(), chord->end_tick().ticks() - 1);
    for (auto i : spanners) {
        if (!i.value->is_trill()) {
            continue;
        }
        if (i.value->track() != chord->track()) {
            continue;
        }
        Trill *trill = to_trill(i.value);
        if (!trill->play_spanner()) {
            continue;
        }
        return trill;
    }
    return nullptr;
}

int CompatMidiRender::adjust_trailtime(int trailtime, Chord *current_chord, Chord *next_chord) {
    if (!next_chord) {
        return trailtime;
    }

    const std::vector<Chord *> &grace_before_chords = next_chord->grace_notes_before();
    std::vector<Chord *> grace_notes_before_bar;

    bool has_grace_bend = std::any_of(grace_before_chords.begin(), grace_before_chords.end(), [](Chord *ch) {
        return std::any_of(ch->notes().begin(), ch->notes().end(), [](Note *n) { return n->is_grace_bend_start(); });
    });

    if (!has_grace_bend) {
        std::copy_if(grace_before_chords.begin(), grace_before_chords.end(), std::back_inserter(grace_notes_before_bar),
                     [](Chord *ch) { return ch->note_type() == NoteType::ACCIACCATURA; });
    }

    const int current_ticks = current_chord->ticks().ticks();
    IF_ASSERT_FAILED(current_ticks > 0) { return trailtime; }

    int reduced_ticks = 0;

    const auto &notes = next_chord->notes();
    if (has_grace_bend) {
        reduced_ticks = grace_bend_ticks(current_chord);
    } else if (!grace_notes_before_bar.empty()) {
        reduced_ticks = std::min(grace_notes_before_bar[0]->ticks().ticks(), current_ticks / 2);
    } else {
        bool any_slides_in = std::any_of(notes.begin(), notes.end(), [](const Note *note) {
            return note->slide_to_type() == Note::SlideType::DOWN_TO_NOTE ||
                   (note->slide_to_type() == Note::SlideType::UP_TO_NOTE && note->fret() > 1);
        });

        if (any_slides_in) {
            reduced_ticks = slide_ticks(current_chord);
        }
    }

    trailtime += reduced_ticks / (double) current_ticks * NoteEvent::note_length;
    return trailtime;
}

//---------------------------------------------------------
//   noteIsGlissandoStart
//---------------------------------------------------------

bool CompatMidiRender::note_is_glissando_start(mu::engraving::Note *note) {
    for (Spanner *spanner : note->spanner_for()) {
        if (spanner->is_glissando() && spanner->end_element() && spanner->end_element()->is_note()) {
            return true;
        }
    }
    return false;
}

int CompatMidiRender::slide_length_chord_dependent(Chord *chord) {
    const int current_ticks = chord->ticks().ticks();
    if (current_ticks <= ::mu::engraving::slide_duration) {
        return NoteEvent::note_length / 2;
    }

    return (double) NoteEvent::note_length * ::mu::engraving::slide_duration / current_ticks;
}

std::set<size_t> CompatMidiRender::get_notes_indexes_to_render(Chord *chord) {
    std::set<size_t> notes_indexes_to_render;

    auto &notes = chord->notes();
    /// not adding sounds for the same pitches in chord (for example, on different strings)
    std::map<int, int> longest_play_ticks_for_pitch;

    for (Note *note : notes) {
        int pitch = note->pitch();
        auto &pitch_length = longest_play_ticks_for_pitch[pitch];
        pitch_length = std::max(pitch_length, note->play_ticks());
    }

    auto note_should_be_rendered = [](Note *n) {
        while (n->tie_back_non_partial() && n != n->tie_back()->start_note()) {
            n = n->tie_back()->start_note();
            if (find_first_trill(n->chord())) {
                // The previous tied note probably has events for this note too.
                // That is, we don't need to render this note separately.
                return false;
            }

            for (Articulation *a : n->chord()->articulations()) {
                if (a->is_ornament()) {
                    return false;
                }
            }
        }

        return true;
    };

    for (size_t i = 0; i < notes.size(); i++) {
        Note *n = notes[i];
        if (note_should_be_rendered(n) && longest_play_ticks_for_pitch[n->pitch()] == n->play_ticks()) {
            notes_indexes_to_render.insert(i);
        }
    }

    return notes_indexes_to_render;
}

//---------------------------------------------------------
//   backGlissando
//---------------------------------------------------------

Glissando *CompatMidiRender::back_glissando(Note *note) {
    for (Spanner *spanner : note->spanner_back()) {
        if (spanner->is_glissando() && spanner->start_element() && spanner->start_element()->is_note()) {
            return to_glissando(spanner);
        }
    }
    return nullptr;
}

//---------------------------------------------------------
//   isGlissandoValid
//
//   checking if glissando can be played
//---------------------------------------------------------

bool CompatMidiRender::is_glissando_valid(Glissando *glissando) {
    EngravingItem *start_item = glissando->start_element();
    EngravingItem *end_item = glissando->end_element();
    if (!start_item || !end_item || !start_item->is_note() || !end_item->is_note()) {
        return false;
    }

    return to_note(start_item)->string() == to_note(end_item)->string();
}

const Drumset *CompatMidiRender::get_drumset(const Chord *chord) {
    if (chord->staff() && chord->staff()->is_drum_staff(chord->tick())) {
        const Drumset *ds = chord->staff()->part()->instrument(chord->tick())->drumset();
        return ds;
    }
    return nullptr;
}

//---------------------------------------------------------
// totalTiedNoteTicks
//      return the total of the actualTicks of the given note plus
//      the chain of zero or more notes tied to it to the right.
//---------------------------------------------------------

int CompatMidiRender::total_tied_note_ticks(Note *note) {
    Fraction total = note->chord()->actual_ticks();
    while (note->tie_for() && note->tie_for()->end_note() && (note->chord()->tick() < note->tie_for()->end_note()->chord()->tick())) {
        note = note->tie_for()->end_note();
        total += note->chord()->actual_ticks();
    }
    return total.ticks();
}

static int slide_ticks(const Chord *chord) {
    const int current_ticks = chord->ticks().ticks();
    if (current_ticks <= ::mu::engraving::slide_duration) {
        return current_ticks / 2;
    }

    return ::mu::engraving::slide_duration;
}

static int grace_bend_ticks(const Chord *chord) {
    const int current_ticks = chord->ticks().ticks();
    if (current_ticks / 2 <= ::mu::engraving::grace_bend_duration_ticks) {
        return current_ticks / 2;
    }

    return ::mu::engraving::grace_bend_duration_ticks;
}

int CompatMidiRender::tick(const CompatMidiRendererInternal::Context &ctx, int tick) {
    return ctx.apply_caesuras ? ctx.pause_map->tick_with_pauses(tick) : tick;
}
} // namespace mu::engraving
