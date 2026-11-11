#include "engine_context.h"

#include <cstdio>
#include <filesystem>
#include <stdexcept>

#include "draw/fontmetrics.h"
#include "draw/internal/ifontsdatabase.h"
#include "engrave/src/audio/engine_audio.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/figuredbass.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/mscore.h"
#include "engraving/dom/stafftype.h"
#include "engraving/engravingservices.h"
#include "engraving/infrastructure/smufl.h"
#include "engraving/internal/engravingconfiguration.h"
#include "engraving/internal/engravingcontextconfiguration.h"
#include "engraving/internal/engravingfontsprovider.h"
#include "engraving/rendering/score/scorerenderer.h"
#include "engraving/style/defaultstyle.h"
#include "importexport/musicxml/internal/musicxmlconfiguration.h"
#include "mpe/internal/articulationprofilesrepository.h"
#include "resource_filesystem.h"
#include "runtime_assets.h"
#include "score_io.h"

namespace music {
using namespace muse;
using namespace muse::draw;
using namespace mu::engraving;

namespace {
// The built-in text, music and tablature faces and their defaults (upstream's
// EngravingModule::onInit).
void register_builtin_fonts(IFontsDatabase &fdb, EngravingFontsProvider &engraving_fonts) {
    // Text
    fdb.add_font(FontDataKey(u"Edwin", false, false), ":/fonts/edwin/Edwin-Roman.otf");
    fdb.add_font(FontDataKey(u"Edwin", false, true), ":/fonts/edwin/Edwin-Italic.otf");
    fdb.add_font(FontDataKey(u"Edwin", true, false), ":/fonts/edwin/Edwin-Bold.otf");
    fdb.add_font(FontDataKey(u"Edwin", true, true), ":/fonts/edwin/Edwin-BdIta.otf");

    // MusicSymbol[Text]
    auto add_music_font = [&](const std::string &name, const FontDataKey &font_data_key, const io::PathT &file_path) {
        fdb.add_font(FontDataKey(font_data_key), file_path);
        engraving_fonts.add_internal_font(name, font_data_key.family().id().to_std_string(), file_path);
    };

    add_music_font("Bravura", FontDataKey(u"Bravura"), ":/fonts/bravura/Bravura.otf");
    fdb.add_font(FontDataKey(u"Bravura Text"), ":/fonts/bravura/BravuraText.otf");
    add_music_font("Leland", FontDataKey(u"Leland"), ":/fonts/leland/Leland.otf");
    fdb.add_font(FontDataKey(u"Leland Text"), ":/fonts/leland/LelandText.otf");
    add_music_font("Emmentaler", FontDataKey(u"MScore"), ":/fonts/mscore/MScore.otf");
    fdb.add_font(FontDataKey(u"MScore Text"), ":/fonts/mscore/MScoreText.otf");
    add_music_font("Gonville", FontDataKey(u"Gootville"), ":/fonts/gootville/Gootville.otf");
    fdb.add_font(FontDataKey(u"Gootville Text"), ":/fonts/gootville/GootvilleText.otf");
    add_music_font("MuseJazz", FontDataKey(u"MuseJazz"), ":/fonts/musejazz/MuseJazz.otf");
    fdb.add_font(FontDataKey(u"MuseJazz Text"), ":/fonts/musejazz/MuseJazzText.otf");
    add_music_font("Petaluma", FontDataKey(u"Petaluma"), ":/fonts/petaluma/Petaluma.otf");
    fdb.add_font(FontDataKey(u"Petaluma Text"), ":/fonts/petaluma/PetalumaText.otf");
    add_music_font("Finale Maestro", FontDataKey(u"Finale Maestro"), ":/fonts/finalemaestro/FinaleMaestro.otf");
    fdb.add_font(FontDataKey(u"Finale Maestro Text"), ":/fonts/finalemaestro/FinaleMaestroText.otf");
    add_music_font("Finale Broadway", FontDataKey(u"Finale Broadway"), ":/fonts/finalebroadway/FinaleBroadway.otf");
    fdb.add_font(FontDataKey(u"Finale Broadway Text"), ":/fonts/finalebroadway/FinaleBroadwayText.otf");

    // Tabulature
    fdb.add_font(FontDataKey(u"FreeSerif"), ":/fonts/FreeSerif.ttf");
    fdb.add_font(FontDataKey(u"FreeSerif", true, false), ":/fonts/FreeSerifBold.ttf");
    fdb.add_font(FontDataKey(u"FreeSerif", false, true), ":/fonts/FreeSerifItalic.ttf");
    fdb.add_font(FontDataKey(u"FreeSerif", true, true), ":/fonts/FreeSerifBoldItalic.ttf");
    fdb.add_font(FontDataKey(u"FreeSans"), ":/fonts/FreeSans.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabRenaiss"), ":/fonts/MuseScoreTabRenaiss.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabPhalese"), ":/fonts/MuseScoreTabPhalese.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabBonneuilDeVisee"), ":/fonts/MuseScoreTabBonneuilDeVisee.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabBonneuilGaultier"), ":/fonts/MuseScoreTabBonneuilGaultier.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabDowland"), ":/fonts/MuseScoreTabDowland.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabLuteDidactic"), ":/fonts/MuseScoreTabLuteDidactic.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabModern"), ":/fonts/MuseScoreTabModern.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabItalian"), ":/fonts/MuseScoreTabItalian.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabFrench"), ":/fonts/MuseScoreTabFrench.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabFrenchBaroqueHeadless"), ":/fonts/MuseScoreTabFrenchBaroqueHeadless.ttf");
    fdb.add_font(FontDataKey(u"MuseScoreTabFrenchBaroque"), ":/fonts/MuseScoreTabFrenchBaroque.ttf");

    // Figured Bass
    fdb.add_font(FontDataKey(u"MscoreBC"), ":/fonts/mscore-BC.ttf");

    // Roman Numeral Analysis
    fdb.add_font(FontDataKey(u"Campania"), ":/fonts/campania/Campania.otf");

    // Defaults
    fdb.set_default_font(Font::Type::UNKNOWN, FontDataKey(u"Edwin"));
    fdb.set_default_font(Font::Type::TEXT, FontDataKey(u"Edwin"));
    fdb.set_default_font(Font::Type::TABLATURE, FontDataKey(u"FreeSerif"));
    fdb.set_default_font(Font::Type::MUSIC_SYMBOL_TEXT, FontDataKey(u"Bravura Text"));
    fdb.set_default_font(Font::Type::MUSIC_SYMBOL, FontDataKey(u"Bravura"));
    engraving_fonts.set_fallback_font("Bravura");

    fdb.insert_substitution(u"Leland Text", u"Bravura Text");
    fdb.insert_substitution(u"Bravura Text", u"Leland Text");
    fdb.insert_substitution(u"MScore Text", u"Leland Text");
    fdb.insert_substitution(u"Gootville Text", u"Leland Text");
    fdb.insert_substitution(u"MuseJazz Text", u"Leland Text");
    fdb.insert_substitution(u"Petaluma Text", u"MuseJazz Text");
    fdb.insert_substitution(u"Finale Maestro Text", u"Leland Text");
    fdb.insert_substitution(u"Finale Broadway Text", u"MuseJazz Text");
    fdb.insert_substitution(u"ScoreFont", u"Leland Text"); // alias for current Musical Text Font

    // Symbols
    Smufl::init();

    // A painter may name one of these families without requesting the font
    // first; unloaded faces would be missing from the database and drawn wrong.
    engraving_fonts.load_all_fonts();
}
} // namespace

struct EngineContext::Impl {
    std::unique_ptr<NativeFileSystem> file_system;
    std::unique_ptr<RuntimeAssets> assets;
    std::shared_ptr<EngravingFontsProvider> engraving_fonts;
    std::shared_ptr<EngravingConfiguration> configuration;
    std::shared_ptr<rendering::score::ScoreRenderer> renderer;
    std::unique_ptr<mpe::ArticulationProfilesRepository> articulation_profiles;
    std::unique_ptr<mu::iex::musicxml::MusicXmlConfiguration> music_xml;
    std::shared_ptr<EngravingContextConfiguration> context_configuration;
    EngravingServices engraving;

    std::unique_ptr<EngineAudio> audio;
    void teardown() noexcept;
};

// Reverse of the constructor; tolerates a partially built context.
void EngineContext::Impl::teardown() noexcept {
    try {
        audio.reset();
        engraving = EngravingServices();
        context_configuration.reset();
        music_xml.reset();
        articulation_profiles.reset();
        renderer.reset();
        configuration.reset();
        if (engraving_fonts) engraving_fonts->deinit();
        engraving_fonts.reset();
        if (assets) {
            FontMetrics::set_provider(nullptr);
            assets->shutdown();
            assets.reset();
        }
        if (file_system) {
            io::set_file_system(nullptr);
            file_system.reset();
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "engrave: engine teardown failed: %s\n", error.what());
    }
}

EngineContext::EngineContext(const std::string &assets, const std::string &cache_root) : impl_(std::make_unique<Impl>()) {
    // Validates the resources and claims the process; diagnostics go to stderr.
    initialize_native_runtime(assets);
    Impl &d = *impl_;
    try {
        d.file_system = std::make_unique<NativeFileSystem>(std::filesystem::u8path(assets));
        io::set_file_system(d.file_system.get());

        d.assets = std::make_unique<RuntimeAssets>(std::filesystem::u8path(cache_root));
        FontMetrics::set_provider(&d.assets->font_provider());

        d.engraving_fonts = std::make_shared<EngravingFontsProvider>(d.assets->font_provider());
        d.configuration = std::make_shared<EngravingConfiguration>();
        d.renderer = std::make_shared<rendering::score::ScoreRenderer>();
        d.articulation_profiles = std::make_unique<mpe::ArticulationProfilesRepository>(*d.file_system);
        d.audio = std::make_unique<EngineAudio>(*d.articulation_profiles);
        d.music_xml = std::make_unique<mu::iex::musicxml::MusicXmlConfiguration>();
        d.context_configuration = std::make_shared<EngravingContextConfiguration>();
        d.engraving.configuration = d.configuration;
        d.engraving.engraving_fonts = d.engraving_fonts;
        d.engraving.context_configuration = d.context_configuration;
        d.engraving.renderer = d.renderer;

        register_builtin_fonts(d.assets->fonts_database(), *d.engraving_fonts);

        d.configuration->init();
        DefaultStyle::instance()->init(d.configuration->default_style_file_path(), d.configuration->part_style_file_path(),
                                       d.configuration->default_page_size());
        StaffType::init_staff_types(d.configuration->default_color());

        Drumset::init_drumset();
        FiguredBass::read_config_file(String());

        d.music_xml->init();

        load_instrument_templates(":/engraving/instruments/instruments.xml");
    } catch (...) {
        d.teardown();
        throw;
    }
}

EngineContext::~EngineContext() { impl_->teardown(); }

io::IFileSystem &EngineContext::file_system() const { return *impl_->file_system; }

EngineAudio &EngineContext::audio() const { return *impl_->audio; }

RuntimeAssets &EngineContext::assets() const { return *impl_->assets; }

IFontsDatabase &EngineContext::fonts_database() const { return impl_->assets->fonts_database(); }

EngravingServices &EngineContext::engraving() const { return impl_->engraving; }

const mpe::IArticulationProfilesRepository &EngineContext::articulation_profiles() const { return *impl_->articulation_profiles; }

const mu::iex::musicxml::IMusicXmlConfiguration &EngineContext::music_xml() const { return *impl_->music_xml; }
} // namespace music
