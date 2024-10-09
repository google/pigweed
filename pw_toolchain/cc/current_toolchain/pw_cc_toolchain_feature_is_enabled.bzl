# Copyright 2024 The Pigweed Authors
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
"""A rule for letting enabled toolchain features drive build configuration."""

load("@bazel_skylib//rules:common_settings.bzl", "BuildSettingInfo")
load("@bazel_tools//tools/cpp:toolchain_utils.bzl", "find_cpp_toolchain", "use_cpp_toolchain")

def _cc_toolchain_feature_is_enabled_impl(ctx):
    toolchain = find_cpp_toolchain(ctx)
    feature_configuration = cc_common.configure_features(
        ctx = ctx,
        cc_toolchain = toolchain,
    )
    val = cc_common.is_enabled(
        feature_configuration = feature_configuration,
        feature_name = ctx.attr.feature_name,
    )
    return [
        config_common.FeatureFlagInfo(value = str(val)),
        BuildSettingInfo(value = val),
    ]

_cc_toolchain_feature_is_enabled = rule(
    implementation = _cc_toolchain_feature_is_enabled_impl,
    attrs = {
        "feature_name": attr.string(
            mandatory = True,
            doc = "The feature name to match against",
        ),
        "_cc_toolchain": attr.label(default = Label("@bazel_tools//tools/cpp:current_cc_toolchain")),
    },
    doc = """Extracts a matching, enabled feature from the current C/C++ toolchain.

This rule is a bridge that allows feature presence/absence to be communicated
from a toolchain configuration as if it was just a `bool_flag`.

This allows toolchain features to become branches in `select()` statements:

```py
_cc_toolchain_feature_is_enabled(
    name = "toolchain_flavor_chocolate",
    feature_name = "toolchain_flavor_chocolate",
)

config_setting(
    name = "chocolate",
    flag_values = {":toolchain_flavor_chocolate": "True"},
)

cc_library(
    name = "libfoo",
    copts = select({
        ":chocolate": ["-fno-dog-food"],
        "//conditions:default": [],
    }),
    srcs = ["foo.cc"],
)
```
""",
    toolchains = use_cpp_toolchain(),
    fragments = ["cpp"],
)

def pw_cc_toolchain_feature_is_enabled(*, name, feature_name, **kwargs):
    _cc_toolchain_query_name = name + ".value"
    _cc_toolchain_feature_is_enabled(
        name = _cc_toolchain_query_name,
        feature_name = feature_name,
        **kwargs
    )

    native.config_setting(
        name = name,
        flag_values = {":{}".format(_cc_toolchain_query_name): "True"},
    )
