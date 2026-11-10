-- Native xmake build for the engrave engine library.
--
-- Standalone build:
--   xmake f -m release && xmake build
-- Tests: xmake f --engrave_tests=y && xmake build engine_tests
--
-- The engine targets themselves live in xmake/engrave.lua, which is also the
-- file the engrave-mcp superproject includes from its pinned vendor/engrave
-- submodule (or --engine_source checkout).  Everything here is what a build
-- of the engine *as a project* needs but a library must not impose on its
-- consumer: project identity, languages, package policies and the platform
-- feature macros and toolchain flags the engine translation
-- units were configured with.

set_project("engrave")
set_xmakever("3.1.1")
set_version("0.1.0")
set_languages("cxx23")
set_targetdir("$(builddir)")
add_rules("mode.debug", "mode.release", "mode.releasedbg")
-- Packages install into the project (.xmake, gitignored) so a checkout is
-- self-contained, and everything is built from source: a precompiled artifact
-- could carry a different CRT or feature set than the pinned recipe asks for.
set_policy("package.install_locally", true)
set_policy("package.precompiled", false)

-- The engine's third-party libraries are required by xmake/engrave.lua, which
-- also registers the pinned package repository (xmake/packages).  cairo and
-- zlib come from this checkout's Pixi environment (pixi.toml; run
-- `pixi install` once).
includes("xmake/engrave.lua")

-- Native platform/toolchain configuration.
if is_plat("windows") then
    set_runtimes("MD")
    set_exceptions("cxx")
    add_defines("WIN32", "WIN32_LEAN_AND_MEAN", "NOGDI", "NOMINMAX", "_USE_MATH_DEFINES", "_UNICODE", "UNICODE")
    add_cxflags("/utf-8", "/Zc:__cplusplus", {tools = {"cl", "clang_cl"}})
    add_cflags("/utf-8", {tools = {"cl", "clang_cl"}})
else
    add_cxflags("-fPIC", "-pthread")
    add_ldflags("-pthread")
end
