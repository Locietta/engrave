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

import("lib.detect.find_tool")
import("package.manager.pixi.utils", {alias = "pixi_utils"})

function main(name, opt)
    local pixi = find_tool("pixi")
    if not pixi then
        raise("pixi not found!")
    end

    local search_result = pixi_utils.run_json(pixi, {"search", name, "--json"}, opt or {})
    local results = {}
    local seen = {}
    for _, packages in pairs(search_result or {}) do
        for _, package_info in ipairs(packages) do
            local key = package_info.name .. "@" .. package_info.version
            if not seen[key] then
                seen[key] = true
                table.insert(results, {
                    name = "pixi::" .. package_info.name,
                    version = package_info.version,
                    description = package_info.license or package_info.url or ""
                })
            end
        end
    end
    return results
end
