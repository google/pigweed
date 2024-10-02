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
"""Deprecated hub for Pigweed's Bazel rules."""

load("//pw_perf_test:pw_cc_perf_test.bzl", _pw_cc_perf_test = "pw_cc_perf_test")
load("//pw_unit_test:pw_cc_test.bzl", _pw_cc_test = "pw_cc_test")
load(":pw_cc_binary.bzl", _pw_cc_binary = "pw_cc_binary", _pw_cc_binary_with_map = "pw_cc_binary_with_map")
load(":pw_cc_blob_library.bzl", _pw_cc_blob_info = "pw_cc_blob_info", _pw_cc_blob_library = "pw_cc_blob_library")
load(":pw_facade.bzl", _pw_facade = "pw_facade")
load(":pw_linker_script.bzl", _pw_linker_script = "pw_linker_script")

# WARNING: This file is deprecated! Do not use this or extend it!
# Instead, directly use the individual .bzl files.

pw_facade = _pw_facade
pw_cc_binary = _pw_cc_binary
pw_cc_binary_with_map = _pw_cc_binary_with_map
pw_cc_test = _pw_cc_test
pw_cc_perf_test = _pw_cc_perf_test
pw_cc_blob_library = _pw_cc_blob_library
pw_cc_blob_info = _pw_cc_blob_info
pw_linker_script = _pw_linker_script
