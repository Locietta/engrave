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

-- Install the workspace's locked environment.  engrave changes the upstream
-- adapter here: it never runs `pixi add`, so a missing or incomplete
-- environment is restored from pixi.lock instead of rewriting the manifest
-- and re-solving the lock.  A package absent from the manifest stays missing.
function main(name, opt)
    opt = opt or {}
    assert(pixi_utils.is_host_target(opt),
        "pixi cannot install %s for %s/%s", name, opt.plat, opt.arch)

    local pixi = find_tool("pixi")
    if not pixi then
        raise("pixi not found!")
    end

    local argv = {"install", "--locked", "--environment", pixi_utils.environment(opt)}
    if option.get("verbose") then
        table.insert(argv, "-v")
    end
    pixi_utils.add_workspace_args(argv, opt)
    os.vrunv(pixi.program, argv, {curdir = os.projectdir()})
end
