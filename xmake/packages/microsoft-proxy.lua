-- Local reduced header-only recipe, following utfcpp.lua's pinning convention.
-- Provenance: upstream tagged release microsoft/proxy@4.0.1; archive SHA-256
-- measured from codeload.github.com on 2026-11-08. No xmake-repo patches.
package("microsoft-proxy")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/microsoft/proxy")
    set_description("Pinned microsoft-proxy headers for playback compile probes")
    set_license("MIT")
    add_urls("https://github.com/microsoft/proxy/archive/refs/tags/$(version).tar.gz")
    add_versions("4.0.1", "78e1d88c36d2e7ee8f8dc47f112cdb96b8838dfd4177e104b53d9b64ed9b2357")
    on_install(function (package)
        os.cp("include", package:installdir())
    end)
    on_test(function (package)
        assert(os.isfile(path.join(package:installdir("include"), "proxy", "proxy.h")))
    end)
package_end()
