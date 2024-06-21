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
"""Rules and macros related to platform compatibility."""

load("@bazel_skylib//lib:selects.bzl", "selects")

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

def boolean_constraint_value(name):
    """Syntactic sugar for a constraint with just two possible values.

    Args:
      name: The name of the "True" value of the generated constraint.
    """
    constraint_setting_name = name + ".constraint_setting"
    false_value_name = name + ".not"

    native.constraint_setting(
        name = constraint_setting_name,
        default_constraint_value = ":" + false_value_name,
        # Do not allow anyone to declare additional values for this setting.
        # It's boolean, so by definition it's true or false, that's it!
        visibility = ["//visibility:private"],
    )

    native.constraint_value(
        name = false_value_name,
        constraint_setting = ":" + constraint_setting_name,
        # The false value is not exposed at this time to avoid exposing more
        # API surface than necessary, and for better compliance with
        # https://bazel.build/rules/bzl-style#macros. But we may make it public
        # in the future.
        visibility = ["//visibility:private"],
    )

    native.constraint_value(
        name = name,
        constraint_setting = ":" + constraint_setting_name,
    )
