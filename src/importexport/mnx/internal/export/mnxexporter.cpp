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

#include "mnxexporter.h"
#include "internal/shared/mnxtypesconv.h"

#include <stdexcept>

#include "global/iapplication.h"
#include "global/io/path.h"
#include "global/log.h"
#include "global/appinfo.h"
#include "global/translation.h"

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/score.h"

using namespace mu::engraving;

namespace mu::iex::mnxio {
//---------------------------------------------------------
//   getOrAssignEID
//---------------------------------------------------------

EID MnxExporter::get_or_assign_eid(EngravingObject *item) {
    EID eid = item->eid();
    if (!eid.is_valid()) {
        eid = item->assign_new_eid();
    }
    return eid;
}

//---------------------------------------------------------
//   mnxEventFromCR
//---------------------------------------------------------

std::optional<mnx::sequence::Event> MnxExporter::mnx_event_from_cr(const engraving::ChordRest *cr) {
    auto pointer = muse::value(cr_to_mnx_event_, cr);
    if (!pointer.empty()) {
        return mnx::sequence::Event(mnx_document().root(), pointer);
    }
    return std::nullopt;
}

//---------------------------------------------------------
//   mnxMeasureIndexFromMeasure
//---------------------------------------------------------

size_t MnxExporter::mnx_measure_index_from_measure(const engraving::Measure *measure) const {
    IF_ASSERT_FAILED(measure) { throw std::logic_error("Measure is null while resolving MNX measure index."); }
    const size_t result = muse::value(meas_to_mnx_meas_, measure, muse::nidx);
    IF_ASSERT_FAILED(result != muse::nidx) { throw std::logic_error("Measure is not mapped to an MNX measure index."); }
    return result;
}

//---------------------------------------------------------
//   mnxPartStaffFromStaffIdx
//---------------------------------------------------------

std::pair<size_t, int> MnxExporter::mnx_part_staff_from_staff_idx(engraving::staff_idx_t staff_idx) const {
    const auto it = staff_to_part_staff_.find(staff_idx);
    IF_ASSERT_FAILED(it != staff_to_part_staff_.end()) { throw std::logic_error("Staff index is not mapped to an MNX part/staff."); }
    return it->second;
}

//---------------------------------------------------------
//   mnxFermataFromFermata
//---------------------------------------------------------

mnx::Fermata MnxExporter::mnx_fermata_from_fermata(const Fermata *fermata) {
    mnx::Fermata result;
    result.set_or_clear_duration(to_mnx_fermata_duration(fermata->fermata_type()));
    result.set_or_clear_symbol(to_mnx_fermata_symbol(fermata->sym_id()));
    result.set_or_clear_orient(to_mnx_orientation(fermata->placement()));
    return result;
}

//---------------------------------------------------------
//   mnxChordTargetPosition
//   calculate exact MNX position for an object targeting
//   a chord, including grace-note targets
//---------------------------------------------------------

std::optional<MnxChordTargetPosition> MnxExporter::mnx_chord_target_position(const Chord *chord, const Measure *measure) {
    IF_ASSERT_FAILED(chord && measure) { return std::nullopt; }

    auto position_from_tick = [measure](const Fraction &tick) {
        return MnxChordTargetPosition{to_mnx_fraction_value(tick - measure->tick()).reduced(), std::nullopt};
    };

    if (!chord->is_grace()) {
        MnxChordTargetPosition position = position_from_tick(chord->tick());
        if (!chord->grace_notes().empty()) {
            position.grace_index = 0;
        }
        return position;
    }

    EngravingObject *parent = chord->ownership_parent();
    if (!parent || !parent->is_chord()) {
        LOGW() << "Skipping grace-note target with missing main chord.";
        return std::nullopt;
    }

    const Chord *main_chord = to_chord(parent);
    const GraceNotesGroup &grace_notes = chord->is_grace_after() ? main_chord->grace_notes_after() : main_chord->grace_notes_before();
    const auto grace_it = std::find(grace_notes.begin(), grace_notes.end(), chord);
    if (grace_it == grace_notes.end()) {
        LOGW() << "Skipping grace-note target whose grace index could not be resolved.";
        return std::nullopt;
    }

    const size_t index = static_cast<size_t>(std::distance(grace_notes.begin(), grace_it));
    MnxChordTargetPosition position =
        position_from_tick(chord->is_grace_after() ? main_chord->tick() + main_chord->ticks() : main_chord->tick());
    position.grace_index = static_cast<unsigned>(grace_notes.size() - index);
    return position;
}

//---------------------------------------------------------
//   exportMnx
//---------------------------------------------------------

muse::Ret MnxExporter::export_mnx() {
    LOGI() << "MNX export started: schema version=" << mnx_document_.mnx().version();
    // Header
    auto client = mnx_document_.mnx().ensure_mnxdom().ensure_client();
    muse::String version = muse::appinfo::version().to_string();
    const muse::String revision = muse::appinfo::revision();
    if (!revision.is_empty()) {
        version += u" rev. " + revision;
    }
    client.set_name("engrave");
    client.set_version(version.to_std_string());
    if (score_ && score_->master_score()) {
        IFileInfoProviderPtr file_info = score_->master_score()->file_info();
        if (file_info) {
            const muse::io::PathT file_name = file_info->file_name();
            const std::string format = muse::io::suffix(file_name);
            if (!format.empty() || !file_name.empty()) {
                auto source = mnx_document_.mnx().ensure_mnxdom().ensure_source();
                if (!format.empty()) {
                    source.set_format(format);
                }
                if (!file_name.empty()) {
                    source.set_filename(file_name.to_std_string());
                }
            }
        }
    }

    if (export_beams_) {
        mnx::MnxMetaData::Support support = mnx_document_.mnx().ensure_support();
        support.set_useBeams(true);
    }

    create_global();
    if (!create_parts()) {
        const String msg =
            muse::TranslatableString("importexport/mnx",
                                     "MNX export skipped because the score contains no exportable parts. (Tablature is not supported yet).")
                .str;
        return muse::make_ret(muse::Ret::Code::NOT_SUPPORTED, msg);
    }
    create_layout(exported_staves_, "full-score");
    /// @todo Creation of all layouts and scores, including excerpts. (Deferred to a future dev cycle.)
    return muse::make_ok();
}
} // namespace mu::iex::mnxio
