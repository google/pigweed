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
"""Utility functions for testing toolchain rules."""

load(
    "//cc_toolchain/private:providers.bzl",
    "PwFlagGroupInfo",
    "PwFlagSetInfo",
)

visibility("//cc_toolchain/tests/...")

# Intentionally add two spaces after got, to ensure the lines are correctly
# aligned.
def assert_eq(got, want, msg = "\n\nGot  %r\n\nWant %r"):
    if got != want:
        fail(msg % (got, want))

def assert_in(want, container, msg = "\n\nUnable to find %r\n\nIn %r"):
    if want not in container:
        fail(msg % (want, container))

def assert_ne(got, bad, msg = "Got %r, wanted any value other than that"):
    if got == bad:
        fail(msg % got)

def assert_not_none(got, msg = "Got None, wanted a non-None value"):
    if got == None:
        fail(msg)

_RULES = {
    PwFlagSetInfo: "flag_sets",
    PwFlagGroupInfo: "flag_groups",
}

_PROVIDERS = {
    "//cc_toolchain/tests/flag_sets:foo": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:bar": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:baz": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:multiple_actions": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:flag_group": [PwFlagGroupInfo],
    "//cc_toolchain/tests/flag_sets:wraps_flag_group": [PwFlagSetInfo],
}

# This gives all tests access to all providers. This allows us to write
# non-brittle tests, since now we can write something like:
# assert_eq(features.foo.flag_sets, [flag_sets.foo])
# Note that this namespaces by provider instead of by label. We do so because
# this allows you to access both features.foo and feature_sets.foo (since a
# feature defines both a feature and a feature set).
def generate_test_rule(implementation):
    def wrapper(ctx):
        providers = {v: {} for v in _RULES.values()}
        for target in ctx.attr.test_cases:
            pkg = target.label.package
            name = target.label.name
            for provider in _PROVIDERS["//%s:%s" % (pkg, name)]:
                providers[_RULES[provider]][name] = target[provider]

        kwargs = {k: struct(**v) for k, v in providers.items()}
        return implementation(ctx, **kwargs)

    return rule(
        implementation = wrapper,
        attrs = {
            "test_cases": attr.label_list(default = _PROVIDERS.keys()),
        },
    )
