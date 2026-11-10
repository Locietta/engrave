-- Local reduced header-only recipe, following the utfcpp recipe's pinning convention.
-- Provenance: upstream tagged release martinus/unordered_dense@v5.3.3; archive SHA-256
-- measured from codeload.github.com on 2026-11-08. No xmake-repo patches.
package("unordered_dense")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/martinus/unordered_dense")
    set_description("Pinned unordered_dense headers for playback compile probes")
    set_license("MIT")
    add_urls("https://github.com/martinus/unordered_dense/archive/refs/tags/$(version).tar.gz")
    add_versions("v5.3.3", "862605ef21424c6c92881aac4cf8f09c3abd91c80588446c83892583228eca67")
    on_install(function (package)
        os.cp("include", package:installdir())
    end)
    on_test(function (package)
        assert(os.isfile(path.join(package:installdir("include"), "ankerl", "unordered_dense.h")))
    end)
package_end()
