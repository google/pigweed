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
"""All shared providers that act as an API between toolchain-related rules."""

# To reduce the number of require pw_cc_action_config rules, a
# pw_cc_action_config provides a list of ActionConfigInfo providers rather than
# a simpler 1:1 mapping.
ActionConfigListInfo = provider(
    doc = "A provider containing a list of ActionConfigInfo providers.",
    fields = {
        "action_configs": "List[ActionConfigInfo]: A list of ActionConfigInfo providers.",
    },
)

ToolchainFeatureInfo = provider(
    doc = "A provider containing cc_toolchain features and related fields.",
    fields = {
        "feature": "feature: A group of build flags structured as a toolchain feature.",
        "cxx_builtin_include_directories": "List[str]: Builtin C/C++ standard library include directories.",
        "builtin_sysroot": "str: Path to the sysroot directory. Use `external/[repo_name]` for sysroots provided as an external repository.",
    },
)
