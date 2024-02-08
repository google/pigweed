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
"""Implementation of pw_cc_action_files."""

load(
    ":providers.bzl",
    "PwActionNameSetInfo",
    "PwExtraActionFilesInfo",
    "PwExtraActionFilesSetInfo",
)

def _pw_cc_action_files_impl(ctx):
    actions = depset(transitive = [
        action[PwActionNameSetInfo].actions
        for action in ctx.attr.actions
    ]).to_list()
    files = depset(ctx.files.srcs)
    return [
        PwExtraActionFilesSetInfo(srcs = depset([
            PwExtraActionFilesInfo(action = action, files = files)
            for action in actions
        ])),
        DefaultInfo(files = files),
    ]

pw_cc_action_files = rule(
    implementation = _pw_cc_action_files_impl,
    attrs = {
        "actions": attr.label_list(
            providers = [PwActionNameSetInfo],
            mandatory = True,
            doc = "The actions that the files are required for",
        ),
        "srcs": attr.label_list(
            allow_files = True,
            doc = "Files required for these actions to correctly operate.",
        ),
    },
    provides = [PwExtraActionFilesSetInfo],
)

def _pw_cc_action_files_set_impl(ctx):
    return [
        PwExtraActionFilesSetInfo(srcs = depset(transitive = [
            ffa[PwExtraActionFilesSetInfo].srcs
            for ffa in ctx.attr.srcs
        ])),
        DefaultInfo(files = depset(transitive = [
            src[DefaultInfo].files
            for src in ctx.attr.srcs
        ])),
    ]

pw_cc_action_files_set = rule(
    implementation = _pw_cc_action_files_set_impl,
    attrs = {
        "srcs": attr.label_list(
            providers = [PwExtraActionFilesSetInfo],
            doc = "The pw_cc_action_files to combine.",
        ),
    },
    provides = [PwExtraActionFilesSetInfo],
)
