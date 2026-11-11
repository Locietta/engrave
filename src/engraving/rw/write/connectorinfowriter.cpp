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
#include "connectorinfowriter.h"

#include "../../dom/engravingitem.h"
#include "../../dom/measure.h"
#include "../../dom/measurebase.h"
#include "../../dom/score.h"
#include "../../dom/spanner.h"
#include "../xmlwriter.h"
#include "log.h"
#include "twrite.h"

using namespace mu::engraving;
using namespace mu::engraving::write;

ConnectorInfoWriter::ConnectorInfoWriter(XmlWriter &xml, WriteContext *ctx, const EngravingItem *current, const EngravingItem *connector,
                                         int track, Fraction frac)
    : ConnectorInfo(current, track, frac), connector_value(connector), xml_(&xml), ctx_(ctx) {
    IF_ASSERT_FAILED(current) { return; }
    type_ = connector->type();
    update_current_info(ctx_->clipboardmode());
}

void ConnectorInfoWriter::write() {
    XmlWriter &xml = *xml_;
    WriteContext &ctx = *ctx_;
    xml.start_element(tag_name(), {{"type", connector_value->type_name()}});
    if (is_start()) {
        TWrite::write_item(connector_value, xml, ctx);
    }
    if (has_previous()) {
        xml.start_element("prev");
        prev_loc_.to_relative(current_loc_);
        TWrite::write(&prev_loc_, xml, ctx);
        xml.end_element();
    }
    if (has_next()) {
        xml.start_element("next");
        next_loc_.to_relative(current_loc_);
        TWrite::write(&next_loc_, xml, ctx);
        xml.end_element();
    }
    xml.end_element();
}

//---------------------------------------------------------
//   SpannerWriter::fillSpannerPosition
//---------------------------------------------------------

void SpannerWriter::fill_spanner_position(Location &l, const MeasureBase *m, const Fraction &tick, bool clipboardmode) {
    if (clipboardmode) {
        l.set_measure(0);
        l.set_frac(tick);
    } else {
        if (!m) {
            LOGW("fillSpannerPosition: couldn't find spanner's endpoint's measure");
            l.set_measure(0);
            l.set_frac(tick);
            return;
        }
        l.set_measure(m->measure_index());
        l.set_frac(tick - m->tick());
    }
}

//---------------------------------------------------------
//   SpannerWriter::SpannerWriter
//---------------------------------------------------------

SpannerWriter::SpannerWriter(XmlWriter &xml, WriteContext *ctx, const EngravingItem *current, const Spanner *sp, int track, Fraction frac,
                             bool start)
    : ConnectorInfoWriter(xml, ctx, current, sp, track, frac) {
    const bool clipboardmode = ctx->clipboardmode();
    if ((!sp->start_element() || !sp->end_element()) &&
        (sp->anchor() == Spanner::Anchor::CHORDREST || sp->anchor() == Spanner::Anchor::NOTE)) {
        LOGW("SpannerWriter: spanner (%s) doesn't have an endpoint!", sp->type_name());
        return;
    }
    if (current->is_measure() || current->is_segment() || !sp->start_element() || !sp->end_element() ||
        (sp->start_element()->type() != current->type())) {
        // (The latter is the hairpins' case, for example, though they are
        // covered by the other checks too.)
        // We cannot determine position of the spanner from its start/end
        // elements and will try to obtain this info from the spanner itself.
        if (!start) {
            prev_loc_.set_track(static_cast<int>(sp->track()));
            Measure *m = sp->score()->tick2measure(sp->tick());
            fill_spanner_position(prev_loc_, m, sp->tick(), clipboardmode);
        } else {
            const track_idx_t track2 = (sp->track2() != muse::nidx) ? sp->track2() : sp->track();
            next_loc_.set_track(static_cast<int>(track2));
            Measure *m = sp->score()->tick2measure(sp->tick2());
            fill_spanner_position(next_loc_, m, sp->tick2(), clipboardmode);
        }
    } else {
        // We can obtain the spanner position info from its start/end
        // elements and will prefer this source of information.
        // Reason: some spanners contain no or wrong information (e.g. Ties).
        if (!start) {
            update_location(sp->start_element(), prev_loc_, clipboardmode);
        } else {
            update_location(sp->end_element(), next_loc_, clipboardmode);
        }
    }
}
