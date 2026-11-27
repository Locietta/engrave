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
    "//bazel:windows_msvc": ["_MT"],
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
    "//bazel:windows_msvc": [
        "-fstrict-aliasing",
        "-fno-wrapv",
    ],
    "//conditions:default": [],
}) + select({
    # ASan's dynamic stack alloca disables Win64 reserved call frames, causing LLVM to emit
    # dynamic `subq $0x20, %rsp` before calls inside WinEH catch$ funclets and corrupting
    # RtlVirtualUnwind establisher frame lookup on rethrow.
    "//bazel:asan_msvc": [
        "-mllvm",
        "-asan-stack-dynamic-alloca=0",
    ],
    "//conditions:default": [],
}) + select({
    # Under MSVC ABI, empty secondary base std::codecvt_base sits at the end of
    # libc++'s std::codecvt with zero tail bytes, tripping UBSan object-size in fs::u8path.
    "//bazel:ubsan_msvc": ["-fno-sanitize=object-size"],
    "//conditions:default": [],
}) + select({
    "@platforms//os:windows": [],
    "//conditions:default": [
        "-fPIC",
        "-pthread",
    ],
})

ENGINE_LINKOPTS = select({
    "//bazel:windows_msvc": ["-Wl,/STACK:16777216"],
    "//conditions:default": [
        "-pthread",
        "-ldl",
        "-lm",
    ],
}) + select({
    "//bazel:opt_msvc_gc": ["-Wl,/OPT:SAFEICF"],
    "//bazel:opt_linux_gc": ["-Wl,--icf=safe"],
    "//conditions:default": [],
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

def _win_manifest_res_impl(ctx):
    cc_toolchain = find_cc_toolchain(ctx)
    objcopy = cc_toolchain.objcopy_executable
    windows_host = objcopy.endswith(".exe")
    llvm_rc = "%s/llvm-rc%s" % (objcopy.rpartition("/")[0], ".exe" if windows_host else "")
    rc_file = ctx.actions.declare_file(ctx.label.name + ".rc")
    res_file = ctx.actions.declare_file(ctx.label.name + ".res")
    manifest_path = ctx.file.manifest.path.replace("\\", "/")
    ctx.actions.write(rc_file, '1 24 "%s"\n' % manifest_path)
    ctx.actions.run(
        executable = llvm_rc,
        arguments = ["/no-preprocess", "/FO", res_file.path, rc_file.path],
        inputs = depset(
            direct = [rc_file, ctx.file.manifest],
            transitive = [cc_toolchain.all_files],
        ),
        outputs = [res_file],
        mnemonic = "WinManifestRes",
        progress_message = "Compiling Windows manifest %{label}",
    )
    return [DefaultInfo(files = depset([res_file]))]

_win_manifest_res = rule(
    implementation = _win_manifest_res_impl,
    attrs = {
        "manifest": attr.label(allow_single_file = True, mandatory = True),
    },
    fragments = ["cpp"],
    toolchains = use_cc_toolchain(),
)

def win_manifest_library(name, manifest, visibility = None):
    res_name = name + "_res"
    _win_manifest_res(
        name = res_name,
        manifest = manifest,
        target_compatible_with = ["@platforms//os:windows"],
    )
    cc_library(
        name = name,
        additional_linker_inputs = [":" + res_name],
        linkopts = ["-Wl,$(execpath :%s)" % res_name],
        target_compatible_with = ["@platforms//os:windows"],
        visibility = visibility,
    )

def pch_consumer_copts(pch_label):
    """Resolve relocatable PCH inputs relative to the current execroot."""
    return [
        "-Xclang",
        "-fno-validate-pch",
        "-include-pch",
        "$(execpath %s)" % pch_label,
        "-Xclang",
        "-isysroot",
        "-Xclang",
        ".",
    ]
