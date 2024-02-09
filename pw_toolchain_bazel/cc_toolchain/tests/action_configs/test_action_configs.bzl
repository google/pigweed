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
    "generate_test_rule",
)

visibility("private")

def _test_action_configs_impl(_ctx, action_configs, features, flag_sets, extra_action_files, to_untyped_config, **_):
    def get_action_configs(**kwargs):
        action_configs = to_untyped_config(**kwargs).action_configs
        actions = [(action.action_name, action) for action in action_configs]
        deduped_actions = dict(actions)

        # Verify no duplicates
        assert_eq(sorted([x[0] for x in actions]), sorted(deduped_actions))
        return deduped_actions

    def get_files(**kwargs):
        return {
            k: sorted([f.basename for f in v.to_list()])
            for k, v in to_untyped_config(**kwargs).action_to_files.items()
        }

    # Verify that we validate that features with duplicate action names are not
    # permitted
    assert_fail(
        to_untyped_config,
        action_configs = [action_configs.all_c_compile, action_configs.c_compile],
        features = [features.foo],
    )

    # Verify that the validation on implied features works (foo must exist for
    # an action config that implies foo).
    assert_fail(to_untyped_config, action_configs = [action_configs.all_c_compile])
    assert_eq(
        get_action_configs(
            action_configs = [action_configs.all_c_compile],
            features = [features.foo],
        )["c-compile"].implies,
        ["foo"],
    )

    # Verify that flag sets get added iff they match the action.
    acs = get_action_configs(
        action_configs = [action_configs.all_c_compile],
        features = [features.foo],
        flag_sets = [flag_sets.bar],
    )
    assert_eq(
        {k: [fs.flag_groups for fs in v.flag_sets] for k, v in acs.items()},
        {
            "c-compile": [list(flag_sets.bar.flag_groups)],
            "cc-flags-make-variable": [],
        },
    )

    assert_eq(
        get_files(
            action_configs = [
                action_configs.c_compile,
                action_configs.cpp_compile_from_tool,
                action_configs.assemble_from_bin,
            ],
        ),
        {
            "c-compile": [],
            "c++-compile": ["clang_wrapper", "data.txt", "real_clang"],
            "assemble": ["clang_wrapper", "real_clang"],
        },
    )

    assert_fail(
        to_untyped_config,
        action_configs = [action_configs.requires_foo],
    )

    tools = get_action_configs(
        action_configs = [action_configs.requires_foo],
        features = [features.foo],
    )["c-compile"].tools
    assert_eq(len(tools), 1)
    assert_eq(len(tools[0].with_features), 1)
    assert_eq(tools[0].with_features[0].features, ["foo"])
    assert_eq(sorted(tools[0].with_features[0].not_features), ["bar", "baz"])

    want_files = {
        "assemble": ["clang_wrapper.sh", "data.txt"],
        "c-compile": ["data.txt"],
        "c++-compile": ["clang_wrapper.sh"],
    }
    assert_eq(get_files(extra_action_files = [
        extra_action_files.c_compiler_data,
        extra_action_files.cpp_compiler_data,
    ]), want_files)

    assert_eq(get_files(extra_action_files = [extra_action_files.data]), want_files)

    config = to_untyped_config(
        action_configs = [action_configs.c_compile],
    )
    assert_eq(config.action_configs[0].implies, ["implied_by_c-compile"])
    assert_eq(len(config.features), 1)
    assert_eq(config.features[0].name, "implied_by_c-compile")
    assert_eq(config.features[0].flag_sets, [])
    assert_eq(len(config.features[0].env_sets), 1)
    assert_eq(config.features[0].env_sets[0].env_entries[0].key, "foo")
    assert_eq(config.features[0].env_sets[0].env_entries[0].value, "%{bar}")

test_action_configs = generate_test_rule(_test_action_configs_impl)
