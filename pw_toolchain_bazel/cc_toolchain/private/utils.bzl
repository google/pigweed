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
"""Private utilities and global variables."""

load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    rules_cc_action_config = "action_config",
    rules_cc_feature = "feature",
    rules_cc_feature_set = "feature_set",
    rules_cc_flag_set = "flag_set",
    rules_cc_tool = "tool",
    rules_cc_with_feature_set = "with_feature_set",
)
load(":providers.bzl", "PwFlagSetInfo")

visibility(["//cc_toolchain/tests/..."])

ALL_FILE_GROUPS = {
    "ar_files": ["@pw_toolchain//actions:all_ar_actions"],
    "as_files": ["@pw_toolchain//actions:all_asm_actions"],
    "compiler_files": ["@pw_toolchain//actions:all_compiler_actions"],
    "coverage_files": ["@pw_toolchain//actions:llvm_cov"],
    "dwp_files": [],
    "linker_files": ["@pw_toolchain//actions:all_link_actions"],
    "objcopy_files": ["@pw_toolchain//actions:objcopy_embed_data"],
    "strip_files": ["@pw_toolchain//actions:strip"],
}

def _ensure_fulfillable(any_of, known, label, fail = fail):
    # Requirements can be fulfilled if there are no requirements.
    fulfillable = not any_of
    for group in any_of:
        all_met = True
        for entry in group.to_list():
            if entry.label not in known:
                all_met = False
                break
        if all_met:
            fulfillable = True
            break

    if not fulfillable:
        fail("%s cannot possibly be enabled (none of the constraints it requires fully exist). Either remove it from your toolchain, or add the requirements." % label)

def _to_untyped_flag_set(flag_set, known, fail = fail):
    """Converts a PwFlagSet to rules_cc's flag set."""
    _ensure_fulfillable(
        any_of = [constraint.all_of for constraint in flag_set.requires_any_of],
        known = known,
        label = flag_set.label,
        fail = fail,
    )
    return rules_cc_flag_set(
        actions = list(flag_set.actions),
        flag_groups = list(flag_set.flag_groups),
        with_features = [
            _to_untyped_feature_constraint(fc)
            for fc in flag_set.requires_any_of
        ],
    )

def _to_untyped_env_set(env_set):
    return env_set

def _to_untyped_feature_set(feature_set):
    return rules_cc_feature_set([
        feature.name
        for feature in feature_set.features.to_list()
    ])

def _to_untyped_feature_constraint(feature_constraint):
    return rules_cc_with_feature_set(
        features = [ft.name for ft in feature_constraint.all_of.to_list()],
        not_features = [ft.name for ft in feature_constraint.none_of.to_list()],
    )

def _to_untyped_implies(provider, known, fail = fail):
    implies = []
    for feature in provider.implies_features.to_list():
        if feature.label not in known:
            fail("%s implies %s, which is not explicitly mentioned in your toolchain configuration" % (provider.label, feature.label))
        implies.append(feature.name)
    for action_config in provider.implies_action_configs.to_list():
        if action_config.label not in known:
            fail("%s implies %s, which is not explicitly mentioned in your toolchain configuration" % (provider.label, action_config.label))
        implies.append(action_config.action_name)
    return implies

def _to_untyped_feature(feature, known, fail = fail):
    if feature.known:
        return None

    _ensure_fulfillable(
        any_of = [fs.features for fs in feature.requires_any_of],
        known = known,
        label = feature.label,
        fail = fail,
    )

    return rules_cc_feature(
        name = feature.name,
        enabled = feature.enabled,
        flag_sets = [
            _to_untyped_flag_set(flag_set, known, fail = fail)
            for flag_set in feature.flag_sets.to_list()
        ],
        env_sets = [
            _to_untyped_env_set(env_set)
            for env_set in feature.env_sets.to_list()
        ],
        implies = _to_untyped_implies(feature, known, fail = fail),
        requires = [_to_untyped_feature_set(requirement) for requirement in feature.requires_any_of],
        provides = list(feature.provides),
    )

def _to_untyped_tool(tool, known, fail = fail):
    _ensure_fulfillable(
        any_of = [constraint.all_of for constraint in tool.requires_any_of],
        known = known,
        label = tool.label,
        fail = fail,
    )

    return rules_cc_tool(
        path = tool.path,
        tool = tool.exe,
        execution_requirements = list(tool.execution_requirements),
        with_features = [
            _to_untyped_feature_constraint(fc)
            for fc in tool.requires_any_of
        ],
    )

def _to_untyped_action_config(action_config, extra_flag_sets, known, fail = fail):
    # De-dupe, in case the same flag set was specified for both unconditional
    # and for a specific action config.
    flag_sets = depset(
        list(action_config.flag_sets) + extra_flag_sets,
        order = "preorder",
    ).to_list()

    return rules_cc_action_config(
        action_name = action_config.action_name,
        enabled = action_config.enabled,
        tools = [
            _to_untyped_tool(tool, known, fail = fail)
            for tool in action_config.tools
        ],
        flag_sets = [
            _to_untyped_flag_set(
                # Make the flag sets actionless.
                PwFlagSetInfo(
                    label = flag_set.label,
                    actions = tuple(),
                    requires_any_of = flag_set.requires_any_of,
                    flag_groups = flag_set.flag_groups,
                ),
                known = known,
                fail = fail,
            )
            for flag_set in flag_sets
        ],
        implies = _to_untyped_implies(action_config, known, fail = fail),
    )

def to_untyped_config(feature_set, action_config_set, flag_sets, extra_action_files, fail = fail):
    """Converts Pigweed providers into a format suitable for rules_cc.

    Args:
        feature_set: PwFeatureSetInfo: Features available in the toolchain
        action_config_set: ActionConfigSetInfo: Set of defined action configs
        flag_sets: Flag sets that are unconditionally applied
        extra_action_files: Files to be added to actions
        fail: The fail function. Only change this during testing.
    Returns:
        A struct containing parameters suitable to pass to
          cc_common.create_cc_toolchain_config_info.
    """
    flag_sets_by_action = {}
    for flag_set in flag_sets:
        for action in flag_set.actions:
            flag_sets_by_action.setdefault(action, []).append(flag_set)

    known_labels = {}
    known_feature_names = {}
    feature_list = feature_set.features.to_list()
    for feature in feature_list:
        known_labels[feature.label] = None
        existing_feature = known_feature_names.get(feature.name, None)
        if existing_feature != None and feature.overrides != existing_feature and existing_feature.overrides != feature:
            fail("Conflicting features: %s and %s both have feature name %s" % (feature.label, existing_feature.label, feature.name))

        known_feature_names[feature.name] = feature

    untyped_features = []
    for feature in feature_list:
        untyped_feature = _to_untyped_feature(feature, known = known_labels, fail = fail)
        if untyped_feature != None:
            untyped_features.append(untyped_feature)

    acs = action_config_set.action_configs.to_list()
    known_actions = {}
    untyped_acs = []
    for ac in acs:
        if ac.action_name in known_actions:
            fail("In %s, both %s and %s implement %s" % (
                action_config_set.label,
                ac.label,
                known_actions[ac.action_name],
                ac.action_name,
            ))
        known_actions[ac.action_name] = ac.label
        untyped_acs.append(_to_untyped_action_config(
            ac,
            extra_flag_sets = flag_sets_by_action.get(ac.action_name, []),
            known = known_labels,
            fail = fail,
        ))

    action_to_files = {
        ac.action_name: [ac.files]
        for ac in acs
    }
    for ffa in extra_action_files.srcs.to_list():
        action_to_files.setdefault(ffa.action, []).append(ffa.files)

    return struct(
        features = untyped_features,
        action_configs = untyped_acs,
        action_to_files = {k: depset(transitive = v) for k, v in action_to_files.items()},
    )
