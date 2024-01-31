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

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "ActionConfigInfo",
    "EnvEntryInfo",
    "EnvSetInfo",
    "FeatureInfo",
    "FeatureSetInfo",
    "FlagGroupInfo",
    "FlagSetInfo",
    "ToolInfo",
    "WithFeatureSetInfo",
)
load("//actions:providers.bzl", "ActionNameInfo", "ActionNameSetInfo")

visibility(["//cc_toolchain", "//tests/..."])

# Note that throughout this file, we never use a list. This is because mutable
# types cannot be stored in depsets. Thus, we type them as a sequence in the
# provider, and convert them to a tuple in the constructor to ensure
# immutability.

# To reduce the number of require pw_cc_action_config rules, a
# pw_cc_action_config provides a list of ActionConfigInfo providers rather than
# a simpler 1:1 mapping.
PwActionConfigListInfo = provider(
    doc = "A provider containing a list of ActionConfigInfo providers.",
    fields = {
        "action_configs": "List[ActionConfigInfo]: A list of ActionConfigInfo providers.",
    },
)

PwActionNameInfo = ActionNameInfo
PwActionNameSetInfo = ActionNameSetInfo

PwFlagGroupInfo = FlagGroupInfo
PwFlagSetInfo = FlagSetInfo

PwEnvEntryInfo = EnvEntryInfo
PwEnvSetInfo = EnvSetInfo

PwFeatureInfo = FeatureInfo
PwFeatureSetInfo = FeatureSetInfo
PwFeatureConstraintInfo = WithFeatureSetInfo

PwActionConfigInfo = ActionConfigInfo
PwToolInfo = ToolInfo
