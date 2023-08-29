# Copyright 2021 The Pigweed Authors
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
"""Utilities for fuzzing."""

load("@rules_fuzzing//fuzzing:cc_defs.bzl", "cc_fuzz_test")
load(
    "//pw_build/bazel_internal:pigweed_internal.bzl",
    _add_defaults = "add_defaults",
)

def pw_cc_fuzz_test(**kwargs):
    """Wrapper for cc_fuzz_test that adds required Pigweed dependencies.

    Args:
        **kwargs: Arguments to be augmented.
    """
    kwargs["deps"].append("//pw_fuzzer:libfuzzer")

    # TODO(b/234877642): Remove this implicit dependency once we have a better
    # way to handle the facades without introducing a circular dependency into
    # the build.
    kwargs["deps"].append("@pigweed_config//:pw_assert_backend_impl")

    # TODO(b/292628774): Only linux is supported for now.
    kwargs["target_compatible_with"] = ["@platforms//os:linux"]
    _add_defaults(kwargs)
    cc_fuzz_test(**kwargs)
