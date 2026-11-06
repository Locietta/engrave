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

#ifndef MU_ENGRAVING_LINKSINDEXER_H
#define MU_ENGRAVING_LINKSINDEXER_H

#include "../dom/location.h"

namespace mu::engraving {
class LinksIndexer {
public:

    inline bool operator==(const LinksIndexer &i) const {
        return last_local_index == i.last_local_index && last_linked_element_loc == i.last_linked_element_loc;
    }

    inline bool operator!=(const LinksIndexer &i) const { return !this->operator==(i); }

    int assign_local_index(const Location &main_element_location) {
        if (last_linked_element_loc == main_element_location) {
            return ++last_local_index;
        }

        last_local_index = 0;
        last_linked_element_loc = main_element_location;

        return 0;
    }

private:
    int last_local_index = -1;
    Location last_linked_element_loc = Location::absolute();
};
} // namespace mu::engraving

#endif // MU_ENGRAVING_LINKSINDEXER_H
