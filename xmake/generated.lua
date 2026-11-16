local source_root = ENGRAVE_SOURCE_ROOT
local function engine(...) return path.join(source_root, ...) end
local function read_version(os, io)
    local text = io.readfile(engine("version.cmake")) or ""
    local function value(name)
        return text:match("set%(" .. name .. '%s+"([^"]*)"%)')
    end
    local major = value("MUSE_APP_VERSION_MAJOR") or "0"
    local minor = value("MUSE_APP_VERSION_MINOR") or "0"
    local patch = value("MUSE_APP_VERSION_PATCH") or "0"
    local git_args = {"-C", source_root, "rev-parse", "HEAD"}
    local revision = ""
    if os.execv("git", git_args, {try = true, stdout = os.nuldev(), stderr = os.nuldev()}) == 0 then
        local sha = tostring(os.iorunv("git", git_args) or ""):gsub("%s+", "")
        if #sha >= 7 and sha:match("^%x+$") then
            revision = sha:sub(1, 7)
        end
    end
    local version_label = value("MUSE_APP_VERSION_LABEL")
    local result = {
        MUSE_APP_NAME_HUMAN_READABLE = value("MUSE_APP_NAME_HUMAN_READABLE") or "MuseScore Studio",
        MUSE_APP_NAME_MACHINE_READABLE = value("MUSE_APP_NAME_MACHINE_READABLE") or "MuseScoreStudio",
        MUSE_APP_UNSTABLE = not is_mode("release"),
        MUSE_APP_BUILD_NUMBER = "1",
        MUSE_APP_REVISION = revision,
        MUSE_APP_VERSION = major .. "." .. minor .. "." .. patch,
        MUSE_APP_VERSION_MAJOR = major,
        MUSE_APP_VERSION_MINOR = minor,
        MUSE_APP_VERSION_PATCH = patch,
    }
    if version_label and version_label ~= "" then
        result.MUSE_APP_VERSION_LABEL = version_label
    end
    return result
end

local function write_if_changed(os, io, filename, content)
    if os.isfile(filename) then
        local previous = io.readfile(filename)
        if previous == content then
            return false
        end
    end
    io.writefile(filename, content)
    return true
end

target("muse_generated")
    set_kind("phony")
    set_default(false)
    set_targetdir("$(builddir)/xmake-generated")
    local function generate_version_header(target, os, io)
        local output_dir = target:targetdir()
        if not output_dir or output_dir == "" then
            raise("generated target requires a build directory")
        end
        os.mkdir(output_dir)
        local metadata = {}
        for name, value in pairs(read_version(os, io)) do
            if name:find("^MUSE_APP_") then
                if type(value) == "boolean" then
                    if value then table.insert(metadata, "#define " .. name .. " 1") end
                else
                    table.insert(metadata, "#define " .. name .. " " .. string.format("%q", value))
                end
            end
        end
        table.sort(metadata)
        write_if_changed(os, io, path.join(output_dir, "engrave_version.h"),
            table.concat(metadata, "\n") .. "\n")
    end
    on_config(function (target)
        generate_version_header(target, os, io)
    end)
    on_build(function (target)
        generate_version_header(target, os, io)
    end)
target_end()
