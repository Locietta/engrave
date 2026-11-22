"""Shared compiler flags, Clang binary PCH rule, and Windows PE manifest embedding."""

load("@rules_cc//cc:action_names.bzl", "ACTION_NAMES")
load("@rules_cc//cc:defs.bzl", "CcInfo", "cc_common", "cc_library")
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain", "use_cc_toolchain")

ENGINE_PLATFORM_DEFINES = select({
    "@platforms//os:windows": [
        "WIN32",
        "WIN32_LEAN_AND_MEAN",
        "NOGDI",
        "NOMINMAX",
        "_USE_MATH_DEFINES",
        "_UNICODE",
        "UNICODE",
    ],
    "//conditions:default": [],
}) + select({
    "//bazel:playback_harness_enabled": ["ENGRAVE_PLAYBACK_HARNESS"],
    "//conditions:default": [],
})

ENGINE_COPTS = [
    "-Wno-deprecated-declarations",
    "-Wno-unused-parameter",
    "-Wno-unused-variable",
    "-Wno-unused-but-set-variable",
    "-Wno-missing-field-initializers",
    "-Wno-sign-compare",
    "-Wno-reorder-ctor",
    "-Wno-overloaded-virtual",
    "-Wno-switch",
] + select({
    "@platforms//os:windows": [],
    "//conditions:default": [
        "-fPIC",
        "-pthread",
    ],
})

ENGINE_LINKOPTS = select({
    "@platforms//os:windows": ["-Wl,--stack,16777216"],
    "//conditions:default": [
        "-pthread",
        "-ldl",
        "-lm",
    ],
})

def _cc_pch_impl(ctx):
    cc_toolchain = find_cc_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features + [
            "module_maps",
            "layering_check",
            "parse_headers",
        ],
    )
    comp_ctx = cc_common.merge_compilation_contexts(
        compilation_contexts = [dep[CcInfo].compilation_context for dep in ctx.attr.deps],
    )
    pch_out = ctx.actions.declare_file(ctx.label.name + ".h.pch")
    compiler = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = ACTION_NAMES.cpp_compile,
    )
    user_flags = (
        ctx.fragments.cpp.copts +
        ctx.fragments.cpp.cxxopts +
        ctx.attr.copts +
        [
            "-x",
            "c++-header",
            "-fpch-instantiate-templates",
            "-Xclang",
            "-fno-pch-timestamp",
            # Record input paths relative to the execroot for disk cache reuse.
            "-Xclang",
            "-relocatable-pch",
            "-Xclang",
            "-isysroot",
            "-Xclang",
            ".",
        ]
    )
    sys_includes = [comp_ctx.system_includes]
    if hasattr(comp_ctx, "external_includes"):
        sys_includes.append(comp_ctx.external_includes)
    variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        user_compile_flags = user_flags,
        source_file = ctx.file.header.path,
        output_file = pch_out.path,
        include_directories = comp_ctx.includes,
        quote_include_directories = comp_ctx.quote_includes,
        system_include_directories = depset(transitive = sys_includes),
        framework_include_directories = comp_ctx.framework_includes,
        preprocessor_defines = depset(transitive = [comp_ctx.defines, comp_ctx.local_defines]),
    )
    args = cc_common.get_memory_inefficient_command_line(
        feature_configuration = feature_configuration,
        action_name = ACTION_NAMES.cpp_compile,
        variables = variables,
    )
    env = cc_common.get_environment_variables(
        feature_configuration = feature_configuration,
        action_name = ACTION_NAMES.cpp_compile,
        variables = variables,
    )
    ctx.actions.run(
        executable = compiler,
        arguments = args,
        env = env,
        inputs = depset(
            direct = [ctx.file.header],
            transitive = [cc_toolchain.all_files, comp_ctx.headers],
        ),
        outputs = [pch_out],
        mnemonic = "CppPch",
        progress_message = "Precompiling C++ header %{input}",
    )
    return [DefaultInfo(files = depset([pch_out]))]

cc_pch = rule(
    implementation = _cc_pch_impl,
    attrs = {
        "copts": attr.string_list(),
        "deps": attr.label_list(providers = [CcInfo]),
        "header": attr.label(allow_single_file = True, mandatory = True),
    },
    fragments = ["cpp"],
    toolchains = use_cc_toolchain(),
)

def staged_binary_group(name, srcs):
    # Bazel filegroups do not forward their sources' named output groups.
    runtime = name + "_runtime"
    native.filegroup(
        name = runtime,
        srcs = srcs,
        output_group = "runtime_dynamic_libraries",
        visibility = ["//visibility:private"],
    )
    native.filegroup(
        name = name,
        srcs = srcs + [":" + runtime],
    )

def _win_manifest_asm_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + ".s")
    manifest_bytes = ctx.file.manifest
    asm = """\
.section .rsrc,"dr"
.p2align 2
L_rsrc_root:
    .long 0
    .long 0
    .short 0
    .short 0
    .short 0
    .short 1
    .long 24
    .long (L_rsrc_type24 - L_rsrc_root) | 0x80000000
L_rsrc_type24:
    .long 0
    .long 0
    .short 0
    .short 0
    .short 0
    .short 1
    .long 1
    .long (L_rsrc_id1 - L_rsrc_root) | 0x80000000
L_rsrc_id1:
    .long 0
    .long 0
    .short 0
    .short 0
    .short 0
    .short 1
    .long 0x0409
    .long (L_rsrc_data - L_rsrc_root)
L_rsrc_data:
    .rva L_manifest_start
    .long L_manifest_end - L_manifest_start
    .long 0
    .long 0
.p2align 2
L_manifest_start:
    .incbin "%s"
L_manifest_end:
.p2align 2
""" % manifest_bytes.path.replace("\\", "/")
    ctx.actions.write(out, asm)
    return [DefaultInfo(files = depset([out]))]

_win_manifest_asm = rule(
    implementation = _win_manifest_asm_impl,
    attrs = {
        "manifest": attr.label(allow_single_file = True, mandatory = True),
    },
)

def win_manifest_library(name, manifest, visibility = None):
    asm_name = name + "_asm"
    _win_manifest_asm(
        name = asm_name,
        manifest = manifest,
        target_compatible_with = ["@platforms//os:windows"],
    )
    cc_library(
        name = name,
        srcs = [":" + asm_name],
        additional_compiler_inputs = [manifest],
        alwayslink = True,
        copts = ["-Wno-unused-command-line-argument"],
        target_compatible_with = ["@platforms//os:windows"],
        visibility = visibility,
    )
