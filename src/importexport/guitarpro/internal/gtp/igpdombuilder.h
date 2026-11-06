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
#ifndef MU_IMPORTEXPORT_IGPDOMBUILDER_H
#define MU_IMPORTEXPORT_IGPDOMBUILDER_H

#include <memory>

#include "gpdommodel.h"
#include "serialization/xmldom.h"

namespace mu::iex::guitarpro {
struct IGPDomBuilder {
    virtual ~IGPDomBuilder() = default;
    virtual void build_gp_dom_model(muse::XmlDomElement *dom_elem) = 0;
    virtual std::unique_ptr<GPDomModel> get_gp_dom_model() = 0;
};
} // namespace mu::iex::guitarpro
#endif // MU_IMPORTEXPORT_IGPDOMBUILDER_H
