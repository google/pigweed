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
"""A rule for generating the application linker script based on the system
configuration file.
"""

load("@rules_cc//cc/common:cc_common.bzl", "cc_common")
load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")

def _app_linker_script_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.name + ".ld")

    args = [
        "--template",
        "app=" + ctx.file.template.path,
        "--config",
        ctx.file.system_config.path,
        "--output",
        output.path,
        "app-linker-script",
        "--app-name",
        ctx.attr.app_name,
    ]

    ctx.actions.run(
        inputs = ctx.files.system_config + [ctx.file.template],
        outputs = [output],
        executable = ctx.executable.system_generator,
        mnemonic = "AppLinkerScript",
        arguments = args,
    )

    linker_input = cc_common.create_linker_input(
        owner = ctx.label,
        user_link_flags = ["-T", output.path],
        additional_inputs = depset(direct = [output]),
    )
    linking_context = cc_common.create_linking_context(
        linker_inputs = depset(direct = [linker_input]),
    )
    return [
        DefaultInfo(files = depset([output])),
        CcInfo(linking_context = linking_context),
    ]

_app_linker_script = rule(
    implementation = _app_linker_script_impl,
    attrs = {
        "app_name": attr.string(
            doc = "Name of the application in the configuration file.",
            mandatory = True,
        ),
        "system_config": attr.label(
            doc = "System config file which defines the system.",
            allow_single_file = True,
            mandatory = True,
        ),
        "system_generator": attr.label(
            executable = True,
            cfg = "exec",
            default = "//pw_kernel/tooling/system_generator:system_generator_bin",
        ),
        "template": attr.label(
            doc = "Application linker script template file.",
            allow_single_file = True,
            mandatory = True,
        ),
    },
    doc = "Generate the linker script for an app based on the system config.",
)

def app_linker_script(name, system_config, app_name, **kwargs):
    # buildifier: disable=function-docstring-args
    """
    Wrapper function to set default platform specific arguments.
    """
    if kwargs.get("target_compatible_with") == None:
        kwargs["target_compatible_with"] = select({
            "//pw_kernel/target:system_config_not_set": ["@platforms//:incompatible"],
            "//conditions:default": [],
        })

    if kwargs.get("template") == None:
        template = select({
            "@platforms//cpu:armv8-m": "//pw_kernel/tooling/system_generator/templates:armv8m_app.ld.tmpl",
            "@platforms//cpu:riscv32": "//pw_kernel/tooling/system_generator/templates:riscv_app.ld.tmpl",
            "//conditions:default": None,
        })
        kwargs["template"] = template

    _app_linker_script(
        name = name,
        system_config = system_config,
        app_name = app_name,
        **kwargs
    )
