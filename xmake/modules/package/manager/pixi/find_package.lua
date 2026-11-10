--!A cross-platform build utility based on Lua
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

import("core.project.target")
import("lib.detect.find_tool")
import("package.manager.pixi.utils", {alias = "pixi_utils"})

-- Find a conda package installed in a Pixi workspace environment.
--
-- @param name  package name, e.g. fmt
-- @param opt   options, including configs.manifest and configs.environment
--
function main(name, opt)
    opt = opt or {}
    if not pixi_utils.is_host_target(opt) then
        return
    end

    local pixi = find_tool("pixi")
    if not pixi then
        return
    end

    local prefix = pixi_utils.prefixdir(pixi, opt)
    if not prefix then
        return
    end

    local metadata = pixi_utils.package_metadata(prefix, name)
    if not metadata then
        return
    end

    local package_files = pixi_utils.package_files(metadata)
    local windows = opt.plat == "windows" or opt.plat == "mingw"
    local dll_names = {}
    if windows then
        for _, file in ipairs(package_files) do
            local filename = path.filename(file:trim()):lower()
            if filename:endswith(".dll") then
                dll_names[filename:sub(1, -#".dll" - 1)] = true
            end
        end
    end

    -- A conda package can ship several libraries, and on Windows a static
    -- archive beside each import library (cairo: cairo-static.lib, cairo.lib,
    -- cairo-gobject.lib, ...).  configs.links selects the ones to link; their
    -- DLLs are kept so that run environments still find them.
    local selected_links
    local configs = opt.configs or {}
    if configs.links then
        selected_links = {}
        for _, link in ipairs(table.wrap(configs.links)) do
            selected_links[link:lower()] = true
        end
    end

    local result = {}
    for _, file in ipairs(package_files) do
        local relative_file = file:trim()
        local lower_file = relative_file:lower()

        local include_pos = lower_file:find("include/", 1, true)
        if include_pos then
            result.includedirs = result.includedirs or {}
            local include_dir = relative_file:sub(1, include_pos + #"include" - 1)
            table.insert(result.includedirs, path.join(prefix, include_dir))
        end

        local is_library
        if windows then
            is_library = lower_file:endswith(".lib")
                or (opt.plat == "mingw" and lower_file:endswith(".a"))
            if is_library and lower_file:endswith(".lib") then
                local library_name = path.basename(lower_file)
                -- Conda packages can contain an MSVC static archive named
                -- lib<name>.lib alongside the <name>.lib import library for
                -- <name>.dll. MinGW can consume the import library, but not
                -- the MSVC archive, so retain only the DLL-matching form.
                if library_name:startswith("lib")
                    and dll_names[library_name:sub(#"lib" + 1)] then
                    is_library = false
                end
            end
        elseif opt.plat == "macosx" then
            is_library = lower_file:endswith(".a") or lower_file:endswith(".dylib")
        else
            is_library = lower_file:endswith(".a")
                or lower_file:endswith(".so")
                or lower_file:find(".so.", 1, true) ~= nil
        end
        local link = is_library and target.linkname(path.filename(relative_file), {plat = opt.plat})
        if is_library and selected_links and not (link and selected_links[link:lower()]) then
            is_library = false
        end
        if is_library then
            local library_dir = path.join(prefix, path.directory(relative_file))
            local library_file = path.join(prefix, relative_file)
            result.linkdirs = result.linkdirs or {}
            result.libfiles = result.libfiles or {}
            table.insert(result.linkdirs, library_dir)
            table.insert(result.libfiles, library_file)
            if link then
                result.links = result.links or {}
                table.insert(result.links, link)
            end
        end

        if lower_file:endswith(".dll") then
            local binary_dir = path.join(prefix, path.directory(relative_file))
            result.linkdirs = result.linkdirs or {}
            result.libfiles = result.libfiles or {}
            table.insert(result.linkdirs, binary_dir)
            table.insert(result.libfiles, path.join(prefix, relative_file))
        end
    end

    if not result.includedirs and not result.linkdirs and not result.links then
        return
    end

    result.version = metadata.version
    if result.includedirs then
        result.includedirs = table.unique(result.includedirs)
    end
    if result.linkdirs then
        result.linkdirs = table.unique(result.linkdirs)
    end
    if result.links then
        result.links = table.unique(result.links)
    end
    -- Conda shared libraries live outside the loader's default search path, so
    -- binaries that link them carry the environment's library directories as
    -- their run path (Windows finds the DLLs through PATH instead).
    if not windows and result.linkdirs then
        result.rpathdirs = result.linkdirs
    end
    if result.libfiles then
        result.libfiles = table.unique(result.libfiles)
    end
    return result
end
