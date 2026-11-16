-- Instrument owned targets; pinned external packages retain their release configuration.
rule("mode.asan")
    after_load(function (target)
        if not is_mode("asan") then return end
        target:set("symbols", "debug")
        if target:is_plat("windows") then
            -- The engine and Pixi dependencies use the release DLL CRT in every mode.
            target:set("runtimes", "MD")
            target:set("policy", "build.sanitizer.address", true)
        else
            target:set("policy", "build.sanitizer.address", true)
            target:set("policy", "build.sanitizer.undefined", true)
        end
    end)
    on_config(function (target)
        if not is_mode("asan") then return end
        if target:has_tool("cxx", "cl", "clang_cl") then
            local dll_dir
            if target:has_tool("cxx", "clang_cl") then
                import("private.utils.toolchain", {alias = "toolchain_utils"})
                local tc = target:toolchain("clang-cl") or target:toolchain("clang")
                local dirs = tc and toolchain_utils.get_llvm_dirs(tc)
                dll_dir = dirs and dirs.rtlibdir or nil
            else
                import("lib.detect.find_tool")
                local msvc = target:toolchain("msvc")
                local cl = msvc and find_tool("cl", {envs = msvc:runenvs()})
                dll_dir = cl and path.directory(cl.program) or nil
            end
            if dll_dir then target:add("runenvs", "PATH", dll_dir) end
            target:set("symbols", "debug", "embed")
            target:add("cxflags", "/O1", {force = true})
            target:add("ldflags", "/INCREMENTAL:NO", {force = true})
            target:add("shflags", "/INCREMENTAL:NO", {force = true})
        elseif target:has_tool("cxx", "gcc", "gxx", "g%+%+", "clang", "clangxx") then
            target:add("cxflags", "-O1", "-fno-omit-frame-pointer", {force = true})
            -- Xmake's built-in sanitizer helper matches "gxx" but not raw "g++".
            if target:has_tool("cxx", "g%+%+") then
                target:add("cxflags", "-fsanitize=address,undefined", {force = true})
            end
            if target:has_tool("ld", "g%+%+") then
                target:add("ldflags", "-fsanitize=address,undefined", {force = true})
                target:add("shflags", "-fsanitize=address,undefined", {force = true})
            end
        end
    end)
    after_link(function (target)
        if not is_mode("asan") or target:kind() ~= "binary" then return end
        if not target:has_tool("cxx", "cl", "clang_cl") then return end
        local dll_dir
        if target:has_tool("cxx", "clang_cl") then
            import("private.utils.toolchain", {alias = "toolchain_utils"})
            local tc = target:toolchain("clang-cl") or target:toolchain("clang")
            local dirs = tc and toolchain_utils.get_llvm_dirs(tc)
            dll_dir = dirs and dirs.rtlibdir or nil
        else
            import("lib.detect.find_tool")
            local msvc = target:toolchain("msvc")
            local cl = msvc and find_tool("cl", {envs = msvc:runenvs()})
            dll_dir = cl and path.directory(cl.program) or nil
        end
        if not dll_dir then return end
        local runtime_dll = path.join(dll_dir, "clang_rt.asan_dynamic-x86_64.dll")
        if not os.isfile(runtime_dll) then return end
        local dest = path.join(target:targetdir(), path.filename(runtime_dll))
        if not os.isfile(dest) or os.filesize(dest) ~= os.filesize(runtime_dll) then
            os.cp(runtime_dll, dest)
        end
    end)
rule_end()
