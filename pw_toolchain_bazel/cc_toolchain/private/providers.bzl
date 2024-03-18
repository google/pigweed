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
    "@rules_cc//cc:cc_toolchain_config_lib.bzl",
    "FlagGroupInfo",
)
load("//actions:providers.bzl", "ActionNameInfo", "ActionNameSetInfo")

visibility(["//cc_toolchain", "//cc_toolchain/tests/..."])

# Note that throughout this file, we never use a list. This is because mutable
# types cannot be stored in depsets. Thus, we type them as a sequence in the
# provider, and convert them to a tuple in the constructor to ensure
# immutability.

PwActionNameInfo = ActionNameInfo
PwActionNameSetInfo = ActionNameSetInfo

PwFlagGroupInfo = FlagGroupInfo
PwFlagSetInfo = provider(
    doc = "A type-safe version of @bazel_tools's FlagSetInfo",
    fields = {
        "label": "Label: The label that defined this flag set. Put this in error messages for easy debugging",
        "actions": "Sequence[str]: The set of actions this is associated with",
        "requires_any_of": "Sequence[FeatureConstraintInfo]: This will be enabled if any of the listed predicates are met. Equivalent to with_features",
        "flag_groups": "Sequence[FlagGroupInfo]: Set of flag groups to include.",
        "env": "Mapping[str, str]: Environment variables to apply with the flags",
        "env_expand_if_available": "Option[str]: The build variable that needs to be available in order to expand the env entry.",
    },
)

PwFeatureInfo = provider(
    doc = "A type-safe version of @bazel_tools's FeatureInfo",
    fields = {
        "label": "Label: The label that defined this feature. Put this in error messages for easy debugging",
        "name": "str: The name of the feature",
        "enabled": "bool: Whether this feature is enabled by default",
        "flag_sets": "depset[FlagSetInfo]: Flag sets enabled by this feature",
        "implies_features": "depset[FeatureInfo]: Set of features implied by this feature",
        "implies_action_configs": "depset[ActionConfigInfo]: Set of action configs enabled by this feature",
        "requires_any_of": "Sequence[FeatureSetInfo]: A list of feature sets, at least one of which is required to enable this feature. This is semantically equivalent to the requires attribute of rules_cc's FeatureInfo",
        "provides": "Sequence[str]: Indicates that this feature is one of several mutually exclusive alternate features.",
        "known": "bool: Whether the feature is a known feature. Known features are assumed to be defined elsewhere.",
        "overrides": "Optional[FeatureInfo]: The feature that this overrides",
    },
)
PwFeatureSetInfo = provider(
    doc = "A type-safe version of @bazel_tools's FeatureSetInfo",
    fields = {
        "features": "depset[FeatureInfo]: The set of features this corresponds to",
    },
)
PwFeatureConstraintInfo = provider(
    doc = "A type-safe version of @bazel_tools's WithFeatureSetInfo",
    fields = {
        "label": "Label: The label that defined this predicate. Put this in error messages for easy debugging",
        "all_of": "depset[FeatureInfo]: A set of features which must be enabled",
        "none_of": "depset[FeatureInfo]: A set of features, none of which can be enabled",
    },
)
PwBuiltinFeatureInfo = provider(
    doc = "A tag marking something as a known feature. The only use of this is to ensure that pw_cc_feature disallows override = <non known feature>.",
    fields = {},
)
PwMutuallyExclusiveCategoryInfo = provider(
    doc = "Multiple features with the category will be mutally exclusive",
    fields = {
        "name": "str: The name of the provider",
    },
)

PwActionConfigInfo = provider(
    doc = "A type-safe version of @bazel_tools's ActionConfigInfo",
    fields = {
        "label": "Label: The label that defined this action config. Put this in error messages for easy debugging",
        "action_name": "str: The name of the action",
        "enabled": "bool: If True, this action is enabled unless a rule type explicitly marks it as unsupported",
        "tools": "Sequence[ToolInfo]: The tool applied to the action will be the first tool in the sequence with a feature set that matches the feature configuration",
        "flag_sets": "Sequence[FlagSetInfo]: Set of flag sets the action sets",
        "implies_features": "depset[FeatureInfo]: Set of features implied by this action config",
        "implies_action_configs": "depset[ActionConfigInfo]: Set of action configs enabled by this action config",
        "files": "depset[File]: The files required to run these actions",
    },
)

PwActionConfigSetInfo = provider(
    doc = "A set of action configs",
    fields = {
        "label": "Label: The label that defined this action config set. Put this in error messages for easy debugging",
        "action_configs": "depset[ActionConfigInfo]: A set of action configs",
    },
)

PwToolInfo = provider(
    doc = "A type-safe version of @bazel_tool's ToolInfo",
    fields = {
        "label": "Label: The label that defined this tool",
        "exe": "Optional[File]: The file corresponding to the tool",
        "path": "Optional[str]: The path to the tool. Prefer tool (mutually exclusive with tool).",
        "files": "Depset[File]: The files associated with the tool",
        "requires_any_of": "Sequence[PwFeatureConstraintInfo]: A set of constraints required to enable the tool. Equivalent to with_features",
        "execution_requirements": "Sequence[str]: A set of execution requirements of the tool",
    },
)

PwExtraActionFilesInfo = provider(
    doc = "Extra files to provide to an action",
    fields = {
        "action": "str: The action to associate with",
        "files": "depset[File]: Files to add to this action",
    },
)
PwExtraActionFilesSetInfo = provider(
    doc = "Set of PwExtraActionFilesInfo",
    fields = {
        "srcs": "depset[PwExtraActionFilesInfo]: Sets of action files",
    },
)

PwToolchainConfigInfo = provider(
    doc = "Additional metadata about the config of the pigweed toolchain.",
    fields = {
        "action_to_files": "dict[str, depset[File]]: A set of files required to execute a given action",
    },
)
