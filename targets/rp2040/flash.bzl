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
"""Bazel macros for flashing an rp2040 binary."""

load("@bazel_skylib//rules:native_binary.bzl", "native_binary")

def flash_rp2040(
        name,
        rp2040_binary,
        **kwargs):
    """Flash the binary to a connected rp2040 device.

    Args:
      name: The name of this flash_rp2040 rule.
      rp2040_binary: The binary target to flash.
      **kwargs: Forwarded to the underlying native_binary rule.
    """
    data = [rp2040_binary]
    args = ["$(rootpath " + rp2040_binary + ")"]

    native_binary(
        name = name,
        src = str(Label("//targets/rp2040/py:flash")),
        args = args,
        data = data,
        # Note: out is mandatory in older bazel-skylib versions.
        out = name + ".exe",
        **kwargs
    )
