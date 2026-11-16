local fork_root = ENGRAVE_FORK_ROOT
add_repositories("engrave-pinned xmake", {rootdir = fork_root})
-- cairo and zlib come from the including project's Pixi environment (conda-
-- forge builds, pinned by its pixi.lock: the PDF/PNG goldens and the archive
-- bytes depend on them).  xmake has no Pixi package manager of its own; the
-- pixi:: one lives in xmake/modules and has to be registered before any
-- pixi:: requirement is loaded, which the option check below does.
add_moduledirs(path.join(fork_root, "xmake", "modules"))
option("__pixi_package_manager")
    set_showmenu(false)
    on_check(function (option)
        import("package.manager.pixi.register")()
        option:enable(true)
    end)
option_end()
-- conda's cairo also ships cairo-static, cairo-gobject and
-- cairo-script-interpreter libraries, and its zlib four .lib names on
-- Windows: link only the shared cairo and z.
add_requires("pixi::cairo", {alias = "cairo", configs = {links = "cairo"}})
add_requires("pixi::zlib", {alias = "zlib", configs = {links = "z"}})

add_requires("ngcpp-proxy 4.1.0", {system = false})
add_requires("amc v2.8.1", {system = false})
add_requires("unordered_dense v5.3.3", {system = false})
add_requires("glaze 9.1.0", {system = false})

add_requires("freetype 2.14.1", {system = false, configs = {
    zlib = false, png = false, bzip2 = false, woff2 = false, harfbuzz = false}})
add_requires("harfbuzz 12.3.0", {system = false})
add_requires("libflac 1.4.3", {system = false})
add_requires("lame 3.100", {system = false})
add_requires("libopus 1.5.2", {system = false})
add_requires("libopusenc 0.2.1", {system = false})
add_requires("fdk-aac 2.0.3", {system = false})
add_requires("pugixml 1.15", {system = false})
add_requires("utfcpp v4.1.1", {system = false})
add_requires("picojson 111c9be", {system = false})
