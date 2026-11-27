"""Pixi C libraries and build metadata for engrave."""

def _pixi_deps_impl(rctx):
    engrave_dir = rctx.path(rctx.attr.anchor).dirname
    override = rctx.os.environ.get("ENGRAVE_PIXI_PREFIX", "")
    if override:
        prefix = rctx.path(override)
    else:
        host_candidate = engrave_dir.dirname.dirname.get_child(".pixi").get_child("envs").get_child("default")
        if host_candidate.exists:
            prefix = host_candidate
        else:
            prefix = engrave_dir.get_child(".pixi").get_child("envs").get_child("default")
    if not prefix.exists:
        fail("Pixi environment not found at %s; run `pixi install`" % prefix)

    rctx.symlink(prefix, "env")
    is_win = rctx.os.name.lower().startswith("windows")
    if is_win:
        build_content = """\
load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

cc_import(
    name = "cairo_import",
    interface_library = "env/Library/lib/cairo.lib",
    shared_library = "env/Library/bin/cairo.dll",
)

cc_library(
    name = "cairo",
    hdrs = glob(
        [
            "env/Library/include/cairo/*.h",
            "env/Library/include/fontconfig/*.h",
        ],
        allow_empty = True,
    ),
    includes = [
        "env/Library/include",
        "env/Library/include/cairo",
    ],
    visibility = ["//visibility:public"],
    deps = [":cairo_import"],
)

cc_import(
    name = "freetype_import",
    interface_library = "env/Library/lib/freetype.lib",
    shared_library = "env/Library/bin/freetype.dll",
)

cc_library(
    name = "freetype",
    hdrs = glob(
        [
            "env/Library/include/ft2build.h",
            "env/Library/include/freetype/**/*.h",
            "env/Library/include/freetype2/**/*.h",
        ],
        allow_empty = True,
    ),
    includes = [
        "env/Library/include",
        "env/Library/include/freetype2",
    ],
    visibility = ["//visibility:public"],
    deps = [":freetype_import"],
)

cc_import(
    name = "zlib_import",
    interface_library = "env/Library/lib/zlib.lib",
    shared_library = "env/Library/bin/zlib.dll",
)

cc_library(
    name = "zlib",
    hdrs = [
        "env/Library/include/zconf.h",
        "env/Library/include/zlib.h",
    ],
    includes = ["env/Library/include"],
    visibility = ["//visibility:public"],
    deps = [":zlib_import"],
)
"""
    else:
        lib_dir = str(prefix.get_child("lib")).replace("\\", "/")
        build_content = """\
load("@rules_cc//cc:defs.bzl", "cc_import", "cc_library")

cc_import(
    name = "cairo_import",
    shared_library = "env/lib/libcairo.so",
)

cc_library(
    name = "cairo",
    hdrs = glob(
        [
            "env/include/cairo/*.h",
            "env/include/fontconfig/*.h",
        ],
        allow_empty = True,
    ),
    includes = [
        "env/include",
        "env/include/cairo",
    ],
    linkopts = ["-Wl,-rpath,%s"],
    visibility = ["//visibility:public"],
    deps = [":cairo_import"],
)

cc_import(
    name = "freetype_import",
    shared_library = "env/lib/libfreetype.so",
)

cc_library(
    name = "freetype",
    hdrs = glob(
        [
            "env/include/ft2build.h",
            "env/include/freetype/**/*.h",
            "env/include/freetype2/ft2build.h",
            "env/include/freetype2/freetype/**/*.h",
        ],
        allow_empty = True,
    ),
    includes = [
        "env/include",
        "env/include/freetype2",
    ],
    linkopts = ["-Wl,-rpath,%s"],
    visibility = ["//visibility:public"],
    deps = [":freetype_import"],
)

cc_import(
    name = "zlib_import",
    shared_library = "env/lib/libz.so",
)

cc_library(
    name = "zlib",
    hdrs = [
        "env/include/zconf.h",
        "env/include/zlib.h",
    ],
    includes = ["env/include"],
    linkopts = ["-Wl,-rpath,%s"],
    visibility = ["//visibility:public"],
    deps = [":zlib_import"],
)
""" % (lib_dir, lib_dir, lib_dir)

    rctx.file("BUILD.bazel", build_content)

pixi_deps_repo = repository_rule(
    implementation = _pixi_deps_impl,
    attrs = {
        "anchor": attr.label(default = Label("//:version.cmake")),
    },
    environ = ["ENGRAVE_PIXI_PREFIX"],
    local = True,
)

def _parse_cmake_var(text, name, default):
    needle = "set(" + name
    for line in text.splitlines():
        s = line.strip()
        if s.startswith(needle) and '"' in s:
            parts = s.split('"')
            if len(parts) >= 2:
                return parts[1]
    return default

def _engrave_config_impl(rctx):
    engrave_dir = rctx.path(rctx.attr.version_cmake).dirname
    source_root = str(engrave_dir).replace("\\", "/")
    version_text = rctx.read(rctx.attr.version_cmake)
    major = _parse_cmake_var(version_text, "MUSE_APP_VERSION_MAJOR", "5")
    minor = _parse_cmake_var(version_text, "MUSE_APP_VERSION_MINOR", "0")
    patch = _parse_cmake_var(version_text, "MUSE_APP_VERSION_PATCH", "0")
    human = _parse_cmake_var(version_text, "MUSE_APP_NAME_HUMAN_READABLE", "Engrave")
    machine = _parse_cmake_var(version_text, "MUSE_APP_NAME_MACHINE_READABLE", "Engrave")
    label = _parse_cmake_var(version_text, "MUSE_APP_VERSION_LABEL", "")

    revision = rctx.os.environ.get("ENGRAVE_GIT_REVISION", "").strip()
    if not revision:
        res = rctx.execute(["git", "-C", source_root, "rev-parse", "HEAD"], quiet = True)
        if res.return_code == 0:
            sha = res.stdout.strip()
            if len(sha) >= 7:
                revision = sha[:7]

    base_lines = [
        '#define MUSE_APP_BUILD_NUMBER "1"',
        '#define MUSE_APP_NAME_HUMAN_READABLE "%s"' % human,
        '#define MUSE_APP_NAME_MACHINE_READABLE "%s"' % machine,
        '#define MUSE_APP_REVISION "%s"' % revision,
        '#define MUSE_APP_VERSION "%s.%s.%s"' % (major, minor, patch),
        '#define MUSE_APP_VERSION_MAJOR "%s"' % major,
        '#define MUSE_APP_VERSION_MINOR "%s"' % minor,
        '#define MUSE_APP_VERSION_PATCH "%s"' % patch,
    ]
    if label:
        base_lines.append('#define MUSE_APP_VERSION_LABEL "%s"' % label)

    release_header = "\n".join(sorted(base_lines)) + "\n"
    unstable_header = "\n".join(sorted(base_lines + ["#define MUSE_APP_UNSTABLE 1"])) + "\n"

    rctx.file("release/engrave_version.h", release_header)
    rctx.file("unstable/engrave_version.h", unstable_header)
    rctx.file(
        "defs.bzl",
        'ENGRAVE_SOURCE_ROOT = "%s"\nENGRAVING_TESTS_DATA_ROOT = "%s/src/engraving/tests"\n' % (source_root, source_root),
    )
    rctx.file(
        "BUILD.bazel",
        """\
load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "release_version",
    hdrs = ["release/engrave_version.h"],
    includes = ["release"],
)

cc_library(
    name = "unstable_version",
    hdrs = ["unstable/engrave_version.h"],
    includes = ["unstable"],
)

cc_library(
    name = "muse_generated",
    visibility = ["//visibility:public"],
    deps = select({
        "@engrave//bazel:opt_mode": [":release_version"],
        "//conditions:default": [":unstable_version"],
    }),
)
""",
    )

engrave_config_repo = repository_rule(
    implementation = _engrave_config_impl,
    attrs = {
        "version_cmake": attr.label(default = Label("//:version.cmake")),
    },
    environ = ["ENGRAVE_GIT_REVISION"],
    local = True,
)

def _engrave_deps_impl(_mctx):
    pixi_deps_repo(name = "pixi_deps")
    engrave_config_repo(name = "engrave_config")

engrave_deps = module_extension(implementation = _engrave_deps_impl)
