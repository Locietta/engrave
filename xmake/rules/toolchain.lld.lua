local probes = {}
rule("toolchain.lld")
    on_config(function (target)
        import("lib.detect.find_tool")
        if target:is_plat("linux") and target:has_tool("cxx", "gcc", "gxx", "clang", "clangxx") then
            local linker = find_tool("mold") and "mold" or (find_tool("ld.lld") and "lld")
            if linker then
                import("core.tool.linker", {alias = "linker_tool"})
                local flag = "-fuse-ld=" .. linker
                if linker_tool.has_flags("binary", {"cxx"}, flag, {target = target}) then
                    target:add("ldflags", flag)
                    target:add("shflags", flag)
                end
            end
        end
        if target:is_plat("windows") and target:has_tool("cxx", "cl") then
            local msvc = target:toolchain("msvc")
            local envs = msvc and msvc:runenvs()
            local lld = find_tool("lld-link", {envs = envs})
            local cl = find_tool("cl", {envs = envs})
            if not lld or not cl then return end
            local key = cl.program .. lld.program .. (is_mode("asan") and "asan" or "plain")
            if probes[key] == nil then
                probes[key] = false
                local dir = path.join(os.projectdir(), "build", ".linker-probe", is_mode("asan") and "asan" or "plain")
                os.mkdir(dir)
                local source = path.join(dir, "main.cpp")
                local object = path.join(dir, "main.obj")
                local binary = path.join(dir, "main.exe")
                local pdb = path.join(dir, "main.pdb")
                io.writefile(source, "int main() { volatile int a[2] = {0, 1}; return a[0]; }\n")
                try {
                    function ()
                        local flags = {"/nologo", "/c", "/MD", "/Z7", "/O1", source, "/Fo" .. object}
                        if is_mode("asan") then table.insert(flags, "/fsanitize=address") end
                        os.iorunv(cl.program, flags, {envs = envs})
                        local links = {"/nologo", "/debug", "/incremental:no", "/out:" .. binary, "/pdb:" .. pdb, object}
                        for _, libdir in ipairs((envs.LIB or ""):split(";", {plain = true})) do
                            table.insert(links, "/libpath:" .. libdir)
                        end
                        os.iorunv(lld.program, links, {envs = envs})
                        local runenv = table.copy(envs)
                        runenv.PATH = path.directory(cl.program) .. ";" .. (envs.PATH or os.getenv("PATH") or "")
                        os.iorunv(binary, {}, {envs = runenv})
                        probes[key] = os.isfile(pdb)
                    end,
                    catch {function () end}
                }
            end
            if probes[key] then
                target:set("toolset", "ld", "link@" .. lld.program)
                target:set("toolset", "sh", "link@" .. lld.program)
            end
        end
    end)
rule_end()
