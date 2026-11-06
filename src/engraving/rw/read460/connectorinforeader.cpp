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
#include "connectorinforeader.h"

#include "../../infrastructure/rtti.h"

#include "../../dom/factory.h"
#include "../../dom/score.h"
#include "../../dom/engravingitem.h"
#include "../../dom/spanner.h"
#include "../../dom/chordrest.h"
#include "../../dom/measure.h"
#include "../../dom/note.h"
#include "../../dom/noteline.h"
#include "../../dom/tie.h"
#include "../../dom/chord.h"
#include "../../dom/staff.h"

#include "../types/typesconv.h"

#include "../xmlreader.h"

#include "tread.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read460;

//---------------------------------------------------------
//   ConnectorInfoReader
//---------------------------------------------------------

ConnectorInfoReader::ConnectorInfoReader(XmlReader &e, ReadContext *ctx, EngravingItem *current, int track)
    : ConnectorInfo(current, track), reader_(&e), ctx_(ctx), connector_(nullptr), connector_receiver_(current) {}

//---------------------------------------------------------
//   readPositionInfo
//---------------------------------------------------------

static Location read_position_info(ReadContext *ctx, int track) {
    Location info = ctx->location();
    info.set_track(track);
    return info;
}

//---------------------------------------------------------
//   ConnectorInfoReader
//---------------------------------------------------------

ConnectorInfoReader::ConnectorInfoReader(XmlReader &e, ReadContext *ctx, Score *current, int track)
    : ConnectorInfo(current, read_position_info(ctx, track)), reader_(&e), ctx_(ctx), connector_(nullptr), connector_receiver_(current) {
    set_current_updated(true);
}

//---------------------------------------------------------
//   ConnectorInfoReader::read
//---------------------------------------------------------

bool ConnectorInfoReader::read() {
    XmlReader &e = *reader_;
    const AsciiStringView name(e.ascii_attribute("type"));
    type_ = TConv::from_xml(name, ElementType::INVALID);

    ctx_->fill_location(current_loc_);

    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());

        if (tag == "prev") {
            read_endpoint_location(prev_loc_);
        } else if (tag == "next") {
            read_endpoint_location(next_loc_);
        } else {
            if (tag == name) {
                connector_ = Factory::create_item_by_name(tag, connector_receiver_->score()->dummy());
            } else {
                LOGW("ConnectorInfoReader::read: element tag (%s) does not match connector type (%s). Is the file corrupted?", tag.ascii(),
                     name.ascii());
            }

            if (!connector_) {
                e.unknown();
                return false;
            }
            connector_->set_track(current_loc_.track());
            TRead::read_item(connector_, e, *ctx_);
        }
    }
    return true;
}

//---------------------------------------------------------
//   ConnectorInfoReader::readEndpointLocation
//---------------------------------------------------------

void ConnectorInfoReader::read_endpoint_location(Location &l) {
    XmlReader &e = *reader_;
    while (e.read_next_start_element()) {
        const AsciiStringView tag(e.name());
        if (tag == "location") {
            l = Location::relative();
            TRead::read(&l, e, *ctx_);
        } else {
            e.unknown();
        }
    }
}

Fraction ConnectorInfoReader::cur_tick() const { return ctx_->tick(); }

//---------------------------------------------------------
//   ConnectorInfoReader::update
//---------------------------------------------------------

void ConnectorInfoReader::update() {
    if (!current_updated()) {
        update_current_info(ctx_->paste_mode());
    }
    if (has_previous()) {
        prev_loc_.to_absolute(current_loc_);
    }
    if (has_next()) {
        next_loc_.to_absolute(current_loc_);
    }
}

//---------------------------------------------------------
//   ConnectorInfoReader::addToScore
//---------------------------------------------------------

class ReadAddConnectorVisitor : public rtti::Visitor<ReadAddConnectorVisitor> {
public:

    template <typename T>
    static bool do_visit(EngravingObject *item, ConnectorInfoReader *info, bool paste_mode) {
        if (T::classof(item) || (std::is_same<T, ChordRest>::value && item->is_chord_rest())) {
            ConnectorInfoReader::read_add_connector(static_cast<T *>(item), info, paste_mode);
            return true;
        }
        return false;
    }
};

using ReadAddConnectorTypes = rtti::TypeList<ChordRest, Measure, Note, Score>;

void ConnectorInfoReader::add_to_score(bool paste_mode) {
    ConnectorInfoReader *r = this;
    while (r->prev()) {
        r = r->prev();
    }
    while (r) {
        bool found = ReadAddConnectorVisitor::visit(ReadAddConnectorTypes{}, r->connector_receiver_, r, paste_mode);
        DO_ASSERT(found);
        r = r->next();
    }
}

//---------------------------------------------------------
//   ConnectorInfoReader::readConnector
//---------------------------------------------------------

void ConnectorInfoReader::read_connector(std::shared_ptr<ConnectorInfoReader> info, XmlReader &e, ReadContext &ctx) {
    if (!info->read()) {
        e.skip_current_element();
        return;
    }
    ctx.add_connector_info_later(info);
}

//---------------------------------------------------------
//   ConnectorInfoReader::connector
//---------------------------------------------------------

EngravingItem *ConnectorInfoReader::connector() {
    // connector should be contained in the first node normally.
    ConnectorInfo *i = find_first();
    if (i) {
        return static_cast<ConnectorInfoReader *>(i)->connector_;
    }
    return nullptr;
}

//---------------------------------------------------------
//   ConnectorInfoReader::connector
//---------------------------------------------------------

const EngravingItem *ConnectorInfoReader::connector() const { return const_cast<ConnectorInfoReader *>(this)->connector(); }

//---------------------------------------------------------
//   ConnectorInfoReader::releaseConnector
//---------------------------------------------------------

EngravingItem *ConnectorInfoReader::release_connector() {
    ConnectorInfoReader *i = static_cast<ConnectorInfoReader *>(find_first());
    if (!i) {
        // circular connector?
        ConnectorInfoReader *ii = this;
        EngravingItem *c = nullptr;
        while (ii->prev()) {
            if (ii->connector_) {
                c = ii->connector_;
                ii->connector_ = nullptr;
            }
            ii = ii->prev();
            if (ii == this) {
                break;
            }
        }
        return c;
    }
    EngravingItem *c = i->connector_;
    i->connector_ = nullptr;
    return c;
}

void ConnectorInfoReader::read_add_connector(ChordRest *item, ConnectorInfoReader *info, bool paste_mode) {
    const ElementType type = info->type();
    switch (type) {
        case ElementType::SLUR:
        case ElementType::HAMMER_ON_PULL_OFF: {
            Spanner *spanner = to_spanner(info->connector());
            const Location &l = info->location();

            if (info->is_start()) {
                spanner->set_track(l.track());
                // trillCueNotes have unreliable tick() while reading so use instead tick from readContext
                Fraction start_tick = item->is_chord() && to_chord(item)->is_trill_cue_note() ? info->cur_tick() : item->tick();
                spanner->set_tick(start_tick);
                spanner->set_start_element(item);
                if (paste_mode) {
                    item->score()->undo_add_element(spanner);
                    for (EngravingObject *linked_spanner : spanner->link_list()) {
                        if (linked_spanner == spanner) {
                            continue;
                        }
                        Spanner *ls = to_spanner(linked_spanner);
                        ls->set_tick(spanner->tick());
                        for (EngravingObject *linked_cr : item->link_list()) {
                            ChordRest *cr = to_chord_rest(linked_cr);
                            if (cr->score() == linked_spanner->score() && cr->staff_idx() == ls->staff_idx()) {
                                ls->set_track(cr->track());
                                if (ls->is_slur()) {
                                    ls->set_start_element(cr);
                                }
                                break;
                            }
                        }
                    }
                } else {
                    item->score()->add_spanner(spanner);
                }
            } else if (info->is_end()) {
                spanner->set_track2(l.track());
                spanner->set_tick2(item->tick());
                spanner->set_end_element(item);
                if (paste_mode) {
                    for (EngravingObject *linked_spanner : spanner->link_list()) {
                        if (linked_spanner == spanner) {
                            continue;
                        }
                        Spanner *ls = to_spanner(linked_spanner);
                        ls->set_tick2(spanner->tick2());
                        for (EngravingObject *linked_cr : item->link_list()) {
                            ChordRest *cr = to_chord_rest(linked_cr);
                            if (cr->score() == linked_spanner->score() && cr->staff_idx() == ls->staff_idx()) {
                                ls->set_track2(cr->track());
                                if (ls->is_slur()) {
                                    ls->set_end_element(cr);
                                }
                                break;
                            }
                        }
                    }
                }
            } else {
                LOGD("ChordRest::readAddConnector(): Slur end is neither start nor end");
            }
        } break;
        default: break;
    }
}

void ConnectorInfoReader::read_add_connector(Measure *item, ConnectorInfoReader *info, bool paste_mode) {
    const ElementType type = info->type();
    switch (type) {
        case ElementType::HAIRPIN:
        case ElementType::PEDAL:
        case ElementType::OTTAVA:
        case ElementType::TRILL:
        case ElementType::TEXTLINE:
        case ElementType::LET_RING:
        case ElementType::GRADUAL_TEMPO_CHANGE:
        case ElementType::VIBRATO:
        case ElementType::PALM_MUTE:
        case ElementType::PARTIAL_LYRICSLINE:
        case ElementType::WHAMMY_BAR:
        case ElementType::RASGUEADO:
        case ElementType::HARMONIC_MARK:
        case ElementType::PICK_SCRAPE:
        case ElementType::VOLTA: {
            Spanner *sp = to_spanner(info->connector());
            const Location &l = info->location();
            Fraction l_tick = l.frac();
            Fraction sp_tick = paste_mode ? l_tick : (item->tick() + l_tick);
            if (info->is_start()) {
                sp->set_track(l.track());
                sp->set_track2(sp->track());
                sp->set_tick(sp_tick);
                // Defer computing the end element: the end anchor (tick2/track2) is only known once the
                // matching end connector is processed (below). Computing it here would cache a wrong value
                // (tick2 == tick) that nothing recomputes until layout. See readAddConnector end branch.
                item->score()->add_spanner(sp, /*computeStartEnd=*/false);
                sp->compute_start_element();
            } else if (info->is_end()) {
                sp->set_track2(l.track());
                sp->set_tick2(sp_tick);
                // Now that the end anchor is set, compute the end element once with the final value.
                sp->compute_end_element();
            }
        } break;
        default: break;
    }
}

void ConnectorInfoReader::read_add_connector(Note *item, ConnectorInfoReader *info, bool paste_mode) {
    const ElementType type = info->type();
    const Location &l = info->location();
    switch (type) {
        case ElementType::TIE:
        case ElementType::TEXTLINE:
        case ElementType::GLISSANDO:
        case ElementType::GUITAR_BEND:
        case ElementType::NOTELINE: {
            Spanner *sp = to_spanner(info->connector());
            if (info->is_start()) {
                sp->set_track(l.track());
                sp->set_tick(item->tick());
                if (sp->is_tie()) {
                    Note *n = item;
                    while (n->tie_for()) {
                        n = n->tie_for()->end_note();
                    }
                    Tie *tie = to_tie(sp);
                    tie->set_ownership_parent(n);
                    tie->set_start_note(n);
                    n->set_tie_for(tie);
                } else {
                    sp->set_start_element(item);
                    item->add_spanner_for(sp);
                    sp->set_ownership_parent(item);
                }
            } else if (info->is_end()) {
                sp->set_track2(l.track());
                sp->set_tick2(item->tick());
                sp->set_end_element(item);
                if (sp->is_tie()) {
                    Tie *tie = to_tie(sp);
                    item->set_tie_back(tie);
                    if (paste_mode) {
                        tie->update_possible_jump_points();
                    }
                } else {
                    bool is_note_anchored_text_line = sp->is_note_line() && to_note_line(sp)->enforce_min_length();
                    if ((sp->is_glissando() || sp->is_guitar_bend() || is_note_anchored_text_line) && item->ownership_parent() &&
                        item->ownership_parent()->is_chord()) {
                        to_chord(item->ownership_parent())->set_ends_note_anchored_line(true);
                    }
                    item->add_spanner_back(sp);
                }

                // As spanners get added after being fully read, they
                // do not get cloned with the note when pasting to
                // linked staves. So add this spanner explicitly.
                if (paste_mode) {
                    item->score()->undo_add_element(sp);
                }
            }
        }
        default: break;
    }
}

void ConnectorInfoReader::read_add_connector(Score *item, ConnectorInfoReader *info, bool paste_mode) {
    if (!paste_mode) {
        // How did we get there?
        LOGD("Score::readAddConnector is called not in paste mode.");
        return;
    }

    if (info->connector()->system_flag()) {
        return;
    }

    const ElementType type = info->type();
    switch (type) {
        case ElementType::HAIRPIN:
        case ElementType::PEDAL:
        case ElementType::OTTAVA:
        case ElementType::TRILL:
        case ElementType::TEXTLINE:
        case ElementType::PALM_MUTE:
        case ElementType::PARTIAL_LYRICSLINE:
        case ElementType::WHAMMY_BAR:
        case ElementType::RASGUEADO:
        case ElementType::HARMONIC_MARK:
        case ElementType::PICK_SCRAPE:
        case ElementType::LET_RING:
        case ElementType::VIBRATO: {
            Spanner *sp = to_spanner(info->connector());
            const Location &l = info->location();
            if (info->is_start()) {
                sp->set_track(l.track());
                sp->set_track2(l.track());
                sp->set_tick(l.frac());
            } else if (info->is_end()) {
                sp->set_tick2(l.frac());
                item->undo_add_element(sp);
                if (sp->is_ottava()) {
                    sp->staff()->update_ottava();
                }
            }
        } break;
        default: break;
    }
}
