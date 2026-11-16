-- Instrument owned targets; pinned external packages retain their release configuration.
rule("mode.asan")
    on_config(function (target)
        if not is_mode("asan") then return end
        target:set("symbols", "debug")
        if target:has_tool("cxx", "cl", "clang_cl") then
            -- The engine and Pixi dependencies use the release DLL CRT in every mode.
            target:set("runtimes", "MD")
            import("lib.detect.find_tool")
            local msvc = target:toolchain("msvc")
            local cl = msvc and find_tool("cl", {envs = msvc:runenvs()})
            if cl then target:add("runenvs", "PATH", path.directory(cl.program)) end
            target:set("symbols", "debug", "embed")
            target:add("cxflags", "/O1", "/fsanitize=address", {force = true})
            target:add("ldflags", "/INCREMENTAL:NO", {force = true})
        elseif target:has_tool("cxx", "gcc", "gxx", "clang", "clangxx") then
            target:add("cxflags", "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", {force = true})
            target:add("ldflags", "-fsanitize=address,undefined", {force = true})
            target:add("shflags", "-fsanitize=address,undefined", {force = true})
        end
    end)
    after_link(function (target)
        if not is_mode("asan") or target:kind() ~= "binary" then return end
        if not target:has_tool("cxx", "cl", "clang_cl") then return end
        import("lib.detect.find_tool")
        local msvc = target:toolchain("msvc")
        local cl = msvc and find_tool("cl", {envs = msvc:runenvs()})
        if not cl then return end
        local runtime_dll = path.join(path.directory(cl.program), "clang_rt.asan_dynamic-x86_64.dll")
        if not os.isfile(runtime_dll) then return end
        local dest = path.join(target:targetdir(), path.filename(runtime_dll))
        if not os.isfile(dest) or os.filesize(dest) ~= os.filesize(runtime_dll) then
            os.cp(runtime_dll, dest)
        end
    end)
rule_end()
