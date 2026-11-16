local source_root = ENGRAVE_SOURCE_ROOT
local function engine(...) return path.join(source_root, ...) end
local engrave_src = engine("engrave/src")
local fluidsynth_dir = engine("muse/framework/audio/thirdparty/fluidsynth/fluidsynth-2.3.3")
local fluidsynth_src = path.join(fluidsynth_dir, "src")
local function add_engine_target_defaults()
    add_includedirs("$(builddir)/xmake-generated")
    add_includedirs(ENGRAVE_INTERNAL_INCLUDES)
    if has_config("engrave_playback_harness") then
        -- Target-local: reopening engrave for Cairo must not lose this define.
        -- Export it so inline OfflineTimer definitions agree in consumers too.
        add_defines("ENGRAVE_PLAYBACK_HARNESS", {public = true})
    end
end

target("muse_global")
    set_kind("static")
    add_deps("muse_generated")
    add_packages("pugixml", "utfcpp", "picojson")
    -- The zip container (serialization/internal/zipcontainer.cpp) is built on
    -- zlib; every library and test that links muse_global links it too.
    add_packages("amc", "unordered_dense", "zlib", {public = true})
    add_engine_target_defaults()
    add_includedirs(
        engine("muse/framework/global"),
        engine("src"),
        engine("muse"),
        engine("muse/framework")
    )
    add_files(
        engine("muse/framework/global/*.cpp"),
        engine("muse/framework/global/io/*.cpp"),
        engine("muse/framework/global/serialization/**.cpp"),
        engine("muse/framework/global/types/*.cpp"),
        engine("muse/framework/global/thirdparty/kors_logger/src/*.cpp")
    )
    -- engrave/src/global implements framework services that upstream builds
    -- such as muse::Settings. Engraving and the other modules
    -- call them, so they must link with muse_global rather than with engrave,
    -- which comes first on the link line.
    add_files(path.join(engrave_src, "global/*.cpp"))
    add_files(path.join(engrave_src, "runtime/resource_filesystem.cpp"))
    if is_plat("linux") then
        add_syslinks("dl", "pthread", {public = true})
    end

target("muse_draw")
    set_kind("static")
    add_deps("muse_generated", "muse_global")
    add_packages("freetype", "harfbuzz", "cairo", {public = true})
    add_engine_target_defaults()
    add_includedirs(
        engine("muse/framework/draw"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global")
    )
    add_files(engine("muse/framework/draw/**.cpp"))
    add_files(path.join(engrave_src, "render/cairo_painter.cpp"))

target("muse_mpe")
    set_kind("static")
    add_deps("muse_generated", "muse_global")
    -- The flat MPE store (mpe/flatevents.h) is built on amc vectors; producers and consumers include it.
    add_packages("amc", "unordered_dense", {public = true})
    add_engine_target_defaults()
    add_includedirs(
        engine("muse/framework/mpe"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global")
    )
    add_files(
        engine("muse/framework/mpe/internal/*.cpp")
    )

target("engraving")
    set_kind("static")
    add_deps("muse_generated", "muse_global", "muse_draw", "muse_mpe")
    add_engine_target_defaults()
    add_includedirs(
        engine("src/engraving"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global"),
        engine("src/engraving/infrastructure"),
        engine("src/engraving/thirdparty/dtl")
    )
    add_files(engine("src/engraving/**.cpp"))
    remove_files(
        engine("src/engraving/tests/**")
    )

target("iex_musicxml")
    set_kind("static")
    add_deps("muse_generated", "muse_global", "engraving")
    add_engine_target_defaults()
    add_includedirs(
        engine("src/importexport/musicxml"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global"),
        engine("src/engraving"),
        engine("src/engraving/infrastructure"),
        engine("src/engraving/thirdparty/dtl")
    )
    add_files(engine("src/importexport/musicxml/**.cpp"))

-- The engine's MNX document model, generated from the MNX schema (docs/mnx.md).
-- Its headers are included as "model/...". glaze reads and writes it.
target("mnx_model")
    set_kind("static")
    add_packages("glaze", "unordered_dense", {public = true})
    if is_plat("windows") then add_cxxflags("/bigobj") end
    add_includedirs(engine("src/importexport/mnx"), {public = true})
    add_includedirs(engine("src/importexport/mnx/thirdparty")) -- <music_theory/music_theory.hpp>
    add_files(engine("src/importexport/mnx/model/*.cpp"))

target("iex_mnx")
    set_kind("static")
    add_deps("mnx_model", {public = true})
    add_deps("muse_generated", "engraving")
    add_engine_target_defaults()
    add_includedirs(
        engine("src/importexport/mnx"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global"),
        engine("src/engraving"),
        engine("src/engraving/infrastructure"),
        engine("src/engraving/thirdparty/dtl")
    )
    add_files(engine("src/importexport/mnx/internal/export/*.cpp"), engine("src/importexport/mnx/internal/import/*.cpp"), engine("src/importexport/mnx/internal/shared/*.cpp"))
target_end()

target("iex_guitarpro")
    set_kind("static")
    add_deps("muse_generated", "engraving")
    add_engine_target_defaults()
    add_includedirs(
        engine("src/importexport/guitarpro"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global"),
        engine("src/engraving"),
        engine("src/engraving/infrastructure"),
        engine("src/engraving/thirdparty/dtl")
    )
    add_files(engine("src/importexport/guitarpro/**.cpp"))
target_end()

-- MIDI import, export and the MIDI file layer they share: upstream's iex_midi.
target("iex_midi")
    set_kind("static")
    add_deps("muse_generated", "engraving")
    add_engine_target_defaults()
    add_includedirs(
        engine("src/importexport/midi"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global"),
        engine("src/engraving"),
        engine("src/engraving/infrastructure"),
        engine("src/engraving/thirdparty/dtl")
    )
    add_files(engine("src/importexport/midi/**.cpp"))
target_end()

target("native_fluidsynth")
    set_kind("static")
    if is_plat("windows") then
        set_languages("c11", "cxx23")
    else
        set_languages("gnu17", "cxx23")
    end
    add_engine_target_defaults()
    add_defines("EXTERN_VORBIS_SUPPORT", "NO_GLIB", "NO_THREADS")
    add_includedirs(
        path.join(fluidsynth_dir, "include"),
        fluidsynth_src,
        path.join(fluidsynth_src, "bindings"),
        path.join(fluidsynth_src, "drivers"),
        path.join(fluidsynth_src, "external"),
        path.join(fluidsynth_src, "midi"),
        path.join(fluidsynth_src, "rvoice"),
        path.join(fluidsynth_src, "sfloader"),
        path.join(fluidsynth_src, "synth"),
        path.join(fluidsynth_src, "utils")
    )
    add_files(
        engine("muse/framework/audio/thirdparty/fluidsynth/vorbis_decode.cpp"),
        path.join(fluidsynth_src, "bindings/*.c"),
        path.join(fluidsynth_src, "drivers/*.c"),
        path.join(fluidsynth_src, "midi/*.c"),
        path.join(fluidsynth_src, "midi/*.cpp"),
        path.join(fluidsynth_src, "rvoice/*.c"),
        path.join(fluidsynth_src, "sfloader/*.c"),
        path.join(fluidsynth_src, "synth/*.c"),
        path.join(fluidsynth_src, "utils/*.c")
    )
    set_warnings("none")

target("muse_midi_sequencer")
    set_kind("static")
    add_deps("muse_generated", "muse_global", "muse_mpe")
    add_packages("amc", "unordered_dense", "ngcpp-proxy", {public = true})
    add_engine_target_defaults()
    add_defines(ENGRAVE_AUDIO_ENGINE.defines)
    add_includedirs(ENGRAVE_AUDIO_ENGINE.includes)
    add_files(engine("muse/framework/audio/engine/internal/synthesizers/fluidsynth/fluidsequencer.cpp"))
target_end()

target("muse_audio_engine")
    set_kind("static")
    add_deps("muse_generated", "muse_global", "muse_midi_sequencer", "native_fluidsynth")
    add_packages("amc", "unordered_dense", "ngcpp-proxy", {public = true})
    add_packages("libflac", "lame", "libopus", "libopusenc", "fdk-aac")
    add_engine_target_defaults()
    add_defines(ENGRAVE_AUDIO_ENGINE.defines)
    add_includedirs(ENGRAVE_AUDIO_ENGINE.includes)
    add_files(
        engine("muse/framework/audio/engine/*.cpp"),
        engine("muse/framework/audio/engine/internal/**.cpp|synthesizers/fluidsynth/fluidsequencer.cpp")
    )
target_end()

