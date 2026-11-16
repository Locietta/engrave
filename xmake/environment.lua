local source_root = ENGRAVE_SOURCE_ROOT
local function engine(...) return path.join(source_root, ...) end
ENGRAVE_SOURCE_ROOT = source_root



ENGRAVE_INCLUDES = {
    engine("muse"),
    engine("muse/framework"),
    engine("muse/framework/global"),
    engine("src"),
}

-- The adapters' internal headers, included from the checkout root as
-- "engrave/src/<area>/<file>.h" so that no engine include root can supply a
-- header of the same name, whatever the include order.  These headers serve
-- the library and its internal regression tests; the engrave-mcp host uses
-- the public API.
ENGRAVE_INTERNAL_INCLUDES = {
    source_root,
}

-- The public API (<engrave/engine.h>, ...): std headers only, so an embedder
-- needs this include root and nothing else.  engrave exports it publicly.
ENGRAVE_PUBLIC_INCLUDES = {
    engine("engrave/include"),
}

local fluidsynth_dir = engine("muse/framework/audio/thirdparty/fluidsynth/fluidsynth-2.3.3")
local fluidsynth_src = path.join(fluidsynth_dir, "src")

ENGRAVE_AUDIO_ENGINE = {
    defines = {
        "NO_GLIB",
        "NO_THREADS",
        "SYSTEM_LAME",
    },
    includes = {
        engine("muse/framework/audio/common"),
        engine("src"),
        engine("muse"),
        engine("muse/framework"),
        engine("muse/framework/global"),
        engine("muse/framework/audio"),
        engine("muse/framework/audio/engine"),
        path.join(fluidsynth_dir, "include"),
        fluidsynth_src,
        path.join(fluidsynth_src, "bindings"),
        path.join(fluidsynth_src, "drivers"),
        path.join(fluidsynth_src, "external"),
        path.join(fluidsynth_src, "midi"),
        path.join(fluidsynth_src, "rvoice"),
        path.join(fluidsynth_src, "sfloader"),
        path.join(fluidsynth_src, "synth"),
        path.join(fluidsynth_src, "utils"),
    },
}

ENGRAVE_AUDIO_INCLUDES = {
    path.join(fluidsynth_dir, "include"),
    fluidsynth_src,
    path.join(fluidsynth_src, "bindings"),
    path.join(fluidsynth_src, "drivers"),
    path.join(fluidsynth_src, "external"),
    path.join(fluidsynth_src, "midi"),
    path.join(fluidsynth_src, "rvoice"),
    path.join(fluidsynth_src, "sfloader"),
    path.join(fluidsynth_src, "synth"),
    path.join(fluidsynth_src, "utils"),
    engine("muse/framework/audio"),
}
