// SPDX-License-Identifier: GPL-3.0-only
#include "mnx.h"

#include "engrave/src/runtime/engine_context.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/part.h"
#include "engraving/engravingproject.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include "importexport/mnx/internal/import/mnximporter.h"

namespace engrave {
music::ScoreProject open_mnx(music::EngineContext &context, const std::string &bytes) {
    auto doc = mnx::Document::create(bytes.data(), bytes.size());
    if (!mnx::validation::schemaValidate(doc) || doc.global().measures().empty() || doc.parts().empty())
        throw std::invalid_argument("Invalid MNX schema or empty score");
    mu::engraving::ScoreLoad loading;
    auto owned = mu::engraving::EngravingProject::create(context.engraving());
    owned->set_file_info_provider(std::make_shared<mu::engraving::LocalFileInfoProvider>("Untitled.mnx"));
    mu::iex::mnxio::MnxImporter importer(owned->master_score(), std::move(doc));
    importer.import_mnx();
    auto &project = *owned;

    auto *score = project.master_score();
    score->connect_ties();
    // Match the runtime post-load MIDI wiring before layout or playback.
    for (auto *part : score->parts()) part->update_harmony_channels(false);
    score->rebuild_midi_mapping();
    score->update_channel();
    const auto result = project.setup_master_score(true);
    if (!result) throw std::runtime_error(result.to_string());
    // Keep imported programs and banks stable across native save/reopen.
    for (auto *part : score->parts()) {
        for (const auto &pair : part->instruments()) {
            for (auto *channel : pair.second->channel()) {
                if (auto *playback = score->playback_channel(channel)) {
                    playback->set_program(channel->program());
                    playback->set_bank(channel->bank());
                }
            }
        }
    }
    score->set_layout_all();
    score->update();
    return owned;
}
} // namespace engrave
