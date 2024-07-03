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
"""Functions for merging flags for platforms and transitions."""

def merge_flags(base, override):
    """Merge flags for platforms.

    See https://pigweed.dev/bazel_compatibility.html#provide-default-backend-collections-as-dicts
    for a usage example.

    Args:
       base: Dict of flags to use as the starting point. Typically one of
          the dictionaries provided in @pigweed//pw_build:backends.bzl.
       override: Dict of overrides or additions to the base dict.

    Returns:
       A list of flags in a format accepted by the `flags` attribute of `platform`.
    """
    flags = base | override
    return ["--{}={}".format(k, v) for k, v in flags.items()]

def merge_flags_for_transition_impl(base, override):
    """Merge flags for a transition's impl function.

    Args:
       base: Dict of flags to use as the starting point. Typically one of
          the dictionaries provided in @pigweed//pw_build:backends.bzl.
       override: Dict of overrides or additions to the base dict.
    """
    return base | override

def merge_flags_for_transition_outputs(base, override):
    """Merge flags for a transition's output function.

    Args:
       base: Dict of flags to use as the starting point. Typically one of
          the dictionaries provided in @pigweed//pw_build:backends.bzl.
       override: Dict of overrides or additions to the base dict.
    """
    return (base | override).keys()
