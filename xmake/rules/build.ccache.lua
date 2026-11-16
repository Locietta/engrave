-- Opt MSVC and CI into xmake's compiler cache unless the user disabled --ccache=n.
rule("build.ccache")
    on_config(function (target)
        import("core.project.config")
        if config.get("ccache") == false then
            target:set("policy", "build.ccache", false)
            return
        end
        target:set("policy", "build.ccache", true)
        if is_mode("debug", "releasedbg", "asan") and target:has_tool("cxx", "cl") then
            -- cl's embed mapping emits /Z7 and suppresses /Zi and /Fd entirely.
            target:set("symbols", "debug", "embed")
        end
    end)
rule_end()
