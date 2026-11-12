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
std::expected<music::ScoreProject, std::string> open_mnx(music::EngineContext &context, const std::string &bytes) {
    // MuseScore cannot target grace notes or voices with ottavas.
    const mu::iex::mnxio::model::IndexOptions options{.ottavas_respect_grace_targets = false, .ottavas_respect_voice_targets = false};
    auto doc = mu::iex::mnxio::model::load_document(bytes, options);
    if (!doc) return std::unexpected(std::move(doc.error().message));
    if (doc->root.global.measures.empty() || doc->root.parts.empty())
        return std::unexpected(std::string("the score has no measures or no parts"));
    mu::engraving::ScoreLoad loading;
    auto owned = mu::engraving::EngravingProject::create(context.engraving());
    owned->set_file_info_provider(std::make_shared<mu::engraving::LocalFileInfoProvider>("Untitled.mnx"));
    mu::iex::mnxio::MnxImporter importer(owned->master_score(), *std::move(doc));
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
    score->init_automation();
    return owned;
}
} // namespace engrave
