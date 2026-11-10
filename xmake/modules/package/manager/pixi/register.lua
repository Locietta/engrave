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

import("core.base.hashset")
import("core.cache.memcache")

-- Register a project-provided package manager with Xmake's package loader.
--
-- Xmake currently discovers third-party manager namespaces only below its
-- program directory. Project module directories participate in import lookup,
-- but not in that manager-name discovery, so register Pixi before Xmake loads
-- any pixi:: requirement objects.
function main()
    local cache = memcache.cache("core.base.package")
    local managers = cache:get("managers")
    if not managers then
        managers = hashset.new()
        for _, dir in ipairs(os.dirs(path.join(os.programdir(), "modules/package/manager/*"))) do
            managers:insert(path.filename(dir))
        end
    end
    managers:insert("pixi")
    cache:set("managers", managers)
end
