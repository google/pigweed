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

def format(target):
    """Reads CcInfo from cquery command and returns header paths for a target

    Args:
        target: Target to get headers from
    Returns:
        List of targets and their header paths.
    """

    prov = providers(target)
    label = "//" + target.label.package + ":" + target.label.name
    if prov == None:
        return [label, []]
    if "CcInfo" in prov:
        files = prov["CcInfo"].compilation_context.direct_public_headers
        return [label, [f.path for f in files]]
    else:
        return [label, []]
