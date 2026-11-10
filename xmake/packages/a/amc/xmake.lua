-- Local reduced header-only recipe, following the utfcpp recipe's pinning convention.
-- Provenance: upstream tagged release AmadeusITGroup/amc@v2.8.1; archive SHA-256
-- measured from codeload.github.com on 2026-11-08. No xmake-repo patches.
package("amc")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/AmadeusITGroup/amc")
    set_description("Pinned amc headers for playback compile probes")
    set_license("MIT")
    add_urls("https://github.com/AmadeusITGroup/amc/archive/refs/tags/$(version).tar.gz")
    add_versions("v2.8.1", "1c0adc6a5deaf07853d16601b6db6cc662188a071c22f08e6de04e1bea6f3830")
    on_install(function (package)
        os.cp("include", package:installdir())
    end)
    on_test(function (package)
        assert(os.isfile(path.join(package:installdir("include"), "amc", "vector.hpp")))
    end)
package_end()
