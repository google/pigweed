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
"""A rule for constructing a pw_kernel system image consisting of the kernel
and any apps.
"""

def _system_image_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.name)

    args = [
        "--kernel",
        ctx.files.kernel[0].path,
        "--output",
        output.path,
    ]
    for f in ctx.files.apps:
        args.append("--app")
        args.append(f.path)

    ctx.actions.run(
        inputs = ctx.files.kernel + ctx.files.apps,
        outputs = [output],
        executable = ctx.executable._system_assembler,
        mnemonic = "SystemImageAssembler",
        arguments = args,
    )
    return [DefaultInfo(executable = output)]

system_image = rule(
    implementation = _system_image_impl,
    executable = True,
    attrs = {
        "apps": attr.label_list(
            doc = "list of application images",
            cfg = "target",
            mandatory = True,
        ),
        "kernel": attr.label(
            doc = "kernel image",
            cfg = "target",
            allow_single_file = True,
            mandatory = True,
        ),
        "_system_assembler": attr.label(
            executable = True,
            cfg = "exec",
            default = "//pw_kernel/tooling:system_assembler",
        ),
    },
    doc = "Assemble a system image combining apps and kernel",
)
