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

DEBUGGING = [
    "-g",
]

# Standard compiler flags to reduce output binary size.
REDUCED_SIZE_COPTS = [
    "-fno-common",
    "-fno-exceptions",
    "-ffunction-sections",
    "-fdata-sections",
]

STRICT_WARNINGS_COPTS = [
    "-Wall",
    "-Wextra",
    # Make all warnings errors, except for the exemptions below.
    "-Werror",
    "-Wno-error=cpp",  # preprocessor #warning statement
    "-Wno-error=deprecated-declarations",  # [[deprecated]] attribute
]

CPP17_COPTS = [
    "-std=c++17",
    "-fno-rtti",
    "-Wnon-virtual-dtor",
    # Allow uses of the register keyword, which may appear in C headers.
    "-Wno-register",
]

DISABLE_PENDING_WORKAROUND_COPTS = [
    "-Wno-private-header",
]

PW_DEFAULT_COPTS = (
    DEBUGGING +
    REDUCED_SIZE_COPTS +
    STRICT_WARNINGS_COPTS +
    DISABLE_PENDING_WORKAROUND_COPTS
)

KYTHE_COPTS = [
    "-Wno-unknown-warning-option",
]

PW_DEFAULT_LINKOPTS = []

def _add_defaults(kwargs):
    """Adds default arguments suitable for both C and C++ code to kwargs."""

    copts = kwargs.get("copts", []) + PW_DEFAULT_COPTS
    kwargs["copts"] = select({
        "//pw_build:kythe": copts + KYTHE_COPTS,
        "//conditions:default": copts,
    })
    kwargs["linkopts"] = kwargs.get("linkopts", []) + PW_DEFAULT_LINKOPTS

    # Set linkstatic to avoid building .so files.
    kwargs["linkstatic"] = True

    kwargs.setdefault("features", [])

    # Crosstool--adding this line to features disables header modules, which
    # don't work with -fno-rtti. Note: this is not a command-line argument,
    # it's "minus use_header_modules".
    kwargs["features"].append("-use_header_modules")

def _default_cc_and_c_kwargs(kwargs):
    _add_defaults(kwargs)
    kwargs.setdefault("srcs", [])

    cc = dict(kwargs.items())
    cc["srcs"] = [src for src in kwargs["srcs"] if not src.endswith(".c")]
    cc["copts"] = cc["copts"] + CPP17_COPTS

    c_srcs = [src for src in kwargs["srcs"] if src.endswith(".c")]

    if c_srcs:
        c = dict(kwargs.items())
        c["name"] += "_c"
        c["srcs"] = c_srcs + [src for src in kwargs["srcs"] if src.endswith(".h")]

        cc["deps"] = cc.get("deps", []) + [":" + c["name"]]
        return cc, c

    return cc, None

def _add_cc_and_c_targets(target, kwargs):
    cc_kwargs, c_kwargs = _default_cc_and_c_kwargs(kwargs)

    if c_kwargs:
        native.cc_library(**c_kwargs)

    target(**cc_kwargs)

def pw_cc_binary(**kwargs):
    _add_cc_and_c_targets(native.cc_binary, kwargs)

def pw_cc_library(**kwargs):
    _add_cc_and_c_targets(native.cc_library, kwargs)

def pw_cc_test(**kwargs):
    kwargs["deps"] = kwargs.get("deps", []) + ["//pw_unit_test:main"]
    _add_cc_and_c_targets(native.cc_test, kwargs)
