-- Pinned glaze: header-only JSON/reflection library.
package("glaze")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/stephenberry/glaze")
    set_description("Extremely fast, in memory, JSON and interface library for modern C++")
    set_license("MIT")

    add_urls("https://github.com/stephenberry/glaze/archive/refs/tags/v$(version).tar.gz")

    add_versions("9.1.0", "55f8a675397c505f9bee6c160e1c26b0085868f185d04d7a7727531fdbff4bd4")

    add_configs("ssl", {description = "Enable SSL support for networking", default = false, type = "boolean"})

    add_deps("cmake")

    on_load(function (package)
        if package:config("ssl") then
            package:add("deps", "openssl3")
            package:add("defines", "GLZ_ENABLE_SSL")
        end
    end)

    on_install(function (package)
        if package:has_tool("cxx", "cl") then
            package:add("cxxflags", "/Zc:preprocessor", "/permissive-", "/Zc:lambda")
        end

        import("package.tools.cmake").install(package, {
            "-Dglaze_DEVELOPER_MODE=OFF",
            "-DCMAKE_CXX_STANDARD=23",
            "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"),
            "-Dglaze_ENABLE_SSL=" .. (package:config("ssl") and "ON" or "OFF")
        })
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            struct obj_t {
                double x{};
                float y{};
            };
            template <>
            struct glz::meta<obj_t> {
                static constexpr auto value = object("x", &obj_t::x, "y", &obj_t::y);
            };
            void test() {
                std::string buffer{};
                obj_t obj{};
                glz::write_json(obj, buffer);
            }
        ]]}, {configs = {languages = "c++23"}, includes = "glaze/glaze.hpp"}))
    end)
