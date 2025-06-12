# Copyright 2024 The Pigweed Authors
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
"""A rule for generated the rust code to initializing the processes and threads
for a system.
"""

def _system_codegen_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.name + ".rs")
    args = [
        "--config",
        ctx.file.system_config.path,
        "--output",
        output.path,
        "codegen-system",
    ]

    ctx.actions.run(
        inputs = ctx.files.system_config,
        outputs = [output],
        executable = ctx.executable.system_generator,
        mnemonic = "CodegenSystem",
        arguments = args,
    )

    return [DefaultInfo(files = depset([output]))]

system_codegen = rule(
    implementation = _system_codegen_impl,
    attrs = {
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
    },
    doc = "Codegen system sources from system config",
)
