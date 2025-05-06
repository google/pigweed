# Copyright 2025 The Pigweed Authors
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
"""Finds and returns header paths for given targets"""

def _get_transitive_header_paths(target):
    target_providers = providers(target)
    if target_providers == None:
        return []

    if "CcInfo" not in target_providers:
        return []  # Target doesn't provide CcInfo

    cc_info = target_providers["CcInfo"]

    # Check essential attributes exist before proceeding
    if not hasattr(cc_info, "compilation_context") or \
       not cc_info.compilation_context or \
       not hasattr(cc_info.compilation_context, "headers") or \
       not cc_info.compilation_context.headers:
        return []  # Missing data needed to get headers

    headers_depset = cc_info.compilation_context.headers

    # Basic check if it's a depset - Bazel will error on to_list() if not
    if type(headers_depset) != "depset":
        return []

    # Convert depset to list of paths.
    header_paths = [f.path for f in headers_depset.to_list()]
    return header_paths

def format(target):
    """Formats the output for cquery."""
    label_str = str(target.label)
    headers = _get_transitive_header_paths(target)

    # Ensure output is always a list [string, list]
    return [label_str, headers if headers != None else []]
