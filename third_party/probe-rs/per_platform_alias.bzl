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

"""Defines the per_platform_alias rule."""

load("@local_config_platform//:constraints.bzl", "HOST_CONSTRAINTS")

def _get_host_platform():
    """Return the <cpu>-<os> platform string (e.g. x86_64-linux)."""

    # HACK: HOST_CONSTRAINTS is a list containing the CPU constraint
    # followed by the OS constraint. We use this to figure out the
    # current CPU+OS pair.
    if len(HOST_CONSTRAINTS) != 2:
        fail("Unexpected HOST_CONSTRAINTS: " + HOST_CONSTRAINTS)
    cpu_constraint, os_constraint = HOST_CONSTRAINTS
    cpu = cpu_constraint.removeprefix("@platforms//cpu:")
    os = os_constraint.removeprefix("@platforms//os:")
    return cpu + "-" + os

def per_platform_alias(name, platform_to_label):
    """Aliases to a different target depending on platform.

    Args:
      name: The name of the resulting target.
      platform_to_label: A mapping from "<cpu>-<os>" platform string
        (e.g. x86_64-linux") to label that you wish to alias.
    """
    host_platform = _get_host_platform()
    if not host_platform in platform_to_label:
        fail("No alias provided for platform " + host_platform)
    label = platform_to_label[host_platform]
    native.alias(name = name, actual = label)
