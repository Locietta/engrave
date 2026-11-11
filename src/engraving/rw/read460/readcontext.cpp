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

#include "readcontext.h"

#include <climits>

#include "connectorinforeader.h"
#include "dom/linkedobjects.h"
#include "dom/masterscore.h"
#include "dom/measure.h"
#include "dom/score.h"
#include "dom/trill.h"
#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::read460;

ReadContext::ReadContext(Score *score) : score_(score) {}

ReadContext::~ReadContext() {
    if (!connectors.empty() || !pending_connectors.empty()) {
        LOGD("XmlReader::~XmlReader: there are unpaired connectors left");
        for (auto &c : connectors) {
            EngravingItem *conn = c->release_connector();
            if (conn && !conn->is_tuplet()) { // tuplets are added to score even when not finished
                delete conn;
            }
        }
        for (auto &c : pending_connectors) {
            delete c->release_connector();
        }
    }
}

Score *ReadContext::score() const { return score_; }

void ReadContext::set_score(Score *score) { score_ = score; }

const MStyle &ReadContext::style() const { return score()->style(); }

std::shared_ptr<EngravingFontsProvider> ReadContext::engraving_fonts() const { return score()->engraving_fonts(); }

String ReadContext::mscore_version() const { return score_->mscore_version(); }

int ReadContext::msc_version() const { return score_->msc_version(); }

int ReadContext::file_division() const { return score_->file_division(); }

int ReadContext::file_division(int t) const { return score_->file_division(t); }

double ReadContext::spatium() const { return score_->style().spatium(); }

void ReadContext::set_spatium(double v) { score_->style().set(Sid::SPATIUM, v); }

DummyParent *ReadContext::dummy() const { return score_->dummy(); }

Staff *ReadContext::staff(staff_idx_t n) { return score_->staff(n); }

bool ReadContext::is_same_score(const EngravingObject *obj) const { return obj->score() == score_; }

Fraction ReadContext::rtick() const { return cur_measure ? tick_value - cur_measure->tick() : tick_value; }

void ReadContext::set_tick(const Fraction &f) {
    tick_value = f.reduced();
    int_tick = tick_value.ticks();
}

void ReadContext::inc_tick(const Fraction &f) {
    tick_value += f;
    tick_value.reduce();
    int_tick += f.ticks();
}

Location ReadContext::location(bool force_abs_frac) const {
    Location l = Location::absolute();
    fill_location(l, force_abs_frac);
    return l;
}

//---------------------------------------------------------
//   fillLocation
//    fills location fields which have default values with
//    values relevant for the current reader's position.
//    When in paste mode (or forceAbsFrac is true) absolute
//    fraction values are used and measure number is set to
//    zero.
//---------------------------------------------------------

void ReadContext::fill_location(Location &l, bool force_abs_frac) const {
    constexpr Location defaults = Location::absolute();
    const bool abs_frac = (paste_mode() || force_abs_frac);
    if (l.track() == defaults.track()) {
        l.set_track(static_cast<int>(track()));
    }
    if (l.frac() == defaults.frac()) {
        l.set_frac(abs_frac ? tick() : rtick());
    }
    if (l.measure() == defaults.measure()) {
        l.set_measure(abs_frac ? 0 : current_measure_index());
    }
}

//---------------------------------------------------------
//   setLocation
//    sets a new reading location, taking into account its
//    type (absolute or relative).
//---------------------------------------------------------

void ReadContext::set_location(const Location &l) {
    if (l.is_relative()) {
        Location new_loc = l;
        new_loc.to_absolute(location());
        int int_ticks = l.frac().ticks();
        if (tick_value == Fraction::from_ticks(int_tick + int_ticks)) {
            int_tick += int_ticks;
            set_track(new_loc.track() - track_offset_value);
            return;
        }
        set_location(new_loc); // recursion
        return;
    }
    set_track(l.track() - track_offset_value);
    set_tick(l.frac() - tick_offset_value);
    if (!paste_mode()) {
        assert(l.measure() == current_measure_index());
        inc_tick(current_measure()->tick());
    }
}

void ReadContext::remove_connector(const ConnectorInfoReader *c) {
    while (c->prev()) {
        c = c->prev();
    }
    while (c) {
        ConnectorInfoReader *next = c->next();
        for (auto it = connectors.begin(); it != connectors.end(); ++it) {
            if (it->get() == c) {
                connectors.erase(it);
                break;
            }
        }
        c = next;
    }
}

void ReadContext::add_connector_info_later(std::shared_ptr<ConnectorInfoReader> c) { pending_connectors.push_back(c); }

void ReadContext::check_connectors() {
    for (std::shared_ptr<ConnectorInfoReader> &c : pending_connectors) {
        add_connector_info(c);
    }
    pending_connectors.clear();
}

void ReadContext::add_connector_info(std::shared_ptr<ConnectorInfoReader> c) {
    connectors.push_back(std::move(c));
    ConnectorInfoReader *c1 = connectors.back().get();
    c1->update();
    for (std::shared_ptr<ConnectorInfoReader> &c2 : connectors) {
        if (c2->connect(c1)) {
            if (c2->finished()) {
                c2->add_to_score(paste_mode());
                remove_connector(c2.get());
            }
            break;
        }
    }
}

static bool distance_sort(const std::pair<int, std::pair<ConnectorInfoReader *, ConnectorInfoReader *>> &p1,
                          const std::pair<int, std::pair<ConnectorInfoReader *, ConnectorInfoReader *>> &p2) {
    return p1.first < p2.first;
}

//---------------------------------------------------------
//   reconnectBrokenConnectors
//---------------------------------------------------------

void ReadContext::reconnect_broken_connectors() {
    if (connectors.empty()) {
        return;
    }
    LOGD("Reconnecting broken connectors (%d nodes)", int(connectors.size()));
    std::vector<std::pair<int, std::pair<ConnectorInfoReader *, ConnectorInfoReader *>>> broken_pairs;
    for (size_t i = 1; i < connectors.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            ConnectorInfoReader *c1 = connectors[i].get();
            ConnectorInfoReader *c2 = connectors[j].get();
            int d = c1->connection_distance(*c2);
            if (d >= 0) {
                broken_pairs.push_back(std::make_pair(d, std::make_pair(c1, c2)));
            } else {
                broken_pairs.push_back(std::make_pair(-d, std::make_pair(c2, c1)));
            }
        }
    }
    std::sort(broken_pairs.begin(), broken_pairs.end(), distance_sort);
    std::set<ConnectorInfoReader *> processed;
    for (auto &dist_pair : broken_pairs) {
        if (dist_pair.first == INT_MAX) {
            continue;
        }
        auto &pair = dist_pair.second;
        if (processed.count(pair.first) || processed.count(pair.second)) {
            continue;
        }
        pair.first->force_connect(pair.second);
        processed.insert(pair.first);
        processed.insert(pair.second);
    }
    std::set<ConnectorInfoReader *> reconnected;
    for (auto &conn : connectors) {
        ConnectorInfoReader *c = conn.get();
        if (c->finished()) {
            reconnected.insert(static_cast<ConnectorInfoReader *>(c->start()));
        }
    }
    for (ConnectorInfoReader *cptr : reconnected) {
        cptr->add_to_score(paste_mode());
        remove_connector(cptr);
    }
    LOGD() << "reconnected broken connectors: " << reconnected.size();
}

void ReadContext::clear_orphaned_connectors() {
    if (connectors.empty() && pending_connectors.empty()) {
        return;
    }

    LOGD("XmlReader::~XmlReader: there are unpaired connectors left");

    std::set<LinkedObjects *> deleted_links;

    auto delete_connectors = [&deleted_links](std::shared_ptr<ConnectorInfoReader> c) {
        EngravingItem *conn = c ? c->release_connector() : nullptr;
        if (!conn) {
            return;
        }

        LinkedObjects *links = conn->links();
        bool links_will_be_deleted = links && links->size() == 1;

        if (!conn->is_tuplet()) { // tuplets are added to score even when not finished
            if (links_will_be_deleted) {
                deleted_links.insert(links);
                if (conn->is_trill()) {
                    EngravingItem *ornament = (EngravingItem *) to_trill(conn)->ornament();
                    if (ornament && ornament->links()) {
                        deleted_links.insert(ornament->links());
                    }
                }
            }

            if (conn->eid().is_valid()) {
                conn->master_score()->eid_register()->remove_item(conn);
            }

            delete conn;
        }
    };

    if (!connectors.empty()) {
        for (auto &c : connectors) {
            delete_connectors(c);
        }
        connectors.clear();
    }

    if (!pending_connectors.empty()) {
        for (auto &c : pending_connectors) {
            delete_connectors(c);
        }
        pending_connectors.clear();
    }
}

void ReadContext::add_part_audio_setting_compat(PartAudioSettingsCompat part_audio_setting) {
    if (settings_compat.audio_settings.count(part_audio_setting.instrument_id.part_id) == 0) {
        settings_compat.audio_settings.insert({part_audio_setting.instrument_id.part_id, part_audio_setting});
    }
}
