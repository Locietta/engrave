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

#include "excerpt.h"

#include "../editing/addremoveelement.h"
#include "../editing/editexcerpt.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"
#include "barline.h"
#include "beam.h"
#include "box.h"
#include "capo.h"
#include "chord.h"
#include "containers.h"
#include "factory.h"
#include "fret.h"
#include "guitarbend.h"
#include "harmony.h"
#include "layoutbreak.h"
#include "linkedobjects.h"
#include "log.h"
#include "lyrics.h"
#include "masterscore.h"
#include "measure.h"
#include "note.h"
#include "ornament.h"
#include "page.h"
#include "part.h"
#include "partialtie.h"
#include "rest.h"
#include "score.h"
#include "segment.h"
#include "staff.h"
#include "stafftype.h"
#include "style/style.h"
#include "system.h"
#include "text.h"
#include "textline.h"
#include "tie.h"
#include "tiemap.h"
#include "tremolotwochord.h"
#include "tuplet.h"
#include "tupletmap.h"
#include "utils.h"

using namespace mu;
using namespace mu::engraving;

Excerpt::Excerpt(const Excerpt &ex, bool copy_contents)
    : master_score_(ex.master_score_), name_(ex.name_), parts_(ex.parts_), initial_part_id_(ex.initial_part_id_) {
    if (copy_contents) {
        tracks_mapping_ = ex.tracks_mapping_;
        excerpt_score_ = ex.excerpt_score_ ? ex.excerpt_score_->clone() : nullptr;

        if (excerpt_score_) {
            excerpt_score_->set_excerpt(this);
        }
    }
}

Excerpt::~Excerpt() { delete excerpt_score_; }

bool Excerpt::inited() const { return inited_; }

void Excerpt::set_inited(bool inited) { inited_ = inited; }

bool Excerpt::custom() const { return !initial_part_id_.is_valid(); }

void Excerpt::mark_as_custom() { set_initial_part_id(ID()); }

const ID &Excerpt::initial_part_id() const { return initial_part_id_; }

void Excerpt::set_initial_part_id(const ID &id) { initial_part_id_ = id; }

void Excerpt::set_excerpt_score(Score *s) {
    excerpt_score_ = s;

    if (s) {
        s->set_excerpt(this);
        write_name_to_meta_tags();
    }
}

const String &Excerpt::name() const { return name_; }

void Excerpt::set_name(const String &name, bool save_and_notify) {
    if (name_ == name) {
        return;
    }

    name_ = name;

    if (save_and_notify) {
        write_name_to_meta_tags();
    }
}

void Excerpt::write_name_to_meta_tags() {
    if (Score *score = excerpt_score()) {
        if (Text *name_item = score->get_text(mu::engraving::TextStyleType::INSTRUMENT_EXCERPT)) {
            name_item->set_plain_text(name_);
            score->set_meta_tag(u"partName", name_);
        }
    }
}

bool Excerpt::has_file_name() const { return !file_name_.empty(); }

const String &Excerpt::file_name() const {
    IF_ASSERT_FAILED(!file_name_.empty()) { const_cast<Excerpt *>(this)->update_file_name(); }

    return file_name_;
}

void Excerpt::set_file_name(const String &file_name) { file_name_ = file_name; }

void Excerpt::update_file_name(size_t index) {
    if (index == muse::nidx && master_score_) {
        index = muse::index_of(master_score_->excerpts(), this);
    }

    const String escaped_name = muse::io::escape_file_name(name_).to_string();

    if (index == muse::nidx) {
        file_name_ = escaped_name;
    } else {
        file_name_ = String(u"%1_%2").arg(String::number(index), escaped_name);
    }
}

bool Excerpt::contains_part(const Part *part) const {
    for (Part *part_value : parts_) {
        if (part_value == part) {
            return true;
        }
    }

    return false;
}

void Excerpt::remove_part(const ID &id) {
    size_t index = 0;
    for (const Part *part : parts()) {
        if (part->id() == id) {
            break;
        }
        ++index;
    }
    if (index >= parts_.size()) {
        return;
    }

    excerpt_score()->undo_remove_part(excerpt_score()->parts().at(index), index);
}

size_t Excerpt::nstaves() const {
    size_t n = 0;
    for (Part *p : parts_) {
        n += p->nstaves();
    }
    return n;
}

bool Excerpt::is_empty() const { return excerpt_score() ? excerpt_score()->parts().empty() : true; }

const TracksMap &Excerpt::tracks_mapping() {
    update_tracks_mapping();

    return tracks_mapping_;
}

void Excerpt::set_tracks_mapping(const TracksMap &tracks_mapping) {
    if (tracks_mapping_ == tracks_mapping) {
        return;
    }

    tracks_mapping_ = tracks_mapping;

    const Score *score = excerpt_score();
    if (!score) {
        return;
    }

    for (Staff *staff : score->staves()) {
        const Staff *master_staff = master_score_->staff_by_id(staff->id());
        if (!master_staff) {
            continue;
        }

        staff->update_visibility_voices(master_staff, tracks_mapping_);
    }
}

void Excerpt::update_tracks_mapping() {
    const Score *score = excerpt_score();
    if (!score) {
        return;
    }

    TracksMap tracks;

    for (const Staff *staff : score->staves()) {
        const Staff *master_staff = master_score()->staff_by_id(staff->id());
        if (!master_staff) {
            continue;
        }

        staff_idx_t master_staff_idx = master_staff->idx();
        staff_idx_t staff_idx = staff->idx();

        voice_idx_t voice = 0;
        const std::array<bool, voices> &voices_visibility = staff->visibility_voices();

        for (voice_idx_t i = 0; i < ::mu::engraving::voices; ++i) {
            if (!voices_visibility[i]) {
                continue;
            }

            tracks.insert({master_staff_idx * ::mu::engraving::voices + i % ::mu::engraving::voices,
                           staff_idx * ::mu::engraving::voices + voice % ::mu::engraving::voices});
            voice++;
        }
    }

    set_tracks_mapping(tracks);
}

void Excerpt::set_voice_visible(Staff *staff, voice_idx_t voice_index, bool visible) {

    if (!staff) {
        return;
    }

    Staff *master_staff = master_score()->staff_by_id(staff->id());
    if (!master_staff) {
        return;
    }

    bool was_visible = staff->is_voice_visible(voice_index);
    if (visible == was_visible) {
        return;
    }

    // update tracks (temporarily modify the old staff)
    staff->set_voice_visible(voice_index, visible);
    update_tracks_mapping();

    // create new staff
    Staff *new_staff = Factory::create_staff(staff->part());
    new_staff->set_id(staff->id());
    new_staff->init(staff);

    // revert changes to old staff, to make undo work properly
    staff->set_voice_visible(voice_index, was_visible);

    // remove old staff, insert new
    staff_idx_t staff_index = staff->idx();
    staff_idx_t part_staff_index = staff_index - excerpt_score()->staff_idx(staff->part());
    excerpt_score()->undo_remove_staff(staff);
    excerpt_score()->undo_insert_staff(new_staff, part_staff_index);

    // clone master staff to new staff with mapped tracks
    Fraction start_tick = staff->score()->first_measure()->tick();
    Fraction end_tick = staff->score()->last_measure()->end_tick();
    clone_staff2(master_staff, new_staff, start_tick, end_tick);

    // link master staff to cloned
    excerpt_score()->undo(new Link(new_staff, master_staff));
}

void Excerpt::create_excerpt(Excerpt *excerpt) {
    MasterScore *master_score = excerpt->master_score();
    Score *score = excerpt->excerpt_score();
    Transaction &tx = master_score->transaction_manager()->current_or_dummy_transaction();

    std::vector<Part *> &parts = excerpt->parts();
    std::vector<staff_idx_t> src_staves;

    score->set_page_number_offset(master_score->page_number_offset());

    // Set instruments and create linked staves
    for (const Part *part : parts) {
        Part *p = new Part(score);
        p->set_id(part->id());
        p->set_instrument(*part->instrument());
        p->set_prefer_sharp_flat(part->prefer_sharp_flat());

        for (Staff *staff : part->staves()) {
            Staff *s = Factory::create_staff(p);
            s->set_id(staff->id());
            s->init(staff);
            s->set_default_clef_type(staff->default_clef_type());
            // the order of staff - s matters as staff should be the first entry in the
            // created link list to make primaryStaff() work
            // TODO: change implementation, maybe create an explicit "primary" flag
            score->undo(new Link(s, staff));
            score->append_staff(s);
            src_staves.push_back(staff->idx());
        }
        score->append_part(p);
    }

    // Fill tracklist (map all tracks of a stave)
    if (excerpt->tracks_mapping().empty()) {
        excerpt->update_tracks_mapping();
    }

    clone_staves(master_score, score, src_staves, excerpt->tracks_mapping());

    MeasureBase *master_measure = master_score->first();
    MeasureBase *score_measure = score->first();

    if ((!score_measure || !score_measure->is_v_box()) && (!master_measure || !master_measure->exclude_from_other_parts())) {
        score->insert_box(ElementType::VBOX, score_measure);
        score_measure = score->first();
    }

    VBox *title_frame_score = score_measure && score_measure->is_v_box() ? to_v_box(score_measure) : nullptr;

    if (title_frame_score && master_measure && master_measure->is_v_box()) {
        VBox *title_frame_master = to_v_box(master_measure);

        title_frame_score->copy_values(title_frame_master);
    }

    String part_label = excerpt->name();
    if (!part_label.empty()) {
        if (title_frame_score) {
            Text *txt = Factory::create_text(title_frame_score, TextStyleType::INSTRUMENT_EXCERPT);
            txt->set_plain_text(part_label);
            title_frame_score->add(txt);
        }

        score->set_meta_tag(u"partName", part_label);
    }

    // initial layout of score
    score->do_layout();

    // handle transposing instruments
    if (master_score->style().style_b(Sid::CONCERT_PITCH) != score->style().style_b(Sid::CONCERT_PITCH)) {
        for (const Staff *staff : score->staves()) {
            if (staff->staff_type(Fraction(0, 1))->group() == StaffGroup::PERCUSSION) {
                continue;
            }

            // if this staff has no transposition, and no instrument changes, we can skip it
            Interval interval = staff->part()->instrument()->transpose(); //tick?
            if (interval.is_zero() && staff->part()->instruments().size() == 1) {
                continue;
            }
            bool flip = false;
            if (master_score->style().style_b(Sid::CONCERT_PITCH)) {
                interval.flip(); // flip the transposition for the original instrument
                flip = true;     // transposeKeys() will flip transposition for each instrument change
            }

            staff_idx_t staff_idx = staff->idx();
            track_idx_t start_track = staff_idx * ::mu::engraving::voices;
            track_idx_t end_track = start_track + ::mu::engraving::voices;

            Fraction end_tick = Fraction(0, 1);
            if (score->last_segment()) {
                end_tick = score->last_segment()->tick();
            }
            Transpose::transpose_keys(tx, score, staff_idx, staff_idx + 1, Fraction(0, 1), end_tick, flip);

            for (auto segment = score->first_segment_mm(SegmentType::CHORD_REST); segment;
                 segment = segment->next1_mm(SegmentType::CHORD_REST)) {
                interval = staff->transpose(segment->tick());
                if (interval.is_zero()) {
                    continue;
                }
                if (master_score->style().style_b(Sid::CONCERT_PITCH)) {
                    interval.flip();
                }

                for (auto e : segment->annotations()) {
                    if (!e->is_harmony() || (e->track() < start_track) || (e->track() >= end_track)) {
                        continue;
                    }
                    Harmony *h = to_harmony(e);
                    // mmrests are on by default in part
                    // if this harmony is attached to an mmrest,
                    // be sure to transpose harmony in underlying measure as well
                    for (EngravingObject *se : h->link_list()) {
                        Harmony *hh = to_harmony(se);
                        // skip links to other staves (including in other scores)
                        if (hh->staff() != h->staff()) {
                            continue;
                        }
                        Transpose::undo_transpose_harmony(tx, hh, interval);
                    }
                }
            }
        }
    }

    // update style values if spatium different for part
    if (master_score->style().spatium() != score->style().spatium()) {
        score->spatium_changed(master_score->style().spatium(), score->style().spatium());
        score->style_changed();
    }

    // second layout of score
    score->invalidate_repeat_list();
    master_score->rebuild_midi_mapping();
    master_score->update_channel();
    score->remap_brackets_and_barlines();

    score->set_layout_all();
    score->do_layout();
}

void MasterScore::delete_excerpt(Excerpt *excerpt) {
    assert(excerpt->master_score() == this);
    Score *part_score = excerpt->excerpt_score();

    if (!part_score) {
        LOGD("deleteExcerpt: no partScore");
        return;
    }

    // unlink the staves in the excerpt
    for (Staff *st : part_score->staves()) {
        bool has_links_in_master = false;
        if (st->links()) {
            for (auto le : *st->links()) {
                if (le->score() == this) {
                    has_links_in_master = true;
                    break;
                }
            }
        }
        if (has_links_in_master) {
            staff_idx_t staff_idx = st->idx();
            // unlink the spanners
            for (auto i = part_score->spanner().begin(); i != part_score->spanner().cend(); ++i) {
                Spanner *sp = i->second;
                if (sp->staff_idx() == staff_idx) {
                    sp->undo_unlink();
                }
            }
            track_idx_t s_track = staff_idx * ::mu::engraving::voices;
            track_idx_t e_track = s_track + ::mu::engraving::voices;
            // unlink elements and annotation
            for (Segment *s = part_score->first_segment_mm(SegmentType::ALL); s; s = s->next1_mm()) {
                for (int track = static_cast<int>(e_track) - 1; track >= static_cast<int>(s_track); --track) {
                    EngravingItem *el = s->element(track);
                    if (el) {
                        el->undo_unlink();
                    }
                }
                for (EngravingItem *e : s->annotations()) {
                    if (e->staff_idx() == staff_idx) {
                        e->undo_unlink();
                    }
                }
            }
            // unlink the staff
            undo(new Unlink(st));
        }
    }
    undo(new RemoveExcerpt(excerpt));
}

void MasterScore::init_and_add_excerpt(Excerpt *excerpt, bool fake_undo) {
    init_excerpt(excerpt);

    auto excerpt_cmd = new AddExcerpt(excerpt);
    if (fake_undo) {
        excerpt_cmd->redo();
    } else {
        excerpt->excerpt_score()->undo(excerpt_cmd);
    }
}

void MasterScore::init_excerpt(Excerpt *excerpt) {
    if (excerpt->inited()) {
        excerpt->excerpt_score()->do_layout();
        return;
    }

    Score *score = new Score(master_score());
    excerpt->set_excerpt_score(score);
    score->style().set(Sid::CREATE_MULTI_MEASURE_RESTS, true);
    init_parts(excerpt);

    Excerpt::create_excerpt(excerpt);
    excerpt->set_inited(true);

    if (!score->style().is_default(Sid::TIME_SIG_PLACEMENT)) {
        score->reset_style_value(Sid::TIME_SIG_PLACEMENT);
    }
}

void MasterScore::init_parts(Excerpt *excerpt) {
    int nstaves{1}; // Initialise to 1 to force writing of the first part.
    std::set<ID> assigned_staves_ids;
    for (Staff *excerpt_staff : excerpt->excerpt_score()->staves()) {
        const LinkedObjects *ls = excerpt_staff->links();
        if (ls == 0) {
            continue;
        }

        for (auto le : *ls) {
            if (le->score() != this) {
                continue;
            }

            Staff *linked_master_staff = to_staff(le);
            if (muse::contains(assigned_staves_ids, linked_master_staff->id())) {
                continue;
            }

            Part *excerpt_part = excerpt_staff->part();
            Part *master_part = linked_master_staff->part();

            //! NOTE: parts/staves of excerpt must have the same ID as parts/staves of the master score
            //! In fact, excerpts are just viewers for the master score
            excerpt_staff->set_id(linked_master_staff->id());
            excerpt_part->set_id(master_part->id());

            assigned_staves_ids.insert(linked_master_staff->id());

            // For instruments with multiple staves, every staff will point to the
            // same part. To prevent adding the same part several times to the excerpt,
            // add only the part of the first staff pointing to the part.
            if (!(--nstaves)) {
                excerpt->parts().push_back(linked_master_staff->part());
                nstaves = static_cast<int>(linked_master_staff->part()->nstaves());
            }
            break;
        }
    }

    if (excerpt->tracks_mapping().empty()) { // SHOULDN'T HAPPEN, protected in the UI, but it happens during read-in!!!
        excerpt->update_tracks_mapping();
    }
}

void MasterScore::init_empty_excerpt(Excerpt *excerpt) {
    if (excerpt->inited()) {
        return;
    }

    Excerpt::clone_measures(this, excerpt->excerpt_score());
    excerpt->set_inited(true);
}

static bool score_contains_spanner(const Score *score, Spanner *spanner) {
    const std::multimap<int, Spanner *> &spanners = score->spanner();

    for (auto it = spanners.cbegin(); it != spanners.cend(); ++it) {
        const Spanner *cur_spanner = it->second;
        if (cur_spanner->links() && cur_spanner->links()->contains(spanner)) {
            return true;
        }
    }

    return false;
}

void Excerpt::clone_spanner(Spanner *s, Score *score, track_idx_t dst_track, track_idx_t dst_track2) {
    // don’t clone system lines for track != 0
    if (is_system_text_line(s) && s->track() != 0) {
        return;
    }

    Spanner *ns = to_spanner(s->linked_clone());
    ns->set_score(score);
    ns->move_to_dummy();
    ns->set_track(dst_track);
    ns->set_track2(dst_track2);

    if (ns->is_slur()) {
        // set start/end element for slur
        ChordRest *cr1 = s->start_cr();
        ChordRest *cr2 = s->end_cr();

        ns->set_start_element(0);
        ns->set_end_element(0);
        if (cr1 && cr1->links()) {
            for (EngravingObject *e : *cr1->links()) {
                ChordRest *cr = to_chord_rest(e);
                if (cr == cr1) {
                    continue;
                }
                if ((cr->score() == score) && (cr->tick() == ns->tick()) && cr->track() == dst_track) {
                    ns->set_start_element(cr);
                    break;
                }
            }
        }
        if (cr2 && cr2->links()) {
            for (EngravingObject *e : *cr2->links()) {
                ChordRest *cr = to_chord_rest(e);
                if (cr == cr2) {
                    continue;
                }
                if ((cr->score() == score) && (cr->tick() == ns->tick2()) && cr->track() == dst_track2) {
                    ns->set_end_element(cr);
                    break;
                }
            }
        }
        if (!ns->start_element()) {
            LOGD("clone Slur: no start element");
        }
        if (!ns->end_element()) {
            LOGD("clone Slur: no end element");
        }
    } else if (ns->is_trill()) {
        ns->compute_start_element();
    } else {
        if (!ns->start_element()) {
            ns->compute_start_element();
        }
        if (!ns->end_element()) {
            ns->compute_end_element();
        }
    }

    if (!ns->start_element() || !ns->end_element()) {
        if (EngravingItem *start_element = ns->start_element()) {
            if (start_element->is_chord()) {
                to_chord(start_element)->remove_starting_spanner(ns);
            }
        }
        if (EngravingItem *end_element = ns->end_element()) {
            if (end_element->is_chord()) {
                to_chord(end_element)->remove_ending_spanner(ns);
            }
        }
        LOGD() << "No start or end element, can't add spanner: " << ns->tick().to_string();
        delete ns;
        return;
    }

    score->do_undo_add_element(ns);
    ns->style_changed();
}

static void update_spatium(EngravingItem *old_element, EngravingItem *new_element) {
    double old_spatium = old_element->spatium();
    double new_spatium = new_element->spatium();
    if (!muse::real_is_equal(old_spatium, new_spatium)) {
        new_element->spatium_changed(old_spatium, new_spatium);
    }
}

static void clone_tuplets(ChordRest *ocr, ChordRest *ncr, Tuplet *ot, TupletMap &tuplet_map, Measure *nm, track_idx_t track) {
    const auto handle_tuplet = [&](Tuplet *tuplet) {
        tuplet->clear();
        tuplet->set_track(track);
        tuplet->set_ownership_parent(nm);
        tuplet->style_changed();
        tuplet->scan_elements([&](EngravingItem *new_element) { update_spatium(ot, new_element); });
    };

    ot->set_track(ocr->track());

    Tuplet *nt = tuplet_map.find_new(ot);
    if (!nt) {
        nt = to_tuplet(ot->linked_clone());
        handle_tuplet(nt);
        tuplet_map.add(ot, nt);

        Tuplet *nt1 = nt;
        while (ot->tuplet()) {
            Tuplet *nt2 = tuplet_map.find_new(ot->tuplet());
            if (nt2 == 0) {
                nt2 = to_tuplet(ot->tuplet()->linked_clone());
                handle_tuplet(nt2);
                tuplet_map.add(ot->tuplet(), nt2);
            }
            nt2->add(nt1);
            nt1->set_tuplet(nt2);
            ot = ot->tuplet();
            nt1 = nt2;
        }
    }
    nt->add(ncr);
    ncr->set_tuplet(nt);
}

static void process_linked_clone(EngravingItem *ne, Score *score, track_idx_t strack) {
    ne->set_track(strack == muse::nidx ? 0 : strack);
    ne->set_score(score);
}

static void add_ties(Note *original_note, Note *new_note, TieMap &tie_map, Score *score) {
    if (original_note->tie_for() && original_note->tie_for()->type() == ElementType::TIE) {
        Tie *tie = to_tie(original_note->tie_for()->linked_clone());
        tie->set_score(score);
        new_note->set_tie_for(tie);
        tie->set_start_note(new_note);
        tie->set_track(new_note->track());
        tie_map.add(original_note->tie_for(), tie);
    }
    if (original_note->tie_back() && original_note->tie_back()->type() == ElementType::TIE) {
        Tie *tie = tie_map.find_new(original_note->tie_back());
        if (tie) {
            new_note->set_tie_back(tie);
            tie->set_end_note(new_note);
            tie->set_track2(new_note->track());
        } else {
            LOGD("addTiesToMap: cannot find tie");
        }
    }

    if (original_note->outgoing_partial_tie()) {
        tie_map.add(original_note->outgoing_partial_tie(), new_note->outgoing_partial_tie());
    }
}

static void add_back_spanners(Note *on, Note *nn, Score *score) {
    // add back spanners (going back from end to start spanner element
    // makes sure the 'other' spanner anchor element is already set up)
    // 'on' is the old spanner end note and 'nn' is the new spanner end note
    for (Spanner *old_sp : on->spanner_back()) {
        Note *new_start = Spanner::start_element_from_spanner(old_sp, nn);
        if (new_start != nullptr) {
            Spanner *new_sp = to_spanner(old_sp->linked_clone());
            new_sp->set_note_span(new_start, nn);
            score->add_element(new_sp);
        } else {
            LOGD("cloneStaff2: cannot find spanner start note");
        }
    }
}

static void add_grace_note_ties_and_back_spanners(GraceNotesGroup &original_grace_notes, Chord *new_chord, TieMap &tie_map, Score *score) {
    for (Chord *old_grace : original_grace_notes) {
        Chord *new_grace = new_chord->grace_note_at(old_grace->grace_index());
        if (!new_grace) {
            continue;
        }

        size_t notes = old_grace->notes().size();
        for (size_t i = 0; i < notes; ++i) {
            Note *original_note = old_grace->notes().at(i);
            Note *new_note = new_grace->notes().at(i);

            add_ties(original_note, new_note, tie_map, score);
            add_back_spanners(original_note, new_note, score);
        }
    }
}

static void add_tremolo_two_chord(Chord *old_chord, Chord *new_chord, TremoloTwoChord *&prev_tremolo) {
    if (!new_chord || !old_chord || !new_chord->tremolo_two_chord()) {
        return;
    }

    if (old_chord == old_chord->tremolo_two_chord()->chord1()) {
        prev_tremolo = new_chord->tremolo_two_chord();
    } else if (old_chord == old_chord->tremolo_two_chord()->chord2()) {
        if (!prev_tremolo) {
            LOGD("first note for two note tremolo missing");
        } else {
            prev_tremolo->set_chords(prev_tremolo->chord1(), new_chord);
            new_chord->set_tremolo_two_chord(prev_tremolo);
        }
    } else {
        LOGD("inconsistent two note tremolo");
    }
}

static void collect_tie_end_points(TieMap &tie_map) {
    for (auto &tie : tie_map) {
        Tie *new_tie = to_tie(tie.second);
        if (new_tie->type() == ElementType::TIE ||
            (new_tie->type() == ElementType::PARTIAL_TIE && to_partial_tie(new_tie)->is_outgoing())) {
            new_tie->update_possible_jump_points();
        }
    }
}

static void transpose_harmony(Harmony *harmony, const Staff *src_staff, bool score_concert_pitch) {
    Interval interval = src_staff->transpose(harmony->tick());
    if (interval.is_zero() && src_staff->part()->instruments().size() == 1) {
        return;
    }

    if (!score_concert_pitch) {
        interval.flip();
    }
    Transpose::do_undo_transpose_harmony(harmony, interval);
}

static MeasureBase *clone_measure(MeasureBase *mb, Score *score, const Score *oscore, const std::vector<staff_idx_t> &source_staves_indexes,
                                  const TracksMap &track_list, TieMap &tie_map) {
    MeasureBase *nmb = nullptr;

    if (mb->is_h_box()) {
        nmb = Factory::create_h_box(score);
        nmb->set_tick(mb->tick());
        nmb->set_ticks(mb->ticks());
    } else if (mb->is_v_box()) {
        if (to_box(mb)->is_title_frame()) {
            nmb = Factory::create_title_v_box(score);
        } else {
            nmb = Factory::create_v_box(score);
        }
        nmb->set_tick(mb->tick());
        nmb->set_ticks(mb->ticks());
    } else if (mb->is_f_box()) {
        nmb = Factory::create_f_box(score);
        nmb->set_tick(mb->tick());
        nmb->set_ticks(mb->ticks());
    } else if (mb->is_t_box()) {
        nmb = Factory::create_t_box(score);
        nmb->set_tick(mb->tick());
        nmb->set_ticks(mb->ticks());
        Text *text = to_t_box(mb)->text();
        EngravingItem *ne = text->linked_clone();
        ne->set_score(score);
        nmb->add(ne);
    } else if (mb->is_measure()) {
        const Measure *m = to_measure(mb);
        Measure *nm = Factory::create_measure(score);
        nmb = nm;
        nm->set_tick(m->tick());
        nm->set_ticks(m->ticks());
        nm->set_timesig(m->timesig());

        nm->set_repeat_count(m->repeat_count());
        nm->set_repeat_start(m->repeat_start());
        nm->set_repeat_end(m->repeat_end());
        nm->set_repeat_jump(m->repeat_jump());

        nm->set_exclude_from_numbering(m->exclude_from_numbering());
        nm->set_measure_number(m->measure_number());
        nm->set_measure_number_offset(m->measure_number_offset());
        nm->set_break_multi_measure_rest(m->break_multi_measure_rest());

        for (staff_idx_t dst_staff_idx = 0; dst_staff_idx < source_staves_indexes.size(); ++dst_staff_idx) {
            nm->set_staff_stemless(dst_staff_idx, m->stemless(source_staves_indexes[dst_staff_idx]));
        }
        size_t tracks = oscore->nstaves() * ::mu::engraving::voices;

        if (source_staves_indexes.empty() && track_list.empty()) {
            tracks = 0;
        }

        for (track_idx_t src_track = 0; src_track < tracks; ++src_track) {
            TupletMap tuplet_map; // tuplets cannot cross measure boundaries

            track_idx_t strack = muse::value(track_list, src_track, muse::nidx);

            TremoloTwoChord *prev_tremolo = nullptr;
            for (Segment *oseg = m->first(); oseg; oseg = oseg->next()) {
                Segment *ns = nullptr; //create segment later, on demand
                for (EngravingItem *e : oseg->annotations()) {
                    if (e->generated() || e->exclude_from_other_parts()) {
                        continue;
                    }
                    if ((e->track() == src_track && strack != muse::nidx && !e->system_flag()) ||
                        (e->system_flag() && src_track == 0 && e->track() == src_track)) {
                        EngravingItem *ne = e->linked_clone();
                        process_linked_clone(ne, score, strack);
                        if (!ns) {
                            ns = nm->get_segment(oseg->segment_type(), oseg->tick());
                        }
                        ns->add(ne);
                        // for chord symbols,
                        // re-render with new style settings
                        if (ne->is_fret_diagram()) {
                            Harmony *h = to_harmony(to_fret_diagram(ne)->harmony());
                            if (h) {
                                process_linked_clone(h, score, strack);
                            }
                        }
                    }
                }

                //If track is not mapped skip the following
                if (muse::value(track_list, src_track, muse::nidx) == muse::nidx) {
                    continue;
                }

                // TimeSig and KeySig announce should never be cloned
                if (oseg->is_time_sig_announce_type() || oseg->is_key_sig_announce_type()) {
                    continue;
                }

                //There are probably more destination tracks for the same source
                std::vector<track_idx_t> t = muse::values(track_list, src_track);

                for (track_idx_t track : t) {
                    //Clone KeySig TimeSig and Clefs if voice 1 of source staff is not mapped to a track
                    EngravingItem *oef = oseg->element(track_zero_voice(src_track));
                    if (oef && !oef->generated() && (oef->is_time_sig() || oef->is_key_sig()) &&
                        !(track_list.size() == (score->excerpt()->nstaves() * ::mu::engraving::voices))) {
                        EngravingItem *ne = oef->linked_clone();
                        ne->set_track(track_zero_voice(track));
                        ne->set_score(score);
                        ns = nm->get_segment(oseg->segment_type(), oseg->tick());
                        ns->add(ne);
                    }

                    EngravingItem *oe = oseg->element(src_track);
                    bool clone = oe && !oe->generated() && !oe->exclude_from_other_parts();

                    if (clone) {
                        EngravingItem *ne;
                        oe->set_generated(false);
                        ne = oe->linked_clone();
                        ne->set_track(track);

                        if (!(ne->track() % ::mu::engraving::voices) && ne->is_rest()) {
                            to_rest(ne)->set_gap(false);
                        }

                        ne->set_score(score);
                        if (oe->is_chord_rest()) {
                            ChordRest *ocr = to_chord_rest(oe);
                            ChordRest *ncr = to_chord_rest(ne);

                            if (ocr->beam() && !ocr->beam()->empty() && ocr->beam()->elements().front() == ocr) {
                                Beam *nb = ocr->beam()->clone();
                                nb->clear();
                                nb->set_track(track);
                                nb->set_score(score);
                                nb->add(ncr);
                                ncr->set_beam(nb);
                            }

                            Tuplet *ot = ocr->tuplet();

                            if (ot) {
                                clone_tuplets(ocr, ncr, ot, tuplet_map, nm, track);
                            }

                            if (oe->is_chord()) {
                                Chord *och = to_chord(ocr);
                                Chord *nch = to_chord(ncr);
                                add_grace_note_ties_and_back_spanners(och->grace_notes_before(), nch, tie_map, score);
                                size_t n = och->notes().size();
                                for (size_t i = 0; i < n; ++i) {
                                    Note *on = och->notes().at(i);
                                    Note *nn = to_note(on->find_linked_in_staff(nch->staff()));
                                    if (!nn) {
                                        continue;
                                    }

                                    add_ties(on, nn, tie_map, score);
                                    // add back spanners (going back from end to start spanner element
                                    // makes sure the 'other' spanner anchor element is already set up)
                                    // 'on' is the old spanner end note and 'nn' is the new spanner end note
                                    for (Spanner *old_sp : on->spanner_back()) {
                                        if (old_sp->start_element() && old_sp->end_element() &&
                                            old_sp->start_element()->find_measure() == old_sp->end_element()->find_measure() &&
                                            old_sp->start_element()->track() > old_sp->end_element()->track()) {
                                            continue;
                                        }
                                        Note *new_start = Spanner::start_element_from_spanner(old_sp, nn);
                                        if (new_start != nullptr) {
                                            Spanner *new_sp = to_spanner(old_sp->linked_clone());
                                            new_sp->set_note_span(new_start, nn);
                                            score->add_element(new_sp);
                                        } else {
                                            LOGD("cloneStaves: cannot find spanner start note");
                                        }
                                    }
                                    for (Spanner *old_sp : on->spanner_for()) {
                                        if (old_sp->start_element() && old_sp->end_element() &&
                                            (old_sp->start_element()->find_measure() != old_sp->end_element()->find_measure() ||
                                             old_sp->start_element()->track() <= old_sp->end_element()->track())) {
                                            continue;
                                        }
                                        Note *new_end = Spanner::end_element_from_spanner(old_sp, nn);
                                        if (new_end != nullptr) {
                                            Spanner *new_sp = to_spanner(old_sp->linked_clone());
                                            new_sp->set_note_span(nn, new_end);
                                            score->add_element(new_sp);
                                        } else {
                                            LOGD("cloneStaves: cannot find spanner end note");
                                        }
                                    }
                                }
                                add_grace_note_ties_and_back_spanners(och->grace_notes_after(), nch, tie_map, score);
                                add_tremolo_two_chord(och, nch, prev_tremolo);
                            }
                        }
                        if (!ns) {
                            ns = nm->get_segment(oseg->segment_type(), oseg->tick());
                        }
                        ns->add(ne);
                    }

                    Segment *tst = nm->segments().first_cr_segment();
                    if (src_track % ::mu::engraving::voices && !(track % ::mu::engraving::voices) && (!tst || (!tst->element(track)))) {
                        Segment *segment = nm->get_segment(SegmentType::CHORD_REST, nm->tick());
                        Rest *rest = Factory::create_rest(segment);
                        rest->set_ticks(nm->ticks());
                        rest->set_duration_type(nm->ticks());
                        rest->set_track(track);
                        segment->add(rest);
                    }
                }
            }
        }
    }

    score->undo(new Link(nmb, mb));
    nmb->set_exclude_from_other_parts(false);

    for (EngravingItem *e : mb->el()) {
        if (e->is_layout_break()) {
            LayoutBreakType st = to_layout_break(e)->layout_break_type();
            if (st != LayoutBreakType::SECTION) {
                continue;
            }
        }
        track_idx_t track = muse::nidx;
        if (e->track() != muse::nidx) {
            // try to map track
            track = muse::value(track_list, e->track(), muse::nidx);
            if (e->system_flag() && !e->is_top_system_object()) {
                continue;
            }
            if (track == muse::nidx) {
                // even if track not in excerpt, we need to clone system elements
                if (e->system_flag() && e->track() == 0) {
                    track = 0;
                } else if (e->is_fret_diagram() && to_fret_diagram(e)->is_in_fret_box()) {
                    track = e->track();
                } else {
                    continue;
                }
            }
        }

        EngravingItem *ne;
        // link text - title, subtitle, also repeats (eg, coda/segno)
        // measure numbers are not stored in this list, but they should not be cloned anyhow
        // layout breaks other than section were skipped above,
        // but section breaks do need to be cloned & linked
        // other measure-attached elements (?) are cloned but not linked
        if (e->is_text() && to_text(e)->text_style_type() == TextStyleType::INSTRUMENT_EXCERPT) {
            // skip part name in score
            continue;
        } else if (e->is_text_base() || e->is_layout_break()) {
            ne = e->linked_clone();
            ne->set_autoplace(true);
        } else {
            ne = e->clone();
        }
        ne->set_score(score);
        ne->set_track(track);
        nmb->add(ne);
    }

    return nmb;
}

void Excerpt::clone_mm_rests(Score *source_score, Score *dst_score, const std::vector<staff_idx_t> &source_staves_indexes,
                             const TracksMap &track_list, TieMap &tie_map) {
    // When MMRests are enabled in the main score and we are creating a new part or revealing a hidden stave within one, we must copy MMRests
    // Elements are moved from underlying measures to MMRest measures. If we do not copy these MMRests, the elements attached to them will be lost
    // It doesn't matter if the MMRest range is different in the part to the score. As long as the MMRest measure is present in the DOM, MMRestLayout will
    // correct this, setting the appropriate length and moving the elements to the correct locations

    for (Measure *src_m = source_score->first_measure(); src_m; src_m = src_m->next_measure()) {
        if (!src_m->has_mm_rest()) {
            continue;
        }

        Measure *dst_m = dst_score->tick2measure(src_m->tick());
        if (!dst_m) {
            continue;
        }

        Measure *src_mm_rest = src_m->mm_rest();

        Measure *dst_mm_rest = to_measure(clone_measure(src_mm_rest, dst_score, source_score, source_staves_indexes, track_list, tie_map));
        dst_mm_rest->set_mm_rest_count(src_mm_rest->mm_rest_count());
        dst_mm_rest->set_prev(dst_m->prev());
        dst_mm_rest->set_next(dst_score->tick2measure(src_mm_rest->tick() + src_mm_rest->ticks()));

        dst_m->set_mm_rest(dst_mm_rest);
    }
}

void Excerpt::clone_staves(Score *source_score, Score *dst_score, const std::vector<staff_idx_t> &source_staves_indexes,
                           const TracksMap &track_list) {
    MeasureBaseList *measures = dst_score->measures();
    TieMap tie_map;

    for (MeasureBase *mb = source_score->measures()->first(); mb; mb = mb->next()) {
        if (mb->exclude_from_other_parts()) {
            // if excluded measure contains section break, add it to precedent measure in part
            if (LayoutBreak *section_break = mb->section_break_element()) {
                if (MeasureBase *prev_mb = measures->last()) {
                    EngravingItem *new_section_break = section_break->linked_clone();
                    new_section_break->set_score(dst_score);
                    new_section_break->set_ownership_parent(prev_mb);
                    dst_score->do_undo_add_element(new_section_break);
                }
            }
            continue;
        }
        MeasureBase *new_measure = clone_measure(mb, dst_score, source_score, source_staves_indexes, track_list, tie_map);
        measures->append(new_measure);
    }

    clone_mm_rests(source_score, dst_score, source_staves_indexes, track_list, tie_map);

    size_t n = source_staves_indexes.size();
    for (staff_idx_t dst_staff_idx = 0; dst_staff_idx < n; ++dst_staff_idx) {
        Staff *src_staff = source_score->staff(source_staves_indexes[dst_staff_idx]);

        Measure *m = source_score->first_measure();
        Measure *nm = dst_score->first_measure();

        while (m && nm) {
            nm->set_measure_repeat_count(m->measure_repeat_count(src_staff->idx()), dst_staff_idx);
            m = m->next_measure();
            nm = nm->next_measure();
        }
    }

    if (dst_score->no_staves()) {
        return;
    }

    for (auto i : source_score->spanner()) {
        Spanner *s = i.second;
        if (s->exclude_from_other_parts()) {
            continue;
        }

        track_idx_t dst_track = muse::nidx;
        track_idx_t dst_track2 = muse::nidx;

        if (is_system_text_line(s)) {
            //always export voltas to first staff in part
            dst_track = 0;
            dst_track2 = 0;
            clone_spanner(s, dst_score, dst_track, dst_track2);
        } else {
            if (muse::value(track_list, s->track(), muse::nidx) == muse::nidx ||
                muse::value(track_list, s->track2(), muse::nidx) == muse::nidx) {
                continue;
            }
            std::vector<track_idx_t> track1 = muse::values(track_list, s->track());
            std::vector<track_idx_t> track2 = muse::values(track_list, s->track2());

            if (track1.size() != track2.size()) {
                continue;
            }

            //export other spanner if staffidx matches
            for (auto it1 = track1.begin(), it2 = track2.begin(); it1 != track1.end(); ++it1, ++it2) {
                dst_track = *it1;
                dst_track2 = *it2;
                clone_spanner(s, dst_score, dst_track, dst_track2);
            }
        }
    }
    collect_tie_end_points(tie_map);
}

void Excerpt::clone_measures(Score *oscore, Score *score) {
    MeasureBaseList *measures = score->measures();
    TieMap tie_map;

    for (MeasureBase *mb = oscore->first_measure(); mb; mb = mb->next()) {
        MeasureBase *new_measure = clone_measure(mb, score, oscore, {}, {}, tie_map);
        measures->append(new_measure);
    }

    clone_mm_rests(oscore, score, {}, {}, tie_map);

    collect_tie_end_points(tie_map);
}

void Excerpt::link_measures(Score *excerpt_score, Score *master_score) {
    MeasureBase *mb_master = master_score->first();
    for (MeasureBase *mb = excerpt_score->first(); mb; mb = mb->next()) {
        if (!mb->is_measure()) {
            continue;
        }
        while (mb_master && !mb_master->is_measure()) {
            mb_master = mb_master->next();
        }
        if (!mb_master) {
            LOGD("Measures in MasterScore and Score are not in sync.");
            break;
        }
        mb->link_to(mb_master);
        mb_master = mb_master->next();
    }
}

//! NOTE For staves in the same score
void Excerpt::clone_staff(Staff *src_staff, Staff *dst_staff, bool clone_spanners) {
    Score *score = src_staff->score();
    TieMap tie_map;

    score->undo(new Link(dst_staff, src_staff));

    staff_idx_t src_staff_idx = src_staff->idx();
    staff_idx_t dst_staff_idx = dst_staff->idx();

    for (Measure *m = score->first_measure(); m; m = m->next_measure()) {
        m->set_measure_repeat_count(m->measure_repeat_count(src_staff_idx), dst_staff_idx);

        track_idx_t s_track = src_staff_idx * ::mu::engraving::voices;
        track_idx_t e_track = s_track + ::mu::engraving::voices;
        for (track_idx_t src_track = s_track; src_track < e_track; ++src_track) {
            TupletMap tuplet_map; // tuplets cannot cross measure boundaries
            track_idx_t dst_track = dst_staff_idx * ::mu::engraving::voices + (src_track - s_track);
            TremoloTwoChord *prev_tremolo = nullptr;
            for (Segment *seg = m->first(); seg; seg = seg->next()) {
                EngravingItem *oe = seg->element(src_track);
                if (oe == 0 || oe->generated()) {
                    continue;
                }
                if (oe->is_time_sig()) {
                    continue;
                }
                EngravingItem *ne = 0;
                if (oe->is_clef()) {
                    // only clone clef if it matches staff group and does not exists yet
                    Clef *clef = to_clef(oe);
                    Fraction tick = seg->tick();
                    if (ClefInfo::staff_group(clef->concert_clef()) == dst_staff->const_staff_type(Fraction(0, 1))->group() &&
                        dst_staff->clef_type(tick) != clef->clef_type_list()) {
                        ne = oe->clone();
                    }
                } else {
                    ne = oe->linked_clone();
                }
                if (ne) {
                    ne->set_track(dst_track);
                    ne->set_ownership_parent(seg);
                    ne->set_score(score);
                    if (ne->is_chord_rest()) {
                        ChordRest *ncr = to_chord_rest(ne);
                        if (ncr->tuplet()) {
                            ncr->set_tuplet(0); //TODO nested tuplets
                        }
                    }
                    score->undo_add_element(ne);
                }
                if (oe->is_chord_rest()) {
                    ChordRest *ocr = to_chord_rest(oe);
                    ChordRest *ncr = to_chord_rest(ne);
                    Tuplet *ot = ocr->tuplet();
                    if (ot) {
                        clone_tuplets(ocr, ncr, ot, tuplet_map, m, dst_track);
                    }

                    // remove lyrics from chord
                    // since only one set of lyrics is used with linked staves
                    for (Lyrics *l : ncr->lyrics()) {
                        if (l) {
                            l->unlink();
                        }
                    }
                    muse::delete_all(ncr->lyrics());
                    ncr->lyrics().clear();

                    ncr->check_staff_move_validity();
                    // creating copy for iteration, cause seg->annotations() may change during loop
                    const std::vector<EngravingItem *> iterable_annotations = seg->annotations();

                    for (EngravingItem *e : iterable_annotations) {
                        if (!e) {
                            LOGD("cloneStaff: corrupted annotation found.");
                            continue;
                        }
                        if (e->generated() || e->system_flag()) {
                            continue;
                        }
                        if (e->track() != src_track) {
                            continue;
                        }
                        switch (e->type()) {
                            // exclude certain element types
                            // this should be same list excluded in Score::undoAddElement()
                            case ElementType::STAFF_TEXT:
                            case ElementType::SYSTEM_TEXT:
                            case ElementType::TRIPLET_FEEL:
                            case ElementType::PLAYTECH_ANNOTATION:
                            case ElementType::STRING_TUNINGS:
                            case ElementType::FRET_DIAGRAM:
                            case ElementType::HARMONY:
                            case ElementType::FIGURED_BASS:
                            case ElementType::DYNAMIC:
                            case ElementType::INSTRUMENT_CHANGE:
                            case ElementType::LYRICS: // not normally segment-attached
                                continue;
                            case ElementType::CAPO: {
                                const Capo *capo = to_capo(e);
                                dst_staff->insert_capo_params(e->tick(), capo->params(), true);
                                continue;
                            }
                            case ElementType::FERMATA: {
                                // Fermatas are special since the belong to a segment but should
                                // be created and linked on each staff.
                                EngravingItem *ne1 = e->linked_clone();
                                ne1->set_track(dst_track);
                                ne1->set_ownership_parent(seg);
                                ne1->set_score(score);
                                score->do_undo_add_element(ne1);
                                continue;
                            }
                            default:
                                EngravingItem *ne1 = e->clone();
                                ne1->set_track(dst_track);
                                ne1->set_ownership_parent(seg);
                                ne1->set_score(score);
                                score->undo_add_element(ne1);
                        }
                    }
                    if (oe->is_chord()) {
                        Chord *och = to_chord(ocr);
                        Chord *nch = to_chord(ncr);
                        add_grace_note_ties_and_back_spanners(och->grace_notes_before(), nch, tie_map, score);
                        size_t n = och->notes().size();
                        for (size_t i = 0; i < n; ++i) {
                            Note *on = och->notes().at(i);
                            Note *nn = to_note(on->find_linked_in_staff(dst_staff));
                            if (!nn) {
                                continue;
                            }
                            add_ties(on, nn, tie_map, score);
                            // add back spanners (going back from end to start spanner element
                            // makes sure the 'other' spanner anchor element is already set up)
                            // 'on' is the old spanner end note and 'nn' is the new spanner end note
                            for (Spanner *old_sp : on->spanner_back()) {
                                Note *new_start = Spanner::start_element_from_spanner(old_sp, nn);
                                if (new_start != nullptr) {
                                    Spanner *new_sp = to_spanner(old_sp->linked_clone());
                                    new_sp->set_note_span(new_start, nn);
                                    score->add_element(new_sp);
                                } else {
                                    LOGD("cloneStaff: cannot find spanner start note");
                                }
                            }
                        }
                        add_grace_note_ties_and_back_spanners(och->grace_notes_after(), nch, tie_map, score);
                        add_tremolo_two_chord(och, nch, prev_tremolo);

                        // Check grace note staff move validity
                        for (Chord *gn : nch->grace_notes()) {
                            gn->check_staff_move_validity();
                        }
                        if (Ornament *o = nch->find_ornament()) {
                            Chord *cue_chord = o->cue_note_chord();
                            if (cue_chord) {
                                cue_chord->check_staff_move_validity();
                            }
                        }
                    }
                }
            }
        }
    }

    if (clone_spanners) {
        for (auto i : score->spanner()) {
            Spanner *s = i.second;
            staff_idx_t staff_idx = s->staff_idx();
            track_idx_t dst_track = muse::nidx;
            track_idx_t dst_track2 = muse::nidx;
            if (!is_system_text_line(s)) {
                //export other spanner if staffidx matches
                if (src_staff_idx == staff_idx) {
                    dst_track = dst_staff_idx * ::mu::engraving::voices + s->voice();
                    dst_track2 = dst_staff_idx * ::mu::engraving::voices + (s->track2() % ::mu::engraving::voices);
                }
            }
            if (dst_track == muse::nidx) {
                continue;
            }
            clone_spanner(s, score, dst_track, dst_track2);
        }
    }

    collect_tie_end_points(tie_map);
}

//! NOTE For staves potentially in different scores
void Excerpt::clone_staff2(Staff *src_staff, Staff *dst_staff, const Fraction &start_tick, const Fraction &end_tick) {
    Score *oscore = src_staff->score();
    Score *score = dst_staff->score();

    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    Excerpt *oex = oscore->excerpt();
    Excerpt *ex = score->excerpt();
    TracksMap otracks, tracks;
    if (oex) {
        otracks = oex->tracks_mapping();
    }
    if (ex) {
        tracks = ex->tracks_mapping();
    }

    Measure *m1 = oscore->tick2measure(start_tick);
    Measure *m2 = oscore->tick2measure(end_tick);

    if (m2->tick() < end_tick) { // end of score
        m2 = 0;
    }

    TieMap tie_map;

    staff_idx_t src_staff_idx = src_staff->idx();
    staff_idx_t dst_staff_idx = dst_staff->idx();

    track_idx_t s_track = src_staff_idx * ::mu::engraving::voices;
    track_idx_t e_track = s_track + ::mu::engraving::voices;

    std::map<track_idx_t, track_idx_t> map;
    for (track_idx_t i = s_track; i < e_track; i++) {
        if (!oex && !ex) {
            map.insert({i, dst_staff_idx * ::mu::engraving::voices + i % ::mu::engraving::voices});
        } else if (oex && !ex) {
            track_idx_t k = muse::key(otracks, i, muse::nidx);
            if (k != muse::nidx) {
                map.insert({i, k});
            }
        } else if (!oex && ex) {
            for (track_idx_t j : muse::values(tracks, i)) {
                if (dst_staff_idx * ::mu::engraving::voices <= j && j < (dst_staff_idx + 1) * ::mu::engraving::voices) {
                    map.insert({i, j});
                    break;
                }
            }
        } else if (oex && ex) {
            track_idx_t k = muse::key(otracks, i, muse::nidx);
            if (k != muse::nidx) {
                for (track_idx_t j : muse::values(tracks, k)) {
                    if (dst_staff_idx * ::mu::engraving::voices <= j && j < (dst_staff_idx + 1) * ::mu::engraving::voices) {
                        map.insert({i, j});
                        break;
                    }
                }
            }
        }
    }

    if (map.empty()) {
        for (track_idx_t i = s_track; i < e_track; i++) {
            map.insert({i, dst_staff_idx * ::mu::engraving::voices + i % ::mu::engraving::voices});
        }
    }

    bool first_voice_visible = dst_staff->is_voice_visible(0);

    const bool oscore_concert_pitch = oscore->style().style_b(Sid::CONCERT_PITCH);
    const bool score_concert_pitch = score->style().style_b(Sid::CONCERT_PITCH);
    const bool needs_transposition = oscore_concert_pitch != score_concert_pitch;

    auto add_element = [score](EngravingItem *element) { score->undo_add_element(element, false /*addToLinkedStaves*/); };

    Selection &sel = score->selection();
    Fraction range_start;
    Fraction range_end;
    if (sel.is_range()) {
        // The for-loop below will invalidate the current range start/end segments. Save their
        // ticks and restore them later using tick2measure (see below)...
        range_start = sel.tick_start();
        range_end = sel.tick_end();
    }

    for (Measure *m = m1; m && (m != m2); m = m->next_measure()) {
        Measure *nm = score->tick2measure(m->tick());
        nm->set_measure_repeat_count(m->measure_repeat_count(src_staff_idx), dst_staff_idx);

        for (EngravingItem *old_el : m->el()) {
            if (old_el->is_layout_break()) {
                continue;
            }
            if ((old_el->system_flag() && dst_staff_idx != 0) || (!old_el->system_flag() && old_el->staff_idx() != src_staff_idx)) {
                continue;
            }
            bool already_cloned = old_el->system_flag() && old_el->find_linked_in_score(score);
            bool clone_element = !already_cloned && old_el->element_applies_to_track(staff2track(dst_staff_idx));
            if (!clone_element) {
                continue;
            }
            EngravingItem *new_el = old_el->linked_clone();
            new_el->set_ownership_parent(nm);
            new_el->set_staff_idx(old_el->system_flag() ? 0 : dst_staff_idx);
            new_el->set_score(score);
            new_el->style_changed();
            add_element(new_el);
        }

        TremoloTwoChord *prev_tremolo = nullptr;
        for (const auto &[src_track, dst_track] : map) {
            TupletMap tuplet_map; // tuplets cannot cross measure boundaries
            for (Segment *oseg = m->first(); oseg; oseg = oseg->next()) {
                Segment *ns = nm->get_segment(oseg->segment_type(), oseg->tick());
                EngravingItem *oef = oseg->element(track_zero_voice(src_track));
                if (oef && !oef->generated() && (oef->is_time_sig() || oef->is_key_sig())) {
                    if (!first_voice_visible) {
                        EngravingItem *ne = oef->linked_clone();
                        ne->set_track(track_zero_voice(dst_track));
                        ne->set_ownership_parent(ns);
                        ne->set_score(score);
                        ne->style_changed();
                        ne->scan_elements([&](EngravingItem *new_element) { update_spatium(oef, new_element); });
                        add_element(ne);
                    }
                }

                for (EngravingItem *e : oseg->annotations()) {
                    if (e->generated() || e->exclude_from_other_parts()) {
                        continue;
                    }
                    bool system_object = e->system_flag() && e->track() == 0;
                    EngravingItem *linked_element = e->find_linked_in_score(score);
                    Segment *linked_parent = linked_element ? to_segment(linked_element->parent()) : nullptr;
                    bool already_cloned = linked_parent && (linked_parent == ns ||
                                                            (linked_parent->is_type(SegmentType::DURATION) &&
                                                             ns->is_type(SegmentType::DURATION) && linked_parent->tick() == ns->tick()));
                    bool clone_annotation = !already_cloned && (e->element_applies_to_track(src_track) || system_object);

                    if (!clone_annotation) {
                        continue;
                    }
                    EngravingItem *ne1 = e->linked_clone();
                    ne1->set_track(dst_track);
                    ne1->set_ownership_parent(ns);
                    ne1->set_score(score);
                    ne1->style_changed();
                    add_element(ne1);

                    if (e->is_harmony() && needs_transposition) {
                        transpose_harmony(to_harmony(ne1), src_staff, score_concert_pitch);
                    }
                }

                EngravingItem *oe = oseg->element(src_track);
                if (oe == 0 || oe->generated() || oe->exclude_from_other_parts()) {
                    continue;
                }

                oe->set_generated(false);
                EngravingItem *ne = oe->linked_clone();
                ne->set_track(dst_track);
                ne->set_ownership_parent(ns);
                ne->set_score(score);
                ne->style_changed();
                ne->scan_elements([&](EngravingItem *new_element) { update_spatium(oe, new_element); });
                add_element(ne);

                if (!oe->is_chord_rest()) {
                    continue;
                }

                ChordRest *ocr = to_chord_rest(oe);
                ChordRest *ncr = to_chord_rest(ne);
                Tuplet *ot = ocr->tuplet();
                if (ot) {
                    clone_tuplets(ocr, ncr, ot, tuplet_map, nm, dst_track);
                }

                if (!oe->is_chord()) {
                    continue;
                }

                Chord *och = to_chord(ocr);
                Chord *nch = to_chord(ncr);
                add_grace_note_ties_and_back_spanners(och->grace_notes_before(), nch, tie_map, score);
                size_t n = och->notes().size();
                for (size_t i = 0; i < n; ++i) {
                    Note *on = och->notes().at(i);
                    Note *nn = to_note(on->find_linked_in_staff(dst_staff));
                    if (!nn) {
                        continue;
                    }
                    add_ties(on, nn, tie_map, score);
                    add_back_spanners(on, nn, score);
                    GuitarBend *bend_back = on->bend_back();
                    Note *new_start_note = bend_back ? to_note(bend_back->start_note()->find_linked_in_staff(dst_staff)) : nullptr;
                    if (bend_back && new_start_note) {
                        GuitarBend *new_bend = to_guitar_bend(bend_back->linked_clone());
                        new_bend->set_score(score);
                        new_bend->set_ownership_parent(new_start_note);
                        new_bend->set_track(new_start_note->track());
                        new_bend->set_track2(nn->track());
                        new_bend->set_start_element(new_start_note);
                        new_bend->set_end_element(nn);
                        new_start_note->add_spanner_for(new_bend);
                        nn->add_spanner_back(new_bend);
                    }
                    GuitarBend *bend_for = on->bend_for();
                    if (bend_for && bend_for->bend_type() == GuitarBendType::SLIGHT_BEND) {
                        // Because slight bends aren't detected as "bendBack"
                        GuitarBend *new_bend = to_guitar_bend(bend_for->linked_clone());
                        new_bend->set_score(score);
                        new_bend->set_ownership_parent(nn);
                        new_bend->set_track(nn->track());
                        new_bend->set_track2(nn->track());
                        new_bend->set_start_element(nn);
                        new_bend->set_end_element(nn);
                        nn->add_spanner_for(new_bend);
                    }
                }
                add_grace_note_ties_and_back_spanners(och->grace_notes_after(), nch, tie_map, score);
                add_tremolo_two_chord(och, nch, prev_tremolo);
            }
        }
        std::vector<Segment *> empty_segments;
        for (Segment &seg : nm->segments()) {
            seg.check_empty();
            if (seg.empty()) {
                empty_segments.push_back(&seg);
            }
        }
        for (Segment *seg : empty_segments) {
            nm->remove(seg);
            delete seg;
        }
        if (!nm->has_voices(dst_staff_idx, nm->tick(), nm->ticks())) {
            promote_gap_rests_to_real_rests(nm, dst_staff_idx);
        }
    }

    for (auto i : oscore->spanner()) {
        Spanner *s = i.second;
        if (!(s->tick() >= start_tick && s->tick2() <= end_tick) || s->exclude_from_other_parts()) {
            continue;
        }

        staff_idx_t staff_idx = s->staff_idx();
        track_idx_t dst_track = muse::nidx;
        track_idx_t dst_track2 = muse::nidx;

        if (is_system_text_line(s)) {
            if (!score_contains_spanner(score, s)) {
                dst_track = s->track();
                dst_track2 = s->track2();
            }
        } else {
            // export other spanner if staffidx matches
            if (src_staff_idx == staff_idx) {
                if (muse::contains(map, s->track())) {
                    dst_track = map.at(s->track());
                }
                if (muse::contains(map, s->track2())) {
                    dst_track2 = map.at(s->track2());
                }
            }
        }

        if (dst_track == muse::nidx) {
            continue;
        }

        clone_spanner(s, score, dst_track, dst_track2);
    }

    if (needs_transposition) {
        Interval interval = src_staff->part()->instrument()->transpose();
        if (!interval.is_zero() || src_staff->part()->instruments().size() != 1) {
            if (!score_concert_pitch) {
                interval.flip();
            }

            Transpose::transpose_keys(tx, score, dst_staff_idx, dst_staff_idx + 1, start_tick, end_tick, !score_concert_pitch);
        }
    }

    collect_tie_end_points(tie_map);

    // Restore the range selection...
    if (range_start.is_valid()) {
        Segment *new_start = score->tick2segment(range_start);
        sel.set_start_segment(new_start);
    }

    if (range_end.is_valid()) {
        Segment *new_end = score->tick2segment(range_end);
        sel.set_end_segment(new_end);
    }
}

void Excerpt::promote_gap_rests_to_real_rests(const Measure *measure, staff_idx_t staff_idx) {
    track_idx_t start_track = staff2track(staff_idx);
    track_idx_t end_track = start_track + ::mu::engraving::voices;
    for (const Segment &seg : measure->segments()) {
        if (!seg.is_chord_rest_type()) {
            continue;
        }
        for (track_idx_t track = start_track; track < end_track; ++track) {
            EngravingItem *item = seg.element(track);
            if (item && item->is_rest() && to_rest(item)->is_gap()) {
                to_rest(item)->set_gap(false);
            }
        }
    }
}

std::vector<Excerpt *> Excerpt::create_excerpts_from_parts(const std::vector<Part *> &parts, MasterScore *score) {
    StringList all_excerpt_lower_names;
    all_excerpt_lower_names.reserve(score->excerpts().size() + parts.size());
    for (const Excerpt *e : score->excerpts()) {
        all_excerpt_lower_names.push_back(e->name().to_lower());
    }

    std::vector<Excerpt *> result;
    result.reserve(parts.size());

    for (Part *part : parts) {
        Excerpt *excerpt = new Excerpt(score);
        excerpt->parts().push_back(part);

        const TrackRange range = part->track_range();

        for (track_idx_t i = range.start_track, j = 0; i < range.end_track; ++i, ++j) {
            excerpt->tracks_mapping_.insert({i, j});
        }

        String name = format_unique_excerpt_name(part->part_name(), all_excerpt_lower_names);
        excerpt->set_name(name);
        excerpt->set_initial_part_id(part->id());

        all_excerpt_lower_names.push_back(name.to_lower());
        result.push_back(excerpt);
    }

    return result;
}

//---------------------------------------------------------
//   createLinkedTabs
//---------------------------------------------------------

void Excerpt::create_linked_tabs(MasterScore *score) {
    // store map of all initial spanners
    std::unordered_map<staff_idx_t, std::vector<Spanner *>> spanners;
    // for moving initial spanner to new index
    std::unordered_map<staff_idx_t, staff_idx_t> index_mapping;
    std::set<staff_idx_t> staff_indexes_to_copy;
    constexpr size_t staves_in_part = 2;

    for (auto it = score->spanner().cbegin(); it != score->spanner().cend(); ++it) {
        Spanner *s = it->second;
        spanners[s->staff_idx()].push_back(s);
    }

    size_t cur_staff_idx = 0;
    size_t staves_operated = 0;

    // creating linked staves and recalculating spanners indexes
    for (size_t part_num = 0; part_num < score->parts().size(); part_num++) {
        Part *part = score->parts()[part_num];
        Fraction fr = Fraction(0, 1);
        size_t lines = part->instrument()->string_data()->strings();
        size_t staves_num = part->nstaves();

        if (staves_num != 1) {
            for (size_t i = 0; i < staves_num; i++) {
                index_mapping[cur_staff_idx] = staves_operated + i;
                cur_staff_idx++;
            }

            staves_operated += staves_num;
            continue;
        }

        bool needs_tab_staff = !part->staff(0)->is_drum_staff(fr);

        if (needs_tab_staff) {
            part->set_staves(static_cast<int>(staves_in_part));

            Staff *src_staff = part->staff(0);
            Staff *dst_staff = part->staff(1);
            clone_staff(src_staff, dst_staff, false);

            static const std::vector<StaffTypes> types{
                StaffTypes::TAB_4SIMPLE, StaffTypes::TAB_5SIMPLE, StaffTypes::TAB_6SIMPLE,  StaffTypes::TAB_7SIMPLE,
                StaffTypes::TAB_8SIMPLE, StaffTypes::TAB_9SIMPLE, StaffTypes::TAB_10SIMPLE,
            };

            size_t index = (lines >= 4 && lines <= 10) ? lines - 4 : 2;

            dst_staff->set_staff_type(fr, *StaffType::preset(types.at(index)));
            dst_staff->set_lines(fr, static_cast<int>(lines));

            staff_indexes_to_copy.insert(cur_staff_idx);
        }

        // each spanner moves down to the staff with index,
        // equal to number of spanners operated before it
        index_mapping[cur_staff_idx] = staves_operated;
        cur_staff_idx++;

        staves_operated += needs_tab_staff ? staves_in_part : 1;
    }

    // moving and copying spanner segments
    for (auto &spanner_map_elem : spanners) {
        auto &spanner_list = spanner_map_elem.second;
        staff_idx_t idx = spanner_map_elem.first;
        bool needs_copy = staff_indexes_to_copy.find(idx) != staff_indexes_to_copy.end();
        for (Spanner *s : spanner_list) {
            /// moving
            staff_idx_t new_idx = index_mapping[idx];
            track_idx_t new_track_idx = staff2track(new_idx);
            s->set_track(new_track_idx);
            s->set_track2(new_track_idx);
            for (SpannerSegment *ss : s->spanner_segments()) {
                ss->set_track(new_track_idx);
            }

            /// copying
            if (needs_copy) {
                staff_idx_t dst_staff_idx = new_idx + 1;

                track_idx_t dst_track = dst_staff_idx * ::mu::engraving::voices + s->voice();
                track_idx_t dst_track2 = dst_staff_idx * ::mu::engraving::voices + (s->track2() % ::mu::engraving::voices);

                clone_spanner(s, score, dst_track, dst_track2);
            }
        }
    }
}
