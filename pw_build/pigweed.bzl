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

# Standard compiler flags to reduce output binary size.
REDUCED_SIZE_COPTS = [
    "-fno-common",
    "-fno-exceptions",
    "-ffunction-sections",
    "-fdata-sections",
    "-fno-rtti",
]

STRICT_WARNINGS_COPTS = [
    "-Wall",
    "-Wextra",
    "-Wnon-virtual-dtor",
    # Make all warnings errors, except for the exemptions below.
    "-Werror",
    "-Wno-error=cpp",  # preprocessor #warning statement
    "-Wno-error=deprecated-declarations",  # [[deprecated]] attribute
]

CPP17_COPTS = [
    "-std=c++17",
    # Allow uses of the register keyword, which may appear in C headers.
    "-Wno-register",
]

INCLUDES_COPTS = [
    "-Ipw_preprocessor/public",
    "-Ipw_span/public",
    "-Ipw_status/public",
    "-Ipw_string/public",
    "-Ipw_unit_test/public",
]

PW_DEFAULT_COPTS = (
    REDUCED_SIZE_COPTS +
    STRICT_WARNINGS_COPTS +
    CPP17_COPTS +
    INCLUDES_COPTS
)

PW_DEFAULT_LINKOPTS = []

def _add_defaults(kwargs):
    kwargs.setdefault("copts", [])
    kwargs["copts"].extend(PW_DEFAULT_COPTS)

    kwargs.setdefault("linkopts", [])
    kwargs["linkopts"].extend(PW_DEFAULT_LINKOPTS)

    kwargs.setdefault("features", [])

    # Crosstool--adding this line to features disables header modules, which
    # don't work with -fno-rtti. Note: this is not a command-line argument,
    # it's "minus use_header_modules".
    kwargs["features"].append("-use_header_modules")

def pw_cc_binary(**kwargs):
    _add_defaults(kwargs)
    native.cc_binary(**kwargs)

def pw_cc_library(**kwargs):
    _add_defaults(kwargs)
    native.cc_library(**kwargs)

def pw_cc_test(**kwargs):
    _add_defaults(kwargs)
    kwargs.setdefault("deps", [])
    kwargs["deps"].append("//pw_unit_test:main")
    native.cc_test(**kwargs)
