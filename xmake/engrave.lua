-- Included by both the standalone engine and the host.
ENGRAVE_FORK_ROOT = path.directory(os.scriptdir())
ENGRAVE_SOURCE_ROOT = path.absolute(ENGRAVE_SOURCE_ROOT or ENGRAVE_FORK_ROOT)
if not ENGRAVE_BUILD_RULES_LOADED then
    includes("rules/*.lua")
end
option("engrave_playback_harness")
    set_default(false)
    set_description("Test-only playback identity capture and measurement hooks")
option_end()
includes("packages.lua")
includes("generated.lua")
includes("environment.lua")
includes("targets/modules.lua")
includes("targets/engrave.lua")
option("engrave_tests")
    set_default(false)
    set_showmenu(true)
    set_description("Enable engine tests and engine_tests (no test targets build by default)")
option_end()
if has_config("engrave_tests") then
    includes(path.join(ENGRAVE_FORK_ROOT, "xmake", "tests.lua"))
end
