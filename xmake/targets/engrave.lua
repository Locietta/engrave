local source_root = ENGRAVE_SOURCE_ROOT
local function engine(...) return path.join(source_root, ...) end
local engrave_src = engine("engrave/src")
local function add_engine_target_defaults()
    add_includedirs("$(builddir)/xmake-generated")
    add_includedirs(ENGRAVE_INTERNAL_INCLUDES)
    if has_config("engrave_playback_harness") then
        -- Target-local: reopening engrave for Cairo must not lose this define.
        -- Export it so inline OfflineTimer definitions agree in consumers too.
        add_defines("ENGRAVE_PLAYBACK_HARNESS", {public = true})
    end
end

ENGRAVE_ADAPTER_INCLUDES = {
    engine("src/engraving/thirdparty/dtl"),
    source_root,
    engine("src"),
    engine("muse/framework"),
    engine("muse/framework/global"),
    engine("src/engraving"),
    engine("muse/framework/draw"),
}
for _, list in ipairs({ENGRAVE_AUDIO_INCLUDES, ENGRAVE_INCLUDES}) do
    for _, dir in ipairs(list) do
        table.insert(ENGRAVE_ADAPTER_INCLUDES, dir)
    end
end

target("engrave_core")
    set_kind("static")
    add_packages("pugixml")
    add_packages("glaze", {public = true})
    if is_plat("windows") then add_cxxflags("/Zc:preprocessor") end
    add_deps("muse_generated", "muse_global", "muse_draw", "muse_mpe", "engraving",
        "iex_musicxml", "iex_guitarpro", "iex_midi", "iex_mnx")
    add_engine_target_defaults()
    add_includedirs(ENGRAVE_ADAPTER_INCLUDES)
    add_files(path.join(engrave_src, "**.cpp|global/*.cpp|runtime/resource_filesystem.cpp|render/cairo_painter.cpp|audio/engine_audio.cpp|runtime/engine_context_audio.cpp|api/audio.cpp|api/soundfonts.cpp"))
    add_includedirs(ENGRAVE_PUBLIC_INCLUDES, {public = true})
    if not is_plat("windows") then
        add_links("m", {public = true})
    end
target_end()

target("engrave_audio")
    set_kind("static")
    add_deps("engrave_core", "muse_audio_engine", "native_fluidsynth")
    add_packages("pugixml")
    add_packages("glaze", {public = true})
    if is_plat("windows") then add_cxxflags("/Zc:preprocessor") end
    add_engine_target_defaults()
    add_includedirs(ENGRAVE_ADAPTER_INCLUDES)
    add_includedirs(ENGRAVE_PUBLIC_INCLUDES, {public = true})
    add_files(path.join(engrave_src, "audio/engine_audio.cpp"),
        path.join(engrave_src, "runtime/engine_context_audio.cpp"),
        path.join(engrave_src, "api/audio.cpp"), path.join(engrave_src, "api/soundfonts.cpp"))
    if not is_plat("windows") then add_links("m", {public = true}) end
target_end()

target("engrave")
    set_kind("phony")
    add_deps("engrave_core", "engrave_audio", {public = true})
target_end()
