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

import("core.base.option")
import("lib.detect.find_tool")
import("package.manager.pixi.utils", {alias = "pixi_utils"})

-- Add a missing conda package to a Pixi workspace and install its environment.
function main(name, opt)
    opt = opt or {}
    assert(pixi_utils.is_host_target(opt),
        "pixi cannot install %s for %s/%s", name, opt.plat, opt.arch)

    local pixi = find_tool("pixi")
    if not pixi then
        raise("pixi not found!")
    end

    local configs = opt.configs or {}
    local spec = name
    if opt.require_version and opt.require_version:find(".", 1, true) then
        spec = spec .. "=" .. opt.require_version
    end

    local argv = {"add", spec}
    if configs.feature then
        table.insert(argv, "--feature")
        table.insert(argv, configs.feature)
    end
    if option.get("verbose") then
        table.insert(argv, "-v")
    end
    pixi_utils.add_workspace_args(argv, opt)
    os.vrunv(pixi.program, argv, {curdir = os.projectdir()})
end
