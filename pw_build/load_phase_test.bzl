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
"""Test helpers for glob_dirs()."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")

def return_error(err):
    """Testing helper to return an error string rather than fail().

    To use this, the starlark function your testing must support injection
    of an alternative `fail()` handler.
    """
    return err

# This provider allows the test logic implementation to see the information
# captured by the build rule.
_TestExpectationInfo = provider(
    "A pair of expected and actual values for testing",
    fields = ["actual", "expected"],
)

def _comparison_case_rule_impl(ctx):
    return [
        _TestExpectationInfo(
            expected = ctx.attr.expected,
            actual = ctx.attr.actual,
        ),
    ]

# `rule()` calls have to be at the top of the file, so we can't just make this
# a lambda of some kind. The best we can do is make it super easy to stamp out
# more.
def build_comparison_case_rule(attr_type):
    return {
        "attrs": {
            "actual": attr_type,
            "expected": attr_type,
        },
        "implementation": _comparison_case_rule_impl,
        "provides": [_TestExpectationInfo],
    }

string_comparison = rule(
    **build_comparison_case_rule(attr.string())
)

string_list_comparison = rule(
    **build_comparison_case_rule(attr.string_list())
)

# Collect into a single struct to make it easier to load in a BUILD file.
PW_LOAD_PHASE_TEST_TYPES = struct(
    STRING = string_comparison,
    STRING_LIST = string_list_comparison,
)

# Implement the actual test logic. In this case, it's pretty trivial: just
# assert that the `expected` and `actual` attributes of the rule match.
def _load_phase_test_impl(ctx):
    env = analysistest.begin(ctx)
    target_under_test = analysistest.target_under_test(env)

    asserts.equals(
        env,
        target_under_test[_TestExpectationInfo].expected,
        target_under_test[_TestExpectationInfo].actual,
    )

    return analysistest.end(env)

_load_phase_test = analysistest.make(_load_phase_test_impl)

# This is the macro for decaring an individual test case.
def pw_load_phase_test(comparison_type):
    def comparison_test(name, expected, actual, tags = [], **rule_kwargs):
        comparison_type(
            name = name + ".case",
            expected = expected,
            actual = actual,
            tags = tags + ["manual"],
            testonly = True,
            **rule_kwargs
        )

        _load_phase_test(
            name = name,
            target_under_test = name + ".case",
            tags = tags,
            **rule_kwargs
        )

    return comparison_test

# Actual test rule types.
pw_string_comparison_test = pw_load_phase_test(PW_LOAD_PHASE_TEST_TYPES.STRING)
pw_string_list_comparison_test = pw_load_phase_test(PW_LOAD_PHASE_TEST_TYPES.STRING_LIST)
