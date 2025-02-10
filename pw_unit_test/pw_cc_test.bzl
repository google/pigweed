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
"""Rules for declaring unit tests."""

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

def pw_cc_test(**kwargs):
    """Wrapper for cc_test providing some defaults.

    Specifically, this wrapper,

    *  Adds a dep on the pw_assert backend.
    *  Adds a dep on //pw_unit_test:simple_printing_main

    In addition, a .lib target is created that's a cc_library with the same
    kwargs. Such library targets can be used as dependencies of firmware images
    bundling multiple tests. The library target has alwayslink = 1, to support
    dynamic registration (ensure the tests are baked into the image even though
    there are no references to them, so that they can be found by RUN_ALL_TESTS
    at runtime).

    Args:
      **kwargs: Passed to cc_test.
    """
    kwargs["deps"] = kwargs.get("deps", []) + [
        str(Label("//pw_build:default_link_extra_lib")),
        str(Label("//pw_unit_test:pw_unit_test_alias_for_migration_only")),
    ]

    # Save the base set of deps minus pw_unit_test:main for the .lib target.
    original_deps = kwargs["deps"]

    # Add the unit test main label flag dep.
    test_main = kwargs.pop("test_main", str(Label("//pw_unit_test:main")))
    kwargs["deps"] = original_deps + [test_main]
    cc_test(**kwargs)

    kwargs["alwayslink"] = 1

    # pw_cc_test deps may include testonly targets.
    kwargs["testonly"] = True

    # Remove any kwargs that cc_library would not recognize.
    for arg in (
        "additional_linker_inputs",
        "args",
        "env",
        "env_inherit",
        "flaky",
        "local",
        "malloc",
        "shard_count",
        "size",
        "stamp",
        "timeout",
    ):
        kwargs.pop(arg, "")

    # Reset the deps for the .lib target.
    kwargs["deps"] = original_deps
    cc_library(name = kwargs.pop("name") + ".lib", **kwargs)
