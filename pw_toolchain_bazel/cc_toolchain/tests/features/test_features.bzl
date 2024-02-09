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
    "@rules_cc//cc:cc_toolchain_config_lib.bzl",
    rules_cc_feature = "feature",
)
load(
    "//cc_toolchain/tests:utils.bzl",
    "assert_eq",
    "assert_fail",
    "assert_labels_eq",
    "generate_test_rule",
)

visibility("private")

def _test_features_impl(_ctx, features, feature_sets, flag_sets, to_untyped_config, feature_constraints, **_):
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

    # Verify that we fail iff the duplicate was not an override.
    assert_eq(
        to_untyped_config(
            features = [features.supports_pic],
        ).features,
        [rules_cc_feature(name = "supports_pic", enabled = True)],
    )
    assert_fail(
        to_untyped_config,
        features = [features.supports_pic_no_override],
    )

    # Verify that you can imply a "known" feature.
    to_untyped_config(
        features = [features.implies_supports_pic],
    )

    assert_labels_eq(
        feature_constraints.foo_not_baz.all_of,
        [features.foo],
    )
    assert_labels_eq(
        feature_constraints.foo_not_baz.none_of,
        [features.baz],
    )

    assert_labels_eq(
        feature_constraints.foo_only.all_of,
        [features.foo],
    )
    assert_labels_eq(
        feature_constraints.foo_only.none_of,
        [features.baz, features.bar],
    )

    # Constrained requires either (foo AND not baz) OR bar.
    to_untyped_config(features = [features.constrained, features.bar])

    # Validate that we don't require baz to exist.
    to_untyped_config(features = [features.constrained, features.foo])
    assert_fail(to_untyped_config, features = [features.constrained])

    # You should be able to do mutual exclusion without all the features being
    # defined
    assert_eq(
        to_untyped_config(features = [features.primary_feature]).features[0].provides,
        ["@@//cc_toolchain/tests/features:category"],
    )
    assert_eq(
        to_untyped_config(features = [features.mutex_provider]).features[0].provides,
        ["@@//cc_toolchain/tests/features:category"],
    )
    assert_eq(
        to_untyped_config(features = [features.mutex_label]).features[0].provides,
        ["primary_feature"],
    )

    ft = to_untyped_config(features = [features.bar]).features[0]
    assert_eq(len(ft.flag_sets), 1)
    assert_eq(ft.flag_sets[0].flag_groups[0].flags, ["--bar"])
    assert_eq(ft.env_sets[0].env_entries[0].key, "foo")
    assert_eq(ft.env_sets[0].env_entries[0].value, "%{bar}")
    assert_eq(ft.env_sets[0].env_entries[0].expand_if_available, "bar")

test_features = generate_test_rule(_test_features_impl)
