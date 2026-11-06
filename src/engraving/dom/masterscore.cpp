/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "masterscore.h"

#include <algorithm>

#include "io/buffer.h"

#include "compat/writescorehook.h"
#include "editing/editautomationpoints.h"
#include "editing/editkeysig.h"
#include "editing/editmeasures.h"
#include "editing/transaction/transaction.h"
#include "editing/transaction/undostack.h"
#include "rw/mscloader.h"
#include "rw/xmlreader.h"
#include "rw/rwregister.h"

#include "style/defaultstyle.h"

#include "engravingproject.h"

#include "automation/internal/scoreautomationcontroller.h"

#include "barline.h"
#include "excerpt.h"
#include "factory.h"
#include "clef.h"
#include "key.h"
#include "keysig.h"
#include "part.h"
#include "repeatlist.h"
#include "rest.h"
#include "sig.h"
#include "staff.h"
#include "timesig.h"
#include "linkedobjects.h"

#include "log.h"

using namespace mu;
using namespace muse::io;
using namespace mu::engraving;

//---------------------------------------------------------
//   MasterScore
//---------------------------------------------------------

MasterScore::MasterScore(const EngravingServices &services, std::weak_ptr<engraving::EngravingProject> project) : Score(services) {
    project_ = project;
    transaction_manager_ = std::make_unique<TransactionManager>(this);
    undo_stack_ = new UndoStack();
    sigmap_ = new TimeSigMap();
    automation_controller_ = new ScoreAutomationController();
    expanded_repeat_list_ = new RepeatList(this);
    non_expanded_repeat_list_ = new RepeatList(this);
    set_master_score(this);

#if defined(_WIN32)
    meta_tags().insert({u"platform", u"Microsoft Windows"});
#elif defined(__APPLE__)
    meta_tags().insert({u"platform", u"Apple Macintosh"});
#elif defined(__linux__)
    meta_tags().insert({u"platform", u"Linux"});
#else
    meta_tags().insert({u"platform", u"Unknown"});
#endif
    meta_tags().insert({u"movementNumber", u""});
    meta_tags().insert({u"movementTitle", u""});
    meta_tags().insert({u"workNumber", u""});
    meta_tags().insert({u"workTitle", u""});
    meta_tags().insert({u"arranger", u""});
    meta_tags().insert({u"composer", u""});
    meta_tags().insert({u"lyricist", u""});
    meta_tags().insert({u"translator", u""});
    meta_tags().insert({u"source", u""});
    meta_tags().insert({u"copyright", u""});
    meta_tags().insert({u"creationDate", muse::Date::current_date().to_string(muse::DateFormat::ISO_DATE)});
}

MasterScore::MasterScore(const EngravingServices &services, const MStyle &s, std::weak_ptr<engraving::EngravingProject> project)
    : MasterScore{services, project} {
    set_style(s);
}

MasterScore::~MasterScore() {
    if (project_.lock()) {
        project_.lock()->master_score_ = nullptr;
    }

    delete expanded_repeat_list_;
    delete non_expanded_repeat_list_;
    delete sigmap_;
    delete undo_stack_;
    delete automation_controller_;
    muse::delete_all(excerpts_);
}

//---------------------------------------------------------
//   fileInfo
//---------------------------------------------------------

IFileInfoProviderPtr MasterScore::file_info() const { return file_info_provider_; }

void MasterScore::set_file_info_provider(IFileInfoProviderPtr file_info_provider) { file_info_provider_ = file_info_provider; }

String MasterScore::name() const { return file_info()->display_name(); }

AutomationDataConstPtr MasterScore::automation_data() const { return automation_controller_->automation_data(); }

void MasterScore::set_automation_data(AutomationDataPtr data) { automation_controller_->set_automation_data(std::move(data)); }

void MasterScore::edit_automation_points(const AutomationCurveKey &key, AutomationPointEdits &edits, bool undoable) {
    IF_ASSERT_FAILED(key.is_valid() && !edits.empty()) { return; }

    if (undoable) {
        undo(new EditAutomationPoints(this, automation_controller_, key, edits));
    } else {
        automation_controller_->edit_points(key, edits);
    }
}

void MasterScore::on_time_inserted(const Fraction &tick, const Fraction &len, const std::vector<RepeatSegmentInfo> &old_segments) {
    automation_controller_->insert_time(tick, len, old_segments);
}

//---------------------------------------------------------
//   invalidateRepeatLists
//---------------------------------------------------------

void MasterScore::invalidate_repeat_list() {
    expanded_repeat_list_->set_score_changed();
    non_expanded_repeat_list_->set_score_changed();
}

//---------------------------------------------------------
//   setExpandRepeats
//---------------------------------------------------------

void MasterScore::set_expand_repeats(bool expand) {
    if (expand_repeats_ == expand) {
        return;
    }
    expand_repeats_ = expand;
    invalidate_repeat_list();
}

//---------------------------------------------------------
//   updateRepeatListTempo
///   needed for usage in Seq::processMessages
//---------------------------------------------------------

void MasterScore::update_repeat_list_tempo() {
    expanded_repeat_list_->update_uticks();
    non_expanded_repeat_list_->update_uticks();
}

void MasterScore::update_repeat_list() {
    expanded_repeat_list_->update(true);
    non_expanded_repeat_list_->update(false);
}

//---------------------------------------------------------
//   repeatList
//---------------------------------------------------------

const RepeatList &MasterScore::repeat_list() const {
    if (expand_repeats_) {
        expanded_repeat_list_->update(true);
        return *expanded_repeat_list_;
    }

    non_expanded_repeat_list_->update(false);
    return *non_expanded_repeat_list_;
}

const RepeatList &MasterScore::repeat_list(bool expand_repeats, bool update_ties) const {
    if (expand_repeats) {
        expanded_repeat_list_->update(true, update_ties);
        return *expanded_repeat_list_;
    }

    non_expanded_repeat_list_->update(false, update_ties);
    return *non_expanded_repeat_list_;
}

const TempoTimeline &MasterScore::tempo_timeline() const { return tempo_timeline(master_score()->expand_repeats()); }

const TempoTimeline &MasterScore::tempo_timeline(bool expand_repeats) const {
    automation_controller_->ensure_initialized(const_cast<MasterScore *>(this));
    return automation_controller_->tempo_timeline(expand_repeats);
}

bool MasterScore::set_tempo_multiplier(BeatsPerSecond val) {
    IF_ASSERT_FAILED(val > BeatsPerSecond(0.0)) { return false; }

    if (automation_controller_->tempo_timeline().tempo_multiplier() == val) {
        return false;
    }

    automation_controller_->set_tempo_multiplier(val);
    return true;
}

void MasterScore::set_tempo_timeline_override(std::optional<TempoTimeline> timeline) {
    automation_controller_->set_tempo_timeline_override(std::move(timeline));
}

//---------------------------------------------------------
//   addExcerpt
//---------------------------------------------------------

void MasterScore::add_excerpt(Excerpt *ex, size_t index) {
    if (!ex->inited()) {
        init_parts(ex);
    }

    excerpts().insert(excerpts().begin() + (index == muse::nidx ? excerpts().size() : index), ex);
    set_excerpts_changed(true);
}

//---------------------------------------------------------
//   removeExcerpt
//---------------------------------------------------------

void MasterScore::remove_excerpt(Excerpt *ex) {
    if (muse::remove(excerpts(), ex)) {
        set_excerpts_changed(true);
        // delete ex;
    } else {
        LOGD("removeExcerpt:: ex not found");
    }
}

//---------------------------------------------------------
//   clone
//---------------------------------------------------------

MasterScore *MasterScore::clone() {
    auto buffer = Buffer::opened(IODevice::WRITE_ONLY);

    rw::RWRegister::writer()->write_score(this, &buffer);

    buffer.close();

    muse::ByteArray score_data = buffer.data();
    MasterScore *score = new MasterScore(services(), style(), project_);

    XmlReader r(score_data);
    MscLoader().read_master_score(score, r, true);

    score->do_layout();
    return score;
}

Score *MasterScore::create_score() { return new Score(this, DefaultStyle::base_style()); }

Score *MasterScore::create_score(const MStyle &s) { return new Score(this, s); }

//---------------------------------------------------------
//   setPos
//---------------------------------------------------------

Fraction MasterScore::loop_boundary_tick(LoopBoundaryType type) const {
    IF_ASSERT_FAILED(type != LoopBoundaryType::UNKNOWN) { return Fraction(); }
    return loop_boundaries_[size_t(type)];
}

void MasterScore::set_loop_boundary_tick(LoopBoundaryType type, Fraction tick) {
    IF_ASSERT_FAILED(type != LoopBoundaryType::UNKNOWN) { return; }
    if (tick < Fraction(0, 1)) {
        tick = Fraction(0, 1);
    }
    if (tick > last_measure()->end_tick()) {
        // End Reverb may last longer than written notation, but cursor position should not
        tick = last_measure()->end_tick();
    }

    loop_boundaries_[size_t(type)] = tick;
}

void MasterScore::add_changes_listener(IScoreChangesListener *listener) {
    if (std::find(changes_listeners_.begin(), changes_listeners_.end(), listener) == changes_listeners_.end()) {
        changes_listeners_.push_back(listener);
    }
}

void MasterScore::remove_changes_listener(IScoreChangesListener *listener) {
    changes_listeners_.erase(std::remove(changes_listeners_.begin(), changes_listeners_.end(), listener), changes_listeners_.end());
}

void MasterScore::notify_changes(const ScoreChanges &changes) {
    // A listener may detach while it handles the changes.
    const std::vector<IScoreChangesListener *> listeners = changes_listeners_;
    for (IScoreChangesListener *listener : listeners) {
        if (std::find(changes_listeners_.begin(), changes_listeners_.end(), listener) != changes_listeners_.end()) {
            listener->on_score_changes(changes);
        }
    }
}

//---------------------------------------------------------
//   setUpdateAll
//---------------------------------------------------------

void MasterScore::set_update_all() { cmd_state_.set_update_mode(UpdateMode::UPDATE_ALL); }

//---------------------------------------------------------
//   setLayoutAll
//---------------------------------------------------------

void MasterScore::set_layout_all(staff_idx_t staff, const EngravingItem *e) {
    cmd_state_.set_tick(Fraction(0, 1));
    cmd_state_.set_tick(Fraction::max());

    if (e && e->score() == this) {
        // TODO: map staff number properly
        const staff_idx_t start_staff = staff == muse::nidx ? 0 : staff;
        const staff_idx_t end_staff = staff == muse::nidx ? (nstaves() - 1) : staff;
        cmd_state_.set_staff(start_staff);
        cmd_state_.set_staff(end_staff);

        cmd_state_.set_element(e);
    }
}

//---------------------------------------------------------
//   setLayout
//---------------------------------------------------------

void MasterScore::set_layout(const Fraction &t, staff_idx_t staff, const EngravingItem *e) {
    if (t >= Fraction(0, 1)) {
        cmd_state_.set_tick(t);
    }

    if (e && e->score() == this) {
        // TODO: map staff number properly
        cmd_state_.set_staff(staff);
        cmd_state_.set_element(e);
    }
}

void MasterScore::set_layout(const Fraction &tick1, const Fraction &tick2, staff_idx_t staff1, staff_idx_t staff2, const EngravingItem *e) {
    if (tick1 >= Fraction(0, 1)) {
        cmd_state_.set_tick(tick1);
    }
    if (tick2 >= Fraction(0, 1)) {
        cmd_state_.set_tick(tick2);
    }

    if (e && e->score() == this) {
        // TODO: map staff number properly
        cmd_state_.set_staff(staff1);
        cmd_state_.set_staff(staff2);

        cmd_state_.set_element(e);
    }
}

void MasterScore::init_automation() {
    IF_ASSERT_FAILED(automation_controller_) { return; }
    automation_controller_->init(this);
}

void MasterScore::update_automation(const ScoreChanges &changes) { automation_controller_->update(changes); }

//---------------------------------------------------------
//   setPlaybackScore
//---------------------------------------------------------

void MasterScore::set_playback_score(Score *score) {
    if (playback_score_ == score) {
        return;
    }

    playback_score_ = score;
    playback_settings_links_.clear();

    if (!playback_score_) {
        return;
    }

    for (Part *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            Instrument *instr = pair.second;
            for (InstrChannel *ch : instr->channel()) {
                InstrChannel *p_channel = playback_channel(ch);
                IF_ASSERT_FAILED(p_channel) { continue; }
                playback_settings_links_.emplace_back(p_channel, ch, /* excerpt */ true);
            }
        }
    }
}

//---------------------------------------------------------
//   updateExpressive
//    change patches to their expressive equivalent or vica versa, if possible
//    This works only with MuseScore general soundfont
//
//    The first version of the function decides whether to make patches expressive
//    or not, based on the synth settings. The second will switch patches based on
//    the value of the expressive parameter.
//---------------------------------------------------------

void MasterScore::update_expressive(Synthesizer *synth) {
    SynthesizerState s = synthesizer_state();
    SynthesizerGroup g = s.group(u"master");

    int method = 1;
    for (const IdValue &id_val : g) {
        if (id_val.id == 4) {
            method = id_val.data.to_int();
            break;
        }
    }

    update_expressive(synth, (method != 0));
}

void MasterScore::update_expressive(Synthesizer *synth, bool expressive, bool force /* = false */) {
    if (!synth) {
        return;
    }

    if (!force) {
        SynthesizerState s = synthesizer_state();
        SynthesizerGroup g = s.group(u"master");

        for (const IdValue &id_val : g) {
            if (id_val.id == 4) {
                int method = id_val.data.to_int();
                if (expressive == (method == 0)) {
                    return; // method and expression change don't match, so don't switch
                }
            }
        }
    }

    for (Part *p : parts()) {
        for (const auto &pair : p->instruments()) {
            pair.second->switch_expressive(this, synth, expressive, force);
        }
    }
}

//---------------------------------------------------------
//   rebuildAndUpdateExpressive
//    implicitly rebuild midi mappings as well. Should be preferred over
//    just updateExpressive, in most cases.
//---------------------------------------------------------

void MasterScore::rebuild_and_update_expressive(Synthesizer *synth) {
    // Rebuild midi mappings to make sure we have playback channels
    rebuild_midi_mapping();

    update_expressive(synth);

    // Rebuild midi mappings again to be safe
    rebuild_midi_mapping();
}

//---------------------------------------------------------
//   insertMeasure
//    Create a new MeasureBase of Measure type and insert
//    before measure.
//    If measure is zero, append new MeasureBase.
//---------------------------------------------------------

MeasureBase *MasterScore::insert_measure(MeasureBase *before_measure, const InsertMeasureOptions &options) {
    Fraction tick;
    if (before_measure) {
        if (before_measure->is_measure()) {
            if (to_measure(before_measure)->is_mm_rest()) {
                before_measure = to_measure(before_measure)->prev();
                before_measure = before_measure ? before_measure->next() : first_measure();
                deselect_all();
            }
            for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
                if (to_measure(before_measure)->is_measure_repeat_group_with_prev_m(staff_idx)) {
                    MScore::set_error(MsError::CANNOT_SPLIT_MEASURE_REPEAT);
                    return nullptr;
                }
            }
        }
        tick = before_measure->tick();
    } else {
        tick = last() ? last()->end_tick() : Fraction(0, 1);
    }

    const bool is_beginning = tick.is_zero();

    // Use nominal time signature of current or previous measure, depending on whether
    // the time sig from `beforeMeasure` will be moved to the newly created measure
    const Fraction current_time_sig =
        sigmap()
            ->timesig(!options.move_signatures_clef && !is_beginning && before_measure && before_measure->prev_measure() ?
                          before_measure->prev_measure()->tick() :
                          tick)
            .nominal();

    Measure *master_measure = nullptr;
    Fraction ticks = {0, 1};

    for (Score *score : score_list()) {
        MeasureBase *actual_before_measure = nullptr;

        if (before_measure) {
            if (before_measure->score() == score) {
                actual_before_measure = before_measure;
            } else if (!before_measure->is_measure() && before_measure->links()) {
                for (EngravingObject *m : *before_measure->links()) {
                    if (m && m->is_measure_base() && m->score() == score) {
                        actual_before_measure = to_measure_base(m);
                        break;
                    }
                }
            }
            // if beforeMeasure is measure; or if din't find linked frame, use measure
            if (!actual_before_measure) {
                actual_before_measure = score->tick2measure(tick);
            }

            if (!actual_before_measure) {
                LOGD("measure not found");
            }
        }

        Measure *new_measure = Factory::create_measure(score);
        new_measure->set_tick(tick);

        if (actual_before_measure) {
            actual_before_measure = actual_before_measure->top(); // don't try to insert in front of nested frame
        }
        new_measure->set_next(actual_before_measure);
        new_measure->set_prev(actual_before_measure ? actual_before_measure->prev() : score->last());

        new_measure->set_timesig(current_time_sig);
        new_measure->set_ticks(current_time_sig);

        undo(new InsertMeasures(new_measure, new_measure, options.move_staff_type_changes));

        ticks = new_measure->ticks();
        Measure *measure_insert = nullptr; // insert before
        if (actual_before_measure) {
            if (actual_before_measure->is_measure()) {
                measure_insert = to_measure(actual_before_measure);
            } else {
                measure_insert = score->tick2measure(actual_before_measure->tick());
            }
        }

        if (score->is_master()) {
            master_measure = new_measure;
        }

        std::vector<TimeSig *> time_sig_list;
        std::vector<KeySig *> key_sig_list;
        std::vector<Clef *> init_clef_list;
        std::vector<Clef *> previous_clef_list;
        std::vector<Clef *> special_case_clefs;
        std::vector<Clef *> after_barline_clefs;
        std::vector<BarLine *> previous_bar_lines_list;

        Measure *pm = new_measure->prev_measure();

        //
        // remove clefs, barlines, time and key signatures
        //
        bool header_key_sig = false;
        if (measure_insert) {
            // if inserting before first measure, always preserve clefs and signatures
            // at the begining of the score (move them back)

            if (measure_insert->has_mm_rest() && score->style().value(Sid::CREATE_MULTI_MEASURE_RESTS).to_bool()) {
                measure_insert = measure_insert->mm_rest();
            }

            if (pm && !options.move_signatures_clef && !is_beginning) {
                Segment *ps = pm->find_segment(SegmentType::CLEF, tick);
                if (ps && ps->enabled()) {
                    for (size_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
                        EngravingItem *pc = ps->element(staff_idx * ::mu::engraving::voices);
                        if (pc) {
                            previous_clef_list.push_back(to_clef(pc));
                            do_undo_remove_element(pc);
                            pc->undo_unlink();
                            if (ps->empty()) {
                                undo_remove_element(ps);
                            }
                        }
                    }
                }
            }

            if (options.move_signatures_clef || is_beginning) {
                for (size_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
                    for (Segment *s = measure_insert->first(); s && s->rtick().is_zero(); s = s->next()) {
                        if (!s->enabled()) {
                            continue;
                        }
                        EngravingItem *e = s->element(staff_idx * ::mu::engraving::voices);
                        bool special_case = false;
                        bool init_clef = false;
                        bool move_clef = false;
                        bool move_other = false;
                        if (e && e->is_clef()) {
                            if (s->is_header_clef_type()) {
                                // if it's a header clef, we only add if it's an init clef
                                // (other header clefs are handled by undo(new InsertMeasures())
                                init_clef = is_beginning && !to_clef(e)->for_instrument_change();
                                move_clef = init_clef;
                            } else {
                                ClefToBarlinePosition clef_pos = to_clef(e)->clef_to_barline_position();
                                if (is_beginning) {
                                    // special case:
                                    // there is a non-header clef at global tick 0, and we are inserting at the beginning of the score.
                                    // this clef will be moved with the measure it accompanies, but it will be moved before the barline.
                                    special_case = true;
                                    move_clef = true;
                                } else if (clef_pos == ClefToBarlinePosition::AFTER) {
                                    // non header clef at the begining of the measure
                                    move_clef = true;
                                }
                            }
                        } else {
                            // otherwise, we only add if e is not generated
                            move_other = e && (!e->generated() || tick.is_zero());
                        }
                        if (!move_clef && !move_other) {
                            continue; // this item will remain with the old measure
                        }
                        // otherwise, this item is moved back to the new measure
                        EngravingItem *ee = 0;
                        if (e->is_key_sig()) {
                            KeySig *ks = to_key_sig(e);
                            if (ks->for_instrument_change()) {
                                continue;
                            }
                            key_sig_list.push_back(ks);
                            // if instrument change on that place, set correct key signature for instrument change
                            const TrackRange track_range = e->part()->track_range();
                            bool ic =
                                s->next(SegmentType::CHORD_REST)
                                    ->find_annotation(ElementType::INSTRUMENT_CHANGE, track_range.start_track, track_range.end_track - 1);
                            if (ic) {
                                KeySigEvent ke = ks->key_sig_event();
                                ke.set_for_instrument_change(true);
                                EditKeySig::undo_change_key_sig(transaction_manager()->current_or_dummy_transaction(), this, ks->staff(),
                                                                e->tick(), ke);
                            } else {
                                ee = e;
                            }
                            if (s->header()) {
                                header_key_sig = true;
                            }
                        } else if (e->is_time_sig()) {
                            TimeSig *ts = to_time_sig(e);
                            time_sig_list.push_back(ts);
                            ee = e;
                        } else if (e->is_clef()) {
                            Clef *clef = to_clef(e);
                            if (special_case) {
                                special_case_clefs.push_back(clef);
                            } else if (init_clef) {
                                init_clef_list.push_back(clef);
                            } else {
                                after_barline_clefs.push_back(to_clef(e));
                            }
                            ee = e;
                        }
                        if (ee) {
                            do_undo_remove_element(ee);
                            ee->undo_unlink();
                            if (s->empty() && s->is_time_sig_type()) {
                                undo_remove_element(s);
                            }
                        }
                    }
                }

                if (master_measure && measure_insert->repeat_start()) {
                    master_measure->undo_change_property(Pid::REPEAT_START, true);
                    measure_insert->undo_change_property(Pid::REPEAT_START, false);
                }
            }
        } else if (!measure_insert && tick == Fraction(0, 1)) {
            // If inserting measure into an empty score, restore default C key signature
            // and 4/4 time signature
            score->restore_initial_key_sig_and_time_sig();
        }

        if (pm && !options.move_signatures_clef && !options.ignore_bar_lines) {
            Segment *pbs = pm->find_segment(SegmentType::END_BAR_LINE, tick);
            if (pbs && pbs->enabled()) {
                for (size_t staff_idx = 0; staff_idx < score->nstaves(); ++staff_idx) {
                    EngravingItem *pb = pbs->element(staff_idx * ::mu::engraving::voices);
                    if (pb && !pb->generated()) {
                        previous_bar_lines_list.push_back(to_bar_line(pb));
                        do_undo_remove_element(pb);
                        if (pbs->empty()) {
                            pbs->set_enabled(false);
                        }
                    }
                }
            }
            if (master_measure && pm->repeat_end()) {
                master_measure->undo_change_property(Pid::REPEAT_END, true);
                pm->undo_change_property(Pid::REPEAT_END, false);
            }
        }

        //
        // move clef, barline, time, key signatures
        //
        for (TimeSig *ts : time_sig_list) {
            TimeSig *nts = Factory::copy_time_sig(*ts);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::TIME_SIG, Fraction(0, 1));
            nts->set_ownership_parent(s);
            do_undo_add_element(nts);
        }
        for (KeySig *ks : key_sig_list) {
            KeySig *nks = Factory::copy_key_sig(*ks);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::KEY_SIG, Fraction(0, 1));
            if (header_key_sig || new_measure->tick().is_zero()) {
                s->set_header(true);
            }
            nks->set_ownership_parent(s);
            if (!nks->is_atonal()) {
                nks->set_key(nks->concert_key()); // to set correct (transposing) key
            }
            do_undo_add_element(nks);
        }
        for (Clef *clef : init_clef_list) {
            Clef *n_clef = Factory::copy_clef(*clef);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::HEADER_CLEF, Fraction(0, 1));
            s->set_header(true);
            n_clef->set_ownership_parent(s);
            do_undo_add_element(n_clef);
        }
        for (Clef *clef : after_barline_clefs) {
            Clef *n_clef = Factory::copy_clef(*clef);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::CLEF, Fraction(0, 1));
            s->set_header(true);
            n_clef->set_ownership_parent(s);
            do_undo_add_element(n_clef);
        }
        for (Clef *clef : previous_clef_list) {
            Clef *n_clef = Factory::copy_clef(*clef);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::CLEF, new_measure->ticks());
            n_clef->set_ownership_parent(s);
            do_undo_add_element(n_clef);
        }
        for (Clef *clef : special_case_clefs) {
            Clef *n_clef = Factory::copy_clef(*clef);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::CLEF, new_measure->ticks());
            n_clef->set_ownership_parent(s);
            do_undo_add_element(n_clef);
        }
        for (BarLine *bar_line : previous_bar_lines_list) {
            BarLine *n_bar_line = Factory::copy_bar_line(*bar_line);
            Segment *s = new_measure->undo_get_segment_r(SegmentType::END_BAR_LINE, new_measure->ticks());
            n_bar_line->set_ownership_parent(s);
            do_undo_add_element(n_bar_line);
        }
    }

    undo_insert_time(tick, ticks);

    if (master_measure && !options.create_measure_rests) {
        //
        // fill measure with rest
        // undoAddCR adds rest to linked staves as well
        for (size_t staff_idx = 0; staff_idx < nstaves(); ++staff_idx) {
            size_t track = staff_idx * ::mu::engraving::voices;
            Rest *rest = Factory::create_rest(dummy(), TDuration(DurationType::V_MEASURE));
            Fraction time_stretch(staff(staff_idx)->time_stretch(master_measure->tick()));
            rest->set_ticks(master_measure->ticks() * time_stretch);
            rest->set_track(track);
            undo_add_cr(rest, master_measure, tick);
        }
    }

    if (options.need_deselect_all) {
        deselect_all();
    }

    return master_measure;
}
