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

"""
Transitions used when building rust std libraries from source
"""

def _no_stdlibs_transition_impl(_, __):
    return {"//pw_toolchain/rust:stdlibs_flavor": "none"}

_no_stdlibs_transition = transition(
    implementation = _no_stdlibs_transition_impl,
    inputs = [],
    outputs = ["//pw_toolchain/rust:stdlibs_flavor"],
)

def _core_only_transition_impl(_, __):
    return {"//pw_toolchain/rust:stdlibs_flavor": "core_only"}

_core_only_transition = transition(
    implementation = _core_only_transition_impl,
    inputs = [],
    outputs = ["//pw_toolchain/rust:stdlibs_flavor"],
)

def _forward_and_symlink(ctx):
    outs = []
    for file in ctx.files.deps:
        # Append the label name to the file name but keep the same extension. i.e.
        # "<file>.<extension>" -> "<file>_<label>.<extension>"
        stem = file.basename.removesuffix(".{}".format(file.extension))
        out = ctx.actions.declare_file("{}_{}.{}".format(stem, ctx.label.name, file.extension))
        ctx.actions.symlink(output = out, target_file = file)
        outs.append(out)
    return [DefaultInfo(files = depset(outs))]

build_with_no_stdlibs = rule(
    implementation = _forward_and_symlink,
    cfg = _no_stdlibs_transition,
    attrs = {
        "deps": attr.label_list(allow_files = True, mandatory = True),
        # Mandatory attribute for rules with transition.
        "_allowlist_function_transition": attr.label(
            default = Label("@bazel_tools//tools/allowlists/function_transition_allowlist"),
        ),
    },
)

build_with_core_only = rule(
    implementation = _forward_and_symlink,
    cfg = _core_only_transition,
    attrs = {
        "deps": attr.label_list(allow_files = True, mandatory = True),
        # Mandatory attribute for rules with transition.
        "_allowlist_function_transition": attr.label(
            default = Label("@bazel_tools//tools/allowlists/function_transition_allowlist"),
        ),
    },
)
