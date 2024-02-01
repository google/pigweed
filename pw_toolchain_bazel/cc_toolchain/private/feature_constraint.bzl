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
"""Implementation of the pw_cc_feature_constraint rule."""

load(":providers.bzl", "PwFeatureConstraintInfo", "PwFeatureSetInfo")

visibility("//cc_toolchain")

def _pw_cc_feature_constraint_impl(ctx):
    all_of = [fp[PwFeatureConstraintInfo] for fp in ctx.attr.all_of]
    none_of = [
        fs[PwFeatureSetInfo].features
        for fs in ctx.attr.none_of
    ]
    none_of.extend([fp.none_of for fp in all_of])
    return [PwFeatureConstraintInfo(
        all_of = depset(transitive = [fp.all_of for fp in all_of]),
        none_of = depset(transitive = none_of),
    )]

pw_cc_feature_constraint = rule(
    implementation = _pw_cc_feature_constraint_impl,
    attrs = {
        "all_of": attr.label_list(
            providers = [PwFeatureConstraintInfo],
        ),
        "none_of": attr.label_list(
            providers = [PwFeatureSetInfo],
        ),
    },
    provides = [PwFeatureConstraintInfo],
)
