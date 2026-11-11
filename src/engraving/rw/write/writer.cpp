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
#include "writer.h"

#include "../inoutdata.h"
#include "../types/types.h"
#include "../xmlwriter.h"
#include "dom/excerpt.h"
#include "dom/masterscore.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "global/appinfo.h"
#include "log.h"
#include "staffwrite.h"
#include "twrite.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::engraving::write;

Writer::Writer() {}

bool Writer::write_score(Score *score, io::IODevice *device, rw::WriteInOutData *inout) {

    XmlWriter xml(device);
    WriteContext ctx(score);
    if (inout) {
        ctx = inout->ctx;
    }

    xml.start_document();

    xml.start_element("museScore", {{"version", Constants::msc_version_str}});

    if (!MScore::test_mode) {
        xml.tag("programVersion", muse::appinfo::version().to_string());
        xml.tag("programRevision", muse::appinfo::revision());
    }

    compat::WriteScoreHook hook;
    write(score, xml, ctx, hook);

    xml.end_element();
    xml.flush();

    if (!inout || !inout->ctx.should_write_range()) {
        //update version values for i.e. plugin access
        score->mscore_version_ = muse::appinfo::version().to_string();
        score->mscore_revision_ = muse::appinfo::revision().to_int(nullptr, 16);
        score->msc_version_ = Constants::msc_version;
    }

    if (inout) {
        inout->ctx = ctx;
    }

    return true;
}

void Writer::write(Score *score, XmlWriter &xml, WriteContext &ctx, compat::WriteScoreHook &hook) {

    xml.start_element(score);

    TWrite::write_item_eid(score, xml);

    if (Excerpt *e = score->excerpt()) {
        if (!e->name().empty()) {
            xml.tag("name", e->name());
        }

        if (!ctx.standalone()) {
            const TracksMap &tracks = e->tracks_mapping();
            if (!(tracks.size() == e->nstaves() * ::mu::engraving::voices) && !tracks.empty()) {
                for (auto it = tracks.begin(); it != tracks.end(); ++it) {
                    xml.tag("Tracklist", {{"sTrack", it->first}, {"dstTrack", it->second}});
                }
            }

            if (e->initial_part_id().is_valid()) {
                xml.tag("initialPartId", e->initial_part_id().to_uint64());
            }
        }
    }

    if (score->is_layout_mode(LayoutMode::LINE)) {
        xml.tag("layoutMode", "line");
    }
    if (score->is_layout_mode(LayoutMode::SYSTEM)) {
        xml.tag("layoutMode", "system");
    }

    if (score->is_master() && !MScore::test_mode) {
        score->synthesizer_state_.write(xml);
    }

    if (score->page_number_offset()) {
        xml.tag("page-offset", score->page_number_offset());
    }
    xml.tag("Division", Constants::division);
    ctx.set_cur_track(muse::nidx);

    hook.on_write_style302(score, xml, ctx.inline_style());

    xml.tag("showInvisible", score->show_invisible_);
    xml.tag("showUnprintable", score->show_unprintable_);
    xml.tag("showFrames", score->show_frames_);
    xml.tag("showMargins", score->show_pageborders_);
    xml.tag("markIrregularMeasures", score->mark_irregular_measures_, true);

    if (!score->show_sound_flags_) { // true by default
        xml.tag("showSoundFlags", score->show_sound_flags_);
    }

    if (score->is_open_) {
        xml.tag("open", score->is_open_);
    }

    for (const auto &t : score->meta_tags_) {
        // do not output "platform" and "creationDate" in test and save template mode
        if ((!MScore::test_mode && !MScore::save_template_mode) || (t.first != "platform" && t.first != "creationDate")) {
            xml.tag("metaTag", {{"name", t.first.to_xml_escaped()}}, t.second);
        }
    }

    if (score->score_order_.is_valid()) {
        ScoreOrder order = score->score_order_;
        order.update_instruments(score);
        order.write(xml);
    }

    staff_idx_t staff_start = 0;
    staff_idx_t staff_end = 0;
    MeasureBase *measure_start = nullptr;
    MeasureBase *measure_end = nullptr;

    if (ctx.should_write_range()) {
        const WriteRange &r = ctx.range().value();
        staff_start = r.start_staff_idx;
        staff_end = r.end_staff_idx;
        measure_start = r.start_measure;
        measure_end = r.end_measure;
    } else {
        staff_end = score->nstaves();
        measure_start = score->first();
    }

    if (!score->system_object_staves_.empty()) {
        bool save_sys_obj_staves = false;
        for (const Staff *s : score->system_object_staves_) {
            IF_ASSERT_FAILED(s->idx() != muse::nidx) { continue; }
            save_sys_obj_staves = true;
            break;
        }

        if (save_sys_obj_staves) {
            xml.start_element("SystemObjects");
            for (const Staff *s : score->system_object_staves_) {
                const staff_idx_t idx = s->idx();
                IF_ASSERT_FAILED(idx != muse::nidx) { continue; }

                if (ctx.should_write_range()) {
                    if (idx < staff_start || idx >= staff_end) {
                        continue;
                    }
                }

                xml.tag("Instance", {{"staffId", idx + 1}});
            }
            xml.end_element();
        }
    }

    ctx.set_cur_track(0);

    if (score->is_master()) {
        // Let's decide: write midi mapping to a file or not
        score->master_score()->check_midi_mapping();
    }

    auto should_write_part = [&ctx, score, staff_start, staff_end](const Part *part) {
        if (!ctx.should_write_range()) {
            return true;
        }

        const staff_idx_t first_staff_idx = score->staff_idx(part);
        const staff_idx_t last_staff_idx = first_staff_idx + part->nstaves() - 1;

        return (first_staff_idx >= staff_start && first_staff_idx < staff_end) ||
               (last_staff_idx >= staff_start && last_staff_idx < staff_end);
    };

    for (const Part *part : score->parts_) {
        if (should_write_part(part)) {
            TWrite::write(part, xml, ctx);
        }
    }

    ctx.set_cur_track(0);
    ctx.set_track_diff(-static_cast<int>(staff_start * ::mu::engraving::voices));
    if (measure_start) {
        for (staff_idx_t staff_idx = staff_start; staff_idx < staff_end; ++staff_idx) {
            const Staff *st = score->staff(staff_idx);
            StaffWrite::write_staff(st, xml, ctx, measure_start, measure_end, staff_start, staff_idx);
        }
    }
    ctx.set_cur_track(muse::nidx);

    TWrite::write_score_spanners(score, xml, ctx);

    hook.on_write_excerpts302(score, xml, ctx);

    TWrite::write_page_locks(score, xml);
    TWrite::write_system_locks(score, xml);
    TWrite::write_system_dividers(score, xml, ctx);

    xml.end_element(); // score
}

void Writer::write_segments(XmlWriter &xml, WriteContext &ctx, track_idx_t strack, track_idx_t etrack, Segment *sseg, Segment *eseg,
                            bool write_system_elements, bool force_time_sig) {
    ctx.set_cur_track(strack);
    TWrite::write_segments(xml, ctx, strack, etrack, sseg, eseg, write_system_elements, force_time_sig);
}

void Writer::do_write_item(const EngravingItem *item, XmlWriter &xml) {
    WriteContext ctx(item->score(), /* clipboardMode = */ true);
    TWrite::write_item(item, xml, ctx);
}
