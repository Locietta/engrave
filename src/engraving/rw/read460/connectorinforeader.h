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
#pragma once

#include "../xmlreader.h"
#include "../../dom/connector.h"

//---------------------------------------------------------
//   @@ ConnectorInfoReader
///    Helper class for reading beams, tuplets and spanners.
//---------------------------------------------------------

namespace mu::engraving {
struct ChordRest;
struct Measure;
struct Note;
} // namespace mu::engraving

namespace mu::engraving::read460 {
struct ReadContext;

struct ConnectorInfoReader final : ConnectorInfo {
private:
    OBJECT_ALLOCATOR(engraving, ConnectorInfoReader)

public:
    ConnectorInfoReader(XmlReader &e, ReadContext *ctx, EngravingItem *current, int track = -1);
    ConnectorInfoReader(XmlReader &e, ReadContext *ctx, Score *current, int track = -1);

    ConnectorInfoReader *prev() const { return static_cast<ConnectorInfoReader *>(prev_); }

    ConnectorInfoReader *next() const { return static_cast<ConnectorInfoReader *>(next_); }

    EngravingItem *connector();
    const EngravingItem *connector() const;
    EngravingItem *release_connector(); // returns connector and "forgets" it by
    // setting an internal pointer to it to zero

    bool read();
    void update();
    void add_to_score(bool paste_mode);

    static void read_connector(std::shared_ptr<ConnectorInfoReader> info, XmlReader &e, ReadContext &ctx);

    static void read_add_connector(ChordRest *item, ConnectorInfoReader *info, bool paste_mode);
    static void read_add_connector(Measure *item, ConnectorInfoReader *info, bool paste_mode);
    static void read_add_connector(Note *item, ConnectorInfoReader *info, bool paste_mode);
    static void read_add_connector(Score *item, ConnectorInfoReader *info, bool paste_mode);

private:
    void read_endpoint_location(Location &l);
    Fraction cur_tick() const;

    XmlReader *reader_ = nullptr;
    ReadContext *ctx_ = nullptr;
    EngravingItem *connector_ = nullptr;
    EngravingObject *connector_receiver_ = nullptr;
};
} // namespace mu::engraving::read460
