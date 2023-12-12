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
"""Implementation of `pw_cc_action_config_file_collector`.

This library is intended to be a private implementation detail of
pw_toolchain_bazel, DO NOT export the contents of this file to be used publicly.
"""

load("@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl", "ActionConfigInfo")
load("//cc_toolchain/private:providers.bzl", "ActionConfigListInfo")

def _pw_cc_action_config_file_collector_impl(ctx):
    all_file_depsets = []
    for dep in ctx.attr.all_action_configs:
        action_names = []
        if ActionConfigInfo in dep:
            action_names.append(dep[ActionConfigInfo].action_name)
        if ActionConfigListInfo in dep:
            action_names.extend([ac.action_name for ac in dep[ActionConfigListInfo].action_configs])

        # NOTE: This intentionally doesn't do a check to ensure that the
        # items in `action_names` are `pw_cc_action_config`s because the
        # `pw_cc_toolchain_config` does the same check and will surface a more
        # contextually useful error. Instead, we silently but safely continue
        # even if `action_names` ends up empty.

        for action_name in action_names:
            if (action_name in ctx.attr.collect_files_from_actions and
                DefaultInfo in dep):
                all_file_depsets.append(dep[DefaultInfo].files)

    # Tag on the `filegroup` specified in `extra_files` if specified.
    if ctx.attr.extra_files:
        if DefaultInfo not in ctx.attr.extra_files:
            fail(
                "{} in `extra_files` does not provide any files".format(
                    ctx.attr.extra_files.label,
                ),
            )
        all_file_depsets.append(ctx.attr.extra_files[DefaultInfo].files)

    return [DefaultInfo(files = depset(None, transitive = all_file_depsets))]

pw_cc_action_config_file_collector = rule(
    implementation = _pw_cc_action_config_file_collector_impl,
    attrs = {
        "all_action_configs": attr.label_list(default = []),
        "collect_files_from_actions": attr.string_list(mandatory = True),
        "extra_files": attr.label(),
    },
)
