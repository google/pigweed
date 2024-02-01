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
    "PwFeatureConstraintInfo",
    "PwFeatureInfo",
    "PwFeatureSetInfo",
    "PwFlagGroupInfo",
    "PwFlagSetInfo",
)
load(
    "//cc_toolchain/private:utils.bzl",
    _to_untyped_config = "to_untyped_config",
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

def assert_fail(fn, *args, want = None, **kwargs):
    """Asserts that a function threw an exception

    Args:
        fn: A function that can take in a parameter "fail"
        *args: Arguments to be passed to fn
        **kwargs: Arguments to be papssed to fn
        want: Option[str]: The expected error message.
          If not provided, any error message is allowed.
    """

    # Use a mutable type so that the inner function can modify the outer scope.
    fails = []

    def set_fail_msg(msg):
        fails.append(msg)

    fn(fail = set_fail_msg, *args, **kwargs)
    if want == None:
        assert_ne(fails, [], msg = "Expected %r(**%r) to fail. Unexpectedly passed and got %%r" % (fn, kwargs))
    else:
        assert_ne(fails, [], msg = "Expected %r(**%r) to fail with msg %r. Unexpectedly passed and got %%r" % (fn, want, kwargs))
        assert_eq(fails[0], want, msg = "\n\nGot failure message  %r\n\nWant failure message %r")

def assert_labels_eq(got, want):
    """Asserts that two lists of providers with the label attribute are equal

    Args:
        got: A list or depset of providers containing the attribute "label"
        want: A list or depset of providers containing the attribute "label"
    """
    if type(got) == "depset":
        got = got.to_list()
    if type(want) == "depset":
        want = want.to_list()
    got = sorted([entry.label for entry in got])
    want = sorted([entry.label for entry in want])
    assert_eq(got, want)

_RULES = {
    PwFlagSetInfo: "flag_sets",
    PwFlagGroupInfo: "flag_groups",
    PwFeatureConstraintInfo: "feature_constraints",
    PwFeatureInfo: "features",
    PwFeatureSetInfo: "feature_sets",
}

_PROVIDERS = {
    "//cc_toolchain/tests/features:bar": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/features:baz": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/features:conflict": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/features:constrained": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/features:foo": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/features:foo_not_baz": [PwFeatureConstraintInfo],
    "//cc_toolchain/tests/features:foo_only": [PwFeatureConstraintInfo],
    "//cc_toolchain/tests/features:foobar": [PwFeatureSetInfo],
    "//cc_toolchain/tests/features:implies": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/features:requires": [PwFeatureInfo, PwFeatureSetInfo],
    "//cc_toolchain/tests/flag_sets:bar": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:baz": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:flag_group": [PwFlagGroupInfo],
    "//cc_toolchain/tests/flag_sets:foo": [PwFlagSetInfo],
    "//cc_toolchain/tests/flag_sets:multiple_actions": [PwFlagSetInfo],
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

        def to_untyped_config(features = [], feature_sets = [], fail = fail):
            feature_set = PwFeatureSetInfo(features = depset(
                features,
                transitive = [fs.features for fs in feature_sets],
            ))
            return _to_untyped_config(feature_set, fail = fail)

        kwargs = {k: struct(**v) for k, v in providers.items()}
        return implementation(ctx, to_untyped_config = to_untyped_config, **kwargs)

    return rule(
        implementation = wrapper,
        attrs = {
            "test_cases": attr.label_list(default = _PROVIDERS.keys()),
        },
    )
