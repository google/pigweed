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
"""Implementation of the pw_cc_unsafe_feature rule."""

load(
    ":providers.bzl",
    "PwBuiltinFeatureInfo",
    "PwFeatureConstraintInfo",
    "PwFeatureInfo",
    "PwFeatureSetInfo",
)

def _pw_cc_unsafe_feature_impl(ctx):
    feature = PwFeatureInfo(
        label = ctx.label,
        name = ctx.attr.feature_name,
        enabled = False,
        flag_sets = depset([]),
        implies_features = depset([]),
        implies_action_configs = depset([]),
        requires_any_of = tuple([]),
        provides = depset([]),
        known = True,
        overrides = None,
    )
    providers = [
        feature,
        PwFeatureSetInfo(features = depset([feature])),
        PwFeatureConstraintInfo(all_of = depset([feature]), none_of = depset([])),
    ]
    if ctx.attr.builtin:
        providers.append(PwBuiltinFeatureInfo())
    return providers

pw_cc_unsafe_feature = rule(
    implementation = _pw_cc_unsafe_feature_impl,
    attrs = {
        "feature_name": attr.string(
            mandatory = True,
            doc = "The name of the feature",
        ),
        "builtin": attr.bool(
            doc = "Whether the feature is builtin, and can be overridden",
        ),
    },
    provides = [PwFeatureInfo, PwFeatureSetInfo, PwFeatureConstraintInfo],
    doc = "A declaration that a feature with this name is defined elsewhere.",
)
