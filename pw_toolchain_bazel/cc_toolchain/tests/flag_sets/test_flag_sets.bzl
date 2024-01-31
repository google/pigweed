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
"""Tests for pw_cc_flag_set."""

load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl", "flag_group")
load("//cc_toolchain/tests:utils.bzl", "assert_eq", "generate_test_rule")

visibility("private")

def _test_flag_sets_impl(_ctx, flag_sets, flag_groups, **_):
    assert_eq(sorted(flag_sets.foo.actions), ["c-compile"])
    assert_eq(list(flag_sets.foo.flag_groups), [flag_group(
        flags = ["--foo"],
    )])

    assert_eq(
        sorted(flag_sets.multiple_actions.actions),
        ["c++-compile", "c++-header-parsing", "c-compile", "cc-flags-make-variable"],
    )

    assert_eq(flag_groups.flag_group, flag_group(
        flags = ["-bar", "%{bar}"],
        iterate_over = "bar",
    ))

    assert_eq(list(flag_sets.wraps_flag_group.flag_groups), [flag_groups.flag_group])

test_flag_sets = generate_test_rule(_test_flag_sets_impl)
