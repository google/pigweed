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

def reduced_size_copts():
    """Standard compiler flags to reduce output binary size."""

    return [
        "-fno-common",
        "-fno-exceptions",
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-rtti",
    ]

def strict_warnings_copts():
    return [
        "-Wall",
        "-Wextra",

        # Make all warnings errors, except for the exemptions below.
        "-Werror",
        "-Wno-error=cpp",  # preprocessor #warning statement
        "-Wno-error=deprecated-declarations",  # [[deprecated]] attribute
    ]

def cpp17_copts():
    return [
        "-std=c++17",

        # Allow uses of the register keyword, which may appear in C headers.
        "-Wno-register",
    ]

def includes_copts():
    includes = [
        "pw_preprocessor/public",
        "pw_span/public",
        "pw_status/public",
        "pw_unit_test/public",
    ]
    return ["-I" + x for x in includes]

def pw_default_copts():
    return (
        reduced_size_copts() +
        strict_warnings_copts() +
        cpp17_copts() +
        includes_copts()
    )

def pw_default_linkopts():
    return []

def pw_test(
        name,
        srcs,
        deps = None,
        **kwargs):
    """Create a Pigweed test.

    Args:
      name: name of target to create
      srcs: test source files
      deps: dependencies of target
    """

    if not deps:
        deps = []
    deps.append("//pw_unit_test:main")

    native.cc_test(
        name = name,
        srcs = srcs,
        deps = deps,
        **kwargs
    )
