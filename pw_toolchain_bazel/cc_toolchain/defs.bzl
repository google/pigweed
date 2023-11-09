# Copyright 2023 The Pigweed Authors
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
"""Toolchain configuration for Bazel."""

load(
    "//cc_toolchain/private:cc_toolchain.bzl",
    _OBJ_COPY_ACTION_NAME = "OBJ_COPY_ACTION_NAME",
    _OBJ_DUMP_ACTION_NAME = "OBJ_DUMP_ACTION_NAME",
    _pw_cc_toolchain = "pw_cc_toolchain",
)
load(
    "//cc_toolchain/private:toolchain_feature.bzl",
    _pw_cc_toolchain_feature = "pw_cc_toolchain_feature",
)

# TODO(b/301004620): Remove when bazel 7 is released and this constant exists in
# ACTION_NAMES
OBJ_COPY_ACTION_NAME = _OBJ_COPY_ACTION_NAME
OBJ_DUMP_ACTION_NAME = _OBJ_DUMP_ACTION_NAME

pw_cc_toolchain = _pw_cc_toolchain

pw_cc_toolchain_feature = _pw_cc_toolchain_feature
