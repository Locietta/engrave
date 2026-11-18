"""Pinned third-party dependencies, Pixi C libraries, and build metadata for engrave."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

FLAC_CONFIG_H = """\
#pragma once
#define CPU_IS_BIG_ENDIAN 0
#define ENABLE_64_BIT_WORDS 1
#define OGG_FOUND 0
#define FLAC__HAS_OGG 0
#define FLAC__HAS_X86INTRIN 1
#define FLAC__HAS_NEONINTRIN 0
#define FLAC__HAS_A64NEONINTRIN 0
#define WITH_AVX 1
#define FLAC__USE_AVX 1
#define HAVE_BSWAP16 1
#define HAVE_BSWAP32 1
#define HAVE_CPUID_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LROUND 1
#define HAVE_STDINT_H 1
#define HAVE_STRING_H 1
#define PACKAGE_VERSION "1.4.3"
#define WORDS_BIGENDIAN 0
#ifndef _WIN32
#define HAVE_CLOCK_GETTIME 1
#define HAVE_FSEEKO 1
#define HAVE_SYS_PARAM_H 1
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
"""

OPUS_CONFIG_H = """\
#pragma once
#define PACKAGE_VERSION "1.5.2"
"""

OPUSENC_CONFIG_H = """\
#pragma once
#define PACKAGE_NAME "libopusenc"
#define PACKAGE_VERSION "0.2.1"
"""

LAME_CONFIG_H = """\
#pragma once
#define SIZEOF_DOUBLE 8
#define SIZEOF_FLOAT 4
#define SIZEOF_INT 4
#define SIZEOF_SHORT 2
#define SIZEOF_UNSIGNED_INT 4
#define SIZEOF_UNSIGNED_SHORT 2
#ifdef _WIN32
#define SIZEOF_LONG 4
#define SIZEOF_UNSIGNED_LONG 4
#define SIZEOF_LONG_DOUBLE 12
#else
#define SIZEOF_LONG 8
#define SIZEOF_UNSIGNED_LONG 8
#define SIZEOF_LONG_DOUBLE 16
#endif
#define STDC_HEADERS 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define PACKAGE "lame"
#define PROTOTYPES 1
#define USE_FAST_LOG 1
#define HAVE_MPGLIB 1
#define DECODE_ON_THE_FLY 1
#define HAVE_XMMINTRIN_H 1
#define LAME_LIBRARY_BUILD 1
typedef long double ieee854_float80_t;
typedef double ieee754_float64_t;
typedef float ieee754_float32_t;
"""

def _configured_archive_impl(rctx):
    rctx.download_and_extract(
        url = rctx.attr.urls,
        sha256 = rctx.attr.sha256,
        stripPrefix = rctx.attr.strip_prefix,
    )
    for dst, content in rctx.attr.extra_files.items():
        rctx.file(dst, content)
    for dst, src in rctx.attr.header_copies.items():
        rctx.file(dst, rctx.read(src))
    rctx.file("BUILD.bazel", rctx.read(rctx.attr.build_file))

configured_archive = repository_rule(
    implementation = _configured_archive_impl,
    attrs = {
        "build_file": attr.label(mandatory = True),
        "extra_files": attr.string_dict(),
        "header_copies": attr.string_dict(),
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
        "urls": attr.string_list(mandatory = True),
    },
)

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
""" % (lib_dir, lib_dir)

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
    human = _parse_cmake_var(version_text, "MUSE_APP_NAME_HUMAN_READABLE", "MuseScore Studio")
    machine = _parse_cmake_var(version_text, "MUSE_APP_NAME_MACHINE_READABLE", "MuseScoreStudio")
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
    http_archive(
        name = "amc",
        build_file = Label("//bazel/third_party:amc.BUILD.bazel"),
        sha256 = "1c0adc6a5deaf07853d16601b6db6cc662188a071c22f08e6de04e1bea6f3830",
        strip_prefix = "amc-2.8.1",
        urls = ["https://github.com/AmadeusITGroup/amc/archive/refs/tags/v2.8.1.tar.gz"],
    )
    http_archive(
        name = "unordered_dense",
        build_file = Label("//bazel/third_party:unordered_dense.BUILD.bazel"),
        sha256 = "862605ef21424c6c92881aac4cf8f09c3abd91c80588446c83892583228eca67",
        strip_prefix = "unordered_dense-5.3.3",
        urls = ["https://github.com/martinus/unordered_dense/archive/refs/tags/v5.3.3.tar.gz"],
    )
    http_archive(
        name = "ngcpp_proxy",
        build_file = Label("//bazel/third_party:ngcpp_proxy.BUILD.bazel"),
        sha256 = "f46fd0fbcaf461f4904f35416edf94abc62d5582b66d6e769e676fd7bd478311",
        strip_prefix = "proxy-4.1.0",
        urls = ["https://github.com/ngcpp/proxy/archive/refs/tags/4.1.0.tar.gz"],
    )
    http_archive(
        name = "glaze",
        build_file = Label("//bazel/third_party:glaze.BUILD.bazel"),
        sha256 = "55f8a675397c505f9bee6c160e1c26b0085868f185d04d7a7727531fdbff4bd4",
        strip_prefix = "glaze-9.1.0",
        urls = ["https://github.com/stephenberry/glaze/archive/refs/tags/v9.1.0.tar.gz"],
    )
    http_archive(
        name = "utfcpp",
        build_file = Label("//bazel/third_party:utfcpp.BUILD.bazel"),
        sha256 = "1ca68016f0abc24172998e39ce0d8f8e2b7a26f7579a0ff85d4e1b9a7aea56f8",
        strip_prefix = "utfcpp-4.1.1",
        urls = ["https://github.com/nemtrif/utfcpp/archive/refs/tags/v4.1.1.tar.gz"],
    )
    http_archive(
        name = "picojson",
        build_file = Label("//bazel/third_party:picojson.BUILD.bazel"),
        patch_args = ["-p1"],
        patches = [Label("//bazel/third_party:picojson.patch")],
        sha256 = "671f89832a17e9e71398f80c0a326afa2ebe81f4c26d5a9992e1fcd0888ae151",
        strip_prefix = "picojson-111c9be5188f7350c2eac9ddaedd8cca3d7bf394",
        urls = ["https://github.com/kazuho/picojson/archive/111c9be.tar.gz"],
    )
    http_archive(
        name = "pugixml",
        build_file = Label("//bazel/third_party:pugixml.BUILD.bazel"),
        sha256 = "655ade57fa703fb421c2eb9a0113b5064bddb145d415dd1f88c79353d90d511a",
        strip_prefix = "pugixml-1.15",
        urls = ["https://github.com/zeux/pugixml/releases/download/v1.15/pugixml-1.15.tar.gz"],
    )
    http_archive(
        name = "freetype",
        build_file = Label("//bazel/third_party:freetype.BUILD.bazel"),
        sha256 = "174d9e53402e1bf9ec7277e22ec199ba3e55a6be2c0740cb18c0ee9850fc8c34",
        strip_prefix = "freetype-2.14.1",
        urls = [
            "https://downloads.sourceforge.net/project/freetype/freetype2/2.14.1/freetype-2.14.1.tar.gz",
            "https://download.savannah.gnu.org/releases/freetype/freetype-2.14.1.tar.gz",
        ],
    )
    http_archive(
        name = "harfbuzz",
        build_file = Label("//bazel/third_party:harfbuzz.BUILD.bazel"),
        sha256 = "8660ebd3c27d9407fc8433b5d172bafba5f0317cb0bb4339f28e5370c93d42b7",
        strip_prefix = "harfbuzz-12.3.0",
        urls = ["https://github.com/harfbuzz/harfbuzz/releases/download/12.3.0/harfbuzz-12.3.0.tar.xz"],
    )
    http_archive(
        name = "fdk_aac",
        build_file = Label("//bazel/third_party:fdk_aac.BUILD.bazel"),
        sha256 = "e25671cd96b10bad896aa42ab91a695a9e573395262baed4e4a2ff178d6a3a78",
        strip_prefix = "fdk-aac-2.0.3",
        urls = ["https://github.com/mstorsjo/fdk-aac/archive/refs/tags/v2.0.3.tar.gz"],
    )
    configured_archive(
        name = "libflac",
        build_file = Label("//bazel/third_party:libflac.BUILD.bazel"),
        extra_files = {"config_include/config.h": FLAC_CONFIG_H},
        sha256 = "6c58e69cd22348f441b861092b825e591d0b822e106de6eb0ee4d05d27205b70",
        strip_prefix = "flac-1.4.3",
        urls = ["https://github.com/xiph/flac/releases/download/1.4.3/flac-1.4.3.tar.xz"],
    )
    configured_archive(
        name = "libopus",
        build_file = Label("//bazel/third_party:libopus.BUILD.bazel"),
        extra_files = {"config_include/config.h": OPUS_CONFIG_H},
        sha256 = "65c1d2f78b9f2fb20082c38cbe47c951ad5839345876e46941612ee87f9a7ce1",
        strip_prefix = "opus-1.5.2",
        urls = ["https://github.com/xiph/opus/releases/download/v1.5.2/opus-1.5.2.tar.gz"],
    )
    configured_archive(
        name = "libopusenc",
        build_file = Label("//bazel/third_party:libopusenc.BUILD.bazel"),
        extra_files = {"config_include/config.h": OPUSENC_CONFIG_H},
        header_copies = {"staged_include/opus/opusenc.h": "include/opusenc.h"},
        sha256 = "56952a926ff962c62a468b43cc8506c069bda767cade4dc92824b74edd570d68",
        strip_prefix = "libopusenc-0.2.1",
        urls = ["https://github.com/xiph/libopusenc/archive/v0.2.1.tar.gz"],
    )
    configured_archive(
        name = "lame",
        build_file = Label("//bazel/third_party:lame.BUILD.bazel"),
        extra_files = {"config_include/config.h": LAME_CONFIG_H},
        header_copies = {"staged_include/lame/lame.h": "include/lame.h"},
        sha256 = "ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e",
        strip_prefix = "lame-3.100",
        urls = ["https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz"],
    )
    http_archive(
        name = "gtest",
        build_file = Label("//bazel/third_party:gtest.BUILD.bazel"),
        sha256 = "40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff",
        strip_prefix = "googletest-1.17.0",
        urls = ["https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip"],
    )
    pixi_deps_repo(name = "pixi_deps")
    engrave_config_repo(name = "engrave_config")

engrave_deps = module_extension(implementation = _engrave_deps_impl)
