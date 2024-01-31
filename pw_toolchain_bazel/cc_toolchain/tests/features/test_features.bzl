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
"""Tests for pw_cc_feature and pw_cc_feature_set."""

load(
    "//cc_toolchain/tests:utils.bzl",
    "assert_eq",
    "assert_fail",
    "assert_labels_eq",
    "generate_test_rule",
)

visibility("private")

def _test_features_impl(_ctx, features, feature_sets, flag_sets, to_untyped_config, **_):
    # Verify that we the builtin features don't generate any configs.
    assert_eq(to_untyped_config(features = []).features, [])

    assert_eq(features.foo.name, "foo")
    assert_eq(features.foo.flag_sets.to_list(), [flag_sets.foo])

    # Verify that we can't have two features with the same feature name.
    assert_eq(len(to_untyped_config(features = [features.foo]).features), 1)
    assert_fail(to_untyped_config, features = [features.foo, features.conflict])

    assert_labels_eq(
        feature_sets.foobar.features,
        [features.foo, features.bar],
    )

    assert_labels_eq(
        features.implies.implies_features,
        [features.foo, features.bar],
    )

    assert_eq(list(features.requires.requires_any_of), [feature_sets.foobar, feature_sets.baz])

    # Implies requires both foo and bar to be listed.
    assert_fail(
        to_untyped_config,
        features = [features.implies, features.foo],
    )
    to_untyped_config(
        features = [features.implies, features.foo, features.bar],
    )

    # Requires requires either baz OR (foo AND bar) to be listed.
    to_untyped_config(
        features = [features.requires, features.baz],
    )

    to_untyped_config(
        features = [features.requires, features.foo, features.bar],
    )
    assert_fail(
        to_untyped_config,
        features = [features.requires, features.foo],
    )

test_features = generate_test_rule(_test_features_impl)
