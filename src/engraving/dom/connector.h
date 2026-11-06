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

#ifndef MU_ENGRAVING_CONNECTOR_H
#define MU_ENGRAVING_CONNECTOR_H

#include <climits>

#include "global/allocator.h"

#include "location.h"
#include "../types/types.h"

namespace mu::engraving {
class EngravingItem;
class EngravingObject;
class Score;

//---------------------------------------------------------
//   @@ ConnectorInfo
///    Stores a general information on various connecting
///    elements (currently only spanners) including their
///    endpoints locations.
///    Base class of helper classes used to read and write
///    such elements.
//---------------------------------------------------------

class ConnectorInfo {
    OBJECT_ALLOCATOR(engraving, ConnectorInfo)

public:
    ConnectorInfo(const EngravingItem *current, int track = -1, Fraction = {-1, 1});
    ConnectorInfo(const Score *score, const Location &current_location);
    virtual ~ConnectorInfo() = default;

    ConnectorInfo *prev() const { return prev_; }

    ConnectorInfo *next() const { return next_; }

    ConnectorInfo *start();
    ConnectorInfo *end();

    ElementType type() const { return type_; }

    const Location &location() const { return current_loc_; }

    bool connect(ConnectorInfo *other);
    bool finished() const;

    // for reconnection of broken connectors
    int connection_distance(const ConnectorInfo &c2) const;
    void force_connect(ConnectorInfo *c2);

    bool has_previous() const { return prev_loc_.measure() != INT_MIN; }

    bool has_next() const { return next_loc_.measure() != INT_MIN; }

    bool is_start() const { return !has_previous() && has_next(); }

    bool is_middle() const { return has_previous() && has_next(); }

    bool is_end() const { return has_previous() && !has_next(); }

protected:

    void update_location(const EngravingItem *e, Location &i, bool clipboardmode);
    void update_current_info(bool clipboardmode);

    bool current_updated() const { return current_updated_; }

    void set_current_updated(bool v) { current_updated_ = v; }

    ConnectorInfo *find_first();
    const ConnectorInfo *find_first() const;
    ConnectorInfo *find_last();
    const ConnectorInfo *find_last() const;

    ElementType type_ = ElementType::INVALID;
    Location current_loc_;
    Location prev_loc_ = Location::absolute();
    Location next_loc_ = Location::absolute();

    ConnectorInfo *prev_ = nullptr;
    ConnectorInfo *next_ = nullptr;

private:
    bool finished_left() const;
    bool finished_right() const;

    static int ordered_connection_distance(const ConnectorInfo &c1, const ConnectorInfo &c2);

    const EngravingItem *current_ = nullptr;
    bool current_updated_ = false;
    const Score *score_ = nullptr;
};
} // namespace mu::engraving
#endif
