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
"""Sample pw_env_setup project action plugin.

A sample/starter project action plugin template for pw_env_setup.
"""


def run_action(**kwargs):
    """Sample project action."""
    if "env" not in kwargs:
        raise ValueError(f"Missing required kwarg 'env', got %{kwargs}")

    kwargs["env"].prepend("PATH", "PATH_TO_NEW_TOOLS")
    raise NotImplementedError("Sample project action running!")
