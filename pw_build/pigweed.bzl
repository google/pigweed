# Copyright 2019 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Pigweed build environment for bazel."""

load("@bazel_skylib//lib:selects.bzl", "selects")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")
load("@rules_cc//cc:action_names.bzl", "C_COMPILE_ACTION_NAME")
load(
    "//pw_build/bazel_internal:pigweed_internal.bzl",
    _compile_cc = "compile_cc",
    _link_cc = "link_cc",
)

# Used by `pw_cc_test`.
FUZZTEST_OPTS = [
    "-Wno-sign-compare",
    "-Wno-unused-parameter",
]

def pw_cc_binary(**kwargs):
    """Wrapper for cc_binary providing some defaults.

    Specifically, this wrapper adds deps on backend_impl targets for pw_assert
    and pw_log.

    Args:
      **kwargs: Passed to cc_binary.
    """

    # TODO: b/234877642 - Remove this implicit dependency once we have a better
    # way to handle the facades without introducing a circular dependency into
    # the build.
    kwargs["deps"] = kwargs.get("deps", []) + ["@pigweed//pw_build:default_link_extra_lib"]
    native.cc_binary(**kwargs)

def pw_cc_test(**kwargs):
    """Wrapper for cc_test providing some defaults.

    Specifically, this wrapper,

    *  Adds a dep on the pw_assert backend.
    *  Adds a dep on //pw_unit_test:simple_printing_main

    In addition, a .lib target is created that's a cc_library with the same
    kwargs. Such library targets can be used as dependencies of firmware images
    bundling multiple tests. The library target has alwayslink = 1, to support
    dynamic registration (ensure the tests are baked into the image even though
    there are no references to them, so that they can be found by RUN_ALL_TESTS
    at runtime).

    Args:
      **kwargs: Passed to cc_test.
    """
    kwargs["deps"] = kwargs.get("deps", []) + \
                     ["@pigweed//targets:pw_unit_test_main"]

    # TODO: b/234877642 - Remove this implicit dependency once we have a better
    # way to handle the facades without introducing a circular dependency into
    # the build.
    kwargs["deps"] = kwargs["deps"] + ["@pigweed//pw_build:default_link_extra_lib"]

    # Some tests may include FuzzTest, which includes headers that trigger
    # warnings. This check must be done here and not in `add_defaults`, since
    # the `use_fuzztest` config setting can refer by label to a library which
    # itself calls `add_defaults`.
    extra_copts = select({
        "@pigweed//pw_fuzzer:use_fuzztest": FUZZTEST_OPTS,
        "//conditions:default": [],
    })
    kwargs["copts"] = kwargs.get("copts", []) + extra_copts

    native.cc_test(**kwargs)

    kwargs["alwayslink"] = 1

    # pw_cc_test deps may include testonly targets.
    kwargs["testonly"] = True

    # Remove any kwargs that cc_library would not recognize.
    for arg in (
        "additional_linker_inputs",
        "args",
        "env",
        "env_inherit",
        "flaky",
        "local",
        "malloc",
        "shard_count",
        "size",
        "stamp",
        "timeout",
    ):
        kwargs.pop(arg, "")
    native.cc_library(name = kwargs.pop("name") + ".lib", **kwargs)

def pw_cc_perf_test(**kwargs):
    """A Pigweed performance test.

    This macro produces a cc_binary and,

    *  Adds a dep on the pw_assert backend.
    *  Adds a dep on //pw_perf_test:logging_main

    Args:
      **kwargs: Passed to cc_binary.
    """
    kwargs["deps"] = kwargs.get("deps", []) + \
                     ["@pigweed//pw_perf_test:logging_main"]
    kwargs["deps"] = kwargs["deps"] + ["@pigweed//targets:pw_assert_backend_impl"]
    native.cc_binary(**kwargs)

def pw_cc_facade(**kwargs):
    # Bazel facades should be source only cc_library's this is to simplify
    # lazy header evaluation. Bazel headers are not 'precompiled' so the build
    # system does not check to see if the build has the right dependant headers
    # in the sandbox. If a source file is declared here and includes a header
    # file the toolchain will compile as normal and complain about the missing
    # backend headers.
    if "srcs" in kwargs.keys():
        fail("'srcs' attribute does not exist in pw_cc_facade, please use \
        main implementing target.")
    native.cc_library(**kwargs)

def host_backend_alias(name, backend):
    """An alias that resolves to the backend for host platforms."""
    native.alias(
        name = name,
        actual = selects.with_or({
            (
                "@platforms//os:android",
                "@platforms//os:chromiumos",
                "@platforms//os:linux",
                "@platforms//os:macos",
                "@platforms//os:windows",
            ): backend,
            "//conditions:default": "@pigweed//pw_build:unspecified_backend",
        }),
    )

CcBlobInfo = provider(
    "Input to pw_cc_blob_library",
    fields = {
        "symbol_name": "The C++ symbol for the byte array.",
        "file_path": "The file path for the binary blob.",
        "linker_section": "If present, places the byte array in the specified " +
                          "linker section.",
        "alignas": "If present, the byte array is aligned as specified. The " +
                   "value of this argument is used verbatim in an alignas() " +
                   "specifier for the blob byte array.",
    },
)

def _pw_cc_blob_info_impl(ctx):
    return [CcBlobInfo(
        symbol_name = ctx.attr.symbol_name,
        file_path = ctx.file.file_path,
        linker_section = ctx.attr.linker_section,
        alignas = ctx.attr.alignas,
    )]

pw_cc_blob_info = rule(
    implementation = _pw_cc_blob_info_impl,
    attrs = {
        "symbol_name": attr.string(),
        "file_path": attr.label(allow_single_file = True),
        "linker_section": attr.string(default = ""),
        "alignas": attr.string(default = ""),
    },
    provides = [CcBlobInfo],
)

def _pw_cc_blob_library_impl(ctx):
    # Python tool takes a json file with info about blobs to generate.
    blobs = []
    blob_paths = []
    for blob in ctx.attr.blobs:
        blob_info = blob[CcBlobInfo]
        blob_paths.append(blob_info.file_path)
        blob_dict = {
            "file_path": blob_info.file_path.path,
            "symbol_name": blob_info.symbol_name,
            "linker_section": blob_info.linker_section,
        }
        if (blob_info.alignas):
            blob_dict["alignas"] = blob_info.alignas
        blobs.append(blob_dict)
    blob_json = ctx.actions.declare_file(ctx.label.name + "_blob.json")
    ctx.actions.write(blob_json, json.encode(blobs))

    hdr = ctx.actions.declare_file(ctx.attr.out_header)
    src = ctx.actions.declare_file(ctx.attr.out_header.removesuffix(".h") + ".cc")

    if (not ctx.attr.namespace):
        fail("namespace required for pw_cc_blob_library")

    args = ctx.actions.args()
    args.add("--blob-file={}".format(blob_json.path))
    args.add("--namespace={}".format(ctx.attr.namespace))
    args.add("--header-include={}".format(ctx.attr.out_header))
    args.add("--out-header={}".format(hdr.path))
    args.add("--out-source={}".format(src.path))

    ctx.actions.run(
        inputs = depset(direct = blob_paths + [blob_json]),
        progress_message = "Generating cc blob library for %s" % (ctx.label.name),
        tools = [
            ctx.executable._generate_cc_blob_library,
            ctx.executable._python_runtime,
        ],
        outputs = [hdr, src],
        executable = ctx.executable._generate_cc_blob_library,
        arguments = [args],
    )

    return _compile_cc(
        ctx,
        [src],
        [hdr],
        deps = ctx.attr.deps,
        includes = [ctx.bin_dir.path + "/" + ctx.label.package],
        defines = [],
    )

pw_cc_blob_library = rule(
    implementation = _pw_cc_blob_library_impl,
    doc = """Turns binary blobs into a C++ library of hard-coded byte arrays.

    The byte arrays are constant initialized and are safe to access at any time,
    including before main().

    Args:
        ctx: Rule context.
        blobs: A list of CcBlobInfo where each entry corresponds to a binary
               blob to be transformed from file to byte array. This is a
               required field. Blob fields include:

               symbol_name [required]: The C++ symbol for the byte array.

               file_path [required]: The file path for the binary blob.

               linker_section [optional]: If present, places the byte array
                in the specified linker section.

               alignas [optional]: If present, the byte array is aligned as
                specified. The value of this argument is used verbatim
                in an alignas() specifier for the blob byte array.

        out_header: The header file to generate. Users will include this file
                    exactly as it is written here to reference the byte arrays.

        namespace: The C++ namespace in which to place the generated blobs.
    """,
    attrs = {
        "blobs": attr.label_list(providers = [CcBlobInfo]),
        "out_header": attr.string(),
        "namespace": attr.string(),
        "_python_runtime": attr.label(
            default = Label("//:python3_interpreter"),
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
        "_generate_cc_blob_library": attr.label(
            default = Label("//pw_build/py:generate_cc_blob_library"),
            executable = True,
            cfg = "exec",
        ),
        "deps": attr.label_list(default = [Label("//pw_preprocessor")]),
    },
    provides = [CcInfo],
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)

def _pw_cc_binary_with_map_impl(ctx):
    [cc_info] = _compile_cc(
        ctx,
        ctx.files.srcs,
        [],
        deps = ctx.attr.deps + [ctx.attr.link_extra_lib, ctx.attr.malloc],
        includes = ctx.attr.includes,
        defines = ctx.attr.defines,
        local_defines = ctx.attr.local_defines,
    )

    map_file = ctx.actions.declare_file(ctx.label.name + ".map")
    map_flags = ["-Wl,-Map=" + map_file.path]

    return _link_cc(
        ctx,
        [cc_info.linking_context],
        ctx.attr.linkstatic,
        ctx.attr.stamp,
        user_link_flags = ctx.attr.linkopts + map_flags,
        additional_outputs = [map_file],
    )

pw_cc_binary_with_map = rule(
    implementation = _pw_cc_binary_with_map_impl,
    doc = """Links a binary like cc_binary does but generates a linker map file
    and provides it as an output after the executable in the DefaultInfo list
    returned by this rule.

    This rule makes an effort to somewhat mimic cc_binary args and behavior but
    doesn't fully support all options currently. Make variable substitution and
    tokenization handling isn't implemented by this rule on any of it's attrs.

    Args:
        ctx: Rule context.
    """,
    attrs = {
        "srcs": attr.label_list(
            allow_files = True,
            doc = "The list of C and C++ files that are processed to create the target.",
        ),
        "deps": attr.label_list(
            providers = [CcInfo],
            doc = "The list of other libraries to be linked in to the binary target.",
        ),
        "malloc": attr.label(
            default = "@bazel_tools//tools/cpp:malloc",
            doc = "Override the default dependency on malloc.",
        ),
        "link_extra_lib": attr.label(
            default = "@bazel_tools//tools/cpp:link_extra_lib",
            doc = "Control linking of extra libraries.",
        ),
        "linkstatic": attr.bool(
            doc = "True if binary should be link statically",
        ),
        "includes": attr.string_list(
            doc = "List of include dirs to add to the compile line.",
        ),
        "defines": attr.string_list(
            doc = "List of defines to add to the compile line.",
        ),
        "local_defines": attr.string_list(
            doc = "List of defines to add to the compile line.",
        ),
        "linkopts": attr.string_list(
            doc = "Add these flags to the C++ linker command.",
        ),
        "stamp": attr.int(
            default = -1,
            doc = "Whether to encode build information into the binary.",
        ),
    },
    provides = [DefaultInfo],
    fragments = ["cpp"],
    toolchains = use_cpp_toolchain(),
)

def _preprocess_linker_script_impl(ctx):
    cc_toolchain = find_cpp_toolchain(ctx)
    output_script = ctx.actions.declare_file(ctx.label.name + ".ld")
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = cc_toolchain,
        requested_features = ctx.features,
        unsupported_features = ctx.disabled_features,
    )
    cxx_compiler_path = cc_common.get_tool_for_action(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
    )
    c_compile_variables = cc_common.create_compile_variables(
        feature_configuration = feature_configuration,
        cc_toolchain = cc_toolchain,
        user_compile_flags = ctx.fragments.cpp.copts + ctx.fragments.cpp.conlyopts,
    )
    env = cc_common.get_environment_variables(
        feature_configuration = feature_configuration,
        action_name = C_COMPILE_ACTION_NAME,
        variables = c_compile_variables,
    )
    ctx.actions.run(
        outputs = [output_script],
        inputs = depset(
            [ctx.file.linker_script],
            transitive = [cc_toolchain.all_files],
        ),
        executable = cxx_compiler_path,
        arguments = [
            "-E",
            "-P",
            # TODO: b/296928739 - This flag is needed so cc1 can be located
            # despite the presence of symlinks. Normally this is provided
            # through copts inherited from the toolchain, but since those are
            # not pulled in here the flag must be explicitly added.
            "-no-canonical-prefixes",
            "-xc",
            ctx.file.linker_script.path,
            "-o",
            output_script.path,
        ] + [
            "-D" + d
            for d in ctx.attr.defines
        ] + ctx.attr.copts,
        env = env,
    )
    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        user_link_flags = ["-T", output_script.path],
        additional_inputs = depset(direct = [output_script]),
    )
    linking_context = cc_common.create_linking_context(
        linker_inputs = depset(direct = [linker_input]),
    )
    return [
        DefaultInfo(files = depset([output_script])),
        CcInfo(linking_context = linking_context),
    ]

pw_linker_script = rule(
    _preprocess_linker_script_impl,
    attrs = {
        "copts": attr.string_list(doc = "C compile options."),
        "defines": attr.string_list(doc = "C preprocessor defines."),
        "linker_script": attr.label(
            mandatory = True,
            allow_single_file = True,
            doc = "Linker script to preprocess.",
        ),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
)
