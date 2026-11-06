/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include "../imnxconfiguration.h"

namespace mu::iex::mnxio {
class MnxConfiguration : public IMnxConfiguration {
public:
    void init();

    int mnx_indent_spaces() const override;
    void set_mnx_indent_spaces(int value) override;

    bool mnx_export_beams() const override;
    void set_mnx_export_beams(bool value) override;

    bool mnx_export_rest_positions() const override;
    void set_mnx_export_rest_positions(bool value) override;

    bool mnx_require_exact_schema_validation() const override;
    void set_mnx_require_exact_schema_validation(bool value) override;
};
} // namespace mu::iex::mnxio
