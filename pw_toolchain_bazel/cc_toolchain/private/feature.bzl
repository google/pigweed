# Copyright 2023 The Pigweed Authors
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
"""Implementation of the pw_cc_feature and pw_cc_feature_set rules."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "feature_set",
)
load(
    ":providers.bzl",
    "PwFeatureInfo",
    "PwFeatureSetInfo",
    "PwFlagSetInfo",
)
load(":utils.bzl", "to_untyped_flag_set")

def _pw_cc_feature_set_impl(ctx):
    if not ctx.attr.feature_names:
        fail("At least one feature name must be specified in `feature_names`")
    return feature_set(
        feature_names = ctx.attr.features,
    )

pw_cc_feature_set = rule(
    implementation = _pw_cc_feature_set_impl,
    attrs = {
        "feature_names": attr.string_list(
            doc = """Features that must be enabled for this feature set to be deemed compatible with the current toolchain configuration.""",
        ),
    },
    provides = [PwFeatureSetInfo],
    doc = """Defines a set of required features.

This rule is effectively a wrapper for the `feature_set` constructor in
@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl.

This rule is used to express a group of features that may satisfy a
`pw_cc_feature`'s' `requires` list. If all of the specified features in a
`pw_cc_feature_set` are enabled, the `pw_cc_feature` that lists the feature
set *can* also be enabled. Note that this does cause the feature to be enabled;
it only means it is possible for the feature to be enabled.

Example:

    pw_cc_feature_set(
        name = "thin_lto_requirements",
        feature_names = [
            "thin_lto",
            "opt",
        ],
    )
""",
)

def _pw_cc_feature_impl(ctx):
    return feature(
        name = ctx.attr.feature_name,
        enabled = ctx.attr.enabled,
        flag_sets = [
            # TODO: b/311679764 - Add label propagation for deduping.
            to_untyped_flag_set(fs[PwFlagSetInfo])
            for fs in ctx.attr.flag_sets
        ],
        requires = [req[PwFeatureSetInfo] for req in ctx.attr.requires],
        implies = ctx.attr.implies,
        provides = ctx.attr.provides,
    )

pw_cc_feature = rule(
    implementation = _pw_cc_feature_impl,
    attrs = {
        "feature_name": attr.string(
            mandatory = True,
            doc = """The name of the feature that this rule implements.

Feature names are used to express feature dependencies and compatibility.
Because features are tracked by string names rather than labels, there's great
flexibility in swapping out feature implementations or overriding the built-in
legacy features that Bazel silently binds to every toolchain.

`feature_name` is used rather than `name` to distinguish between the rule
name, and the intended final feature name. This allows similar rules to exist
in the same package, even if slight differences are required.

Example:

    pw_cc_feature(
        name = "sysroot_macos",
        feature_name = "sysroot",
        ...
    )

    pw_cc_feature(
        name = "sysroot_linux",
        feature_name = "sysroot",
        ...
    )

While two features with the same `feature_name` may not be bound to the same
toolchain, they can happily live alongside each other in the same BUILD file.
""",
        ),
        "enabled": attr.bool(
            mandatory = True,
            doc = """Whether or not this feature is enabled by default.""",
        ),
        "flag_sets": attr.label_list(
            doc = """Flag sets that, when expanded, implement this feature.""",
            providers = [PwFlagSetInfo],
        ),
        "requires": attr.label_list(
            doc = """A list of feature sets that define toolchain compatibility.

If *at least one* of the listed `pw_cc_feature_set`s are fully satisfied (all
features exist in the toolchain AND are currently enabled), this feature is
deemed compatible and may be enabled.

Note: Even if `pw_cc_feature.requires` is satisfied, a feature is not enabled
unless another mechanism (e.g. command-line flags, `pw_cc_feature.implies`,
`pw_cc_feature.enabled`) signals that the feature should actually be enabled.
""",
            providers = [PwFeatureSetInfo],
        ),
        "implies": attr.string_list(
            doc = """Names of features enabled along with this feature.

Warning: If any of the named features cannot be enabled, this feature is
silently disabled.
""",
        ),
        "provides": attr.string_list(
            doc = """A list of additional feature names this feature fulfills.
If this feature has a side-effect of implementing another feature, it can be
useful to list that feature's name here to ensure they aren't enabled at the
same time.

Note: This feature cannot be enabled if another feature also provides the listed
feature names.
""",
        ),
    },
    provides = [PwFeatureInfo],
    doc = """Defines the implemented behavior of a C/C++ toolchain feature.

This rule is effectively a wrapper for the `feature` constructor in
@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl.

A feature is basically a dynamic flag set. There are a variety of dependencies
and compatibility requirements that must be satisfied for the listed flag sets
to be applied.

A feature may be enabled or disabled through the following mechanisms:
* Via command-line flags, or a `.bazelrc`.
* Through inter-feature relationships (enabling one feature may implicitly
  enable another).
* Individual rules may elect to manually enable or disable features through the
  builtin ``features`` attribute.

Because of the dynamic nature of toolchain features, it's generally best to
avoid enumerating features as part of your toolchain with the following
exceptions:
* You want the flags to be controllable via Bazel's CLI. For example, adding
  `-v` to a compiler invocation is often too verbose to be useful for most
  workflows, but can be instrumental when debugging obscure errors. By
  expressing compiler verbosity as a feature, users may opt-in when necessary.
* You need to carry forward Starlark toolchain behaviors. If you're migrating a
  complex Starlark-based toolchain definition to these rules, many of the
  workflows and flags were likely based on features. This rule exists to support
  those existing structures.

For more details about how Bazel handles features, see the official Bazel
documentation at
https://bazel.build/docs/cc-toolchain-config-reference#features.

Note: `env_sets` are not yet supported.

Examples:

    # A feature that can be easily toggled to include extra compiler output to
    # help debug things like include search path ordering and showing all the
    # flags passed to the compiler.
    #
    # Add `--features=verbose_compiler_output` to your Bazel invocation to
    # enable.
    pw_cc_feature(
        name = "verbose_compiler_output",
        enabled = False,
        feature_name = "verbose_compiler_output",
        flag_sets = [":verbose_compiler_flags"],
    )

    # This feature signals a capability, and doesn't have associated flags.
    #
    # For a list of well-known features, see:
    #    https://bazel.build/docs/cc-toolchain-config-reference#wellknown-features
    pw_cc_feature(
        name = "link_object_files",
        enabled = True,
        feature_name = "supports_start_end_lib",
    )
""",
)
