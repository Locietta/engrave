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

#include "../compat/readstyle.h"
#include "../compat/tremolocompat.h"
#include "../editing/noteinput.h"
#include "../editing/transaction/transaction.h"
#include "../editing/transpose.h"
#include "../inoutdata.h"
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
#include "log.h"
#include "staffread.h"
#include "tread.h"

using namespace mu::engraving;
using namespace mu::engraving::read500;

muse::Ret Read500::read_score_file(Score *score, XmlReader &e, rw::ReadInOutData *data) {
    ReadContext ctx(score);
    if (data) {
        if (data->overridden_spatium.has_value()) {
            ctx.set_spatium(data->overridden_spatium.value());
        }

        ctx.set_properties_to_skip(data->properties_to_skip);
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

void Read500::read_tremolo_compat(compat::TremoloCompat *tc, XmlReader &xml) {
    ReadContext ctx(static_cast<EngravingObject *>(tc->parent)->score(), /* pasteMode = */ true);
    TRead::read(tc, xml, ctx);
}

void Read500::do_read_item(EngravingItem *item, XmlReader &xml) {
    ReadContext ctx(item->score(), /* pasteMode = */ true);
    TRead::read_item(item, xml, ctx);
}
