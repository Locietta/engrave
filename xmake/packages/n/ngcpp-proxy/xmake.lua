package("ngcpp-proxy")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/ngcpp/proxy")
    set_description("Proxy: Next Generation Polymorphism in C++")
    set_license("MIT")

    add_urls("https://github.com/ngcpp/proxy/archive/refs/tags/$(version).tar.gz",
             "https://github.com/ngcpp/proxy.git")

    add_versions("4.1.0", "f46fd0fbcaf461f4904f35416edf94abc62d5582b66d6e769e676fd7bd478311")

    add_includedirs("include")

    on_install(function (package)
        os.cp("include", package:installdir())
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <proxy/proxy.h>

            PRO_DEF_MEM_DISPATCH(MemGetValue, get_value);

            struct ValueFacade
                : pro::facade_builder
                  ::add_convention<MemGetValue, int() const>
                  ::build {};

            struct Value {
                int get_value() const { return 42; }
            };

            void test() {
                static_assert(__msft_lib_proxy >= 202606L);
                auto value = pro::make_proxy<ValueFacade, Value>();
                (void)value->get_value();
            }
        ]]}, {configs = {languages = "c++20"}}))
    end)
