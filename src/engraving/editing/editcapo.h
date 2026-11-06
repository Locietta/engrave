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

#include <functional>
#include "../types/types.h"

namespace mu::engraving {
class Note;

class Staff;
class StringData;

class EditCapo {
public:
    static void update_notation_for_capo_change(const CapoParams &old_params, const CapoParams &new_params, const Staff *staff,
                                                int start_tick, int end_tick);

private:
    struct UpdateCtx {
        const Staff *staff = nullptr;
        const StringData *string_data = nullptr;
        CapoParams params = {};
        int note_offset = 0;
        bool update_ignored_strings = false;
        bool possible_fret_conflict = false;
    };

    static void apply_capo_transpose(int start_tick, int end_tick, UpdateCtx &ctx);
    static void update(Note *note, const UpdateCtx &ctx);
    static void update_string(Note *note, const UpdateCtx &ctx);

    static void handle_mode_change(const CapoParams &old_params, const CapoParams &new_params, int start_tick, int end_tick,
                                   UpdateCtx &ctx);
    static void handle_fret_change(const CapoParams &old_params, const CapoParams &new_params, int start_tick, int end_tick,
                                   UpdateCtx &ctx);
    static void handle_string_changed(const CapoParams &new_params, int start_tick, int end_tick, UpdateCtx &ctx);
    static void handle_active_changed(const CapoParams &new_params, int start_tick, int end_tick, UpdateCtx &ctx);
};
} // namespace mu::engraving
