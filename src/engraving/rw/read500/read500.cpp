/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited
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
#include "read500.h"

#include "../editing/noteinput.h"
#include "../editing/paste.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"
#include "../types/types.h"

#include "dom/anchors.h"
#include "dom/beam.h"
#include "dom/breath.h"
#include "dom/chord.h"
#include "dom/dynamic.h"
#include "dom/excerpt.h"
#include "dom/factory.h"
#include "dom/figuredbass.h"
#include "dom/fret.h"
#include "dom/harmony.h"
#include "dom/lyrics.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/measurerepeat.h"
#include "dom/note.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/sharedpart.h"
#include "dom/spanner.h"
#include "dom/staff.h"
#include "dom/text.h"
#include "dom/tie.h"
#include "dom/tremolotwochord.h"
#include "dom/tuplet.h"

#include "engravingerrors.h"

#include "../compat/readstyle.h"
#include "../compat/tremolocompat.h"
#include "../inoutdata.h"
#include "staffread.h"
#include "tread.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read500;

muse::Ret Read500::read_score_file(Score *score, XmlReader &e, rw::ReadInOutData *data) {
    ReadContext ctx(score);
    if (data) {
        if (data->overridden_spatium.has_value()) {
            ctx.set_spatium(data->overridden_spatium.value());
        }

        ctx.set_properties_to_skip(data->properties_to_skip);
        ctx.set_force_page_mode(data->force_page_mode);
    }

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "programVersion") {
            String ver = e.read_text();
            if (score->is_master()) {
                score->set_mscore_version(ver);
            }
        } else if (tag == "programRevision") {
            int rev = e.read_int(nullptr, 16);
            if (score->is_master()) {
                score->set_mscore_revision(rev);
            }
        } else if (tag == "Score") {
            if (!read_score_tag(score, e, ctx)) {
                if (e.error() == muse::XmlStreamReader::CUSTOM_ERROR) {
                    return make_ret(Err::FILE_CRITICALLY_CORRUPTED, e.error_string());
                }
                return make_ret(Err::FILE_BAD_FORMAT, e.error_string());
            }
        } else if (tag == "museScore") {
            // pass
        } else {
            e.unknown();
        }
    }

    if (!score->is_master()) {
        Excerpt *ex = score->excerpt();
        ex->set_tracks_mapping(ctx.tracks());
    }

    if (data) {
        data->settings_compat = ctx.setting_compat();
    }

    return muse::make_ok();
}

bool Read500::read_score_tag(Score *score, XmlReader &e, ReadContext &ctx) {
    std::vector<int> sys_staves;
    while (e.read_next_start_element()) {
        ctx.set_track(muse::nidx);
        const AsciiStringView tag(e.name());
        if (tag == "eid") {
            TRead::read_item_eid(score, e, ctx);
        } else if (tag == "Staff") {
            StaffRead::read_staff(score, e, ctx);
        } else if (tag == "Synthesizer") {
            score->synthesizer_state_.read(e);
        } else if (tag == "page-offset") {
            score->page_number_offset_ = e.read_int();
        } else if (tag == "Division") {
            score->file_division_ = e.read_int();
        } else if (tag == "open") {
            score->is_open_ = e.read_bool();
        } else if (tag == "showInvisible") {
            score->show_invisible_ = e.read_int();
        } else if (tag == "showUnprintable") {
            score->show_unprintable_ = e.read_int();
        } else if (tag == "showFrames") {
            score->show_frames_ = e.read_int();
        } else if (tag == "showMargins") {
            score->show_pageborders_ = e.read_int();
        } else if (tag == "showSoundFlags") {
            score->show_sound_flags_ = e.read_int();
        } else if (tag == "markIrregularMeasures") {
            score->mark_irregular_measures_ = e.read_int();
        } else if (tag == "Style") {
            // Since version 400, the Style is usually stored in a separate file,
            // but we also support reading it from the mscx file.
            compat::ReadStyleHook::read_style_tag(score, e);
            score->engraving_font_ =
                score->engraving_fonts()->font_by_name(score->style().style_st(Sid::MUSICAL_SYMBOL_FONT).to_std_string());
        } else if (tag == "copyright" || tag == "rights") {
            score->set_meta_tag(u"copyright", Text::read_xml_text(e, score));
        } else if (tag == "movement-number") {
            score->set_meta_tag(u"movementNumber", e.read_text());
        } else if (tag == "movement-title") {
            score->set_meta_tag(u"movementTitle", e.read_text());
        } else if (tag == "work-number") {
            score->set_meta_tag(u"workNumber", e.read_text());
        } else if (tag == "work-title") {
            score->set_meta_tag(u"workTitle", e.read_text());
        } else if (tag == "source") {
            score->set_meta_tag(u"source", e.read_text());
        } else if (tag == "metaTag") {
            String name = e.attribute("name");
            score->set_meta_tag(name, e.read_text());
        } else if (tag == "Order") {
            ScoreOrder order;
            order.read(e);
            if (order.is_valid()) {
                score->set_score_order(order);
            }
        } else if (tag == "SystemObjects") {
            score->clear_system_object_staves();
            while (e.read_next_start_element()) {
                if (e.name() == "Instance") {
                    int staff_idx = e.attribute("staffId").to_int() - 1;
                    if (staff_idx > 0) {
                        sys_staves.push_back(staff_idx);
                    }
                    e.skip_current_element();
                } else {
                    e.skip_current_element();
                }
            }
        } else if (tag == "PageLocks") {
            TRead::read_page_locks(score, e);
        } else if (tag == "SystemLocks") {
            TRead::read_system_locks(score, e);
        } else if (tag == "SystemDividers") {
            TRead::read_system_dividers(score, e, ctx);
        } else if (tag == "SpannerMap") {
            TRead::read_score_spanners(score, e, ctx);
        } else if (tag == "Part") {
            Part *part = new Part(score);
            TRead::read(part, e, ctx);
            score->append_part(part);
        } else if (tag == "SharedPart") {
            SharedPart *shared_part = new SharedPart(score);
            TRead::read(shared_part, e, ctx);
            score->append_part(shared_part);
        } else if (e.name() == "initialPartId") {
            if (score->excerpt()) {
                score->excerpt()->set_initial_part_id(ID(e.read_int()));
            } else {
                e.skip_current_element();
            }
        } else if (e.name() == "Tracklist") {
            int strack = e.int_attribute("sTrack", -1);
            int dtrack = e.int_attribute("dstTrack", -1);
            if (strack != -1 && dtrack != -1) {
                ctx.tracks().insert({strack, dtrack});
            }
            e.skip_current_element();
        } else if (tag == "Score") {
            // Since version 400, Excerpts are usually stored in separate files,
            // but we also support reading them from the main mscx file.

            ctx.tracks().clear(); // ???
            MasterScore *m = score->master_score();
            Score *s = m->create_score();

            compat::ReadStyleHook::setup_default_style(s);

            Excerpt *ex = new Excerpt(m);
            ex->set_excerpt_score(s);
            ctx.set_last_measure(nullptr);

            Score *cur_score = ctx.score();
            ctx.set_score(s);

            read_score_tag(s, e, ctx); // recursion

            ctx.set_score(cur_score);

            Excerpt::link_measures(s, m);
            ex->set_tracks_mapping(ctx.tracks());
            m->add_excerpt(ex);
        } else if (tag == "name") {
            String n = e.read_text();
            if (!score->is_master()) { //ignore the name if it's not a child score
                score->excerpt()->set_name(n, /*saveAndNotify=*/false);
            }
        } else if (tag == "layoutMode") {
            if (ctx.force_page_mode()) {
                e.skip_current_element();
                continue;
            }
            String s = e.read_text();
            if (s == "line") {
                score->set_layout_mode(LayoutMode::LINE);
            } else if (s == "system") {
                score->set_layout_mode(LayoutMode::SYSTEM);
            } else {
                LOGD("layoutMode: %s", muPrintable(s));
            }
        } else {
            e.unknown();
        }
    }
    ctx.set_mm_rest_end_measures();
    ctx.connect_note_anchored_spanners();
    if (e.error() != muse::XmlStreamReader::NO_ERROR) {
        if (e.error() == muse::XmlStreamReader::CUSTOM_ERROR) {
            LOGE() << e.error_string();
        } else {
            LOGE() << String(u"XML read error at byte offset %1: %2").arg(e.byte_offset()).arg(String::from_ascii(e.name().ascii()));
        }
        return false;
    }

    for (Spanner *sp : score->unmanaged_spanners()) {
        if (sp->is_lyrics_line() && to_lyrics_line(sp)->is_dash()) {
            LyricsLine *line = to_lyrics_line(sp);
            line->set_next_lyrics(
                search_next_lyrics(line->lyrics()->segment(), line->staff_idx(), line->lyrics()->verse(), line->lyrics()->placement()));
            line->set_track2(line->next_lyrics() ? line->next_lyrics()->track() : line->track());
        }
    }

    score->file_division_ = Constants::division;

    // Make sure every instrument has an instrumentId set.
    for (Part *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            pair.second->update_instrument_id();
        }
    }

    score->update_ticks_and_time_sig_map();
    if (score->is_master()) {
        // While reading the score, some elements might use `score->repeatList()` (which is incorrect
        // anyway, because the repeatList will be incomplete because the score is incomplete, but some
        // elements still do it).
        // `score->repeatList()` calls `_repeatList->update()`; the repeat list then thinks that it is
        // up-to-date from that point. But we weren't finished reading the score, so the score will still
        // change. We need to tell the repeat list about that, so that it will be updated next time
        // someone uses it.
        static_cast<MasterScore *>(score)->invalidate_repeat_list();
    }
    score->connect_ties();
    score->undo_remove_stale_tie_jump_points(false);

    for (Part *p : score->parts()) {
        p->update_harmony_channels(false);
    }

    score->master_score()->rebuild_midi_mapping();
    score->master_score()->update_channel();

    for (Staff *staff : score->staves()) {
        staff->update_ottava();
    }
    for (int idx : sys_staves) {
        score->add_system_object_staff(score->staff(idx));
    }

    return true;
}

bool Read500::prepare_paste_duration_element(Score *score, const Fraction &tick, const Fraction &ticks, const track_idx_t track) {
    Measure *destination_measure = score->undo_get_measure(tick);
    IF_ASSERT_FAILED(destination_measure) { return false; }

    Segment *paste_destination_seg = destination_measure->undo_get_segment(SegmentType::CHORD_REST, tick);
    IF_ASSERT_FAILED(paste_destination_seg) { return false; }

    // First make a gap for as long as we need...
    IF_ASSERT_FAILED(score->make_gap_voice(paste_destination_seg, track, ticks, tick, /*deleteAnnotations*/ false)) { return false; }

    // And shorten any segments that overlap with our destination...
    if (Segment *left_seg = score->tick2left_segment(tick)) {
        ChordRest *prev_cr = left_seg->next_chord_rest(track, /*backwards*/ true, /*stopAtMeasureBoundary*/ true);
        if (prev_cr && prev_cr->end_tick() > tick) {
            NoteInput::truncate_chord_rest(score->transaction_manager()->current_or_dummy_transaction(), score, prev_cr, tick,
                                           /*fillWithRest*/ false);
        }
    }

    return true;
}

bool Read500::paste_staff(XmlReader &e, Segment *dst, staff_idx_t dst_staff, Fraction scale) {
    assert(dst->is_type(SegmentType::DURATION));

    Score *score = dst->score();
    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    ReadContext ctx(score, /* pasteMode = */ true);

    std::vector<Harmony *> pasted_harmony;
    std::vector<Chord *> grace_notes;
    Beam *starting_beam = nullptr;
    Tuplet *tuplet = nullptr;
    TremoloTwoChord *prev_tremolo = nullptr;
    Fraction dst_tick = dst->tick();
    bool pasted = false;
    Fraction tick_len = Fraction(0, 1);
    int staves = 0;
    bool done = false;
    bool do_scale = (scale != Fraction(1, 1));

    while (e.read_next_start_element()) {
        if (done) {
            break;
        }
        if (e.name() != "StaffList") {
            e.unknown();
            break;
        }
        String version = e.attribute("version", u"NONE");
        if (!MScore::test_mode) {
            if (version != Constants::msc_version_str) {
                LOGD("pasteStaff: bad version");
                break;
            }
        }
        Fraction tick_start = Fraction::from_string(e.attribute("tick"));
        Fraction o_tick_len = Fraction::from_string(e.attribute("len"));
        Fraction time_stretch = e.has_attribute("timeStretch") ? Fraction::from_string(e.attribute("timeStretch")) : Fraction(1, 1);
        tick_len = o_tick_len * scale;
        int staff_start = e.int_attribute("staff", 0);
        staves = e.int_attribute("staves", 0);

        if (tick_len.is_zero() || staves == 0) {
            break;
        }
        if (do_scale) {
            Fraction tick_len_stretched = tick_len * time_stretch;
            if (!TDuration(tick_len_stretched).is_valid()) {
                LOGD("Can't paste: invalid duration %d/%d", tick_len_stretched.numerator(), tick_len_stretched.denominator());
                return false;
            }
        }

        Fraction o_end_tick = dst_tick + o_tick_len;
        auto o_spanner = score->spanner_map().find_contained(dst_tick.ticks(), o_end_tick.ticks());
        bool spanner_found = false;

        ctx.set_tick_offset(dst_tick - tick_start);
        ctx.set_tick(Fraction(0, 1));

        while (e.read_next_start_element()) {
            if (done) {
                break;
            }
            if (e.name() == "SpannerMap") {
                spanner_found = true;
                TRead::read_score_spanners(score, e, ctx);
                continue;
            }
            if (e.name() != "Staff") {
                e.unknown();
                break;
            }
            ctx.set_transpose_chromatic(0);
            ctx.set_transpose_diatonic(0);

            int src_staff_idx = e.int_attribute("id", 0);
            ctx.set_track(src_staff_idx * static_cast<int>(::mu::engraving::voices));
            ctx.set_track_offset(static_cast<int>((dst_staff - staff_start) * ::mu::engraving::voices));
            size_t dst_staff_idx = ctx.track() / ::mu::engraving::voices;
            if (dst_staff_idx >= dst->score()->nstaves()) {
                LOGD("paste beyond staves");
                done = true;
                break;
            }
            if (dst->is_inside_tuplet_on_staff(dst_staff_idx)) {
                done = true;
                break;
            }
            // Check the time stretch for all measures overlapping the destination range.
            for (Measure *m = dst->measure(); m && m->tick() < o_end_tick; m = m->next_measure()) {
                Fraction m_time_stretch = dst->score()->staff(dst_staff_idx)->time_stretch(m->tick());
                if (m_time_stretch != time_stretch) {
                    LOGD("Can't paste due to different time stretch ratios (src time stretch: %d/%d, dst time stretch: %d/%d)",
                         time_stretch.numerator(), time_stretch.denominator(), m_time_stretch.numerator(), m_time_stretch.denominator());
                    MScore::set_error(MsError::DEST_LOCAL_TIME_SIGNATURE);
                    return false;
                }
            }

            while (e.read_next_start_element()) {
                pasted = true;
                const AsciiStringView tag(e.name());

                if (tag == "transposeChromatic") {
                    ctx.set_transpose_chromatic(static_cast<int8_t>(e.read_int()));
                } else if (tag == "transposeDiatonic") {
                    ctx.set_transpose_diatonic(static_cast<int8_t>(e.read_int()));
                } else if (tag == "location") {
                    Location loc = Location::relative();
                    TRead::read(&loc, e, ctx);
                    ctx.set_location(loc);
                    if (loc.is_time_tick()) {
                        Measure *measure = score->tick2measure(ctx.tick());
                        EditTimeTickAnchors::create_time_tick_anchor(measure, ctx.tick() - measure->tick(), track2staff(ctx.track()));
                    }
                } else if (tag == "Tuplet") {
                    Tuplet *old_tuplet = tuplet;
                    Fraction tick = do_scale ? (ctx.tick() - dst_tick) * scale + dst_tick : ctx.tick();
                    Measure *measure = score->tick2measure(tick);
                    tuplet = Factory::create_tuplet(measure);
                    tuplet->set_track(ctx.track());
                    TRead::read(tuplet, e, ctx);
                    if (do_scale) {
                        Fraction ticks_scaled = tuplet->ticks() * scale;
                        if (!TDuration(ticks_scaled).is_valid()) {
                            LOGD("Can't paste: invalid duration %d/%d", ticks_scaled.numerator(), ticks_scaled.denominator());
                            return false;
                        }
                        tuplet->set_ticks(ticks_scaled);
                        tuplet->set_base_len(tuplet->base_len().fraction() * scale);
                    }
                    tuplet->set_ownership_parent(measure);
                    tuplet->set_tick(tick);
                    tuplet->set_tuplet(old_tuplet);
                    if (tuplet->rtick() + tuplet->actual_ticks() > measure->ticks()) {
                        delete tuplet;
                        if (old_tuplet && old_tuplet->elements().empty()) {
                            delete old_tuplet;
                        }
                        MScore::set_error(MsError::TUPLET_CROSSES_BAR);
                        return false;
                    }
                    if (!tuplet->tuplet()) {
                        IF_ASSERT_FAILED(prepare_paste_duration_element(score, tick, tuplet->actual_ticks_at(tick), tuplet->track())) {
                            e.skip_current_element();
                            continue;
                        }
                    }
                    if (old_tuplet) {
                        tuplet->read_add_tuplet(old_tuplet);
                    }
                } else if (tag == "endTuplet") {
                    if (!tuplet) {
                        LOGD("Score::pasteStaff: encountered <endTuplet/> when no tuplet was started");
                        e.skip_current_element();
                        continue;
                    }
                    Tuplet *old_tuplet = tuplet;
                    tuplet = tuplet->tuplet();
                    if (old_tuplet->elements().empty()) {
                        LOGD("Score::pasteStaff: ended tuplet is empty");
                        if (tuplet) {
                            tuplet->remove(old_tuplet);
                        }
                        delete old_tuplet;
                    } else {
                        old_tuplet->sort_elements();
                    }
                    e.read_next();
                } else if (tag == "Chord" || tag == "Rest" || tag == "MeasureRepeat") {
                    ChordRest *cr = to_chord_rest(Factory::create_item_by_name(tag, score->dummy()));
                    cr->set_track(ctx.track());
                    TRead::read_item(cr, e, ctx);
                    cr->set_selected(false);
                    Fraction tick = do_scale ? (ctx.tick() - dst_tick) * scale + dst_tick : ctx.tick();
                    if (score->tick2measure(tick)->is_measure_repeat_group(dst_staff_idx)) {
                        MeasureRepeat *mr = score->tick2measure(tick)->measure_repeat_element(dst_staff_idx);
                        score->delete_item(mr); // resets any measures related to mr
                    }
                    if (starting_beam) {
                        starting_beam->add(cr); // also calls cr->setBeam(startingBeam)
                        starting_beam = nullptr;
                    }
                    if (cr->is_grace()) {
                        grace_notes.push_back(to_chord(cr));
                    } else {
                        if (tuplet) {
                            cr->read_add_tuplet(tuplet);
                        }
                        ctx.inc_tick(cr->actual_ticks_at(tick));
                        if (do_scale) {
                            Fraction ticks_scaled = cr->ticks() * scale;
                            if (!TDuration(ticks_scaled).is_valid()) {
                                LOGD("Can't paste: invalid duration %d/%d", ticks_scaled.numerator(), ticks_scaled.denominator());
                                return false;
                            }
                            cr->set_ticks(ticks_scaled);
                            cr->set_duration_type(cr->duration_type_ticks() * scale);
                            for (Lyrics *l : cr->lyrics()) {
                                l->set_ticks(l->ticks() * scale);
                            }
                        }
                        if (cr->is_chord()) {
                            Chord *chord = to_chord(cr);
                            // disallow tie across barline within two-note tremolo
                            // tremolos can potentially still straddle the barline if no tie is required
                            // but these will be removed later

                            if (chord->tremolo_two_chord()) {
                                prev_tremolo = chord->tremolo_two_chord();
                                prev_tremolo->set_chord1(chord);
                                chord->set_tremolo_two_chord(prev_tremolo);
                            } else if (!chord->tremolo_two_chord() && prev_tremolo) {
                                prev_tremolo->set_chord2(chord);
                                chord->set_tremolo_two_chord(prev_tremolo);
                                prev_tremolo = nullptr;
                            }

                            TremoloTwoChord *tremolo = chord->tremolo_two_chord();
                            if (tremolo && chord == tremolo->chord2()) {
                                if (do_scale) {
                                    Fraction ticks_scaled = tremolo->duration_type().ticks() * scale;
                                    if (!TDuration(ticks_scaled).is_valid()) {
                                        LOGD("Can't paste: invalid duration %d/%d", ticks_scaled.numerator(), ticks_scaled.denominator());
                                        return false;
                                    }
                                    tremolo->set_duration_type(ticks_scaled);
                                }
                                Fraction tremolo_end_tick = tick + chord->actual_ticks_at(tick);
                                Fraction measure_end_tick = score->tick2measure(tick)->end_tick();
                                if (tremolo_end_tick > measure_end_tick) {
                                    MScore::set_error(MsError::DEST_TREMOLO);
                                    return false;
                                }
                            }
                            for (size_t i = 0; i < grace_notes.size(); ++i) {
                                Chord *gc = grace_notes.at(i);
                                gc->set_grace_index(i);
                                if (gc->v_staff_idx() >= gc->score()->nstaves()) {
                                    // check if staffMove moves a note to a
                                    // nonexistent staff
                                    gc->set_staff_move(0);
                                }
                                Transpose::transpose_chord(gc, tick);
                                chord->add(gc);
                            }
                            grace_notes.clear();
                        }
                        // delete pending ties, they are not selected when copy
                        if ((tick - dst_tick) + cr->actual_ticks_at(tick) >= tick_len) {
                            if (cr->is_chord()) {
                                Chord *c = to_chord(cr);
                                for (Note *note : c->notes()) {
                                    Tie *tie = note->tie_for_non_partial();
                                    if (tie) {
                                        note->set_tie_for(0);
                                        delete tie;
                                    }
                                }
                            }
                        }
                        // shorten last cr to fit in the space made by makeGap
                        if ((tick - dst_tick) + cr->actual_ticks_at(tick) > tick_len) {
                            Fraction new_length = tick_len - (tick - dst_tick);
                            // check previous CR on same track, if it has tremolo, delete the tremolo
                            // we don't want a tremolo and two different chord durations
                            if (cr->is_chord()) {
                                Segment *s = score->tick2left_segment(tick - Fraction::eps());
                                if (s) {
                                    ChordRest *crt = to_chord_rest(s->element(cr->track()));
                                    if (!crt) {
                                        crt = s->next_chord_rest(cr->track(), true);
                                    }
                                    if (crt && crt->is_chord()) {
                                        Chord *chrt = to_chord(crt);
                                        TremoloTwoChord *tr = chrt->tremolo_two_chord();
                                        if (tr) {
                                            tr->set_chords(chrt, to_chord(cr));
                                            chrt->remove(tr);
                                            delete tr;
                                        }
                                    }
                                }
                            }
                            if (!cr->tuplet()) {
                                // shorten duration
                                // exempt notes in tuplets, since we don't allow copy of partial tuplet anyhow
                                // TODO: figure out a reasonable fudge factor to make sure shorten tuplets appropriately if we do ever copy a partial tuplet
                                cr->set_ticks(new_length * time_stretch);
                                cr->set_duration_type(new_length * time_stretch);
                            }
                        }
                        if (!cr->tuplet()) {
                            IF_ASSERT_FAILED(prepare_paste_duration_element(score, tick, cr->actual_ticks_at(tick), cr->track())) {
                                e.skip_current_element();
                                continue;
                            }
                        }
                        Paste::paste_chord_rest(tx, score, cr, tick);
                    }
                } else if (tag == "Harmony") {
                    // transpose
                    Fraction tick = do_scale ? (ctx.tick() - dst_tick) * scale + dst_tick : ctx.tick();
                    Measure *m = score->tick2measure(tick);
                    Segment *seg = m->undo_get_segment(SegmentType::CHORD_REST, tick);
                    Harmony *harmony = Factory::create_harmony(seg);
                    harmony->set_track(ctx.track());
                    TRead::read(harmony, e, ctx);
                    harmony->set_track(ctx.track());

                    Staff *staff_dest = score->staff(ctx.track() / ::mu::engraving::voices);
                    Interval interval = staff_dest->transpose(tick);
                    if (!ctx.style().style_b(Sid::CONCERT_PITCH) && !interval.is_zero()) {
                        interval.flip();
                        Transpose::undo_transpose_harmony(tx, harmony, interval);
                    }

                    // remove pre-existing chords on this track
                    // but be sure not to remove any we just added
                    for (EngravingItem *el : seg->find_annotations(ElementType::HARMONY, ctx.track(), ctx.track())) {
                        if (std::find(pasted_harmony.begin(), pasted_harmony.end(), el) == pasted_harmony.end()) {
                            score->undo_remove_element(el);
                        }
                    }
                    harmony->set_ownership_parent(seg);
                    score->undo_add_element(harmony);
                    pasted_harmony.push_back(harmony);
                } else if (tag == "Dynamic" || tag == "Expression" || tag == "Symbol" || tag == "FretDiagram" || tag == "TremoloBar" ||
                           tag == "Marker" || tag == "Jump" || tag == "Image" || tag == "Text" || tag == "StaffText" ||
                           tag == "PlayTechAnnotation" || tag == "Capo" || tag == "StringTunings" || tag == "TempoText" ||
                           tag == "FiguredBass" || tag == "Sticking" || tag == "Fermata" || tag == "HarpPedalDiagram") {
                    EngravingItem *el = Factory::create_item_by_name(tag, score->dummy());
                    el->set_track(ctx.track()); // a valid track might be necessary for el->read() to work
                    if (el->is_fermata()) {
                        el->set_placement(el->track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE);
                    }
                    TRead::read_item(el, e, ctx);

                    Fraction tick = do_scale ? (ctx.tick() - dst_tick) * scale + dst_tick : ctx.tick();
                    Measure *m = score->tick2measure(tick);
                    Segment *seg = el->is_fret_diagram() ? m->undo_get_segment(SegmentType::CHORD_REST, tick) :
                                                           m->undo_get_chord_rest_or_time_tick_segment(tick);
                    el->set_ownership_parent(seg);

                    // be sure to paste the element in the destination track;
                    // setting track needs to be repeated, as it might have been overwritten by el->read()
                    // preserve *voice* from source, though
                    el->set_staff_idx(ctx.track() / ::mu::engraving::voices);
                    score->undo_add_element(el);
                } else if (tag == "Clef") {
                    Fraction tick = do_scale ? (ctx.tick() - dst_tick) * scale + dst_tick : ctx.tick();
                    Measure *m = score->tick2measure(tick);
                    if (m->tick().is_not_zero() && m->tick() == tick) {
                        m = m->prev_measure();
                    }
                    Segment *segment = m->undo_get_segment(SegmentType::CLEF, tick);
                    Clef *clef = Factory::create_clef(segment);
                    TRead::read(clef, e, ctx);
                    clef->set_track(ctx.track());
                    score->undo_change_element(segment->element(ctx.track()), clef);
                } else if (tag == "Breath") {
                    Fraction tick = do_scale ? (ctx.tick() - dst_tick) * scale + dst_tick : ctx.tick();
                    Measure *m = score->tick2measure(tick);
                    if (m->tick() == tick) {
                        m = m->prev_measure();
                    }
                    Segment *segment = m->undo_get_segment(SegmentType::BREATH, tick);
                    Breath *breath = Factory::create_breath(segment);
                    breath->set_track(ctx.track());
                    breath->set_placement(breath->track() & 1 ? PlacementV::BELOW : PlacementV::ABOVE);
                    TRead::read(breath, e, ctx);
                    score->undo_change_element(segment->element(ctx.track()), breath);
                } else if (tag == "Beam") {
                    Beam *beam = Factory::create_beam(score);
                    beam->set_track(ctx.track());
                    TRead::read(beam, e, ctx);
                    if (starting_beam) {
                        LOGD("The read beam was not used");
                        delete starting_beam;
                    }
                    starting_beam = beam;
                } else if (tag == "BarLine") {
                    e.skip_current_element(); // ignore bar line
                } else {
                    LOGD("PasteStaff: element %s not handled", tag.ascii());
                    e.skip_current_element(); // ignore
                }
            }

            if (starting_beam) {
                LOGD("The read beam was not used");
                delete starting_beam;
            }
            if (tuplet) {
                LOGD("<endTuplet/> not found");
                if (tuplet->elements().empty()) {
                    if (tuplet->tuplet()) {
                        tuplet->tuplet()->remove(tuplet);
                    }
                    delete tuplet;
                }
            }
        }
        // fix up spanners
        if (do_scale && spanner_found) {
            // build list of original spanners
            std::vector<Spanner *> o_spanner_list;
            for (auto interval : o_spanner) {
                Spanner *sp = interval.value;
                o_spanner_list.push_back(sp);
            }
            auto n_spanner = score->spanner_map().find_contained(dst_tick.ticks(), o_end_tick.ticks());
            for (auto interval : n_spanner) {
                Spanner *sp = interval.value;
                // skip if not in this staff list
                if (sp->staff_idx() < dst_staff || sp->staff_idx() >= dst_staff + staves) {
                    continue;
                }
                // CHORDREST and NOTE spanners are normally handled already
                if (sp->anchor() == Spanner::Anchor::CHORDREST || sp->anchor() == Spanner::Anchor::NOTE) {
                    continue;
                }
                // skip if present originally
                auto i = std::find(o_spanner_list.begin(), o_spanner_list.end(), sp);
                if (i != o_spanner_list.end()) {
                    continue;
                }
                Fraction tick = (sp->tick() - dst_tick) * scale + dst_tick;
                sp->undo_change_property(Pid::SPANNER_TICK, tick);
                sp->undo_change_property(Pid::SPANNER_TICKS, sp->ticks() * scale);
            }
        }
    }

    ctx.connect_note_anchored_spanners();

    for (Score *s : score->score_list()) { // for all parts
        s->connect_ties();
        s->undo_remove_stale_tie_jump_points(false);

        for (Spanner *sp : score->unmanaged_spanners()) {
            if (sp->is_lyrics_line() && to_lyrics_line(sp)->is_dash()) {
                LyricsLine *line = to_lyrics_line(sp);
                line->set_next_lyrics(
                    search_next_lyrics(line->lyrics()->segment(), line->staff_idx(), line->lyrics()->verse(), line->lyrics()->placement()));
                line->set_track2(line->next_lyrics() ? line->next_lyrics()->track() : line->track());
            }
        }
    }

    if (pasted) { //select only if we pasted something
        staff_idx_t end_staff = dst_staff + staves;
        if (end_staff > score->nstaves()) {
            end_staff = score->nstaves();
        }

        if (score->cmd_state().layout_range()) {
            score->set_layout(dst_tick, dst_tick + tick_len, dst_staff, end_staff, dst);
        }

        //TODO: look if this could be done different
        Measure *dst_m = score->tick2measure(dst_tick);
        Measure *end_m = score->tick2measure(dst_tick + tick_len);
        for (staff_idx_t i = dst_staff; i < end_staff; i++) {
            for (Measure *m = dst_m; m && m != end_m->next_measure(); m = m->next_measure()) {
                m->check_measure(i);
            }
        }
        score->selection_.set_range_ticks(dst_tick, dst_tick + tick_len, dst_staff, end_staff);

        //finding the first element that has a track
        //the canvas position will be set to this element
        EngravingItem *el = nullptr;
        Segment *s = score->tick2segment_mm(dst_tick);
        Segment *s2 = score->tick2segment_mm(dst_tick + tick_len);
        bool found = false;
        if (s2) {
            s2 = s2->next1_mm();
        }
        while (!found && s != s2) {
            for (size_t i = dst_staff * ::mu::engraving::voices; i < (end_staff + 1) * ::mu::engraving::voices; i++) {
                el = s->element(i);
                if (el) {
                    found = true;
                    break;
                }
            }
            s = s->next1_mm();
        }

        if (!score->selection().is_range()) {
            score->selection_.set_state(SelState::RANGE);
        }
    }
    return true;
}

void Read500::paste_symbols(XmlReader &e, ChordRest *dst) {
    Score *score = dst->score();
    Transaction &tx = score->transaction_manager()->current_or_dummy_transaction();

    ReadContext ctx(score, /* pasteMode = */ true);

    Segment *curr_segm = dst->segment();
    Fraction dest_tick = Fraction(0, 1); // the tick and track to place the pasted element at
    track_idx_t dest_track = 0;
    bool done = false;
    int seg_delta = 0;
    Segment *start_segm = curr_segm;
    Fraction start_tick = dst->tick(); // the initial tick and track where to start pasting
    track_idx_t start_track = dst->track();
    track_idx_t max_track = score->ntracks();

    while (e.read_next_start_element()) {
        if (done) {
            break;
        }
        if (e.name() != "SymbolList") {
            e.unknown();
            break;
        }
        String version = e.attribute("version", u"NONE");
        if (version != Constants::msc_version_str) {
            break;
        }

        while (e.read_next_start_element()) {
            if (done) {
                break;
            }
            const AsciiStringView tag(e.name());

            if (tag == "trackOffset") {
                dest_track = start_track + e.read_int();
                curr_segm = start_segm;
            } else if (tag == "tickOffset") {
                dest_tick = start_tick + Fraction::from_ticks(e.read_int());
            } else if (tag == "segDelta") {
                seg_delta = e.read_int();
            } else if (tag == "Articulation" || tag == "Ornament" || tag == "Arpeggio" || tag == "TremoloSingleChord") {
                // Elements that can be attached only to a Chord
                if (dest_track >= max_track) {
                    LOGD() << "No track for " << tag;
                    e.skip_current_element();
                    continue;
                }
                Measure *meas = score->tick2measure(dest_tick);
                Segment *seg = meas ? meas->undo_get_segment(SegmentType::CHORD_REST, dest_tick) : nullptr;
                if (!seg) {
                    LOGD() << "No ChordRest segment for " << tag << " at tick " << dest_tick.ticks();
                    e.skip_current_element();
                    continue;
                }

                ChordRest *cr = seg->cr(dest_track);
                if (!cr || !cr->is_chord()) {
                    LOGD() << "No Chord for " << tag << " at tick " << dest_tick.ticks();
                    e.skip_current_element();
                    continue;
                }

                EngravingItem *el = Factory::create_item_by_name(tag, score->dummy());
                el->set_track(dest_track);
                TRead::read_item(el, e, ctx);
                el->set_track(dest_track);
                el->set_ownership_parent(cr);
                score->undo_add_element(el);
            } else if (tag == "Fermata") {
                if (dest_track >= max_track) {
                    LOGD() << "No track for Fermata";
                    e.skip_current_element();
                    continue;
                }
                Measure *meas = score->tick2measure(dest_tick);
                Segment *seg = meas ? meas->undo_get_segment(SegmentType::CHORD_REST, dest_tick) : nullptr;
                if (!seg) {
                    LOGD() << "No ChordRest segment for Fermata at tick " << dest_tick.ticks();
                    e.skip_current_element();
                    continue;
                }

                Fermata *b = Factory::create_fermata(score->dummy());
                b->set_track(dest_track);
                TRead::read(b, e, ctx);
                b->set_track(dest_track);
                b->set_ownership_parent(seg);
                score->undo_add_element(b);
            } else if (tag == "Breath") {
                if (dest_track >= max_track) {
                    LOGD() << "No track for Breath";
                    e.skip_current_element();
                    continue;
                }
                Measure *meas = score->tick2measure(dest_tick);
                Segment *seg = meas ? meas->undo_get_segment(SegmentType::BREATH, dest_tick) : nullptr;
                if (!seg) {
                    LOGD() << "No Breath segment for Breath at tick " << dest_tick.ticks();
                    e.skip_current_element();
                    continue;
                }

                Breath *b = Factory::create_breath(score->dummy());
                b->set_track(dest_track);
                TRead::read(b, e, ctx);
                b->set_track(dest_track);
                b->set_ownership_parent(seg);
                score->undo_add_element(b);
            } else if (tag == "Dynamic" || tag == "Expression" || tag == "StaffText" || tag == "PlayTechAnnotation" || tag == "Sticking" ||
                       tag == "Capo" || tag == "HarpPedalDiagram" || tag == "StringTunings") {
                if (dest_track >= max_track) {
                    LOGD() << "No track for " << tag;
                    e.skip_current_element();
                    continue;
                }
                // Text elements that can be attached to ChordRest or TimeTick segments
                Measure *meas = score->tick2measure(dest_tick);
                Segment *seg = meas ? meas->undo_get_chord_rest_or_time_tick_segment(dest_tick) : nullptr;
                if (!seg) {
                    LOGD() << "No segment for " << tag << " at tick " << dest_tick.ticks();
                    e.skip_current_element();
                    continue;
                }

                EngravingItem *el = Factory::create_item_by_name(tag, score->dummy());
                el->set_track(dest_track);
                TRead::read_item(el, e, ctx);
                el->set_track(dest_track);
                el->set_ownership_parent(seg);
                score->undo_add_element(el);
            } else if (tag == "Slur" || tag == "HairPin" || tag == "Ottava" || tag == "Trill" || tag == "LetRing" || tag == "Vibrato" ||
                       tag == "PalmMute" || tag == "WhammyBar" || tag == "Rasgueado" || tag == "HarmonicMark" || tag == "PickScrape" ||
                       tag == "TextLine" || tag == "Pedal") {
                // Spanners
                if (dest_track >= max_track) {
                    LOGD() << "No track for " << tag;
                    e.skip_current_element();
                    continue;
                }
                Spanner *s = to_spanner(Factory::create_item_by_name(tag, score->dummy()));
                s->set_track(dest_track);
                TRead::read_item(s, e, ctx);
                s->set_track(dest_track);
                s->set_track2(dest_track);
                s->set_tick(dest_tick);
                score->undo_add_element(s);
            } else if (tag == "Harmony" || tag == "FretDiagram") {
                //
                // Harmony elements (= chord symbols) are positioned respecting
                // the original tickOffset: advance to destTick (or near)
                // same for FretDiagram elements
                //
                Measure *meas = score->tick2measure(dest_tick);
                Segment *seg = meas ? meas->undo_get_chord_rest_or_time_tick_segment(dest_tick) : nullptr;

                if (dest_track >= max_track || seg == nullptr) {
                    LOGD() << "No track or segment for " << tag << " at tick " << dest_tick.ticks();
                    e.skip_current_element(); // ignore
                    continue;
                }
                if (tag == "Harmony") {
                    Harmony *el = Factory::create_harmony(seg);
                    el->set_track(track_zero_voice(dest_track));
                    TRead::read(el, e, ctx);
                    el->set_track(track_zero_voice(dest_track));
                    // transpose
                    Staff *staff_dest = score->staff(track2staff(dest_track));
                    Interval interval = staff_dest->transpose(dest_tick);
                    if (!ctx.style().style_b(Sid::CONCERT_PITCH) && !interval.is_zero()) {
                        interval.flip();
                        Transpose::undo_transpose_harmony(tx, el, interval);
                    }
                    el->set_ownership_parent(seg);
                    score->undo_add_element(el);
                } else {
                    FretDiagram *el = Factory::create_fret_diagram(seg);
                    el->set_track(track_zero_voice(dest_track));
                    TRead::read(el, e, ctx);
                    el->set_track(track_zero_voice(dest_track));
                    score->undo_add_element(el);
                }
            } else if (tag == "Lyrics" || tag == "FiguredBass") {
                // These elements are positioned respecting the distance in chords,
                // rather than the time distance.
                // TODO: is that appropriate for FiguredBass too? To be considered
                // when overhauling FiguredBass.
                for (; curr_segm && seg_delta > 0; seg_delta--) {
                    curr_segm = curr_segm->next_cr(dest_track);
                }
                // check the intended dest. track and segment exist
                if (dest_track >= max_track || curr_segm == nullptr) {
                    LOGD("PasteSymbols: no track or segment for %s", tag.ascii());
                    e.skip_current_element(); // ignore
                    continue;
                }
                // check there is a segment element in the required track
                if (curr_segm->element(dest_track) == nullptr) {
                    LOGD("PasteSymbols: no track element for %s", tag.ascii());
                    e.skip_current_element();
                    continue;
                }

                if (tag == "Lyrics") {
                    // with lyrics, skip rests
                    ChordRest *cr = to_chord_rest(curr_segm->element(dest_track));
                    while (!cr->is_chord() && curr_segm) {
                        curr_segm = curr_segm->next_cr(dest_track);
                        if (curr_segm) {
                            cr = to_chord_rest(curr_segm->element(dest_track));
                        } else {
                            break;
                        }
                    }
                    if (curr_segm == nullptr) {
                        LOGD("PasteSymbols: no segment for Lyrics");
                        e.skip_current_element();
                        continue;
                    }
                    if (!cr->is_chord()) {
                        LOGD("PasteSymbols: can't paste Lyrics to rest");
                        e.skip_current_element();
                        continue;
                    }
                    Lyrics *el = Factory::create_lyrics(cr);
                    el->set_track(dest_track);
                    TRead::read(el, e, ctx);
                    el->set_track(dest_track);
                    score->undo_add_element(el);
                } else if (tag == "FiguredBass") {
                    // FiguredBass always belongs to first staff voice
                    dest_track = track_zero_voice(dest_track);
                    Fraction ticks;
                    FiguredBass *el = Factory::create_figured_bass(curr_segm);
                    el->set_track(dest_track);
                    TRead::read(el, e, ctx);
                    el->set_track(dest_track);
                    // if f.b. is off-note, we have to locate a place before currSegm
                    // where an on-note f.b. element could (potentially) be
                    // (while having an off-note f.b. without an on-note one before it
                    // is un-idiomatic, possible mismatch in rhythmic patterns between
                    // copy source and paste destination does not allow to be too picky)
                    if (!el->on_note()) {
                        FiguredBass *on_note_fb = nullptr;
                        Segment *prev_segm = curr_segm;
                        bool done1 = false;
                        while (prev_segm) {
                            if (done1) {
                                break;
                            }
                            prev_segm = prev_segm->prev1(SegmentType::CHORD_REST);
                            // if there is a ChordRest in the dest. track
                            // this segment is a (potential) f.b. location
                            if (prev_segm->element(dest_track) != nullptr) {
                                done1 = true;
                            }
                            // in any case, look for a f.b. in annotations:
                            // if there is a f.b. element in the right track,
                            // this is an (actual) f.b. location
                            for (EngravingItem *a : prev_segm->annotations()) {
                                if (a->is_figured_bass() && a->track() == dest_track) {
                                    on_note_fb = to_figured_bass(a);
                                    done1 = true;
                                }
                            }
                        }
                        if (!prev_segm) {
                            LOGD("PasteSymbols: can't place off-note FiguredBass");
                            delete el;
                            continue;
                        }
                        // by default, split on-note duration in half: half on-note and half off-note
                        Fraction tot_ticks = curr_segm->tick() - prev_segm->tick();
                        Fraction dest_tick1 = prev_segm->tick() + (tot_ticks * Fraction(1, 2));
                        ticks = tot_ticks * Fraction(1, 2);
                        if (on_note_fb) {
                            on_note_fb->set_ticks(tot_ticks * Fraction(1, 2));
                        }
                        // look for a segment at this tick; if none, create one
                        Segment *next_segm = prev_segm;
                        while (next_segm && next_segm->tick() < dest_tick1) {
                            next_segm = next_segm->next1(SegmentType::CHORD_REST);
                        }
                        if (!next_segm || next_segm->tick() > dest_tick1) { // no ChordRest segm at this tick
                            next_segm = Factory::create_segment(prev_segm->measure(), SegmentType::CHORD_REST, dest_tick1);
                            if (!next_segm) {
                                LOGD("PasteSymbols: can't find or create destination segment for FiguredBass");
                                delete el;
                                continue;
                            }
                            score->undo_add_element(next_segm);
                        }
                        curr_segm = next_segm;
                    } else {
                        // by default, assign to FiguredBass element the duration of the chord it refers to
                        ticks = to_chord_rest(curr_segm->element(dest_track))->ticks();
                    }
                    // in both cases, look for an existing f.b. element in segment and remove it, if found
                    FiguredBass *old_fb = nullptr;
                    for (EngravingItem *a : curr_segm->annotations()) {
                        if (a->is_figured_bass() && a->track() == dest_track) {
                            old_fb = to_figured_bass(a);
                            break;
                        }
                    }
                    if (old_fb) {
                        score->undo_remove_element(old_fb);
                    }
                    el->set_ownership_parent(curr_segm);
                    el->set_ticks(ticks);
                    score->undo_add_element(el);
                }
            } else {
                LOGD("PasteSymbols: element %s not handled", tag.ascii());
                e.skip_current_element(); // ignore
            }
        } // outer while readNextstartElement()
    } // inner while readNextstartElement()
}

void Read500::read_tremolo_compat(compat::TremoloCompat *tc, XmlReader &xml) {
    ReadContext ctx(static_cast<EngravingObject *>(tc->parent)->score(), /* pasteMode = */ true);
    TRead::read(tc, xml, ctx);
}

void Read500::do_read_item(EngravingItem *item, XmlReader &xml) {
    ReadContext ctx(item->score(), /* pasteMode = */ true);
    TRead::read_item(item, xml, ctx);
}
