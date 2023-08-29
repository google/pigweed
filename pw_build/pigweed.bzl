# Copyright 2019 The Pigweed Authors
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
"""Pigweed build environment for bazel."""

load("@bazel_skylib//lib:selects.bzl", "selects")
load(
    "//pw_build/bazel_internal:pigweed_internal.bzl",
    _add_defaults = "add_defaults",
)

# Used by `pw_cc_test`.
FUZZTEST_OPTS = [
    "-Wno-sign-compare",
    "-Wno-unused-parameter",
]

def pw_cc_binary(**kwargs):
    """Wrapper for cc_binary providing some defaults.

    Specifically, this wrapper,

    *  Adds default copts.
    *  Adds a dep on the pw_assert backend.
    *  Sets "linkstatic" to True.
    *  Disables header modules (via the feature -use_header_modules).

    Args:
      **kwargs: Passed to cc_binary.
    """
    kwargs["deps"] = kwargs.get("deps", [])

    # TODO(b/234877642): Remove this implicit dependency once we have a better
    # way to handle the facades without introducing a circular dependency into
    # the build.
    kwargs["deps"] = kwargs["deps"] + ["@pigweed_config//:pw_assert_backend_impl"]
    _add_defaults(kwargs)
    native.cc_binary(**kwargs)

def pw_cc_library(**kwargs):
    """Wrapper for cc_library providing some defaults.

    Specifically, this wrapper,

    *  Adds default copts.
    *  Sets "linkstatic" to True.
    *  Disables header modules (via the feature -use_header_modules).

    Args:
      **kwargs: Passed to cc_library.
    """
    _add_defaults(kwargs)
    native.cc_library(**kwargs)

def pw_cc_test(**kwargs):
    """Wrapper for cc_test providing some defaults.

    Specifically, this wrapper,

    *  Adds default copts.
    *  Adds a dep on the pw_assert backend.
    *  Adds a dep on //pw_unit_test:simple_printing_main
    *  Sets "linkstatic" to True.
    *  Disables header modules (via the feature -use_header_modules).

    In addition, a .lib target is created that's a cc_library with the same
    kwargs. Such library targets can be used as dependencies of firmware images
    bundling multiple tests. The library target has alwayslink = 1, to support
    dynamic registration (ensure the tests are baked into the image even though
    there are no references to them, so that they can be found by RUN_ALL_TESTS
    at runtime).

    Args:
      **kwargs: Passed to cc_test.
    """
    kwargs["deps"] = kwargs.get("deps", []) + \
                     ["@pigweed_config//:pw_unit_test_main"]

    # TODO(b/234877642): Remove this implicit dependency once we have a better
    # way to handle the facades without introducing a circular dependency into
    # the build.
    kwargs["deps"] = kwargs["deps"] + ["@pigweed_config//:pw_assert_backend_impl"]
    _add_defaults(kwargs)

    # Some tests may include FuzzTest, which includes headers that trigger
    # warnings. This check must be done here and not in `add_defaults`, since
    # the `use_fuzztest` config setting can refer by label to a library which
    # itself calls `add_defaults`.
    extra_copts = select({
        "@pigweed//pw_fuzzer:use_fuzztest": FUZZTEST_OPTS,
        "//conditions:default": [],
    })
    kwargs["copts"] = kwargs.get("copts", []) + extra_copts

    native.cc_test(**kwargs)

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
    native.cc_library(name = kwargs.pop("name") + ".lib", **kwargs)

def pw_cc_perf_test(**kwargs):
    """A Pigweed performance test.

    This macro produces a cc_binary and,

    *  Adds default copts.
    *  Adds a dep on the pw_assert backend.
    *  Adds a dep on //pw_perf_test:logging_main
    *  Sets "linkstatic" to True.
    *  Disables header modules (via the feature -use_header_modules).

    Args:
      **kwargs: Passed to cc_binary.
    """
    kwargs["deps"] = kwargs.get("deps", []) + \
                     ["@pigweed//pw_perf_test:logging_main"]
    kwargs["deps"] = kwargs["deps"] + ["@pigweed_config//:pw_assert_backend_impl"]
    _add_defaults(kwargs)
    native.cc_binary(**kwargs)

def pw_cc_facade(**kwargs):
    # Bazel facades should be source only cc_library's this is to simplify
    # lazy header evaluation. Bazel headers are not 'precompiled' so the build
    # system does not check to see if the build has the right dependant headers
    # in the sandbox. If a source file is declared here and includes a header
    # file the toolchain will compile as normal and complain about the missing
    # backend headers.
    if "srcs" in kwargs.keys():
        fail("'srcs' attribute does not exist in pw_cc_facade, please use \
        main implementing target.")
    _add_defaults(kwargs)
    native.cc_library(**kwargs)

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
