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
"""Bazel macros for launching the pw_system console."""

load("@bazel_skylib//rules:native_binary.bzl", "native_binary")

def device_simulator_console(
        name,
        script,
        host_binary,
        pw_console_config = None,
        socket_addr = "default",
        **kwargs):
    """Launch the host_binary and connect to it with the pw_system console.

    This macro is a wrapper around bazel_skylib native_binary to
    launch a device_sim.py script.

    Args:
      name: The name of this device_simulator_console rule.
      script: The py_binary target that calls upstream Pigweed's
        pw_console.device_sim main or main_with_compiled_protos. See:
        https://cs.opensource.google/pigweed/pigweed/+/main:pw_system/py/pw_system/device_sim.py
      host_binary: The host_device_simulator_binary target to run.
      socket_addr: Optional --socket-addr value to pass to pw-system-console.
      pw_console_config: Optional file group target containing a
        pw_console.yaml config file. For example:

          filegroup(
              name = "pw_console_config",
              srcs = [".pw_console.yaml"],
          )

      **kwargs: Passed to native_binary.
    """
    data = [host_binary]
    args = [
        "--sim-binary",
        "$(rootpath " + host_binary + ")",
        "--socket-addr",
        socket_addr,
    ]
    if pw_console_config:
        data.append(pw_console_config)
        args.append("--config-file")
        args.append("$(rootpath " + pw_console_config + ")")

    native_binary(
        name = name,
        src = script,
        args = args,
        data = data,
        # Note: out is mandatory in older bazel-skylib versions.
        out = name + ".exe",
        **kwargs
    )
