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

import("core.base.json")

-- Can the host Pixi environment serve packages for the requested plat/arch?
--
-- Pixi only ever installs packages for the host, so a cross build can never be
-- satisfied. Windows is the exception worth allowing: building with GCC there
-- selects Xmake's `mingw` platform, which is still the host, and names x64 as
-- `x86_64`.
function is_host_target(opt)
    opt = opt or {}
    local plat_ok = is_host(opt.plat) or (opt.plat == "mingw" and is_host("windows"))
    local host_arch = os.arch() == "x64" and "x86_64" or os.arch()
    return plat_ok and (opt.arch == os.arch() or opt.arch == host_arch)
end

function manifest(opt)
    local configs = opt and opt.configs or {}
    local manifest_path = configs.manifest or os.getenv("PIXI_PROJECT_MANIFEST")
    if manifest_path then
        if not path.is_absolute(manifest_path) then
            manifest_path = path.absolute(manifest_path, os.projectdir())
        end
        return manifest_path
    end

    local pixi_toml = path.join(os.projectdir(), "pixi.toml")
    if os.isfile(pixi_toml) then
        return pixi_toml
    end

    local pyproject_toml = path.join(os.projectdir(), "pyproject.toml")
    if os.isfile(pyproject_toml) then
        return pyproject_toml
    end
end

function environment(opt)
    local configs = opt and opt.configs or {}
    return configs.environment or os.getenv("PIXI_ENVIRONMENT_NAME") or "default"
end

function add_workspace_args(argv, opt)
    local manifest_path = manifest(opt)
    if manifest_path then
        table.insert(argv, "--manifest-path")
        table.insert(argv, manifest_path)
    end
    local configs = opt and opt.configs or {}
    if configs.workspace then
        table.insert(argv, "--workspace")
        table.insert(argv, configs.workspace)
    end
    return argv
end

function run_json(pixi, argv, opt)
    add_workspace_args(argv, opt)
    local output = os.iorunv(pixi.program, argv, {curdir = os.projectdir()})
    return json.decode(output)
end

function prefixdir(pixi, opt)
    local environment_name = environment(opt)
    local active_environment = os.getenv("PIXI_ENVIRONMENT_NAME")
    local active_prefix = os.getenv("CONDA_PREFIX")
    if active_prefix and active_environment == environment_name and os.isdir(active_prefix) then
        return active_prefix
    end

    local info = try {function ()
        return run_json(pixi, {"info", "--json"}, opt)
    end}
    if not info then
        return
    end

    for _, environment_info in ipairs(info.environments_info or {}) do
        if environment_info.name == environment_name then
            return environment_info.prefix
        end
    end
end

function package_metadata(prefix, name)
    local metadata_dir = path.join(prefix, "conda-meta")
    if not os.isdir(metadata_dir) then
        return
    end

    for _, metadata_file in ipairs(os.files(path.join(metadata_dir, name .. "-*.json"))) do
        local metadata = try {function ()
            return json.loadfile(metadata_file)
        end}
        if metadata and metadata.name and metadata.name:lower() == name:lower() then
            return metadata, metadata_file
        end
    end
end

function package_files(metadata)
    if metadata.files then
        return metadata.files
    end

    local files = {}
    local paths_data = metadata.paths_data or {}
    for _, path_info in ipairs(paths_data.paths or {}) do
        if path_info._path then
            table.insert(files, path_info._path)
        end
    end
    return files
end
